
#ifndef VOZ_H
#define VOZ_H

#include <Arduino.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "sensores.h"
#include "dados_sitio.h"

// Sensores das tres caixas do Sitio 4 Irmaos estao habilitados.
#ifndef CAIXA_2_INSTALADA
#define CAIXA_2_INSTALADA 1
#endif
#ifndef CAIXA_PRINCIPAL_INSTALADA
#define CAIXA_PRINCIPAL_INSTALADA 1
#endif

// =====================================================
// COMANDOS DE VOZ
// =====================================================

enum ComandoVoz {
  COMANDO_NENHUM,
  NIVEL_CAIXA_1,
  NIVEL_CAIXA_2,
  NIVEL_CAIXA_PRINCIPAL,
  STATUS_CAIXAS,
  TEMPERATURA,
  UMIDADE,
  TEMPERATURA_SITIO,
  UMIDADE_SITIO,
  PRESSAO_SITIO,
  CHUVA,
  HORA,
  AGRADECIMENTO,
  CONFIRMACAO,
  ENCERRAR
};

// =====================================================
// IDENTIFICAR COMANDO
// =====================================================

inline ComandoVoz identificarComando(String texto) {
  texto.toLowerCase();
  texto.trim();

  // Remover pontuação comum da transcrição
  texto.replace(".", "");
  texto.replace(",", "");
  texto.replace("!", "");
  texto.replace("?", "");
  texto.trim();

  // ---------------------------------------------------
  // Encerrar atendimento
  // ---------------------------------------------------

  if (texto.indexOf("encerrar") >= 0 ||
      texto.indexOf("parar de ouvir") >= 0 ||
      texto.indexOf("pode parar") >= 0 ||
      texto == "tchau" ||
      texto.indexOf("até logo") >= 0 ||
      texto.indexOf("ate logo") >= 0) {
    return ENCERRAR;
  }

  // ---------------------------------------------------
  // Agradecimentos e confirmações
  // ---------------------------------------------------

  if (texto.indexOf("obrigad") >= 0 ||
      texto == "valeu" ||
      texto.indexOf("valeu obrigado") >= 0 ||
      texto.indexOf("valeu, obrigado") >= 0 ||
      texto == "brigado" ||
      texto == "brigada") {
    return AGRADECIMENTO;
  }

  if (texto == "ok" ||
      texto == "certo" ||
      texto == "beleza" ||
      texto == "entendi" ||
      texto == "tá bom" ||
      texto == "ta bom") {
    return CONFIRMACAO;
  }

  // ---------------------------------------------------
  // Temperatura do sítio
  // Sempre verificar antes da temperatura local
  // ---------------------------------------------------

  if (texto.indexOf("temperatura") >= 0 &&
      (texto.indexOf("sítio") >= 0 ||
       texto.indexOf("sitio") >= 0 ||
       texto.indexOf("lá") >= 0 ||
       texto.indexOf("la ") >= 0 ||
       texto.indexOf("fazenda") >= 0 ||
       texto.indexOf("lá fora") >= 0 ||
       texto.indexOf("la fora") >= 0)) {
    return TEMPERATURA_SITIO;
  }

  if (texto.indexOf("clima do sítio") >= 0 ||
      texto.indexOf("clima do sitio") >= 0 ||
      texto.indexOf("clima no sítio") >= 0 ||
      texto.indexOf("clima no sitio") >= 0 ||
      texto.indexOf("tempo no sítio") >= 0 ||
      texto.indexOf("tempo no sitio") >= 0 ||
      texto.indexOf("temperatura do sítio") >= 0 ||
      texto.indexOf("temperatura do sitio") >= 0 ||
      texto.indexOf("temperatura no sítio") >= 0 ||
      texto.indexOf("temperatura no sitio") >= 0 ||
      texto.indexOf("temperatura lá") >= 0 ||
      texto.indexOf("temperatura la ") >= 0 ||
      texto.indexOf("como está o clima") >= 0 ||
      texto.indexOf("como esta o clima") >= 0) {
    return TEMPERATURA_SITIO;
  }

  // ---------------------------------------------------
  // Umidade do sítio
  // ---------------------------------------------------

  if (texto.indexOf("umidade") >= 0 &&
      (texto.indexOf("sítio") >= 0 ||
       texto.indexOf("sitio") >= 0 ||
       texto.indexOf("lá") >= 0 ||
       texto.indexOf("la ") >= 0 ||
       texto.indexOf("fazenda") >= 0 ||
       texto.indexOf("lá fora") >= 0 ||
       texto.indexOf("la fora") >= 0)) {
    return UMIDADE_SITIO;
  }

  if (texto.indexOf("umidade do sítio") >= 0 ||
      texto.indexOf("umidade do sitio") >= 0 ||
      texto.indexOf("umidade no sítio") >= 0 ||
      texto.indexOf("umidade no sitio") >= 0 ||
      texto.indexOf("umidade lá") >= 0 ||
      texto.indexOf("umidade la ") >= 0) {
    return UMIDADE_SITIO;
  }

  // ---------------------------------------------------
  // Pressão atmosférica do sítio
  // ---------------------------------------------------

  if (texto.indexOf("pressão") >= 0 ||
      texto.indexOf("pressao") >= 0) {
    return PRESSAO_SITIO;
  }

  // ---------------------------------------------------
  // Chuva no sítio
  // ---------------------------------------------------

  if (texto.indexOf("chuva") >= 0 ||
      texto.indexOf("chovendo") >= 0 ||
      texto.indexOf("chove") >= 0 ||
      texto.indexOf("sensor de chuva") >= 0 ||
      texto.indexOf("tempo seco") >= 0 ||
      texto.indexOf("está chovendo") >= 0 ||
      texto.indexOf("esta chovendo") >= 0) {
    return CHUVA;
  }

  // ---------------------------------------------------
  // Hora
  // ---------------------------------------------------

  if (texto.indexOf("que horas") >= 0 ||
      texto.indexOf("horas sao") >= 0 ||
      texto.indexOf("horas são") >= 0 ||
      texto.indexOf("me fala as horas") >= 0 ||
      texto.indexOf("horário") >= 0 ||
      texto.indexOf("horario") >= 0 ||
      texto.indexOf("que hora é") >= 0 ||
      texto.indexOf("que hora e") >= 0 ||
      texto == "hora" || texto == "as horas") {
    return HORA;
  }

  // ---------------------------------------------------
  // Status geral: somente Caixa 1 por enquanto
  // ---------------------------------------------------

  if (texto.indexOf("status das caixas") >= 0 ||
      texto.indexOf("estado das caixas") >= 0 ||
      texto.indexOf("nível das caixas") >= 0 ||
      texto.indexOf("nivel das caixas") >= 0 ||
      texto.indexOf("todas as caixas") >= 0 ||
      texto.indexOf("como estão as caixas") >= 0 ||
      texto.indexOf("como estao as caixas") >= 0) {
    return STATUS_CAIXAS;
  }

  // ---------------------------------------------------
  // Caixa principal: temporariamente desativada
  // ---------------------------------------------------

  if (texto.indexOf("caixa principal") >= 0 ||
      texto.indexOf("nível principal") >= 0 ||
      texto.indexOf("nivel principal") >= 0 ||
      texto.indexOf("como está a principal") >= 0 ||
      texto.indexOf("como esta a principal") >= 0) {
    return NIVEL_CAIXA_PRINCIPAL;
  }

  // ---------------------------------------------------
  // Caixa 2: temporariamente desativada
  // Verificar antes da Caixa 1
  // ---------------------------------------------------

  if (texto.indexOf("caixa 2") >= 0 ||
      texto.indexOf("caixa número 2") >= 0 ||
      texto.indexOf("caixa numero 2") >= 0 ||
      texto.indexOf("caixa número dois") >= 0 ||
      texto.indexOf("caixa numero dois") >= 0 ||
      texto.indexOf("caixa dois") >= 0 ||
      texto.indexOf("segunda caixa") >= 0 ||
      texto.indexOf("caixa número 2") >= 0) {
    return NIVEL_CAIXA_2;
  }

  // ---------------------------------------------------
  // Caixa 1
  // ---------------------------------------------------

  if (texto.indexOf("caixa 1") >= 0 ||
      texto.indexOf("caixa número 1") >= 0 ||
      texto.indexOf("caixa numero 1") >= 0 ||
      texto.indexOf("caixa número um") >= 0 ||
      texto.indexOf("caixa numero um") >= 0 ||
      texto.indexOf("caixa um") >= 0 ||
      texto.indexOf("primeira caixa") >= 0 ||
      texto.indexOf("capacidade da caixa") >= 0 ||
      texto.indexOf("nível da caixa") >= 0 ||
      texto.indexOf("nivel da caixa") >= 0 ||
      texto.indexOf("como está a caixa") >= 0 ||
      texto.indexOf("como esta a caixa") >= 0) {
    return NIVEL_CAIXA_1;
  }

  // ---------------------------------------------------
  // Temperatura local da Central
  // ---------------------------------------------------

  if (texto.indexOf("temperatura") >= 0 ||
      texto.indexOf("quantos graus") >= 0 ||
      texto.indexOf("temperatura local") >= 0 ||
      texto.indexOf("temperatura aqui") >= 0) {
    return TEMPERATURA;
  }

  // ---------------------------------------------------
  // Umidade local da Central
  // ---------------------------------------------------

  if (texto.indexOf("umidade") >= 0) {
    return UMIDADE;
  }

  return COMANDO_NENHUM;
}

// =====================================================
// GERAR RESPOSTA
// =====================================================

inline String gerarResposta(ComandoVoz comando) {
  char resposta[220];

  switch (comando) {

    case NIVEL_CAIXA_1:
      if (!caixa1.disponivel) {
        return "Não tenho dados disponíveis da caixa um.";
      }

      snprintf(resposta, sizeof(resposta),
               "A caixa um está com %.0f por cento de água, aproximadamente %.0f litros.",
               caixa1.nivel, caixa1.litros);
      return String(resposta);

    case NIVEL_CAIXA_2:
      if (!caixa2.disponivel) return "Não tenho dados disponíveis da caixa dois no momento.";
      snprintf(resposta, sizeof(resposta), "A caixa dois está com %.0f por cento de água, aproximadamente %.0f litros.", caixa2.nivel, caixa2.litros);
      return String(resposta);

    case NIVEL_CAIXA_PRINCIPAL:
      if (!caixa3.disponivel) return "Não tenho dados disponíveis da caixa principal no momento.";
      snprintf(resposta, sizeof(resposta), "A caixa principal está com %.0f por cento de água, aproximadamente %.0f litros.", caixa3.nivel, caixa3.litros);
      return String(resposta);

    case STATUS_CAIXAS: {
      String r = "";
      if (caixa1.disponivel) { snprintf(resposta, sizeof(resposta), "Caixa um: %.0f por cento, %.0f litros. ", caixa1.nivel, caixa1.litros); r += resposta; }
      else r += "Caixa um sem dados atuais. ";
      if (caixa2.disponivel) { snprintf(resposta, sizeof(resposta), "Caixa dois: %.0f por cento, %.0f litros. ", caixa2.nivel, caixa2.litros); r += resposta; }
      else r += "Caixa dois sem dados atuais. ";
      if (caixa3.disponivel) { snprintf(resposta, sizeof(resposta), "Caixa principal: %.0f por cento, %.0f litros.", caixa3.nivel, caixa3.litros); r += resposta; }
      else r += "Caixa principal sem dados atuais.";
      return r;
    }

    case TEMPERATURA:
      if (!ambienteDisponivel) {
        return "Não consigo ler a temperatura aqui na Central.";
      }

      snprintf(resposta, sizeof(resposta),
               "A temperatura aqui na Central é de %.1f graus Celsius.",
               temperatura);
      return String(resposta);

    case UMIDADE:
      if (!ambienteDisponivel) {
        return "Não consigo ler a umidade aqui na Central.";
      }

      snprintf(resposta, sizeof(resposta),
               "A umidade aqui na Central é de %.0f por cento.",
               umidade);
      return String(resposta);

    case TEMPERATURA_SITIO:
      if (!climaSitio.disponivel) {
        return "Não tenho dados disponíveis da temperatura do sítio.";
      }

      snprintf(resposta, sizeof(resposta),
               "A temperatura no sítio é de %.1f graus Celsius.",
               climaSitio.temperatura);
      return String(resposta);

    case UMIDADE_SITIO:
      if (!climaSitio.disponivel) {
        return "Os dados de umidade do sítio estão indisponíveis no momento.";
      }

      snprintf(resposta, sizeof(resposta),
               "A umidade no sítio está em %.0f por cento.",
               climaSitio.umidade);
      return String(resposta);

    case PRESSAO_SITIO:
      if (!climaSitio.disponivel) {
        return "Não tenho dados disponíveis da pressão atmosférica do sítio.";
      }

      snprintf(resposta, sizeof(resposta),
               "A pressão atmosférica no sítio é de %.1f hectopascais.",
               climaSitio.pressao);
      return String(resposta);

    case CHUVA:
      if (!chuvaSitio.disponivel ||
          strlen(chuvaSitio.status) == 0) {
        return "Não tenho dados disponíveis do sensor de chuva do sítio.";
      }

      if (strcmp(chuvaSitio.status, "seco") == 0) {
        return "O tempo no sítio está seco, sem chuva detectada.";
      }

      if (strcmp(chuvaSitio.status, "chuva") == 0 ||
          strcmp(chuvaSitio.status, "molhado") == 0) {
        return "O sensor do sítio indica presença de água ou chuva.";
      }

      snprintf(resposta, sizeof(resposta),
               "O status do sensor de chuva é: %s.",
               chuvaSitio.status);
      return String(resposta);

    case HORA: {
      time_t agora;
      time(&agora);

      if (agora < 1700000000) {
        return "Ainda não consegui sincronizar o relógio.";
      }

      struct tm horario;
      localtime_r(&agora, &horario);

      snprintf(resposta, sizeof(resposta),
               "Agora são %02d horas e %02d minutos.",
               horario.tm_hour, horario.tm_min);
      return String(resposta);
    }

    case AGRADECIMENTO:
      return "Por nada! Estou aqui se precisar.";

    case CONFIRMACAO:
      return "Certo. Pode perguntar.";

    case ENCERRAR:
      return "Tudo bem. Vou encerrar por aqui.";

    case COMANDO_NENHUM:
    default:
      return "Não entendi. Pode repetir, por favor?";
  }
}

#endif
