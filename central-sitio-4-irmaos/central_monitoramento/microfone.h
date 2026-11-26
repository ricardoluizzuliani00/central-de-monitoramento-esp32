#ifndef MICROFONE_H
#define MICROFONE_H

#include <Arduino.h>
#include <driver/i2s_std.h>   // driver I2S NOVO (nao usar driver/i2s.h)
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <math.h>

#include "firebase_secrets.h"

#define MIC_I2S_PORT I2S_NUM_0

#define MIC_PIN_SCK  5
#define MIC_PIN_WS   6
#define MIC_PIN_SD   7

#define MIC_SAMPLE_RATE 16000


// ======================================================
// MODO DE GRAVACAO
// 1 = a gravacao comeca a valer quando voce fala e PARA
//     quando voce fica em silencio (ate MIC_TEMPO_MAXIMO s).
//     Frases curtas ficam mais rapidas e frases longas nao
//     sao mais cortadas aos 3 segundos.
// 0 = comportamento anterior: sempre 3 segundos fixos.
// ======================================================

#define MIC_GRAVACAO_DINAMICA 1

#define MIC_TEMPO_GRAVACAO 3     // modo fixo (DINAMICA = 0)
#define MIC_TEMPO_MAXIMO   7     // maximo no modo dinamico (s)

#if MIC_GRAVACAO_DINAMICA
  #define MIC_TOTAL_AMOSTRAS (MIC_SAMPLE_RATE * MIC_TEMPO_MAXIMO)
#else
  #define MIC_TOTAL_AMOSTRAS (MIC_SAMPLE_RATE * MIC_TEMPO_GRAVACAO)
#endif


// ======================================================
// GANHO E LIMPEZA DO SINAL
// ======================================================

// O INMP441 entrega 24 bits dentro de 32. Para 16 bits o
// correto e deslocar 16 (antes era 14, o que multiplicava o
// som por 4 e fazia o pico bater no maximo, 32767).
//   16 = ganho normal
//   15 = 2x mais alto | 14 = 4x (como era antes)
#define MIC_DESLOCAMENTO_BITS 16

// Filtro que remove o "nivel fixo" (DC) do microfone. Sem ele,
// o programa achava que sempre havia som.
#define MIC_DC_FILTRO 0.002f

// Se, depois da gravacao, o pico ficar baixo, o som e
// amplificado (no maximo MIC_GANHO_MAXIMO vezes).
#define MIC_PICO_ALVO        12000
#define MIC_PICO_MINIMO_OK   4000
#define MIC_GANHO_MAXIMO     8.0f


// ======================================================
// DETECCAO DE FALA (modo dinamico)
// ======================================================

#define MIC_BLOCO_AMOSTRAS       128   // 8 ms por bloco

// Nos primeiros 250 ms mede o ruido de fundo.
#define MIC_RUIDO_CALIBRACAO_MS  250
#define MIC_RUIDO_MAXIMO         150   // teto do ruido medido

// Limiar de voz = maior entre o minimo e (ruido x relativo).
#define MIC_LIMIAR_VOZ_MINIMO    120
#define MIC_LIMIAR_VOZ_RELATIVO  3.0f

// Blocos seguidos acima do limiar para considerar que comecou a fala.
#define MIC_BLOCOS_INICIO_VOZ    4

// Silencio depois da fala para encerrar a gravacao.
#define MIC_SILENCIO_FIM_MS      900

// Se ninguem falar nesse tempo, desiste.
#define MIC_ESPERA_FALA_MS       4000

// Sobra de audio guardada antes e depois da fala.
#define MIC_MARGEM_MS            300


// ======================================================
// VALIDACAO FINAL DO AUDIO
// ======================================================

#define MIC_LIMIAR_AMOSTRA_ATIVA 180
#define MIC_RMS_MINIMO 55.0f
#define MIC_PERCENTUAL_ATIVO_MINIMO 0.5f

enum StatusGravacao {
  GRAVACAO_OK,
  GRAVACAO_SILENCIO,
  GRAVACAO_ERRO
};

inline StatusGravacao ultimoStatusGravacao = GRAVACAO_ERRO;

enum StatusTranscricao {
  TRANSCRICAO_OK,
  TRANSCRICAO_SEM_AUDIO,
  TRANSCRICAO_SEM_WIFI,
  TRANSCRICAO_ERRO_LOCAL,
  TRANSCRICAO_ERRO_GROQ,
  TRANSCRICAO_VAZIA
};

inline StatusTranscricao ultimoStatusTranscricao = TRANSCRICAO_SEM_AUDIO;


// ======================================================
// CONEXAO COM A GROQ ABERTA ENQUANTO VOCE FALA
// Abrir a conexao segura (TLS) leva mais de 1 segundo. Em vez
// de abrir so depois da gravacao (com "..." no visor), abre
// numa tarefa separada assim que a central comeca a ouvir.
// Depois da transcricao a conexao e fechada de novo (para nao
// gastar memoria que a voz precisa).
// 1 = ativado | 0 = comportamento anterior
// ======================================================

#define GROQ_PREAQUECER 1

inline WiFiClientSecure clienteGroq;
inline volatile bool preaquecendoGroq = false;

inline void tarefaPreaquecerGroq(void* parametro) {
  clienteGroq.stop();
  clienteGroq.setInsecure();
  clienteGroq.connect("api.groq.com", 443);

  preaquecendoGroq = false;
  vTaskDelete(NULL);
}

inline void esperarPreaquecimentoGroq(unsigned long limiteMs = 6000) {
  unsigned long inicio = millis();

  while (preaquecendoGroq && millis() - inicio < limiteMs) {
    delay(10);
  }
}

inline void preaquecerGroq() {
#if GROQ_PREAQUECER
  if (WiFi.status() != WL_CONNECTED) return;
  if (preaquecendoGroq) return;
  if (clienteGroq.connected()) return;

  preaquecendoGroq = true;

  BaseType_t ok = xTaskCreatePinnedToCore(
    tarefaPreaquecerGroq,
    "groqwarm",
    10240,
    NULL,
    1,
    NULL,
    0
  );

  if (ok != pdPASS) {
    preaquecendoGroq = false;
  }
#endif
}

inline void fecharGroq() {
  esperarPreaquecimentoGroq();
  clienteGroq.stop();
}

inline int16_t* bufferAudio = nullptr;
inline size_t amostrasGravadas = 0;
inline bool microfoneDisponivel = false;
inline bool audioComVoz = false;

inline float ultimoRMSAudio = 0.0f;
inline int16_t ultimoPicoAudio = 0;
inline float ultimoPercentualAtivo = 0.0f;

// Canal I2S de recepcao (driver novo)
inline i2s_chan_handle_t micRxHandle = nullptr;

// Leitura de amostras de 32 bits do microfone.
// Usada tambem pelo wake_word.h.
inline esp_err_t micLer(
  void* destino,
  size_t bytes,
  size_t* bytesLidos
) {
  if (micRxHandle == nullptr) {
    *bytesLidos = 0;
    return ESP_ERR_INVALID_STATE;
  }

  return i2s_channel_read(
    micRxHandle,
    destino,
    bytes,
    bytesLidos,
    1000
  );
}

inline void iniciarMicrofone() {
  i2s_chan_config_t configCanal =
    I2S_CHANNEL_DEFAULT_CONFIG(MIC_I2S_PORT, I2S_ROLE_MASTER);

  configCanal.dma_desc_num = 8;
  configCanal.dma_frame_num = 256;

  esp_err_t resultado =
    i2s_new_channel(&configCanal, NULL, &micRxHandle);

  if (resultado != ESP_OK) {
    Serial.println("Erro ao iniciar I2S do microfone.");
    micRxHandle = nullptr;
    ultimoStatusGravacao = GRAVACAO_ERRO;
    return;
  }

  i2s_std_config_t configI2S = {};

  configI2S.clk_cfg =
    I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE);

  configI2S.slot_cfg =
    I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_32BIT,
      I2S_SLOT_MODE_MONO
    );

  // Canal esquerdo (L/R do INMP441 ligado ao GND)
  configI2S.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  configI2S.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  configI2S.gpio_cfg.bclk = (gpio_num_t)MIC_PIN_SCK;
  configI2S.gpio_cfg.ws   = (gpio_num_t)MIC_PIN_WS;
  configI2S.gpio_cfg.dout = I2S_GPIO_UNUSED;
  configI2S.gpio_cfg.din  = (gpio_num_t)MIC_PIN_SD;
  configI2S.gpio_cfg.invert_flags.mclk_inv = false;
  configI2S.gpio_cfg.invert_flags.bclk_inv = false;
  configI2S.gpio_cfg.invert_flags.ws_inv   = false;

  resultado =
    i2s_channel_init_std_mode(micRxHandle, &configI2S);

  if (resultado != ESP_OK) {
    Serial.println("Erro ao configurar pinos do microfone.");
    i2s_del_channel(micRxHandle);
    micRxHandle = nullptr;
    ultimoStatusGravacao = GRAVACAO_ERRO;
    return;
  }

  resultado = i2s_channel_enable(micRxHandle);

  if (resultado != ESP_OK) {
    Serial.println("Erro ao ativar canal do microfone.");
    i2s_del_channel(micRxHandle);
    micRxHandle = nullptr;
    ultimoStatusGravacao = GRAVACAO_ERRO;
    return;
  }

  bufferAudio =
    (int16_t*)ps_malloc(MIC_TOTAL_AMOSTRAS * sizeof(int16_t));

  if (bufferAudio == nullptr) {
    Serial.println("Erro ao reservar memoria na PSRAM para audio.");
    ultimoStatusGravacao = GRAVACAO_ERRO;
    return;
  }

  microfoneDisponivel = true;
  Serial.println("INMP441 configurado.");
}

inline bool analisarAudioGravado() {
  if (bufferAudio == nullptr || amostrasGravadas == 0) {
    audioComVoz = false;
    return false;
  }

  uint64_t somaQuadrados = 0;
  size_t amostrasAtivas = 0;
  int32_t pico = 0;

  for (size_t i = 0; i < amostrasGravadas; i++) {
    int32_t valor = bufferAudio[i];
    int32_t absoluto = abs(valor);

    if (absoluto > pico) {
      pico = absoluto;
    }

    somaQuadrados +=
      (uint64_t)((int64_t)valor * (int64_t)valor);

    if (absoluto >= MIC_LIMIAR_AMOSTRA_ATIVA) {
      amostrasAtivas++;
    }
  }

  float mediaQuadrados =
    (float)somaQuadrados / (float)amostrasGravadas;

  ultimoRMSAudio = sqrtf(mediaQuadrados);
  ultimoPicoAudio = (int16_t)min(pico, (int32_t)32767);
  ultimoPercentualAtivo =
    ((float)amostrasAtivas / (float)amostrasGravadas) * 100.0f;

  Serial.print("Audio RMS: ");
  Serial.println(ultimoRMSAudio, 1);

  Serial.print("Audio pico: ");
  Serial.println(ultimoPicoAudio);

  Serial.print("Audio ativo: ");
  Serial.print(ultimoPercentualAtivo, 2);
  Serial.println("%");

  bool rmsValido = ultimoRMSAudio >= MIC_RMS_MINIMO;
  bool atividadeValida =
    ultimoPercentualAtivo >= MIC_PERCENTUAL_ATIVO_MINIMO;

  audioComVoz = rmsValido && atividadeValida;

  if (!audioComVoz) {
    Serial.println("Silencio detectado.");
    return false;
  }

  Serial.println("Voz detectada.");
  return true;
}

inline bool gravarAudio() {
  ultimoStatusGravacao = GRAVACAO_ERRO;
  ultimoStatusTranscricao = TRANSCRICAO_SEM_AUDIO;

  if (!microfoneDisponivel || bufferAudio == nullptr) {
    Serial.println("Microfone indisponivel.");
    return false;
  }

#if MIC_GRAVACAO_DINAMICA
  Serial.println("Gravando audio (para quando voce terminar de falar)...");
#else
  Serial.println("Gravando audio por 3 segundos...");
#endif

  amostrasGravadas = 0;
  audioComVoz = false;
  ultimoRMSAudio = 0.0f;
  ultimoPicoAudio = 0;
  ultimoPercentualAtivo = 0.0f;

  const size_t CHUNK_SIZE = MIC_BLOCO_AMOSTRAS;
  int32_t chunk[CHUNK_SIZE];
  size_t bytesLidos = 0;

  // Filtro de nivel fixo (DC)
  float dc = 0.0f;
  bool dcPronto = false;

#if MIC_GRAVACAO_DINAMICA
  const size_t blocosCalibracao =
    ((size_t)MIC_RUIDO_CALIBRACAO_MS * MIC_SAMPLE_RATE / 1000) / CHUNK_SIZE;

  const size_t blocosSilencioFim =
    ((size_t)MIC_SILENCIO_FIM_MS * MIC_SAMPLE_RATE / 1000) / CHUNK_SIZE;

  const size_t amostrasEsperaFala =
    (size_t)MIC_ESPERA_FALA_MS * MIC_SAMPLE_RATE / 1000;

  size_t blocos = 0;
  float somaRuido = 0.0f;
  float limiarVoz = (float)MIC_LIMIAR_VOZ_MINIMO;

  bool vozIniciou = false;
  size_t blocosAcima = 0;
  size_t blocosSilencio = 0;
  size_t inicioVoz = 0;
#endif

  while (amostrasGravadas < MIC_TOTAL_AMOSTRAS) {
    size_t amostrasParaLer = CHUNK_SIZE;

    if (MIC_TOTAL_AMOSTRAS - amostrasGravadas < CHUNK_SIZE) {
      amostrasParaLer = MIC_TOTAL_AMOSTRAS - amostrasGravadas;
    }

    esp_err_t resultado = micLer(
      chunk,
      amostrasParaLer * sizeof(int32_t),
      &bytesLidos
    );

    if (resultado != ESP_OK || bytesLidos == 0) {
      Serial.println("Erro durante gravacao.");
      audioComVoz = false;
      ultimoStatusGravacao = GRAVACAO_ERRO;
      return false;
    }

    size_t qtdAmostrasLidas =
      bytesLidos / sizeof(int32_t);

    if (qtdAmostrasLidas == 0) {
      continue;
    }

    // Primeiro bloco: ajusta o filtro de DC ao nivel do microfone.
    if (!dcPronto) {
      int64_t soma = 0;

      for (size_t i = 0; i < qtdAmostrasLidas; i++) {
        soma += (chunk[i] >> MIC_DESLOCAMENTO_BITS);
      }

      dc = (float)soma / (float)qtdAmostrasLidas;
      dcPronto = true;
    }

    uint32_t somaAbs = 0;

    for (size_t i = 0; i < qtdAmostrasLidas; i++) {
      int32_t amostra = chunk[i] >> MIC_DESLOCAMENTO_BITS;

      dc += ((float)amostra - dc) * MIC_DC_FILTRO;

      int32_t valor = amostra - (int32_t)dc;

      if (valor > 32767) valor = 32767;
      if (valor < -32768) valor = -32768;

      bufferAudio[amostrasGravadas++] = (int16_t)valor;
      somaAbs += (uint32_t)abs(valor);
    }

#if MIC_GRAVACAO_DINAMICA
    float nivel = (float)somaAbs / (float)qtdAmostrasLidas;

    blocos++;

    // ---- calibracao do ruido de fundo ----
    if (blocos <= blocosCalibracao) {
      somaRuido += nivel;

      if (blocos == blocosCalibracao) {
        float ruido = somaRuido / (float)blocosCalibracao;

        if (ruido > MIC_RUIDO_MAXIMO) {
          ruido = MIC_RUIDO_MAXIMO;
        }

        limiarVoz = ruido * MIC_LIMIAR_VOZ_RELATIVO;

        if (limiarVoz < MIC_LIMIAR_VOZ_MINIMO) {
          limiarVoz = (float)MIC_LIMIAR_VOZ_MINIMO;
        }

        Serial.print("Ruido de fundo: ");
        Serial.print(ruido, 1);
        Serial.print(" | limiar de voz: ");
        Serial.println(limiarVoz, 1);
      }

      continue;
    }

    bool acima = nivel > limiarVoz;

    if (!vozIniciou) {

      if (acima) {
        blocosAcima++;

        if (blocosAcima >= MIC_BLOCOS_INICIO_VOZ) {
          vozIniciou = true;
          blocosSilencio = 0;

          size_t recuo = blocosAcima * CHUNK_SIZE;
          inicioVoz =
            (amostrasGravadas > recuo) ? (amostrasGravadas - recuo) : 0;
        }

      } else {
        blocosAcima = 0;
      }

      // Ninguem falou a tempo.
      if (!vozIniciou && amostrasGravadas >= amostrasEsperaFala) {
        break;
      }

    } else {

      if (acima) {
        blocosSilencio = 0;
      } else {
        blocosSilencio++;

        if (blocosSilencio >= blocosSilencioFim) {
          break;
        }
      }
    }
#endif
  }

#if MIC_GRAVACAO_DINAMICA
  if (!vozIniciou) {
    Serial.println("Nenhuma fala detectada.");
    audioComVoz = false;
    ultimoStatusGravacao = GRAVACAO_SILENCIO;
    return false;
  }

  // ---- recorta o silencio do comeco e do fim ----
  const size_t margem =
    (size_t)MIC_MARGEM_MS * MIC_SAMPLE_RATE / 1000;

  size_t inicio = (inicioVoz > margem) ? (inicioVoz - margem) : 0;
  size_t fim = amostrasGravadas;
  size_t silencioFinal = blocosSilencio * CHUNK_SIZE;

  if (silencioFinal > margem) {
    fim = amostrasGravadas - (silencioFinal - margem);
  }

  if (fim <= inicio) {
    fim = amostrasGravadas;
    inicio = 0;
  }

  if (inicio > 0) {
    memmove(
      bufferAudio,
      bufferAudio + inicio,
      (fim - inicio) * sizeof(int16_t)
    );
  }

  amostrasGravadas = fim - inicio;

  Serial.print("Fala gravada: ");
  Serial.print((amostrasGravadas * 1000UL) / MIC_SAMPLE_RATE);
  Serial.println(" ms");
#endif

  Serial.println("Gravacao concluida.");

  // ---- som muito baixo: amplifica ----
  {
    int32_t pico = 0;

    for (size_t i = 0; i < amostrasGravadas; i++) {
      int32_t a = abs((int32_t)bufferAudio[i]);
      if (a > pico) pico = a;
    }

    if (pico > 0 && pico < MIC_PICO_MINIMO_OK) {
      float ganho = (float)MIC_PICO_ALVO / (float)pico;

      if (ganho > MIC_GANHO_MAXIMO) {
        ganho = MIC_GANHO_MAXIMO;
      }

      for (size_t i = 0; i < amostrasGravadas; i++) {
        int32_t v = (int32_t)((float)bufferAudio[i] * ganho);

        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;

        bufferAudio[i] = (int16_t)v;
      }

      Serial.print("Ganho aplicado: ");
      Serial.println(ganho, 1);
    }
  }

  if (!analisarAudioGravado()) {
    ultimoStatusGravacao = GRAVACAO_SILENCIO;
    return false;
  }

  ultimoStatusGravacao = GRAVACAO_OK;
  return true;
}

inline String transcreverAudioGroq() {
  ultimoStatusTranscricao = TRANSCRICAO_SEM_AUDIO;

  // Espera a conexao aberta durante a gravacao (se ainda estiver
  // abrindo) e garante que ela e fechada em qualquer saida.
  esperarPreaquecimentoGroq();

  struct GuardaGroq {
    ~GuardaGroq() { clienteGroq.stop(); }
  } guardaGroq;

  if (
    ultimoStatusGravacao != GRAVACAO_OK ||
    !audioComVoz ||
    amostrasGravadas == 0 ||
    bufferAudio == nullptr
  ) {
    Serial.println("Nenhum audio valido para transcrever.");
    return "";
  }

  if (WiFi.status() != WL_CONNECTED) {
    ultimoStatusTranscricao = TRANSCRICAO_SEM_WIFI;
    Serial.println("Sem Wi-Fi para acessar a Groq.");
    return "";
  }

  Serial.println("Enviando audio para Groq...");

  uint32_t dataSize = amostrasGravadas * sizeof(int16_t);
  uint32_t chunkSize = 36 + dataSize;
  uint32_t sampleRate = MIC_SAMPLE_RATE;
  uint32_t byteRate = sampleRate * 2;

  uint8_t wavHeader[44] = {
    'R','I','F','F',
    (uint8_t)(chunkSize),
    (uint8_t)(chunkSize >> 8),
    (uint8_t)(chunkSize >> 16),
    (uint8_t)(chunkSize >> 24),
    'W','A','V','E',
    'f','m','t',' ',
    16,0,0,0,
    1,0,
    1,0,
    (uint8_t)(sampleRate),
    (uint8_t)(sampleRate >> 8),
    (uint8_t)(sampleRate >> 16),
    (uint8_t)(sampleRate >> 24),
    (uint8_t)(byteRate),
    (uint8_t)(byteRate >> 8),
    (uint8_t)(byteRate >> 16),
    (uint8_t)(byteRate >> 24),
    2,0,
    16,0,
    'd','a','t','a',
    (uint8_t)(dataSize),
    (uint8_t)(dataSize >> 8),
    (uint8_t)(dataSize >> 16),
    (uint8_t)(dataSize >> 24)
  };

  String boundary = "----ESP32GroqBoundary";

  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
    "whisper-large-v3-turbo\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"language\"\r\n\r\n"
    "pt\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n"
    "text\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
    "Content-Type: audio/wav\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  size_t totalLen =
    head.length() +
    sizeof(wavHeader) +
    dataSize +
    tail.length();

  uint8_t* corpo = (uint8_t*)ps_malloc(totalLen);

  if (corpo == nullptr) {
    ultimoStatusTranscricao = TRANSCRICAO_ERRO_LOCAL;
    Serial.println("Erro ao reservar PSRAM para envio.");
    return "";
  }

  size_t pos = 0;

  memcpy(corpo + pos, head.c_str(), head.length());
  pos += head.length();

  memcpy(corpo + pos, wavHeader, sizeof(wavHeader));
  pos += sizeof(wavHeader);

  memcpy(corpo + pos, bufferAudio, dataSize);
  pos += dataSize;

  memcpy(corpo + pos, tail.c_str(), tail.length());

  // Usa a conexao ja aberta enquanto voce falava. Se ela nao
  // estiver aberta, o HTTPClient abre uma nova normalmente.
  WiFiClientSecure& client = clienteGroq;

  if (!client.connected()) {
    client.setInsecure();
  }

  HTTPClient http;
  http.setReuse(true);

  if (!http.begin(
        client,
        "https://api.groq.com/openai/v1/audio/transcriptions"
      )) {
    ultimoStatusTranscricao = TRANSCRICAO_ERRO_GROQ;
    Serial.println("Falha ao conectar na Groq.");
    free(corpo);
    return "";
  }

  http.setTimeout(10000);

  http.addHeader(
    "Authorization",
    "Bearer " + String(GROQ_API_KEY)
  );

  http.addHeader(
    "Content-Type",
    "multipart/form-data; boundary=" + boundary
  );

  int httpCode = http.POST(corpo, totalLen);

  free(corpo);

  if (httpCode != 200) {
    ultimoStatusTranscricao = TRANSCRICAO_ERRO_GROQ;

    Serial.print("Erro Groq HTTP: ");
    Serial.println(httpCode);

    String erro = http.getString();
    Serial.println(erro);

    http.end();
    return "";
  }

  String texto = http.getString();
  http.end();

  texto.trim();

  if (texto.length() == 0) {
    ultimoStatusTranscricao = TRANSCRICAO_VAZIA;

    Serial.println("Groq retornou transcricao vazia.");
    return "";
  }

  ultimoStatusTranscricao = TRANSCRICAO_OK;

  Serial.print("Groq reconheceu: ");
  Serial.println(texto);

  return texto;
}

inline void testarMicrofone() {
  if (!microfoneDisponivel) {
    Serial.println("TESTE MIC: microfone indisponivel.");
    return;
  }

  int32_t amostras[128];
  int32_t valores[8 * 128];
  size_t bytesLidos = 0;
  size_t total = 0;

  Serial.println("TESTE MIC: fale perto do microfone...");
  delay(3000);
  Serial.println("Medindo o som...");

  for (int bloco = 0; bloco < 8; bloco++) {
    esp_err_t erro = micLer(
      amostras,
      sizeof(amostras),
      &bytesLidos
    );

    if (erro != ESP_OK || bytesLidos == 0) {
      Serial.println("TESTE MIC: erro de leitura I2S.");
      return;
    }

    size_t quantidade = bytesLidos / sizeof(int32_t);

    for (size_t i = 0; i < quantidade && total < 8 * 128; i++) {
      valores[total++] = amostras[i] >> MIC_DESLOCAMENTO_BITS;
    }
  }

  if (total == 0) {
    Serial.println("TESTE MIC: sem amostras.");
    return;
  }

  // Nivel fixo (DC) medio, para mostrar o RMS sem ele.
  int64_t soma = 0;
  for (size_t i = 0; i < total; i++) {
    soma += valores[i];
  }
  float media = (float)soma / (float)total;

  uint64_t somaQuadrados = 0;
  int32_t pico = 0;

  for (size_t i = 0; i < total; i++) {
    int32_t valor = valores[i] - (int32_t)media;

    if (valor > 32767) valor = 32767;
    if (valor < -32768) valor = -32768;

    int32_t absoluto = abs(valor);
    if (absoluto > pico) pico = absoluto;

    somaQuadrados += (uint64_t)((int64_t)valor * valor);
  }

  float rms = sqrtf((float)somaQuadrados / (float)total);

  Serial.print("TESTE MIC | Nivel fixo (DC): ");
  Serial.print(media, 1);
  Serial.print(" | RMS: ");
  Serial.print(rms, 1);
  Serial.print(" | Pico: ");
  Serial.println(pico);
}
#endif
