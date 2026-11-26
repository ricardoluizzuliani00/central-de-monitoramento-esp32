#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>

struct RedeWiFi {
  const char* ssid;
  const char* senha;
};

// A lista de redes (nome + senha) fica em wifi_redes.h, que NAO vai para o GitHub.
// Copie wifi_redes_exemplo.h para wifi_redes.h e preencha.
#include "wifi_redes.h"

const int TOTAL_REDES = sizeof(REDES_WIFI) / sizeof(REDES_WIFI[0]);

const unsigned long TEMPO_TENTATIVA_WIFI = 5000;
const unsigned long INTERVALO_RECONEXAO_WIFI = 10000;

inline int redeAtual = -1;
inline int redeEmTentativa = -1;
inline unsigned long inicioTentativaWiFi = 0;
inline unsigned long ultimaRodadaWiFi = 0;
inline bool tentativaWiFiAtiva = false;
inline bool wifiEstavaConectado = false;

inline void identificarRedeAtual() {
  String ssidAtual = WiFi.SSID();
  redeAtual = -1;

  for (int i = 0; i < TOTAL_REDES; i++) {
    if (ssidAtual == REDES_WIFI[i].ssid) {
      redeAtual = i;
      break;
    }
  }
}

inline void iniciarTentativaWiFi(int indice) {
  if (indice < 0 || indice >= TOTAL_REDES) return;

  redeEmTentativa = indice;
  tentativaWiFiAtiva = true;
  inicioTentativaWiFi = millis();

  Serial.print("Tentando Wi-Fi: ");
  Serial.println(REDES_WIFI[indice].ssid);

  WiFi.disconnect(false, false);
  WiFi.begin(REDES_WIFI[indice].ssid, REDES_WIFI[indice].senha);
}

inline void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  Serial.println("Iniciando Wi-Fi...");

  redeAtual = -1;
  redeEmTentativa = 0;
  tentativaWiFiAtiva = false;
  wifiEstavaConectado = false;
  ultimaRodadaWiFi = 0;

  iniciarTentativaWiFi(0);
}

inline void verificarConexaoWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiEstavaConectado) {
      wifiEstavaConectado = true;
      tentativaWiFiAtiva = false;
      identificarRedeAtual();

      Serial.print("Wi-Fi conectado: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  if (wifiEstavaConectado) {
    wifiEstavaConectado = false;
    redeAtual = -1;
    tentativaWiFiAtiva = false;
    redeEmTentativa = -1;
    ultimaRodadaWiFi = millis();
    Serial.println("Wi-Fi desconectado.");
  }

  if (tentativaWiFiAtiva) {
    if (millis() - inicioTentativaWiFi < TEMPO_TENTATIVA_WIFI) return;

    Serial.print("Rede indisponivel: ");
    Serial.println(REDES_WIFI[redeEmTentativa].ssid);

    int proximaRede = redeEmTentativa + 1;
    tentativaWiFiAtiva = false;

    if (proximaRede < TOTAL_REDES) {
      iniciarTentativaWiFi(proximaRede);
      return;
    }

    redeEmTentativa = -1;
    ultimaRodadaWiFi = millis();
    Serial.println("Nenhuma rede Wi-Fi disponivel.");
    return;
  }

  if (millis() - ultimaRodadaWiFi < INTERVALO_RECONEXAO_WIFI) return;

  ultimaRodadaWiFi = millis();
  Serial.println("Procurando Wi-Fi novamente...");
  iniciarTentativaWiFi(0);
}

inline bool wifiDisponivel() {
  return WiFi.status() == WL_CONNECTED;
}

#endif
