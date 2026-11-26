/*
  CENTRAL DE MONITORAMENTO
  Sítio 4 Irmãos
  ESP32-S3
*/

// 1 = ativa por voz ("central" + "ativar") E pelo botao fisico
//     (precisa da biblioteca do Edge Impulse; veja o README)
// 0 = somente pelo botao fisico
#define ATIVACAO_POR_VOZ 0

#include "wifi_config.h"
#include "relogio.h"
#include "sensores.h"
#include "dados_sitio.h"
#include "firebase_central.h"
#include "voz.h"
#include "meu_audio.h"
#include "display.h"
#include "microfone.h"
#if ATIVACAO_POR_VOZ
#include "wake_word.h"
#endif
#include "botao.h"

// 1 = nao le o Firebase enquanto o audio esta tocando
//     (evita travadas no som). A leitura atrasada acontece
//     logo depois que a fala termina.
// 0 = comportamento anterior
#define PAUSAR_FIREBASE_FALANDO 1

// Maximo de vezes seguidas que a central pede "Pode repetir?"
// (quando nao entende ou so ouve ruido). Passando disso, ela
// desiste e volta ao repouso em silencio, em vez de ficar em
// loop. Volta a 0 quando entende um comando ou ao ativar.
#define MAX_TENTATIVAS_REPETICAO 2

// Folga (ms) depois que a fala termina, antes de gravar.
// Evita o microfone captar o final/eco da propria voz.
#define FOLGA_ECO_APOS_FALA_MS 600

// 1 = depois de "obrigado" a central responde e ENCERRA a
//     conversa (o Whisper "inventa" muito "Obrigado." em ruido
//     e isso mantinha a central ouvindo para sempre).
// 0 = comportamento anterior (continua ouvindo)
#define ENCERRAR_APOS_AGRADECIMENTO 1

// 1 = ao apertar o botao a central fala "Estou ouvindo." e so
//     depois comeca a ouvir (mais lento: a voz vem da internet).
// 0 = comeca a ouvir NA HORA, so com "OUVINDO" no visor (rapido).
#define FALAR_ATIVACAO 0

// 1 = imprime no Serial a memoria livre e a idade dos dados do
//     sitio (ajuda a achar "audio mudo" e "sem dados").
#define DIAGNOSTICO_SERIAL 1

// 1 = mostra o texto da resposta no visor ANTES de a voz ficar
//     pronta (a voz precisa abrir a conexao com a internet e
//     demora 1 a 2 s; antes o visor ficava em "..." esse tempo todo).
// 0 = comportamento anterior (voz primeiro, depois o texto)
#define RESPOSTA_ANTES_DA_VOZ 1

// Marcas de tempo para o Serial ([TEMPO]): mostram onde esta a demora.
unsigned long tFimGravacao = 0;
unsigned long tFimTranscricao = 0;

// Depuracao leve: informa a etapa atual e mantem batimento no Serial.
volatile const char* passoAtual = "inicio";
volatile unsigned long inicioPasso = 0;
volatile unsigned long contadorLoops = 0;

inline void passo(const char* nome) {
  passoAtual = nome;
  inicioPasso = millis();
}


// ======================================================
// ESTADOS DA CENTRAL
// ======================================================

enum EstadoCentral {
  REPOUSO,
  FALANDO_ATIVACAO,
  OUVINDO,
  PROCESSANDO,
  RESPONDENDO,
  PEDINDO_REPETICAO,
  ENCERRANDO,
  ERRO_TEMPORARIO
};

EstadoCentral estadoCentral = REPOUSO;


// ======================================================
// FIREBASE
// ======================================================

unsigned long ultimaLeituraFirebase = 0;
const unsigned long INTERVALO_FIREBASE = 10000;

// As leituras agora sao feitas UMA por vez (caixa 1, caixa 2,
// caixa principal, clima, chuva), com esta pausa entre elas.
// Assim o botao e verificado entre uma leitura e outra, em vez
// de ficar "surdo" durante as 5 leituras seguidas.
const unsigned long INTERVALO_ETAPA_FIREBASE = 2000;
int etapaFirebase = 0;

// Depois de uma conversa, espera um pouco antes de voltar a ler
// o Firebase (para o botao responder logo ao voltar ao repouso).
const unsigned long FOLGA_FIREBASE_APOS_CONVERSA = 4000;
unsigned long firebaseLiberadoApos = 0;


// ======================================================
// DISPLAY EM REPOUSO
// ======================================================

unsigned long ultimaTrocaDisplay = 0;
const unsigned long TEMPO_TELA = 5000;
int telaRepouso = 0;


// ======================================================
// CONTROLE DA FALA
// ======================================================

bool falaSolicitada = false;
bool falaJaIniciou = false;
unsigned long inicioFala = 0;

const unsigned long TIMEOUT_INICIO_FALA = 3000;


// ======================================================
// RESPOSTA
// ======================================================

unsigned long inicioResposta = 0;
const unsigned long TEMPO_MINIMO_RESPOSTA = 4000;


// ======================================================
// ERRO TEMPORARIO
// ======================================================

unsigned long inicioErroTemporario = 0;
const unsigned long TEMPO_EXIBICAO_ERRO = 2500;


// ======================================================
// ESCUTA
// ======================================================

bool escutaPendente = false;

// Tentativas seguidas sem entender (ver MAX_TENTATIVAS_REPETICAO)
int tentativasRepeticao = 0;

// So grava depois deste instante (folga contra eco da propria fala)
unsigned long escutaLiberadaApos = 0;

// Depois da resposta, volta ao repouso em vez de ouvir de novo
bool encerrarAposResposta = false;


// ======================================================
// PROTOTIPOS
// ======================================================

void voltarAoRepouso();
void atualizarModoRepouso();
void iniciarEscuta();
void ouvirPergunta();
void processarPergunta(String texto);
void ativarCentral();

void iniciarFalaEstado(
  EstadoCentral novoEstado,
  const String& texto
);

bool falaTerminou();

void pedirRepeticao(
  const String& fala
);

void processarEstados();
void processarSerial();

void mostrarErroTemporario(
  const char* texto
);


// ======================================================
// TAREFA DE DEPURACAO
// ======================================================

void tarefaDebug(void* parametro) {
  unsigned long ultimoBatimento = 0;
  for (;;) {
    unsigned long agora = millis();
    unsigned long duracao = agora - inicioPasso;

    if (duracao > 3000) {
      Serial.printf("[DEBUG] Etapa '%s' ha %lu ms sem terminar\n",
                    (const char*)passoAtual, duracao);
    }

    if (agora - ultimoBatimento >= 10000) {
      ultimoBatimento = agora;
      Serial.printf("[DEBUG] vivo | etapa='%s' | estado=%d | loops=%lu\n",
                    (const char*)passoAtual, (int)estadoCentral, contadorLoops);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


// ======================================================
// CONTROLE DA FALA POR ESTADO
// ======================================================

void iniciarFalaEstado(
  EstadoCentral novoEstado,
  const String& texto
) {

  estadoCentral = novoEstado;
  falaJaIniciou = false;
  inicioFala = millis();

  falaSolicitada =
    falar(texto);

#if DIAGNOSTICO_SERIAL
  Serial.printf(
    "[TEMPO] falar() levou %lu ms para abrir a voz\n",
    millis() - inicioFala
  );

  Serial.printf(
    "[DIAG] fala solicitada=%d | heap livre=%u | PSRAM livre=%u\n",
    (int)falaSolicitada,
    (unsigned)ESP.getFreeHeap(),
    (unsigned)ESP.getFreePsram()
  );
#endif
}


// ======================================================
// VERIFICAR SE A FALA TERMINOU
// ======================================================

bool falaTerminou() {

  if (!falaSolicitada) {
    return true;
  }

  if (audioEmExecucao()) {
#if DIAGNOSTICO_SERIAL
    if (!falaJaIniciou) {
      Serial.printf(
        "[TEMPO] som comecou %lu ms depois do pedido de voz\n",
        millis() - inicioFala
      );

      if (tFimGravacao != 0) {
        Serial.printf(
          "[TEMPO] TOTAL: %lu ms entre o fim da gravacao e o som\n",
          millis() - tFimGravacao
        );
        tFimGravacao = 0;
      }
    }
#endif
    falaJaIniciou = true;
    return false;
  }

  if (falaJaIniciou) {
    falaSolicitada = false;
    falaJaIniciou = false;
    return true;
  }

  if (
    millis() - inicioFala >=
    TIMEOUT_INICIO_FALA
  ) {
    Serial.println(
      "Timeout aguardando inicio do audio."
    );

    falaSolicitada = false;
    return true;
  }

  return false;
}


// ======================================================
// PEDIR PARA REPETIR (COM LIMITE DE TENTATIVAS)
// ======================================================

void pedirRepeticao(
  const String& fala
) {

  tentativasRepeticao++;

  if (
    tentativasRepeticao >
    MAX_TENTATIVAS_REPETICAO
  ) {
    Serial.println(
      "Muitas tentativas sem entender. Voltando ao repouso."
    );

    voltarAoRepouso();
    return;
  }

  mostrarDisplay(
    "REPITA"
  );

  iniciarFalaEstado(
    PEDINDO_REPETICAO,
    fala
  );
}


// ======================================================
// ERRO TEMPORARIO SEM BLOQUEAR O LOOP
// ======================================================

void mostrarErroTemporario(
  const char* texto
) {

  estadoCentral = ERRO_TEMPORARIO;
  inicioErroTemporario = millis();
  escutaPendente = false;

  mostrarDisplay(texto);
}


// ======================================================
// VOLTAR AO REPOUSO
// ======================================================

void voltarAoRepouso() {

  // Se a conexao com a Groq foi aberta e nao foi usada
  // (ex.: ninguem falou), fecha para liberar memoria.
  fecharGroq();

  estadoCentral = REPOUSO;

  escutaPendente = false;

  tentativasRepeticao = 0;
  encerrarAposResposta = false;

  falaSolicitada = false;
  falaJaIniciou = false;
  inicioFala = 0;

  inicioResposta = 0;
  inicioErroTemporario = 0;

  centralDetectada = false;

  brilhoRepouso();

  telaRepouso = 0;
  ultimaTrocaDisplay = 0;

  // Folga antes de o Firebase voltar a ser lido.
  firebaseLiberadoApos =
    millis() + FOLGA_FIREBASE_APOS_CONVERSA;

  Serial.println(
    "Estado: REPOUSO"
  );
}


// ======================================================
// DISPLAY EM REPOUSO
// ======================================================

// ======================================================
// RONDA DO SITIO
// De tempos em tempos, sozinha, a central mostra os dados do
// sitio no visor (caixas, temperatura, umidade, pressao e
// chuva) e depois volta para hora / temperatura / umidade.
// Dado velho ou indisponivel e pulado.
// ======================================================

// Intervalo entre rondas (10 minutos)
const unsigned long INTERVALO_RONDA_SITIO_MS = 10UL * 60UL * 1000UL;

// A primeira ronda acontece 1 minuto depois de ligar
unsigned long proximaRondaSitio = 60UL * 1000UL;

bool rondaAtiva = false;
int rondaItem = 0;

#define ITENS_RONDA 7

// Mostra um item da ronda. Devolve false se o dado nao esta
// disponivel (a ronda pula para o proximo).
bool mostrarItemRonda(int item) {

  char valor[24];

  switch (item) {

    case 0:
      if (!caixa1.disponivel) return false;
      snprintf(valor, sizeof(valor), "%.0f%%", caixa1.nivel);
      mostrarNomeValor("CAIXA 1", valor);
      return true;

    case 1:
#if CAIXA_2_INSTALADA
      if (!caixa2.disponivel) return false;
      snprintf(valor, sizeof(valor), "%.0f%%", caixa2.nivel);
      mostrarNomeValor("CAIXA 2", valor);
      return true;
#else
      return false;
#endif

    case 2:
#if CAIXA_PRINCIPAL_INSTALADA
      if (!caixa3.disponivel) return false;
      snprintf(valor, sizeof(valor), "%.0f%%", caixa3.nivel);
      mostrarNomeValor("CAIXA PRINCIPAL", valor);
      return true;
#else
      return false;
#endif

    case 3:
      if (!climaSitio.disponivel) return false;
      snprintf(valor, sizeof(valor), "%.1f C", climaSitio.temperatura);
      mostrarNomeValor("TEMP SITIO", valor);
      return true;

    case 4:
      if (!climaSitio.disponivel) return false;
      snprintf(valor, sizeof(valor), "%.0f%%", climaSitio.umidade);
      mostrarNomeValor("UMID SITIO", valor);
      return true;

    case 5:
      if (!climaSitio.disponivel) return false;
      snprintf(valor, sizeof(valor), "%.0f hPa", climaSitio.pressao);
      mostrarNomeValor("PRESSAO SITIO", valor);
      return true;

    case 6:
      if (!chuvaSitio.disponivel) return false;

      if (strcmp(chuvaSitio.status, "seco") == 0) {
        mostrarNomeValor("COND. TEMPO", "SECO");
      } else if (strcmp(chuvaSitio.status, "gotas") == 0) {
        mostrarNomeValor("COND. TEMPO", "GOTAS");
      } else if (strcmp(chuvaSitio.status, "chuva") == 0) {
        mostrarNomeValor("COND. TEMPO", "CHUVA");
      } else {
        return false;
      }
      return true;
  }

  return false;
}

// Chamada a cada volta do loop (em repouso). Devolve true
// enquanto a ronda estiver usando o visor.
bool atualizarRondaSitio() {

  if (rondaAtiva) {

    // espera o item anterior terminar de rolar / ser lido
    if (displayOcupado() || aguardandoValorDisplay) {
      return true;
    }

    while (rondaItem < ITENS_RONDA) {
      int item = rondaItem++;

      if (mostrarItemRonda(item)) {
        return true;
      }
    }

    // acabou: volta ao ciclo normal logo em seguida
    rondaAtiva = false;
    ultimaTrocaDisplay = 0;
    proximaRondaSitio = millis() + INTERVALO_RONDA_SITIO_MS;
    return false;
  }

  if (millis() >= proximaRondaSitio) {

    // Display apagado (madrugada): adia.
    if (horarioDisplayApagado()) {
      proximaRondaSitio = millis() + 60UL * 1000UL;
      return false;
    }

    bool temDado =
      caixa1.disponivel ||
      caixa2.disponivel ||
      caixa3.disponivel ||
      climaSitio.disponivel ||
      chuvaSitio.disponivel;

    if (temDado) {
      rondaAtiva = true;
      rondaItem = 0;
      return true;
    }

    // Sem dado do sitio agora: tenta de novo em 1 minuto.
    proximaRondaSitio = millis() + 60UL * 1000UL;
  }

  return false;
}


void atualizarModoRepouso() {

  if (
    estadoCentral != REPOUSO
  ) {
    // Se alguem ativou a central no meio da ronda, cancela.
    rondaAtiva = false;
    return;
  }

  // Ronda do sitio tem prioridade sobre o ciclo normal.
  if (atualizarRondaSitio()) {
    return;
  }

  if (
    ultimaTrocaDisplay != 0 &&
    millis() - ultimaTrocaDisplay <
    TEMPO_TELA
  ) {
    return;
  }

  ultimaTrocaDisplay = millis();

  char texto[20];

  switch (telaRepouso) {

    // --------------------------------------------------
    // HORA
    // --------------------------------------------------

    case 0: {

      struct tm horario;

      if (
        getLocalTime(
          &horario
        )
      ) {
        strftime(
          texto,
          sizeof(texto),
          "%H:%M",
          &horario
        );

        mostrarDisplay(texto);
      }

      break;
    }


    // --------------------------------------------------
    // TEMPERATURA LOCAL
    // --------------------------------------------------

    case 1:

      if (ambienteDisponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.1f C",
          temperatura
        );

        mostrarDisplay(texto);

      } else {
        mostrarDisplay("-- C");
      }

      break;


    // --------------------------------------------------
    // UMIDADE LOCAL
    // --------------------------------------------------

    case 2:

      if (ambienteDisponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.0f%%",
          umidade
        );

        mostrarDisplay(texto);

      } else {
        mostrarDisplay("--%");
      }

      break;
  }

  telaRepouso++;

  if (telaRepouso > 2) {
    telaRepouso = 0;
  }
}


// ======================================================
// INICIAR ESCUTA
// ======================================================


void iniciarEscuta() {

  estadoCentral = OUVINDO;
  escutaPendente = true;

  // Espera um pouco antes de gravar (eco da propria fala).
  escutaLiberadaApos =
    millis() + FOLGA_ECO_APOS_FALA_MS;

  brilhoAtivado();

  // Abre a conexao com a Groq enquanto voce fala (ver microfone.h).
  preaquecerGroq();

  // Rolagem mais rapida (30 ms por passo) para o "OUVINDO"
  // aparecer inteiro durante os 3 segundos de gravacao.
  rolarDisplay("OUVINDO", 30);

  Serial.println(
    "Estado: OUVINDO"
  );
}



// ======================================================
// OUVIR PERGUNTA
// ======================================================

void ouvirPergunta() {

  if (
    estadoCentral != OUVINDO ||
    !escutaPendente
  ) {
    return;
  }

  // Nunca grava com o alto-falante ainda tocando, nem antes
  // da folga contra eco. Tenta de novo no proximo loop.
  if (
    audioEmExecucao() ||
    millis() < escutaLiberadaApos
  ) {
    return;
  }

  escutaPendente = false;

  Serial.println(
    "Iniciando gravacao de 3 segundos..."
  );


  // ====================================================
  // GRAVACAO
  // ====================================================

  if (!gravarAudio()) {

    if (
      ultimoStatusGravacao ==
      GRAVACAO_SILENCIO
    ) {
      Serial.println(
        "Nenhuma voz detectada."
      );

      Serial.println(
        "Encerrando ciclo."
      );

      voltarAoRepouso();
      return;
    }

    Serial.println(
      "Falha ao gravar audio."
    );

    mostrarErroTemporario(
      "ERRO MIC"
    );

    return;
  }


  // ====================================================
  // TRANSCRICAO
  // ====================================================

  estadoCentral = PROCESSANDO;

  tFimGravacao = millis();

  mostrarDisplay(
    "..."
  );

  String texto =
    transcreverAudioGroq();

  tFimTranscricao = millis();

#if DIAGNOSTICO_SERIAL
  Serial.printf(
    "[TEMPO] transcricao levou %lu ms\n",
    tFimTranscricao - tFimGravacao
  );
#endif

  texto.trim();


  // ====================================================
  // SEM INTERNET
  // ====================================================

  if (
    ultimoStatusTranscricao ==
    TRANSCRICAO_SEM_WIFI
  ) {
    Serial.println(
      "Transcricao cancelada: sem Wi-Fi."
    );

    // O TTS tambem depende da internet.
    mostrarErroTemporario(
      "SEM WIFI"
    );

    return;
  }


  // ====================================================
  // ERRO LOCAL
  // ====================================================

  if (
    ultimoStatusTranscricao ==
    TRANSCRICAO_ERRO_LOCAL
  ) {
    Serial.println(
      "Erro local ao preparar o audio."
    );

    mostrarDisplay(
      "ERRO AUDIO"
    );

    iniciarFalaEstado(
      ENCERRANDO,
      "Tive um problema ao processar o audio."
    );

    return;
  }


  // ====================================================
  // ERRO GROQ
  // ====================================================

  if (
    ultimoStatusTranscricao ==
    TRANSCRICAO_ERRO_GROQ
  ) {
    Serial.println(
      "Falha no servico de transcricao."
    );

    mostrarDisplay(
      "ERRO VOZ"
    );

    iniciarFalaEstado(
      ENCERRANDO,
      "Nao consegui acessar o servico de voz."
    );

    return;
  }


  // ====================================================
  // GROQ RESPONDEU SEM TEXTO
  // ====================================================

  if (
    ultimoStatusTranscricao ==
    TRANSCRICAO_VAZIA
  ) {
    Serial.println(
      "Groq nao retornou uma fala reconhecida."
    );

    pedirRepeticao(
      "Nao consegui entender. Pode repetir?"
    );

    return;
  }


  // ====================================================
  // QUALQUER OUTRO STATUS INVALIDO
  // ====================================================

  if (
    ultimoStatusTranscricao !=
    TRANSCRICAO_OK
  ) {
    Serial.println(
      "Transcricao indisponivel."
    );

    voltarAoRepouso();
    return;
  }


  // ====================================================
  // TEXTO VALIDO
  // ====================================================

  processarPergunta(texto);
}


// ======================================================
// PROCESSAR PERGUNTA
// ======================================================

// ======================================================
// DADOS FRESCOS ANTES DE RESPONDER
// O Firebase so e lido em repouso. Numa conversa seguida os
// dados do sitio envelhecem e a resposta podia sair como
// "indisponivel". Se o dado que a resposta vai usar estiver
// com mais de 30 s, le de novo antes de responder.
// ======================================================

bool dadoVelho(long timestampDado) {

  if (timestampDado == 0) {
    return true;
  }

  time_t agora = time(nullptr);

  // Relogio ainda nao sincronizou: ler de novo nao ajuda.
  if (agora < 1000000000) {
    return false;
  }

  return ((long)agora - timestampDado) > 30;
}

void atualizarDadoParaResposta(ComandoVoz comando) {

  if (!podeLerFirebase()) {
    return;
  }

  unsigned long inicio = millis();
  bool leu = false;

  switch (comando) {

    case NIVEL_CAIXA_1:
      if (dadoVelho((long)caixa1.timestamp)) {
        lerCaixa(1, caixa1);
        leu = true;
      }
      break;

    case NIVEL_CAIXA_2:
#if CAIXA_2_INSTALADA
      if (dadoVelho((long)caixa2.timestamp)) {
        lerCaixa(2, caixa2);
        leu = true;
      }
#endif
      break;

    case NIVEL_CAIXA_PRINCIPAL:
#if CAIXA_PRINCIPAL_INSTALADA
      if (dadoVelho((long)caixa3.timestamp)) {
        lerCaixa(3, caixa3);
        leu = true;
      }
#endif
      break;

    case STATUS_CAIXAS:
      if (dadoVelho((long)caixa1.timestamp)) {
        lerCaixa(1, caixa1);
        leu = true;
      }
#if CAIXA_2_INSTALADA
      if (dadoVelho((long)caixa2.timestamp)) {
        lerCaixa(2, caixa2);
        leu = true;
      }
#endif
#if CAIXA_PRINCIPAL_INSTALADA
      if (dadoVelho((long)caixa3.timestamp)) {
        lerCaixa(3, caixa3);
        leu = true;
      }
#endif
      break;

    case TEMPERATURA_SITIO:
    case UMIDADE_SITIO:
    case PRESSAO_SITIO:
      if (dadoVelho((long)climaSitio.timestamp)) {
        lerClimaSitio();
        leu = true;
      }
      break;

    case CHUVA:
      if (dadoVelho((long)chuvaSitio.timestamp)) {
        lerChuvaSitio();
        leu = true;
      }
      break;

    default:
      return;
  }

  if (leu) {
    atualizarValidadeDadosSitio();

#if DIAGNOSTICO_SERIAL
    Serial.printf(
      "[TEMPO] atualizar dado do sitio levou %lu ms\n",
      millis() - inicio
    );
#endif
  }
}


void processarPergunta(
  String texto
) {

  texto.trim();

  if (
    texto.length() == 0
  ) {
    voltarAoRepouso();
    return;
  }

  Serial.print(
    "Pergunta reconhecida: "
  );

  Serial.println(texto);

  estadoCentral = PROCESSANDO;

  mostrarDisplay(
    "..."
  );

  ComandoVoz comando =
    identificarComando(texto);

#if DIAGNOSTICO_SERIAL
  {
    time_t agoraDiag = time(nullptr);
    long idadeClima =
      (climaSitio.timestamp == 0) ? -1 :
      (long)agoraDiag - (long)climaSitio.timestamp;

    Serial.printf(
      "[DIAG] clima disponivel=%d | idade=%ld s | relogio ok=%d | Firebase pronto=%d\n",
      (int)climaSitio.disponivel,
      idadeClima,
      (int)(agoraDiag > 1000000000),
      (int)firebasePronto()
    );
  }
#endif


  // ====================================================
  // ENCERRAR
  // ====================================================

  if (
    comando == ENCERRAR
  ) {
    Serial.println(
      "Encerrando conversa."
    );

    // Primeiro pede a voz, depois mostra o texto, para os dois
    // comecarem juntos. A rolagem acompanha a duracao da fala.
    const char* falaEncerrar = "Disponha.";

    iniciarFalaEstado(
      ENCERRANDO,
      falaEncerrar
    );

    definirFalaPrevista(strlen(falaEncerrar));
    rolarDisplay("ATE LOGO");
    definirFalaPrevista(0);

    return;
  }


  // ====================================================
  // COMANDO NAO RECONHECIDO
  // ====================================================

  if (
    comando == COMANDO_NENHUM
  ) {
    Serial.println(
      "Comando nao reconhecido."
    );

    pedirRepeticao(
      "Nao entendi. Pode repetir?"
    );

    return;
  }

  // Entendeu um comando valido: zera as tentativas.
  tentativasRepeticao = 0;

  // Se o dado do sitio estiver velho, le de novo antes de responder.
  atualizarDadoParaResposta(comando);


  // ====================================================
  // RESPOSTA NORMAL
  // ====================================================

  String resposta =
    gerarResposta(comando);

#if ENCERRAR_APOS_AGRADECIMENTO
  encerrarAposResposta =
    (comando == AGRADECIMENTO);
#endif

  inicioResposta =
    millis();

#if RESPOSTA_ANTES_DA_VOZ
  // O texto aparece na hora; a voz vem logo depois (abrir a
  // conexao da voz leva 1 a 2 s). A rolagem acompanha a duracao
  // estimada da fala.
  definirFalaPrevista(resposta.length());
  mostrarRespostaDisplay(
    comando
  );
  definirFalaPrevista(0);

  iniciarFalaEstado(
    RESPONDENDO,
    resposta
  );
#else
  // Primeiro a voz, depois o display: os dois comecam juntos
  // e a rolagem acompanha a duracao estimada da fala.
  iniciarFalaEstado(
    RESPONDENDO,
    resposta
  );

  definirFalaPrevista(resposta.length());
  mostrarRespostaDisplay(
    comando
  );
  definirFalaPrevista(0);
#endif
}


// ======================================================
// ATIVAR CENTRAL
// ======================================================

void ativarCentral() {

  centralDetectada = false;
  tentativasRepeticao = 0;
  encerrarAposResposta = false;

  brilhoAtivado();


  // ====================================================
  // SEM INTERNET
  // ====================================================

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    Serial.println(
      "Central nao ativada: sem Wi-Fi."
    );

    mostrarErroTemporario(
      "SEM WIFI"
    );

    return;
  }


  // ====================================================
  // ATIVACAO NORMAL
  // ====================================================

  Serial.println(
    "CENTRAL ATIVADA"
  );

#if FALAR_ATIVACAO
  const char* falaAtivacao = "Estou ouvindo.";

  iniciarFalaEstado(
    FALANDO_ATIVACAO,
    falaAtivacao
  );

  // Texto curto e parado enquanto fala. O "OUVINDO" rola
  // inteiro depois, durante a gravacao (ver iniciarEscuta).
  mostrarDisplay("OI");
#else
  // Sem fala de ativacao: ouve na hora. Folga curta so para
  // o "clique" do botao nao entrar na gravacao.
  iniciarEscuta();
  escutaLiberadaApos = millis() + 150;
#endif
}


// ======================================================
// PROCESSAR ESTADOS
// ======================================================

void processarEstados() {

  // ====================================================
  // FALA DA ATIVACAO TERMINOU
  // ====================================================

  if (
    estadoCentral ==
    FALANDO_ATIVACAO
  ) {
    if (falaTerminou()) {
      iniciarEscuta();
    }

    return;
  }


  // ====================================================
  // OUVINDO
  // ====================================================

  if (
    estadoCentral ==
    OUVINDO
  ) {
    if (escutaPendente) {
      ouvirPergunta();
    }

    return;
  }


  // ====================================================
  // PEDINDO REPETICAO
  // ====================================================

  if (
    estadoCentral ==
    PEDINDO_REPETICAO
  ) {
    if (falaTerminou()) {
      iniciarEscuta();
    }

    return;
  }


  // ====================================================
  // RESPOSTA NORMAL
  // ====================================================

  if (
    estadoCentral ==
    RESPONDENDO
  ) {
    bool tempoDisplayCumprido =
      millis() -
      inicioResposta >=
      TEMPO_MINIMO_RESPOSTA;

    bool audioTerminou =
      falaTerminou();

    // Espera tambem o visor terminar de mostrar a resposta
    // (nome rolando + valor) antes de voltar a ouvir.
    bool displayTerminou =
      !displayOcupado();

    if (
      audioTerminou &&
      tempoDisplayCumprido &&
      displayTerminou
    ) {
      inicioResposta = 0;

      if (encerrarAposResposta) {
        voltarAoRepouso();
      } else {
        iniciarEscuta();
      }
    }

    return;
  }


  // ====================================================
  // ENCERRANDO
  // ====================================================

  if (
    estadoCentral ==
    ENCERRANDO
  ) {
    // Espera a fala terminar E o visor terminar de rolar
    // (ex.: "ATE LOGO") antes de voltar ao repouso.
    if (
      falaTerminou() &&
      !displayOcupado()
    ) {
      voltarAoRepouso();
    }

    return;
  }


  // ====================================================
  // ERRO TEMPORARIO
  // (espera tambem o texto do erro terminar de rolar)
  // ====================================================

  if (
    estadoCentral ==
    ERRO_TEMPORARIO
  ) {
    if (
      millis() -
      inicioErroTemporario >=
      TEMPO_EXIBICAO_ERRO &&
      !displayOcupado()
    ) {
      voltarAoRepouso();
    }

    return;
  }
}


// ======================================================
// MONITOR SERIAL
// ======================================================

void processarSerial() {

  if (!Serial.available()) {
    return;
  }

  String texto =
    Serial.readStringUntil(
      '\n'
    );

  texto.trim();

  if (
    texto.length() == 0
  ) {
    return;
  }

  String textoComando =
    texto;

  textoComando.toLowerCase();


  // ====================================================
  // TESTE DO ICONE DE ALERTA (sem precisar do sitio)
  //   alerta 1  = caixa baixa      alerta 2  = bateria
  //   alerta 4  = internet         alerta 8  = geral
  //   alerta 15 = todos            alerta off = desliga
  // (some os numeros para combinar alertas)
  // ====================================================

  if (textoComando.startsWith("alerta")) {

    if (textoComando.indexOf("off") >= 0) {
      alertasTeste = -1;
      Serial.println("Teste de alertas desligado.");

    } else {
      int valor =
        textoComando.substring(6).toInt();

      if (valor < 0) valor = 0;
      if (valor > 15) valor = 15;

      alertasTeste = valor;

      Serial.print("Teste de alertas: ");
      Serial.println(valor);
    }

    return;
  }


  // ====================================================
  // ATIVACAO PELO SERIAL
  // ====================================================

  if (
    estadoCentral ==
    REPOUSO
  ) {
    if (
      textoComando.indexOf(
        "central ativar"
      ) >= 0 ||

      textoComando.indexOf(
        "central, ativar"
      ) >= 0
    ) {
      ativarCentral();
      return;
    }
  }


  // ====================================================
  // PERGUNTA PELO SERIAL
  // ====================================================

  if (
    estadoCentral ==
    OUVINDO
  ) {
    escutaPendente = false;

    processarPergunta(
      texto
    );
  }
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(
    1000
  );

  Serial.println();

  Serial.println(
    "=========================="
  );

  Serial.println(
    " CENTRAL DE MONITORAMENTO"
  );

  Serial.println(
    "     SITIO 4 IRMAOS"
  );

  Serial.println(
    "=========================="
  );

  // Motivo do ultimo reinicio (ajuda a achar travamentos/quedas)
  {
    const char* motivo = "desconhecido";

    switch (esp_reset_reason()) {
      case ESP_RST_POWERON:   motivo = "ligou na energia (POWERON)"; break;
      case ESP_RST_EXT:       motivo = "botao de reset (EXT)"; break;
      case ESP_RST_SW:        motivo = "reinicio por software (SW)"; break;
      case ESP_RST_PANIC:     motivo = "ERRO DE PROGRAMA (PANIC)"; break;
      case ESP_RST_INT_WDT:   motivo = "WATCHDOG INTERNO (INT_WDT)"; break;
      case ESP_RST_TASK_WDT:  motivo = "WATCHDOG DE TAREFA (TASK_WDT)"; break;
      case ESP_RST_WDT:       motivo = "WATCHDOG (WDT)"; break;
      case ESP_RST_BROWNOUT:  motivo = "QUEDA DE TENSAO (BROWNOUT)"; break;
      case ESP_RST_DEEPSLEEP: motivo = "saiu do sono profundo"; break;
      default: break;
    }

    Serial.print("Motivo do ultimo reinicio: ");
    Serial.println(motivo);
  }

  // Botao fisico de ativacao (GPIO 18)
  iniciarBotao();

  // ====================================================
  // WIFI
  // ====================================================

  conectarWiFi();


  // ====================================================
  // RELOGIO
  // ====================================================

  configurarRelogio();


  // ====================================================
  // FIREBASE
  // ====================================================

  iniciarFirebase();


  // ====================================================
  // SENSOR LOCAL
  // ====================================================

  iniciarSensores();


  // ====================================================
  // AUDIO
  // ====================================================

  iniciarAudio();


  // ====================================================
  // MICROFONE
  // ====================================================

  iniciarMicrofone();


  // ====================================================
  // ATIVACAO POR VOZ ("central" + "ativar") E PELO BOTAO
  // Para voltar so ao botao: ATIVACAO_POR_VOZ 0 no topo.
  // ====================================================

#if ATIVACAO_POR_VOZ
  iniciarWakeWord();
#endif


  // ====================================================
  // DISPLAY
  // ====================================================

  iniciarDisplay();

  brilhoRepouso();

  // Nome completo da Central; o display rola automaticamente textos longos.
  mostrarDisplay(
    "CENTRAL DE MONITORAMENTO"
  );

  delay(
    2500
  );

  telaRepouso = 0;
  ultimaTrocaDisplay = 0;

  Serial.println();

#if ATIVACAO_POR_VOZ
  Serial.println(
    "Central pronta. Diga 'central, ativar' ou aperte o botao fisico."
  );
#else
  Serial.println(
    "Central pronta. Aperte o botao fisico para ativar."
  );
#endif

  // Da 5 minutos de folga antes de acusar "sitio sem comunicacao".
  ultimoDadoSitioOk = millis();

  passo("fim do setup");
  xTaskCreate(
    tarefaDebug,
    "debug",
    4096,
    NULL,
    1,
    NULL
  );
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  contadorLoops++;

  // ====================================================
  // WIFI
  // ====================================================

  passo("verificarConexaoWiFi");
  verificarConexaoWiFi();

  // ====================================================
  // BOTAO FISICO DE ATIVACAO - PRIORIDADE
  // ====================================================
  // O botao usa interrupcao para registrar o clique mesmo se
  // uma consulta do Firebase estiver demorando. A ativacao e
  // processada assim que o loop voltar a executar.
  passo("verificarBotao");
  if (estadoCentral == REPOUSO) {
    bool ativar = verificarBotaoPressionado();

#if ATIVACAO_POR_VOZ
    // Escuta a palavra de ativacao ("central" + "ativar").
    if (!ativar) {
      passo("verificarWakeWord");
      ativar = verificarWakeWord();
    }

    // Pega um clique registrado durante a captura da palavra.
    if (!ativar) {
      ativar = verificarBotaoPressionado();
    }
#endif

    if (ativar) {
      ativarCentral();
    }
  }

  passo("verificarRelogio");
  verificarRelogio();


  // ====================================================
  // SENSOR LOCAL
  // ====================================================

  passo("atualizarSensores");
  atualizarSensores();


  // ====================================================
  // FIREBASE
  // ====================================================

  passo("atualizarFirebase");
  atualizarFirebase();
  passo("atualizarValidadeDadosSitio");
  atualizarValidadeDadosSitio();
  atualizarSemComunicacao();


  // ====================================================
  // DISPLAY
  // ====================================================

  passo("atualizarDisplay");
  atualizarDisplay();


  // ====================================================
  // AUDIO
  // ====================================================

  passo("atualizarAudio");
  atualizarAudio();


  // ====================================================
  // DADOS DO SITIO
  // Nao le o Firebase enquanto o audio esta tocando.
  // ====================================================

  bool podeLerFirebase = true;

#if PAUSAR_FIREBASE_FALANDO
  // So le o Firebase em repouso. Durante a conversa (ativacao,
  // escuta, resposta) as leituras demoram segundos e seguravam
  // a central antes de ela voltar a ouvir.
  if (estadoCentral != REPOUSO || audioEmExecucao()) {
    podeLerFirebase = false;
  }
#endif

  if (
    podeLerFirebase &&
    millis() >= firebaseLiberadoApos &&
    millis() -
    ultimaLeituraFirebase >=
    INTERVALO_ETAPA_FIREBASE
  ) {

    // Uma leitura por vez; o loop (e o botao) voltam a rodar
    // entre uma leitura e a proxima.
    switch (etapaFirebase) {

      case 0:
        passo("lerCaixa 1");
        lerCaixa(
          1,
          caixa1
        );
        break;

      case 1:
        // So le a caixa 2 se o sensor estiver instalado
        // (ver CAIXA_2_INSTALADA no voz.h).
#if CAIXA_2_INSTALADA
        passo("lerCaixa 2");
        lerCaixa(
          2,
          caixa2
        );
#endif
        break;

      case 2:
        // So le a caixa principal se o sensor estiver instalado
        // (ver CAIXA_PRINCIPAL_INSTALADA no voz.h).
#if CAIXA_PRINCIPAL_INSTALADA
        passo("lerCaixa 3");
        lerCaixa(
          3,
          caixa3
        );
#endif
        break;

      case 3:
        passo("lerClimaSitio");
        lerClimaSitio();
        break;

      case 4:
        passo("lerChuvaSitio");
        lerChuvaSitio();
        break;

      default:
        // Alertas do sitio (icone na matriz)
        passo("lerAlertas");
        lerAlertas();
        break;
    }

    etapaFirebase++;

    if (etapaFirebase > 5) {
      etapaFirebase = 0;
    }

    // A pausa conta a partir do FIM da leitura.
    ultimaLeituraFirebase =
      millis();
  }


  // ====================================================
  // SERIAL
  // ====================================================

  passo("processarSerial");
  processarSerial();


  // ====================================================
  // MAQUINA DE ESTADOS
  // ====================================================

  passo("processarEstados");
  processarEstados();


  // ====================================================
  // REPOUSO
  // ====================================================

  passo("atualizarModoRepouso");
  atualizarModoRepouso();
}
