#ifndef IOT_H
#define IOT_H
 
#include <Arduino.h>
#include "Config.h"
#include "FuzzyLogica.h" //Necessário para obter dados do erro e tendência
 
// ============================================================
//  IoT.h — Módulo de Conectividade Wi-Fi + ThingsBoard 
//  Biblioteca necessária: PubSubClient (Nick O'Leary)
//   Arduino IDE → Manage Libraries → "PubSubClient"
// ============================================================
 
// ── Protótipos públicos ──────────────────────────────────────
void setupIoT();                              // Conecta Wi-Fi e MQTT no setup()
void loopIoT(const DadosSensores &dados, const ResultadoFuzzy &resultado);     // Mantém conexão e envia telemetria
bool mqttConectado();                         // Consulta estado da conexão MQTT
 
// ── Callback para RPC e atributos vindos do ThingsBoard ──────
// Implementada em IoT.cpp; o Main.cpp pode usar o ponteiro
// 'dadosGlobais' para receber comandos remotos (setpoint, etc.)
extern DadosSensores *dadosGlobais;           // Ponteiro para a struct do Main.cpp
 
#endif // IOT_H