#ifndef DADOS_SITIO_H
#define DADOS_SITIO_H

// FIREBASE_URL fica em firebase_secrets.h (nao vai para o GitHub).

const char* PATH_CAIXA_1 = "/caixas/caixa1";
const char* PATH_CAIXA_2 = "/caixas/caixa2";
const char* PATH_CAIXA_3 = "/caixas/caixa3";

const char* PATH_CLIMA = "/clima";
const char* PATH_CHUVA = "/chuva";

struct DadosCaixa {
  float nivel = 0;
  float litros = 0;
  unsigned long timestamp = 0;
  bool disponivel = false;
};

inline DadosCaixa caixa1;
inline DadosCaixa caixa2;
inline DadosCaixa caixa3;

struct DadosClimaSitio {
  float temperatura = 0;
  float umidade = 0;
  float pressao = 0;
  unsigned long timestamp = 0;
  bool disponivel = false;
};

inline DadosClimaSitio climaSitio;

struct DadosChuvaSitio {
  char status[16] = "";
  unsigned long timestamp = 0;
  bool disponivel = false;
};

inline DadosChuvaSitio chuvaSitio;

// ======================================================
// ALERTAS (icone de notificacao na matriz)
// O sitio grava em /alertas/mascara um numero que SOMA:
//   1 = caixa d'agua baixa
//   2 = bateria fraca
//   4 = internet fora
//   8 = alerta geral do sitio
// 0 = nenhum alerta. Ao tocar em "Visto" no Telegram, o
// sitio grava 0 e o icone some da matriz.
// ======================================================

#define ALERTA_CAIXA    1
#define ALERTA_BATERIA  2
#define ALERTA_INTERNET 4
#define ALERTA_GERAL    8

// Ultimo valor lido do Firebase (/alertas/mascara)
inline int alertasFirebase = 0;

// -1 = sem teste. 0 a 15 = forca esse valor
// (comando "alerta N" no Monitor Serial, "alerta off" desliga)
inline int alertasTeste = -1;

// true quando a central ficou muito tempo sem receber dados
// do sitio (sitio sem internet, ou a propria central sem rede)
inline bool semComunicacaoSitio = false;
inline unsigned long ultimoDadoSitioOk = 0;

// Mascara final que o display usa para desenhar o icone
inline int alertasAtivos() {
  int m = (alertasTeste >= 0) ? alertasTeste : alertasFirebase;

  if (semComunicacaoSitio) {
    m |= ALERTA_INTERNET;
  }

  return m & 15;
}

#endif