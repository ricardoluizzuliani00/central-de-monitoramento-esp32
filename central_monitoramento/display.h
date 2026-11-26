#ifndef DISPLAY_H
#define DISPLAY_H

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ctype.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define PIN_CLK  12
#define PIN_DATA 11
#define PIN_CS   10

// 1 = a animacao do display roda numa tarefa propria (a rolagem nao
//     trava mais quando o loop esta ocupado gravando, falando ou
//     lendo o Firebase)
// 0 = comportamento anterior (animacao dentro do loop)
#define DISPLAY_EM_TAREFA 1

inline MD_Parola matriz = MD_Parola(
  HARDWARE_TYPE,
  PIN_DATA,
  PIN_CLK,
  PIN_CS,
  MAX_DEVICES
);

#define BRILHO_BAIXO 2
#define BRILHO_ALTO  12

#define HORA_APAGAR  0
#define HORA_ACENDER 8

// Largura da matriz em colunas (4 modulos x 8 colunas)
#define LARGURA_DISPLAY (MAX_DEVICES * 8)


// ======================================================
// ICONE DE ALERTA (notificacao do sitio)
// Quando ha alerta, um modulo inteiro (8x8) fica reservado
// para o icone e o texto passa a usar so os outros 3 modulos
// (24 colunas). Sem alerta, o texto volta a usar as 32.
// O icone pisca devagar e, com mais de um alerta, os icones
// se revezam no mesmo lugar.
// ======================================================

// 1 = icone no modulo 0 (lado direito da matriz)
// 0 = icone no modulo 3 (lado esquerdo). Se o icone aparecer
//     do lado errado, troque este valor.
#define ICONE_NA_DIREITA 1

// 1 = inverte de cabeca para baixo o icone e o relogio pequeno
//     (so se aparecerem de cabeca para baixo)
#define MATRIZ_INVERTE_VERTICAL 0

#define ICONE_TROCA_MS 2000   // tempo de cada icone quando ha varios
#define ICONE_PISCA_MS 1000   // 1 s aceso, 1 s apagado

#if ICONE_NA_DIREITA
  #define MODULO_ICONE             0
  #define TEXTO_REDUZIDO_PRIMEIRO  1
  #define TEXTO_REDUZIDO_ULTIMO    (MAX_DEVICES - 1)
#else
  #define MODULO_ICONE             (MAX_DEVICES - 1)
  #define TEXTO_REDUZIDO_PRIMEIRO  0
  #define TEXTO_REDUZIDO_ULTIMO    (MAX_DEVICES - 2)
#endif

inline bool zonaReduzida = false;

// Ultimo texto mostrado por mostrarDisplay(), para redesenhar
// quando a zona do texto muda de tamanho.
inline char ultimoTextoDisplay[64] = "";

inline uint16_t larguraZonaTexto() {
  return zonaReduzida ? (LARGURA_DISPLAY - 8) : LARGURA_DISPLAY;
}

// Definida mais abaixo; chamada pela tarefa do display.
inline void atualizarIconeAlerta();

inline bool displayEmUso = false;


// ======================================================
// TRAVA DO DISPLAY
// A animacao roda numa tarefa e o resto do codigo desenha
// pelo loop. A trava impede que os dois mexam na matriz
// ao mesmo tempo. E "recursiva" porque uma funcao do
// display chama outra.
// ======================================================

inline SemaphoreHandle_t mutexDisplay = NULL;

struct TravaDisplay {
  TravaDisplay() {
    if (mutexDisplay) {
      xSemaphoreTakeRecursive(mutexDisplay, portMAX_DELAY);
    }
  }
  ~TravaDisplay() {
    if (mutexDisplay) {
      xSemaphoreGiveRecursive(mutexDisplay);
    }
  }
};


// ======================================================
// NOME + VALOR
// ======================================================

inline bool aguardandoValorDisplay = false;
inline char valorPendenteDisplay[32] = "";
inline char nomePendenteDisplay[32] = "";

// Sequencia NOME + VALOR:
//   1) rola o NOME (ex.: "TEMP LOCAL"), pois nao cabe parado
//      nas 32 colunas da matriz
//   2) ao terminar de rolar, mostra o VALOR parado (ex.: "28.5 C")
//   3) mantem o valor na tela por HOLD_VALOR_MS
// A Central espera isso terminar antes de voltar a ouvir
// (ver displayOcupado()).
#define VELOCIDADE_NOME 60     // ms por passo: maior = mais devagar
#define HOLD_VALOR_MS   2500   // tempo com o valor parado na tela
#define TEMPO_MAX_ROLAGEM 60000

inline volatile bool rolagemEmAndamento = false;
inline unsigned long inicioRolagem = 0;
inline unsigned long manterValorAte = 0;


// ======================================================
// SINCRONIA COM A VOZ
// Antes de mostrar uma resposta, o programa informa quantos
// caracteres vai falar (definirFalaPrevista). A rolagem
// ajusta a velocidade para durar mais ou menos o mesmo
// tempo que a fala.
//   duracao estimada = BASE + caracteres * MS_POR_CHAR
// Se a voz ficar mais rapida/lenta que o texto, ajuste
// DISPLAY_MS_POR_CHAR (maior = rolagem mais lenta).
// ======================================================

#define DISPLAY_BASE_FALA_MS  400
#define DISPLAY_MS_POR_CHAR   65
#define DISPLAY_VEL_MIN       30   // nunca mais rapido que isso (ms/passo)
#define DISPLAY_VEL_MAX       90   // nunca mais lento que isso (ms/passo)

inline unsigned long duracaoFalaPrevistaMs = 0;

inline void definirFalaPrevista(size_t caracteres) {
  if (caracteres == 0) {
    duracaoFalaPrevistaMs = 0;
  } else {
    duracaoFalaPrevistaMs =
      DISPLAY_BASE_FALA_MS +
      (unsigned long)caracteres * DISPLAY_MS_POR_CHAR;
  }
}


// ======================================================
// LARGURA ESTIMADA DO TEXTO (em colunas)
// Estimativa: letras e numeros ~6 colunas, espaco 3,
// pontuacao fina 2. Se passar da largura da matriz, o
// texto rola em vez de ficar parado e cortado.
// ======================================================

inline uint16_t larguraEstimadaTexto(const char* texto) {
  uint16_t largura = 0;

  for (const char* p = texto; *p; p++) {
    char c = *p;

    if (c == ' ') {
      largura += 3;
    } else if (
      c == '.' || c == ':' || c == ',' ||
      c == '!' || c == '\''
    ) {
      largura += 2;
    } else {
      largura += 6;
    }
  }

  return largura;
}


// Velocidade (ms por passo) para a rolagem de um texto.
// Sem fala prevista, usa a velocidade padrao informada.
inline uint16_t velocidadeRolagem(
  const char* texto,
  uint16_t padrao
) {
  if (duracaoFalaPrevistaMs == 0) {
    return padrao;
  }

  unsigned long passos =
    (unsigned long)strlen(texto) * 6 + larguraZonaTexto();

  unsigned long v = duracaoFalaPrevistaMs / passos;

  if (v < DISPLAY_VEL_MIN) v = DISPLAY_VEL_MIN;
  if (v > DISPLAY_VEL_MAX) v = DISPLAY_VEL_MAX;

  return (uint16_t)v;
}


// ======================================================
// BUFFER DO TEXTO NA MATRIZ
// O MD_Parola NAO copia o texto: ele guarda so o ponteiro.
// Por isso o texto precisa ficar num buffer que continue
// existindo depois que a funcao que o montou terminar.
// ======================================================

inline char bufferDisplay[96] = "";


// ======================================================
// ANIMACAO (roda na tarefa do display, ou no loop se
// DISPLAY_EM_TAREFA for 0)
// ======================================================

inline void animarDisplay() {

  TravaDisplay trava;

  // O MD_Parola so anima e desenha quando displayAnimate()
  // e chamado repetidamente. Retorna true ao terminar.
  if (matriz.displayAnimate()) {

    if (rolagemEmAndamento) {
      rolagemEmAndamento = false;

      // Terminou de rolar o nome: agora mostra o valor.
      if (aguardandoValorDisplay) {
        aguardandoValorDisplay = false;

        snprintf(
          bufferDisplay,
          sizeof(bufferDisplay),
          "%s",
          valorPendenteDisplay
        );

        matriz.displayClear();

        if (
          larguraEstimadaTexto(bufferDisplay) >
          larguraZonaTexto()
        ) {
          // Com o icone de alerta a zona do texto fica menor:
          // se o valor nao couber parado, rola uma vez.
          rolagemEmAndamento = true;
          inicioRolagem = millis();

          matriz.displayText(
            bufferDisplay,
            PA_LEFT,
            VELOCIDADE_NOME,
            0,
            PA_SCROLL_LEFT,
            PA_SCROLL_LEFT
          );

          matriz.displayAnimate();

        } else {
          matriz.displayText(
            bufferDisplay,
            PA_CENTER,
            0,
            0,
            PA_PRINT,
            PA_NO_EFFECT
          );

          matriz.displayAnimate();

          manterValorAte = millis() + HOLD_VALOR_MS;
        }
      }
    }
  }

  // Icone de alerta (pisca, reveza, muda a zona do texto).
  atualizarIconeAlerta();
}


inline void tarefaDisplay(void* parametro) {
  for (;;) {
    animarDisplay();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


// ======================================================
// INICIALIZACAO
// ======================================================

inline void iniciarDisplay() {

  if (mutexDisplay == NULL) {
    mutexDisplay = xSemaphoreCreateRecursiveMutex();
  }

  {
    TravaDisplay trava;

    matriz.begin();
    matriz.setIntensity(BRILHO_BAIXO);
    matriz.displayClear();
  }

  Serial.println("MAX7219 iniciado.");

#if DISPLAY_EM_TAREFA
  xTaskCreatePinnedToCore(
    tarefaDisplay,
    "display",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  Serial.println("Animacao do display em tarefa propria.");
#endif
}


// ======================================================
// HORARIO DO DISPLAY
// ======================================================

inline bool horarioDisplayApagado() {
  if (!relogioSincronizado()) {
    return false;
  }

  struct tm horario;

  if (!getLocalTime(&horario, 50)) {
    return false;
  }

  int horaAtual = horario.tm_hour;

  return (
    horaAtual >= HORA_APAGAR &&
    horaAtual < HORA_ACENDER
  );
}


// ======================================================
// BRILHO DE REPOUSO
// ======================================================

inline void brilhoRepouso() {
  TravaDisplay trava;

  displayEmUso = false;

  // Sem hora valida ainda:
  // mantem o display ligado em brilho baixo.
  if (!relogioSincronizado()) {
    matriz.displayShutdown(false);
    matriz.setIntensity(BRILHO_BAIXO);
    return;
  }

  if (horarioDisplayApagado()) {
    matriz.displayShutdown(true);
  } else {
    matriz.displayShutdown(false);
    matriz.setIntensity(BRILHO_BAIXO);
  }
}


// ======================================================
// BRILHO QUANDO CENTRAL ESTA ATIVA
// ======================================================

inline void brilhoAtivado() {
  TravaDisplay trava;

  displayEmUso = true;
  matriz.displayShutdown(false);
  matriz.setIntensity(BRILHO_ALTO);
}


// ======================================================
// DESENHO INTERNO (nao cancela a sequencia NOME + VALOR)
// ======================================================

inline void desenharTextoParado(const char* texto) {
  TravaDisplay trava;

  snprintf(bufferDisplay, sizeof(bufferDisplay), "%s", texto);

  matriz.displayClear();

  matriz.displayText(
    bufferDisplay,
    PA_CENTER,
    0,
    0,
    PA_PRINT,
    PA_NO_EFFECT
  );

  // Texto parado: desenha agora, sem esperar o loop.
  matriz.displayAnimate();
}


inline void desenharTextoRolando(
  const char* texto,
  uint16_t velocidade
) {
  TravaDisplay trava;

  snprintf(
    bufferDisplay,
    sizeof(bufferDisplay),
    "%s",
    texto
  );

  rolagemEmAndamento = true;
  inicioRolagem = millis();

  matriz.displayClear();

  matriz.displayText(
    bufferDisplay,
    PA_LEFT,
    velocidade,
    0,
    PA_SCROLL_LEFT,
    PA_SCROLL_LEFT
  );

  // Inicia a animacao sem esperar o loop principal.
  matriz.displayAnimate();
}


// ======================================================
// TEXTO ROLANDO
// velocidade 0 = automatica (acompanha a fala prevista,
// ou 60 ms por passo se nao houver fala prevista)
// ======================================================

inline void rolarDisplay(
  const char* texto,
  uint16_t velocidade = 0
) {
  TravaDisplay trava;

  aguardandoValorDisplay = false;
  manterValorAte = 0;

  if (velocidade == 0) {
    velocidade = velocidadeRolagem(texto, 60);
  }

  desenharTextoRolando(texto, velocidade);
}


// ======================================================
// RELOGIO PEQUENO (usado quando o icone de alerta esta na
// matriz e a hora com a fonte normal nao cabe em 24 colunas)
// Digitos 3x5 desenhados ponto a ponto na zona do texto.
// ======================================================

inline const char* const DIGITOS_3X5[10][5] = {
  {"111", "101", "101", "101", "111"},  // 0
  {"010", "110", "010", "010", "111"},  // 1
  {"111", "001", "111", "100", "111"},  // 2
  {"111", "001", "111", "001", "111"},  // 3
  {"101", "101", "111", "001", "001"},  // 4
  {"111", "100", "111", "001", "111"},  // 5
  {"111", "100", "111", "101", "111"},  // 6
  {"111", "001", "001", "010", "010"},  // 7
  {"111", "101", "111", "101", "111"},  // 8
  {"111", "101", "111", "001", "111"}   // 9
};

// Acende ou apaga um ponto. linha 0 = topo; coluna do MD_MAX72XX
// (coluna 0 = ponta direita da matriz).
inline void pontoMatriz(
  MD_MAX72XX* mx,
  int linha,
  int colunaMx,
  bool ligado
) {
#if MATRIZ_INVERTE_VERTICAL
  linha = 7 - linha;
#endif
  mx->setPoint(linha, colunaMx, ligado);
}

inline bool pareceHora(const char* t) {
  return (
    strlen(t) == 5 &&
    isdigit((unsigned char)t[0]) &&
    isdigit((unsigned char)t[1]) &&
    t[2] == ':' &&
    isdigit((unsigned char)t[3]) &&
    isdigit((unsigned char)t[4])
  );
}

inline void desenharRelogioCompacto(const char* hhmm) {
  TravaDisplay trava;

  aguardandoValorDisplay = false;
  rolagemEmAndamento = false;
  manterValorAte = 0;

  // Limpa a zona do texto e deixa o Parola sem nada para
  // desenhar, para ele nao apagar os pontos feitos abaixo.
  matriz.displayClear();

  matriz.displayText(
    "",
    PA_CENTER,
    0,
    0,
    PA_PRINT,
    PA_NO_EFFECT
  );

  matriz.displayAnimate();

  MD_MAX72XX* mx = matriz.getGraphicObject();

  // Coluna mais a esquerda da zona do texto (valor mais alto).
  const int colEsquerda = TEXTO_REDUZIDO_ULTIMO * 8 + 7;

  // Relogio ocupa 17 colunas; centraliza nas 24 da zona.
  const int margem = (24 - 17) / 2;

  // Posicao (em colunas) de cada caractere de "HH:MM"
  const int posicoes[5] = {0, 4, 8, 10, 14};

  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  for (int i = 0; i < 5; i++) {

    if (i == 2) {
      // dois pontos
      pontoMatriz(mx, 1 + 1, colEsquerda - (margem + posicoes[i]), true);
      pontoMatriz(mx, 1 + 3, colEsquerda - (margem + posicoes[i]), true);
      continue;
    }

    int digito = hhmm[i] - '0';

    if (digito < 0 || digito > 9) {
      continue;
    }

    for (int y = 0; y < 5; y++) {
      for (int x = 0; x < 3; x++) {
        if (DIGITOS_3X5[digito][y][x] == '1') {
          pontoMatriz(
            mx,
            1 + y,
            colEsquerda - (margem + posicoes[i] + x),
            true
          );
        }
      }
    }
  }

  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  mx->update();
}


// ======================================================
// TEXTO PARADO
// Se o texto nao cabe nas 32 colunas, rola sozinho em vez
// de ficar cortado.
// ======================================================

inline void mostrarDisplay(const char* texto) {
  TravaDisplay trava;

  snprintf(
    ultimoTextoDisplay,
    sizeof(ultimoTextoDisplay),
    "%s",
    texto
  );

  // Com o icone de alerta aceso, a hora nao cabe em 24 colunas
  // na fonte normal: usa o relogio pequeno.
  if (zonaReduzida && pareceHora(texto)) {
    desenharRelogioCompacto(texto);
    return;
  }

  if (larguraEstimadaTexto(texto) > larguraZonaTexto()) {
    rolarDisplay(texto);
    return;
  }

  // Texto novo cancela qualquer sequencia NOME + VALOR.
  aguardandoValorDisplay = false;
  rolagemEmAndamento = false;
  manterValorAte = 0;

  desenharTextoParado(texto);
}


// ======================================================
// MOSTRA O NOME ROLANDO E DEPOIS O VALOR
// ======================================================

inline void mostrarNomeValor(
  const char* nome,
  const char* valor
) {
  TravaDisplay trava;

  snprintf(
    valorPendenteDisplay,
    sizeof(valorPendenteDisplay),
    "%s",
    valor
  );

  manterValorAte = 0;

  // 1) rola o nome; a animacao mostra o valor
  //    quando a rolagem terminar.
  desenharTextoRolando(
    nome,
    velocidadeRolagem(nome, VELOCIDADE_NOME)
  );

  aguardandoValorDisplay = true;
}


// ======================================================
// ICONE DE ALERTA 8x8
// 0 = caixa d'agua   1 = pilha   2 = internet   3 = sitio
// ======================================================

inline const char* const ICONES_8X8[4][8] = {
  {"01111110", "10000001", "10000001", "11111111",
   "11111111", "11111111", "01111110", "00100100"},  // caixa baixa
  {"00011000", "01111110", "01000010", "01000010",
   "01000010", "01111110", "01111110", "01111110"},  // bateria fraca
  {"00111100", "01000010", "10011001", "00100100",
   "00011000", "00000000", "00011000", "00000000"},  // internet (a barra e feita no codigo)
  {"00011000", "00111100", "01111110", "11111111",
   "01111110", "01100110", "01100110", "01100110"}   // alerta geral (sitio)
};

inline int iconeDesenhado = -2;
inline unsigned long ultimoDesenhoIcone = 0;

// id = -1 apaga o modulo do icone
inline void desenharIconeAlerta(int id) {
  TravaDisplay trava;

  MD_MAX72XX* mx = matriz.getGraphicObject();

  const int colBase = MODULO_ICONE * 8;

  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      bool ligado = false;

      if (id >= 0) {
        ligado = (ICONES_8X8[id][y][x] == '1');

        // barra diagonal do icone de internet cortada
        if (id == 2 && x == 7 - y) {
          ligado = true;
        }
      }

      // x = 0 e a esquerda do icone; coluna 0 do MD_MAX72XX e a direita
      pontoMatriz(mx, y, colBase + (7 - x), ligado);
    }
  }

  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  mx->update();

  iconeDesenhado = id;
  ultimoDesenhoIcone = millis();
}

// Muda o tamanho da zona do texto (32 colunas sem alerta,
// 24 com alerta) e redesenha o ultimo texto.
inline void aplicarZonaTexto(bool reduzida) {
  TravaDisplay trava;

  char textoSalvo[64];
  snprintf(textoSalvo, sizeof(textoSalvo), "%s", ultimoTextoDisplay);

  matriz.displayClear();

  if (reduzida) {
    matriz.setZone(0, TEXTO_REDUZIDO_PRIMEIRO, TEXTO_REDUZIDO_ULTIMO);
  } else {
    matriz.setZone(0, 0, MAX_DEVICES - 1);
  }

  zonaReduzida = reduzida;
  iconeDesenhado = -2;

  // limpa a matriz toda (icone e restos de texto)
  matriz.getGraphicObject()->clear();

  if (textoSalvo[0] != '\0') {
    mostrarDisplay(textoSalvo);
  }
}

inline void atualizarIconeAlerta() {
  TravaDisplay trava;

  int mascara = alertasAtivos();
  bool querReduzida = (mascara != 0);

  // So muda a zona do texto quando o visor nao esta no meio de
  // uma resposta rolando (evita cortar a mensagem).
  if (querReduzida != zonaReduzida) {
    bool visorLivre =
      !rolagemEmAndamento &&
      !aguardandoValorDisplay &&
      millis() >= manterValorAte;

    if (visorLivre) {
      aplicarZonaTexto(querReduzida);
    }
  }

  if (!zonaReduzida) {
    return;
  }

  int id = -1;

  if (mascara != 0) {
    int ids[4];
    int n = 0;

    if (mascara & ALERTA_CAIXA)    ids[n++] = 0;
    if (mascara & ALERTA_BATERIA)  ids[n++] = 1;
    if (mascara & ALERTA_INTERNET) ids[n++] = 2;
    if (mascara & ALERTA_GERAL)    ids[n++] = 3;

    unsigned long agora = millis();

    id = ids[(agora / ICONE_TROCA_MS) % n];

    // fase apagada do pisca
    if ((agora / ICONE_PISCA_MS) % 2 == 1) {
      id = -1;
    }
  }

  // Redesenha quando muda e, por garantia, a cada 1 s.
  if (
    id != iconeDesenhado ||
    millis() - ultimoDesenhoIcone > 1000
  ) {
    desenharIconeAlerta(id);
  }
}


// ======================================================
// DISPLAY AINDA MOSTRANDO UMA RESPOSTA?
// A Central usa isto para nao voltar a ouvir (e apagar o
// visor) antes de a resposta terminar de aparecer.
// ======================================================

inline bool displayOcupado() {
  TravaDisplay trava;

  // Rolando (ou aguardando o valor depois do nome).
  if (
    rolagemEmAndamento &&
    millis() - inicioRolagem < TEMPO_MAX_ROLAGEM
  ) {
    return true;
  }

  // Valor parado na tela durante o tempo de leitura.
  if (millis() < manterValorAte) {
    return true;
  }

  return false;
}


// ======================================================
// HORA DE REPOUSO
// ======================================================

inline void mostrarHoraRepouso() {
  if (!relogioSincronizado()) {
    mostrarDisplay("--:--");
    return;
  }

  struct tm horario;

  if (!getLocalTime(&horario, 50)) {
    mostrarDisplay("--:--");
    return;
  }

  char texto[8];

  strftime(
    texto,
    sizeof(texto),
    "%H:%M",
    &horario
  );

  mostrarDisplay(texto);
}


// ======================================================
// RESPOSTAS NO DISPLAY
// ======================================================

inline void mostrarRespostaDisplay(ComandoVoz comando) {

  char texto[64];

  switch (comando) {


    // ==================================================
    // CAIXA 1
    // ==================================================

    case NIVEL_CAIXA_1:

      if (caixa1.disponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "CX1 %.0f%%",
          caixa1.nivel
        );

        mostrarDisplay(texto);

      } else {
        rolarDisplay("CX1 SEM DADOS");
      }

      break;


    // ==================================================
    // CAIXA 2
    // ==================================================

    case NIVEL_CAIXA_2:

      if (caixa2.disponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "CX2 %.0f%%",
          caixa2.nivel
        );

        mostrarDisplay(texto);

      } else {
        rolarDisplay("CX2 SEM DADOS");
      }

      break;


    // ==================================================
    // CAIXA PRINCIPAL
    // ==================================================

    case NIVEL_CAIXA_PRINCIPAL:

      if (caixa3.disponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "PRIN %.0f%%",
          caixa3.nivel
        );

        mostrarDisplay(texto);

      } else {
        rolarDisplay("PRINCIPAL SEM DADOS");
      }

      break;


    // ==================================================
    // TODAS AS CAIXAS
    // ==================================================

    case STATUS_CAIXAS:

      if (
        caixa1.disponivel ||
        caixa2.disponivel ||
        caixa3.disponivel
      ) {
        snprintf(
          texto,
          sizeof(texto),
          "C1 %.0f%% C2 %.0f%% PR %.0f%%",
          caixa1.nivel,
          caixa2.nivel,
          caixa3.nivel
        );

        rolarDisplay(texto);

      } else {
        rolarDisplay("CAIXAS SEM DADOS");
      }

      break;


    // ==================================================
    // TEMPERATURA LOCAL
    // ==================================================

    case TEMPERATURA:

      if (ambienteDisponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.1f C",
          temperatura
        );

        mostrarNomeValor(
          "TEMP LOCAL",
          texto
        );

      } else {
        rolarDisplay("TEMP LOCAL SEM DADOS");
      }

      break;


    // ==================================================
    // UMIDADE LOCAL
    // ==================================================

    case UMIDADE:

      if (ambienteDisponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.0f%%",
          umidade
        );

        mostrarNomeValor(
          "UMID LOCAL",
          texto
        );

      } else {
        rolarDisplay("UMID LOCAL SEM DADOS");
      }

      break;


    // ==================================================
    // TEMPERATURA DO SITIO
    // ==================================================

    case TEMPERATURA_SITIO:

      if (climaSitio.disponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.1f C",
          climaSitio.temperatura
        );

        mostrarNomeValor(
          "TEMP SITIO",
          texto
        );

      } else {
        rolarDisplay("TEMP SITIO SEM DADOS");
      }

      break;


    // ==================================================
    // UMIDADE DO SITIO
    // ==================================================

    case UMIDADE_SITIO:

      if (climaSitio.disponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.0f%%",
          climaSitio.umidade
        );

        mostrarNomeValor(
          "UMID SITIO",
          texto
        );

      } else {
        rolarDisplay("UMID SITIO SEM DADOS");
      }

      break;


    // ==================================================
    // PRESSAO DO SITIO
    // ==================================================

    case PRESSAO_SITIO:

      if (climaSitio.disponivel) {
        snprintf(
          texto,
          sizeof(texto),
          "%.0f hPa",
          climaSitio.pressao
        );

        rolarDisplay(texto);

      } else {
        rolarDisplay("PRESSAO SEM DADOS");
      }

      break;


    // ==================================================
    // CHUVA
    // ==================================================

    case CHUVA:

      if (!chuvaSitio.disponivel) {
        rolarDisplay("CHUVA SEM DADOS");

      } else if (
        strcmp(chuvaSitio.status, "seco") == 0
      ) {
        mostrarDisplay("SECO");

      } else if (
        strcmp(chuvaSitio.status, "gotas") == 0
      ) {
        rolarDisplay("CHUVISCANDO");

      } else if (
        strcmp(chuvaSitio.status, "chuva") == 0
      ) {
        mostrarDisplay("CHUVA");

      } else {
        rolarDisplay("SEM DADOS");
      }

      break;


    // ==================================================
    // HORA
    // ==================================================

    case HORA: {

      if (!relogioSincronizado()) {
        mostrarDisplay("--:--");
        break;
      }

      struct tm horario;

      if (getLocalTime(&horario, 50)) {
        strftime(
          texto,
          sizeof(texto),
          "%H:%M",
          &horario
        );

        mostrarDisplay(texto);

      } else {
        mostrarDisplay("--:--");
      }

      break;
    }


    // ==================================================
    // DESCONHECIDO
    // ==================================================

    case AGRADECIMENTO:
      // Voz: "Por nada! Estou aqui se precisar."
      mostrarDisplay("POR NADA");
      break;

    case CONFIRMACAO:
      // Voz: "Certo. Pode perguntar."
      mostrarDisplay("CERTO");
      break;

    default:
      rolarDisplay("NAO ENTENDI");
      break;
  }
}


// ======================================================
// ATUALIZACAO DO DISPLAY
// Com DISPLAY_EM_TAREFA = 1 a animacao ja roda sozinha e
// esta funcao nao faz nada. Com 0, anima aqui (no loop).
// ======================================================

inline void atualizarDisplay() {

#if !DISPLAY_EM_TAREFA
  animarDisplay();
#endif
}

#endif
