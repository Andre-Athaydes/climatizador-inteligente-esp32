#ifndef SENSORES_H
#define SENSORES_H


#include <Arduino.h>
#include "Config.h"
#include <Adafruit_MLX90614.h>
#include "DHTesp.h"


// Protótipos das funções
void setupSensores();
bool lerSensores(DadosSensores &dados);

#endif