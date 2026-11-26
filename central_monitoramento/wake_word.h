#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <Arduino.h>
#include <Central_de_Monitoramento_inferencing.h>
#include "botao.h"   // PINO_BOTAO (o botao interrompe a captura)

#define WAKE_CONFIANCA 0.55f

// Volume do microfone na escuta da palavra de ativacao.
// A gravacao da voz (pergunta) usa deslocamento 14; aqui usa 11,
// que e 8x mais alto. Menor numero = mais volume.
// Olhe o "pico" no Serial falando normal:
//   pico = 32767 (estourando)  -> aumente: 12, 13 ou 14
//   pico baixo (menos de 1500) -> diminua: 10 ou 9
#define WAKE_SHIFT 11

// 1 = mostra no visor (5 letras) a palavra que o modelo reconheceu,
//     para testar sem o Serial. Depois de ajustar, use 0.
#define WAKE_DEBUG_DISPLAY 1
#define TEMPO_ENTRE_PALAVRAS 4000

inline int16_t* bufferWake = nullptr; // Corrigido o ponteiro

inline int16_t ultimoPicoWake = 0;
inline float ultimoRmsWake = 0.0f;

inline bool centralDetectada = false;
inline unsigned long momentoCentral = 0;


// -----------------------------------------------------
// EDGE IMPULSE - ENVIA AUDIO PARA O MODELO
// -----------------------------------------------------

inline int wakeGetData(size_t offset, size_t length, float* out_ptr) {
  if (bufferWake == nullptr) {
    return -1;
  }

  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (float)bufferWake[offset + i];
  }

  return 0;
}


// -----------------------------------------------------
// INICIALIZA WAKE WORD
// -----------------------------------------------------

inline void iniciarWakeWord() {
  bufferWake = (int16_t*)ps_malloc(
    EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int16_t)
  );

  if (bufferWake == nullptr) {
    Serial.println("Erro ao reservar memoria para Wake Word.");
    return;
  }

  centralDetectada = false;
  momentoCentral = 0;

  Serial.println("Wake Word Edge Impulse iniciado.");
}


// -----------------------------------------------------
// GRAVA AUDIO PARA O EDGE IMPULSE
// -----------------------------------------------------

inline bool capturarAudioWake() {
  if (!microfoneDisponivel || bufferWake == nullptr) {
    return false;
  }

  size_t amostrasCapturadas = 0;
  int32_t picoJanela = 0;
  double somaQuadrados = 0.0;
  const size_t CHUNK_SIZE = 128;
  int32_t chunk[CHUNK_SIZE];

  while (amostrasCapturadas < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {

    // Se o botao for apertado durante a captura (~1 s), aborta na
    // hora para o botao continuar respondendo rapido.
    if (digitalRead(PINO_BOTAO) == LOW) {
      return false;
    }

    size_t faltam = EI_CLASSIFIER_RAW_SAMPLE_COUNT - amostrasCapturadas;
    size_t quantidade = faltam > CHUNK_SIZE ? CHUNK_SIZE : faltam;
    size_t bytesLidos = 0;

    // Leitura pelo driver I2S novo (definida em microfone.h)
    esp_err_t resultado = micLer(
      chunk,
      quantidade * sizeof(int32_t),
      &bytesLidos
    );

    if (resultado != ESP_OK || bytesLidos == 0) {
      Serial.println("Erro ao capturar audio para Wake Word.");
      return false;
    }

    size_t lidas = bytesLidos / sizeof(int32_t);

    for (size_t i = 0; i < lidas && amostrasCapturadas < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
      int32_t amostra = chunk[i] >> WAKE_SHIFT;

      if (amostra > 32767)  amostra = 32767;
      if (amostra < -32768) amostra = -32768;

      int32_t absoluto = abs(amostra);
      if (absoluto > picoJanela) picoJanela = absoluto;
      somaQuadrados += (double)amostra * (double)amostra;

      bufferWake[amostrasCapturadas++] = (int16_t)amostra;
    }
  }

  if (picoJanela > 32767) picoJanela = 32767;
  ultimoPicoWake = (int16_t)picoJanela;
  ultimoRmsWake = sqrt(somaQuadrados / (double)EI_CLASSIFIER_RAW_SAMPLE_COUNT);

  return true;
}


// -----------------------------------------------------
// EXECUTA O MODELO
// -----------------------------------------------------

inline String reconhecerPalavraWake() {
  if (!capturarAudioWake()) {
    return "";
  }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &wakeGetData;

  ei_impulse_result_t resultado = { 0 };
  EI_IMPULSE_ERROR erro = run_classifier(&signal, &resultado, false);

  if (erro != EI_IMPULSE_OK) {
    Serial.print("Erro Edge Impulse: ");
    Serial.println((int)erro);
    return "";
  }

  float maiorConfianca = 0.0f;
  String palavra = "";

  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    float valor = resultado.classification[i].value;

    if (valor > maiorConfianca) {
      maiorConfianca = valor;
      palavra = resultado.classification[i].label;
    }
  }

  if (maiorConfianca >= WAKE_CONFIANCA) {
    palavra.toLowerCase();

    Serial.print("Edge Impulse: ");
    Serial.print(palavra);
    Serial.print(" | ");
    Serial.print(maiorConfianca * 100, 1);
    Serial.print("% | pico: ");
    Serial.print(ultimoPicoWake);
    Serial.print(" | rms: ");
    Serial.println(ultimoRmsWake, 0);

#if WAKE_DEBUG_DISPLAY
    {
      String curta = palavra;
      curta.toUpperCase();

      char textoVisor[8];
      snprintf(textoVisor, sizeof(textoVisor), "%.5s", curta.c_str());

      mostrarDisplay(textoVisor);
    }
#endif

    return palavra;
  }

  static unsigned long ultimaDepuracao = 0;
  if (millis() - ultimaDepuracao >= 1000) { // Corrigido o sinal de menos (-)
    ultimaDepuracao = millis();

    Serial.print("[WAKE] Melhor classe: ");
    Serial.print(palavra);
    Serial.print(" | confianca: ");
    Serial.print(maiorConfianca * 100.0f, 1);
    Serial.print("% | pico: ");
    Serial.print(ultimoPicoWake);
    Serial.print(" | rms: ");
    Serial.println(ultimoRmsWake, 0);
  }

  return "";
}


// -----------------------------------------------------
// PROCURA PALAVRAS DE ATIVACAO
// -----------------------------------------------------

inline bool verificarWakeWord() {
  // Se a janela expirou, volta ao estado inicial.
  if (centralDetectada && (millis() - momentoCentral > TEMPO_ENTRE_PALAVRAS)) { // Corrigido o sinal de menos (-)
    centralDetectada = false;
    momentoCentral = 0;
    Serial.println("Tempo para ATIVAR expirou.");
  }

  String palavra = reconhecerPalavraWake();

  if (palavra.length() == 0) {
    return false;
  }

  // AGUARDANDO A PRIMEIRA PALAVRA
  if (!centralDetectada) {
    // ATENÇÃO: Verifique se no seu Edge Impulse a palavra gravada foi "central" ou "principal"
    if (palavra == "central" || palavra == "principal") { 
      centralDetectada = true;
      momentoCentral = millis();
      Serial.println("Palavra de ativação inicial detectada.");
    }
    return false;
  }

  // JÁ OUVIU A PRIMEIRA PALAVRA
  if (palavra == "ativar") {
    if ((millis() - momentoCentral) <= TEMPO_ENTRE_PALAVRAS) { // Corrigido o sinal de menos (-)
      centralDetectada = false;
      momentoCentral = 0;
      Serial.println("Wake Word confirmado!");
      return true;
    }

    centralDetectada = false;
    momentoCentral = 0;
    return false;
  }

  // Se repetir a primeira palavra, reinicia a janela de tempo.
  if (palavra == "central" || palavra == "principal") {
    momentoCentral = millis();
    Serial.println("Palavra repetida. Reiniciando espera.");
    return false;
  }

  // Qualquer outra palavra cancela a sequencia.
  Serial.print("Sequencia cancelada por: ");
  Serial.println(palavra);

  centralDetectada = false;
  momentoCentral = 0;

  return false;
}

#endif