#ifndef INTERFACE_H
#define INTERFACE_H

#include <Arduino.h>
#include "Config.h"
#include <Adafruit_SSD1306.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h> 
#include <qrcode.h>

// Instâncias compartilháveis (definidas em Interface.cpp)
extern IRrecv irRecv;
extern decode_results irResults;

// ── Estado global do sistema — lido pelo Main.cpp ─────────────
// true  = sistema ativo (AC + ventilador funcionando normalmente)
// false = sistema desligado pelo usuário via botão *
extern bool sistemaLigado;

// --- PROTÓTIPOS DAS FUNÇÕES ---

// Inicializa o display OLED e o receptor IR
void setupInterface();

// Processa sinais do controle remoto. 
// Retorna TRUE se o usuário alterou o Setpoint ou alternou o QR Code.
bool processarComandosUsuario(DadosSensores &dados);

// Atualiza as informações na tela (Temperaturas, Setpoint, Presença ou QR Code)
void atualizarDisplay(const DadosSensores &dados);

// Função interna para desenhar o QR Code no display
void mostrarQRCode();

#endif