#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


// ============================================================
// SELEÇÃO DE HARDWARE (DESCOMENTE APENAS UM)
// ============================================================
//#define MARCA_WHIRLPOOL  // Use para Consul/Whirlpool
//#define MARCA_ELGIN     // Use para Elgin
#define MARCA_PHILCO        // Philco (protocolo TCL112AC)

// ============================================================
// PINAGEM DO ESP32 (Mantida para ambos os modelos)
// ============================================================

// --- Comunicação I2C (OLED e MLX90614) ---
#define I2C_SDA 21
#define I2C_SCL 22

// --- Sensores ---
#define PIN_PIR 34
#define PIN_DHT 23

// --- Atuadores (Relés do Ventilador Externo) ---
#define RELAY_FAN_V1 25
#define RELAY_FAN_V2 26
#define RELAY_FAN_V3 27

// --- Infravermelho ---
#define PIN_IR_RECV 17
#define PIN_IR_SEND 16

// ============================================================
// PARÂMETROS DE CONTROLE E TEMPO (ms)
// ============================================================
#define INTERVALO_LEITURA 2000    // Ciclo de leitura dos sensores
#define INTERVALO_FUZZY 5000      // Ciclo de decisão da IA
#define TIMEOUT_PRESENCA 900000   // 15 minutos (900.000ms)

// Limites e padrão do setpoint — definidos como constexpr float
// para eliminar ambiguidade de cast nos pontos de uso (min/max/constrain)
constexpr float SETPOINT_MIN = 16.0f;
constexpr float SETPOINT_MAX = 30.0f;
constexpr float SETPOINT_DEF = 24.0f;



// ============================================================
// ESTRUTURA DE DADOS COMPARTILHADA ENTRE MÓDULOS
// ============================================================
struct DadosSensores {
    float tempAmbiente = 0.0f;   // Sensor DHT22
    float umidade = 50.0f;        // Sensor DHT22
    float tempCorporal = 36.0f;   // Sensor MLX90614
    bool presenca = false;        // Lógica PIR + Térmica
    float setpoint = SETPOINT_DEF;       // Definido via Controle Remoto / Interface
};

#endif