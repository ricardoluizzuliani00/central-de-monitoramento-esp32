#ifndef RELOGIO_H
#define RELOGIO_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

const char* SERVIDOR_NTP = "pool.ntp.org";
const long GMT_OFFSET = -3 * 3600;
const int DAYLIGHT_OFFSET = 0;

const unsigned long INTERVALO_NTP = 60000;

inline unsigned long ultimaTentativaNTP = 0;
inline bool relogioJaSincronizado = false;

inline bool relogioSincronizado() {
  return time(nullptr) > 1000000000;
}

inline void configurarRelogio() {
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, SERVIDOR_NTP);
  ultimaTentativaNTP = millis();
  Serial.println("Relogio NTP configurado. Aguardando sincronizacao...");
}

inline void verificarRelogio() {
  if (relogioSincronizado()) {
    if (!relogioJaSincronizado) {
      relogioJaSincronizado = true;
      Serial.println("Relogio sincronizado.");
    }
    return;
  }

  relogioJaSincronizado = false;

  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - ultimaTentativaNTP < INTERVALO_NTP) return;

  ultimaTentativaNTP = millis();
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, SERVIDOR_NTP);
  Serial.println("Tentando sincronizar relogio novamente...");
}

inline void mostrarHora() {
  struct tm horario;

  if (!getLocalTime(&horario, 50)) {
    Serial.println("Hora ainda nao sincronizada.");
    return;
  }

  char hora[9];
  strftime(hora, sizeof(hora), "%H:%M:%S", &horario);

  Serial.print("Hora: ");
  Serial.println(hora);
}

#endif
