#ifndef BOTAO_H
#define BOTAO_H

#include <Arduino.h>

// Botao momentaneo do gabinete ligado entre GPIO 18 e GND.
// INPUT_PULLUP dispensa resistor externo.
constexpr int PINO_BOTAO = 18;

// A interrupcao registra o clique mesmo quando o loop principal
// estiver ocupado por uma consulta demorada do Firebase.
inline volatile bool eventoBotaoPendente = false;
inline volatile uint32_t ultimaInterrupcaoBotao = 0;
constexpr uint32_t DEBOUNCE_BOTAO_US = 50000; // 50 ms

void IRAM_ATTR interrupcaoBotao() {
  const uint32_t agora = micros();

  // Debounce dentro da interrupcao; nao usar Serial nem delay aqui.
  if ((uint32_t)(agora - ultimaInterrupcaoBotao) >= DEBOUNCE_BOTAO_US) {
    ultimaInterrupcaoBotao = agora;
    eventoBotaoPendente = true;
  }
}

inline void iniciarBotao() {
  pinMode(PINO_BOTAO, INPUT_PULLUP);

  eventoBotaoPendente = false;
  ultimaInterrupcaoBotao = micros();
  attachInterrupt(digitalPinToInterrupt(PINO_BOTAO), interrupcaoBotao, FALLING);

  Serial.println("Botao fisico inicializado no GPIO 18 (interrupcao ativa).");
}

inline bool verificarBotaoPressionado() {
  // Copia e limpa o evento de forma atomica para nao perder um clique.
  noInterrupts();
  const bool pressionado = eventoBotaoPendente;
  eventoBotaoPendente = false;
  interrupts();

  if (pressionado) {
    Serial.println("Botao fisico acionado!");
    return true;
  }

  return false;
}

#endif
