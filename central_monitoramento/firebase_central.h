#ifndef FIREBASE_CENTRAL_H

#define FIREBASE_CENTRAL_H

#define ENABLE_USER_AUTH

#define ENABLE_DATABASE

#include <WiFi.h>

#include <WiFiClientSecure.h>

#include <FirebaseClient.h>

#include <time.h>

#include "firebase_secrets.h"

// ======================================================

// FIREBASE

// ======================================================

inline UserAuth user_auth(

  FIREBASE_API_KEY,

  FIREBASE_USER_EMAIL,

  FIREBASE_USER_PASSWORD

);

inline FirebaseApp firebaseApp;

inline WiFiClientSecure ssl_client;

using AsyncClient =

  AsyncClientClass;

inline AsyncClient asyncClient(

  ssl_client

);

inline RealtimeDatabase database;

// ======================================================

// CONFIGURACAO

// ======================================================

// Dados do sitio sao considerados atuais

// por no maximo 60 segundos.

const unsigned long TEMPO_DADO_VALIDO =

  60;

// ======================================================

// CALLBACK

// ======================================================

inline void firebaseCallback(

  AsyncResult &aResult

) {

  if (!aResult.isResult()) {

    return;

  }

  if (aResult.isEvent()) {

    Serial.print(

      "Firebase: "

    );

    Serial.println(

      aResult.eventLog()

        .message()

        .c_str()

    );

  }

  if (aResult.isError()) {

    Serial.print(

      "Erro Firebase: "

    );

    Serial.println(

      aResult.error()

        .message()

        .c_str()

    );

  }

}

// ======================================================

// INICIAR FIREBASE

// ======================================================

inline void iniciarFirebase() {

  Serial.println(

    "Iniciando Firebase..."

  );

  ssl_client.setInsecure();

  ssl_client.setConnectionTimeout(

    1000

  );

  ssl_client.setHandshakeTimeout(

    5

  );

  initializeApp(

    asyncClient,

    firebaseApp,

    getAuth(user_auth),

    firebaseCallback,

    "authTask"

  );

  firebaseApp

    .getApp<RealtimeDatabase>(

      database

    );

  database.url(

    FIREBASE_URL

  );

  Serial.println(

    "Firebase configurado."

  );

}

// ======================================================

// LOOP FIREBASE

// ======================================================

inline void atualizarFirebase() {

  firebaseApp.loop();

}

// ======================================================

// FIREBASE PRONTO

// ======================================================

inline bool firebasePronto() {

  if (

    WiFi.status() !=

    WL_CONNECTED

  ) {

    return false;

  }

  return firebaseApp.ready();

}

// ======================================================

// TIMESTAMP RECENTE

// ======================================================

inline bool timestampRecente(

  unsigned long timestamp

) {

  if (

    timestamp == 0

  ) {

    return false;

  }

  time_t agora;

  time(

    &agora

  );

  // Relogio ainda nao sincronizado.

  if (

    agora < 1000000000

  ) {

    return false;

  }

  // Timestamp futuro invalido.

  if (

    agora <

    (time_t)timestamp

  ) {

    return false;

  }

  return (

    (

      (unsigned long)agora -

      timestamp

    )

    <=

    TEMPO_DADO_VALIDO

  );

}

// ======================================================

// REVALIDAR DADOS JA SALVOS

// ======================================================

inline void atualizarValidadeDadosSitio() {

  caixa1.disponivel =

    timestampRecente(

      caixa1.timestamp

    );

  caixa2.disponivel =

    timestampRecente(

      caixa2.timestamp

    );

  caixa3.disponivel =

    timestampRecente(

      caixa3.timestamp

    );

  climaSitio.disponivel =

    timestampRecente(

      climaSitio.timestamp

    );

  chuvaSitio.disponivel =

    timestampRecente(

      chuvaSitio.timestamp

    );

}

// ======================================================

// RESULTADO DA ULTIMA LEITURA

// ======================================================

inline bool leituraFirebaseOK() {

  return (

    asyncClient

      .lastError()

      .code()

    ==

    0

  );

}

// ======================================================

// LOG DE ERRO

// ======================================================

inline void mostrarErroFirebase(

  const char* origem

) {

  if (

    leituraFirebaseOK()

  ) {

    return;

  }

  Serial.print(

    "Erro Firebase em "

  );

  Serial.print(

    origem

  );

  Serial.print(

    ": "

  );

  Serial.println(

    asyncClient

      .lastError()

      .message()

      .c_str()

  );

}

// ======================================================

// VERIFICAR SE PODE CONSULTAR

// ======================================================

inline bool podeLerFirebase() {

  if (

    WiFi.status() !=

    WL_CONNECTED

  ) {

    return false;

  }

  if (

    !firebaseApp.ready()

  ) {

    return false;

  }

  return true;

}

// ======================================================

// CAIXA

// ======================================================

inline void lerCaixa(

  int numero,

  DadosCaixa &caixa

) {

  // Mesmo sem Firebase,

  // reavalia se o dado antigo

  // ainda pode ser considerado valido.

  if (!podeLerFirebase()) {

    caixa.disponivel =

      timestampRecente(

        caixa.timestamp

      );

    return;

  }

  String caminhoBase =

    "/caixas/caixa" +

    String(numero);

  String caminhoNivel =

    caminhoBase +

    "/nivel_percentual";

  String caminhoLitros =

    caminhoBase +

    "/litros";

  String caminhoTimestamp =

    caminhoBase +

    "/timestamp";

  // ====================================================

  // NIVEL

  // ====================================================

  float novoNivel =

    database.get<float>(

      asyncClient,

      caminhoNivel.c_str()

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "nivel da caixa"

    );

    caixa.disponivel =

      timestampRecente(

        caixa.timestamp

      );

    return;

  }

  // ====================================================

  // LITROS

  // ====================================================

  float novosLitros =

    database.get<float>(

      asyncClient,

      caminhoLitros.c_str()

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "litros da caixa"

    );

    caixa.disponivel =

      timestampRecente(

        caixa.timestamp

      );

    return;

  }

  // ====================================================

  // TIMESTAMP

  // ====================================================

  unsigned long novoTimestamp =

    database.get<unsigned long>(

      asyncClient,

      caminhoTimestamp.c_str()

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "timestamp da caixa"

    );

    caixa.disponivel =

      timestampRecente(

        caixa.timestamp

      );

    return;

  }

  // ====================================================

  // SO SUBSTITUI DEPOIS QUE TODAS AS LEITURAS DERAM CERTO

  // ====================================================

  caixa.nivel =

    novoNivel;

  caixa.litros =

    novosLitros;

  caixa.timestamp =

    novoTimestamp;

  caixa.disponivel =

    timestampRecente(

      caixa.timestamp

    );

  // ====================================================

  // LOG

  // ====================================================

  Serial.print(

    "Caixa "

  );

  if (

    numero == 3

  ) {

    Serial.print(

      "Principal"

    );

  } else {

    Serial.print(

      numero

    );

  }

  if (

    caixa.disponivel

  ) {

    Serial.print(

      ": "

    );

    Serial.print(

      caixa.nivel

    );

    Serial.print(

      "% | "

    );

    Serial.print(

      caixa.litros

    );

    Serial.println(

      " litros"

    );

  } else {

    Serial.println(

      ": dados antigos ou indisponiveis"

    );

  }

}

// ======================================================

// CLIMA DO SITIO

// ======================================================

inline void lerClimaSitio() {

  if (

    !podeLerFirebase()

  ) {

    climaSitio.disponivel =

      timestampRecente(

        climaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // TEMPERATURA

  // ====================================================

  float novaTemperatura =

    database.get<float>(

      asyncClient,

      "/clima/temperatura"

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "temperatura do sitio"

    );

    climaSitio.disponivel =

      timestampRecente(

        climaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // UMIDADE

  // ====================================================

  float novaUmidade =

    database.get<float>(

      asyncClient,

      "/clima/umidade"

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "umidade do sitio"

    );

    climaSitio.disponivel =

      timestampRecente(

        climaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // PRESSAO

  // ====================================================

  float novaPressao =

    database.get<float>(

      asyncClient,

      "/clima/pressao"

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "pressao do sitio"

    );

    climaSitio.disponivel =

      timestampRecente(

        climaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // TIMESTAMP

  // ====================================================

  unsigned long novoTimestamp =

    database.get<unsigned long>(

      asyncClient,

      "/clima/timestamp"

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "timestamp do clima"

    );

    climaSitio.disponivel =

      timestampRecente(

        climaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // ATUALIZA SOMENTE COM LEITURA COMPLETA

  // ====================================================

  climaSitio.temperatura =

    novaTemperatura;

  climaSitio.umidade =

    novaUmidade;

  climaSitio.pressao =

    novaPressao;

  climaSitio.timestamp =

    novoTimestamp;

  climaSitio.disponivel =

    timestampRecente(

      climaSitio.timestamp

    );

  // ====================================================

  // LOG

  // ====================================================

  if (

    climaSitio.disponivel

  ) {

    Serial.print(

      "Clima Sitio: "

    );

    Serial.print(

      climaSitio.temperatura,

      1

    );

    Serial.print(

      " C | "

    );

    Serial.print(

      climaSitio.umidade,

      1

    );

    Serial.print(

      "% | "

    );

    Serial.print(

      climaSitio.pressao,

      1

    );

    Serial.println(

      " hPa"

    );

  } else {

    Serial.println(

      "Clima Sitio: dados antigos ou indisponiveis"

    );

  }

}

// ======================================================

// CHUVA DO SITIO

// ======================================================

inline void lerChuvaSitio() {

  if (

    !podeLerFirebase()

  ) {

    chuvaSitio.disponivel =

      timestampRecente(

        chuvaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // STATUS

  // ====================================================

  String novoStatus =

    database.get<String>(

      asyncClient,

      "/chuva/status"

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "status da chuva"

    );

    chuvaSitio.disponivel =

      timestampRecente(

        chuvaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // TIMESTAMP

  // ====================================================

  unsigned long novoTimestamp =

    database.get<unsigned long>(

      asyncClient,

      "/chuva/timestamp"

    );

  if (

    !leituraFirebaseOK()

  ) {

    mostrarErroFirebase(

      "timestamp da chuva"

    );

    chuvaSitio.disponivel =

      timestampRecente(

        chuvaSitio.timestamp

      );

    return;

  }

  // ====================================================

  // ATUALIZA SOMENTE DEPOIS DA LEITURA COMPLETA

  // ====================================================

  snprintf(

    chuvaSitio.status,

    sizeof(

      chuvaSitio.status

    ),

    "%s",

    novoStatus.c_str()

  );

  chuvaSitio.timestamp =

    novoTimestamp;

  chuvaSitio.disponivel =

    timestampRecente(

      chuvaSitio.timestamp

    );

  // ====================================================

  // LOG

  // ====================================================

  if (

    chuvaSitio.disponivel

  ) {

    Serial.print(

      "Chuva Sitio: "

    );

    Serial.println(

      chuvaSitio.status

    );

  } else {

    Serial.println(

      "Chuva Sitio: dados antigos ou indisponiveis"

    );

  }

}

// ======================================================
// ALERTAS DO SITIO
// Le /alertas/mascara (soma de 1, 2, 4 e 8; ver dados_sitio.h)
// ======================================================

inline void lerAlertas() {

  if (!podeLerFirebase()) {
    return;
  }

  int novaMascara =
    database.get<int>(
      asyncClient,
      "/alertas/mascara"
    );

  if (!leituraFirebaseOK()) {
    mostrarErroFirebase("alertas");
    return;
  }

  if (novaMascara < 0) {
    novaMascara = 0;
  }

  novaMascara &= 15;

  if (novaMascara != alertasFirebase) {
    Serial.print("Alertas do sitio mudaram: ");
    Serial.print(alertasFirebase);
    Serial.print(" -> ");
    Serial.println(novaMascara);
  }

  alertasFirebase = novaMascara;
}

// ======================================================
// SITIO SEM COMUNICACAO
// Se nenhum dado do sitio ficar recente por 5 minutos,
// acende o icone de internet na matriz.
// ======================================================

const unsigned long TEMPO_SEM_COMUNICACAO_MS =
  5UL * 60UL * 1000UL;

inline void atualizarSemComunicacao() {

  bool algumDadoRecente =
    caixa1.disponivel ||
    caixa2.disponivel ||
    caixa3.disponivel ||
    climaSitio.disponivel ||
    chuvaSitio.disponivel;

  if (algumDadoRecente) {
    ultimoDadoSitioOk = millis();
    semComunicacaoSitio = false;
    return;
  }

  if (
    millis() - ultimoDadoSitioOk >
    TEMPO_SEM_COMUNICACAO_MS
  ) {
    semComunicacaoSitio = true;
  }
}

#endif