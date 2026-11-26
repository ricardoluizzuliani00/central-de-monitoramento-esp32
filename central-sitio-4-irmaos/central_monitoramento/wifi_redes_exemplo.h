// ======================================================
// MODELO. Copie este arquivo com o nome  wifi_redes.h
// e preencha com as suas redes Wi-Fi (2,4 GHz). O wifi_redes.h de
// verdade NAO vai para o GitHub (esta no .gitignore).
//
// A central tenta as redes na ordem; a primeira que conectar vale.
// (A estrutura RedeWiFi esta definida em wifi_config.h.)
// ======================================================

#ifndef WIFI_REDES_H
#define WIFI_REDES_H

const RedeWiFi REDES_WIFI[] = {
  { "NOME_DA_REDE_1", "SENHA_DA_REDE_1" },
  { "NOME_DA_REDE_2", "SENHA_DA_REDE_2" }
};

#endif
