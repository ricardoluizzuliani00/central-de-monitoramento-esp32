#ifndef SENSORES_H
#define SENSORES_H

#include <DHT.h>

#define DHT_PIN 4
#define DHT_TIPO DHT22

inline DHT dht(DHT_PIN, DHT_TIPO);

inline float temperatura = 0;
inline float umidade = 0;
inline bool ambienteDisponivel = false;

inline unsigned long ultimaLeituraAmbiente = 0;
const unsigned long INTERVALO_AMBIENTE = 5000;

inline void iniciarSensores() {
  dht.begin();
  ambienteDisponivel = false;
  Serial.println("DHT22 iniciado.");
}

inline void lerAmbiente() {
  float t = dht.readTemperature();
  float u = dht.readHumidity();

  if (isnan(t) || isnan(u)) {
    Serial.println("Erro ao ler DHT22.");
    ambienteDisponivel = false;
    return;
  }

  temperatura = t;
  umidade = u;
  ambienteDisponivel = true;

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.print(" C | Umidade: ");
  Serial.print(umidade, 1);
  Serial.println(" %");
}

inline void atualizarSensores() {
  if (ultimaLeituraAmbiente != 0 && millis() - ultimaLeituraAmbiente < INTERVALO_AMBIENTE) {
    return;
  }
  ultimaLeituraAmbiente = millis();
  lerAmbiente();
}

#endif