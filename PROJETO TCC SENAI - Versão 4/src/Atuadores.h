#ifndef ATUADORES_H
#define ATUADORES_H

#include <Arduino.h>
#include "Config.h" 

// ============================================================
// ENUM DE VELOCIDADE DO VENTILADOR
// ============================================================
enum VelocidadeVentilador: uint8_t {
    DESLIGADO = 0,
    BAIXA = 1,
    MEDIA = 2,
    ALTA = 3
};

// ============================================================
// PROTÓTIPOS
// ============================================================
void setupAtuadores();
void enviarComandoAC(float tempFuzzy, bool queroLigar);
void controlarVentilador(VelocidadeVentilador velDesejada);
void desligarRelesImediato();

// Acesso externo ao estado do AC (para exibição no OLED)
extern bool  acLigadoFisicamente;
extern float ultimaTempAC;
extern VelocidadeVentilador velocidadeAtual;
extern uint8_t               velocidadeAtualInt; // Espelho inteiro de velocidadeAtual para IoT

#endif