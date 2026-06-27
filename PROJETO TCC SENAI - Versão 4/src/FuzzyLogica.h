#ifndef FUZZY_LOGICA_H
#define FUZZY_LOGICA_H

#include <Fuzzy.h>
#include "Config.h"


// ============================================================
// STRUCT DE SAÍDA DO MOTOR FUZZY
// Desacopla o cálculo da ação — Main.cpp lê e aciona os atuadores
// ============================================================
struct ResultadoFuzzy {
    float  temperaturaAC;          // °C calculado pelo Fuzzy para o AC
    uint8_t estagioVentilador;     // 0=off 1=baixa 2=média 3=alta
    bool   valido;                 // false se houve erro de defuzzificação
    float erroFuzzy;               // setpoint - tempAmbiente (ºC)
    float tendencia;               // variação entre leituras (ºC)
};


// ============================================================
// PROTÓTIPOS
// ============================================================
void         setupFuzzy();
ResultadoFuzzy processarFuzzy(DadosSensores &dados);

#endif