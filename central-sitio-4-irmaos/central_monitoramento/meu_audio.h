#ifndef MEU_AUDIO_H
#define MEU_AUDIO_H

#include <Arduino.h>
#include <Audio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define MAX_I2S_BCLK 15
#define MAX_I2S_LRC  16
#define MAX_I2S_DIN  17

#define AUDIO_AUMENTAR_BUFFER 0
#define AUDIO_BUFFER_PSRAM_BYTES (512 * 1024)
#define AUDIO_EM_TAREFA 1

inline Audio audioOutput(1);
inline SemaphoreHandle_t mutexAudio = NULL;
inline bool audioTarefaCriada = false;
inline bool audioInicializado = false;

// A tarefa de audio e a maquina de estados usam o mesmo objeto Audio.
// Todas as consultas/alteracoes ao objeto precisam usar o mesmo mutex.
inline void tarefaAudio(void* parametro) {
  (void)parametro;

  for (;;) {
    if (mutexAudio != NULL &&
        xSemaphoreTake(mutexAudio, portMAX_DELAY) == pdTRUE) {
      audioOutput.loop();
      xSemaphoreGive(mutexAudio);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

inline void iniciarAudio() {
  if (mutexAudio == NULL) {
    mutexAudio = xSemaphoreCreateMutex();
  }

  if (mutexAudio == NULL) {
    Serial.println("[AUDIO] ERRO: nao foi possivel criar mutex.");
    audioInicializado = false;
    return;
  }

  audioOutput.setPinout(MAX_I2S_BCLK, MAX_I2S_LRC, MAX_I2S_DIN);
  audioOutput.setVolume(21);

#if AUDIO_AUMENTAR_BUFFER
  if (psramFound()) {
    audioOutput.setBufsize(-1, AUDIO_BUFFER_PSRAM_BYTES);
    Serial.printf("[AUDIO] Buffer na PSRAM: %u bytes\n",
                  (unsigned)AUDIO_BUFFER_PSRAM_BYTES);
  } else {
    Serial.println("[AUDIO] PSRAM nao encontrada: usando buffer padrao.");
  }
#endif

#if AUDIO_EM_TAREFA
  if (!audioTarefaCriada) {
    BaseType_t resultado = xTaskCreatePinnedToCore(
      tarefaAudio,
      "audio",
      8192,
      NULL,
      3,
      NULL,
      1
    );

    audioTarefaCriada = (resultado == pdPASS);

    if (!audioTarefaCriada) {
      Serial.println("[AUDIO] ERRO: nao foi possivel criar tarefa de audio.");
      audioInicializado = false;
      return;
    }

    Serial.println("[AUDIO] loop do audio em tarefa propria.");
  }
#endif

  audioInicializado = true;
  Serial.println("Amplificador MAX98357A configurado.");
}

inline bool falar(const String& texto) {
  if (texto.length() == 0) {
    Serial.println("[AUDIO] Texto vazio.");
    return false;
  }

  if (!audioInicializado || mutexAudio == NULL) {
    Serial.println("[AUDIO] ERRO: audio nao inicializado.");
    return false;
  }

  Serial.print("[AUDIO] Solicitando voz: ");
  Serial.println(texto);

  if (xSemaphoreTake(mutexAudio, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("[AUDIO] Audio ocupado: fala cancelada.");
    return false;
  }

  // Limpa uma reproducao anterior que possa ter ficado ativa.
  // O loop continua sendo executado pela tarefa apos liberar o mutex.
  bool iniciou = audioOutput.connecttospeech(texto.c_str(), "pt");
  bool executando = audioOutput.isRunning();

  xSemaphoreGive(mutexAudio);

  Serial.print("[AUDIO] connecttospeech retornou: ");
  Serial.println(iniciou ? "true" : "false");
  Serial.print("[AUDIO] isRunning apos solicitacao: ");
  Serial.println(executando ? "true" : "false");

  return iniciou;
}

inline bool audioEmExecucao() {
  if (mutexAudio == NULL) {
    return false;
  }

#if AUDIO_EM_TAREFA
  // Se o mutex estiver ocupado, nao presumimos que o som terminou.
  // Isso evita a maquina de estados iniciar o microfone por engano.
  if (xSemaphoreTake(mutexAudio, pdMS_TO_TICKS(100)) != pdTRUE) {
    return true;
  }

  bool executando = audioOutput.isRunning();
  xSemaphoreGive(mutexAudio);
  return executando;
#else
  return audioOutput.isRunning();
#endif
}

inline void atualizarAudio() {
#if !AUDIO_EM_TAREFA
  if (mutexAudio != NULL &&
      xSemaphoreTake(mutexAudio, pdMS_TO_TICKS(100)) == pdTRUE) {
    audioOutput.loop();
    xSemaphoreGive(mutexAudio);
  }
#endif
}

#endif
