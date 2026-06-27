#include "Sensores.h"

static DHTesp dht;
static Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Guarda última leitura corporal válida para fallback
static float ultimaTempCorporalValida = 36.0f;


void setupSensores() {

    // Inicializa DHT22
    dht.setup(PIN_DHT, DHTesp::DHT22);
    
    // Inicializa MLX90614
    if (!mlx.begin()) {
        Serial.println("[ERRO] MLX90614 não detectado no barramento I2C!");
    } else {
        Serial.println("[OK] MLX90614 iniciado.");
    }
    
    pinMode(PIN_PIR, INPUT_PULLDOWN);
    Serial.println("[System] Módulo de Sensores online.");
}

bool lerSensores(DadosSensores &dados) {

    bool leituraOk = true;

    // 1. Leitura do DHT22
    TempAndHumidity tempHum = dht.getTempAndHumidity();
    
    // Validação crítica: se o DHT falhar, o Fuzzy não tem base de cálculo
    if (isnan(tempHum.temperature) || isnan(tempHum.humidity)) {
        Serial.println("[ERRO] DHT22: leitura inválida (nan). Verifique pull-up 10kΩ.");
        leituraOk = false;
    }
    else{
    // Sanidade adicional: temperaturas fora do range físico do sensor
        if (tempHum.temperature > -40.0f && tempHum.temperature < 80.0f) {
            
            dados.tempAmbiente = tempHum.temperature;
        }

        if (tempHum.humidity >= 0.0f && tempHum.humidity <= 100.0f) {
            dados.umidade = tempHum.humidity;
        }
    
    }

    // 2. Leitura do MLX90614
    float tCorpo = mlx.readObjectTempC();
    
    // Filtro básico: se a leitura for absurda (erro de barramento), ignoramos
    if (!isnan(tCorpo) && tCorpo > 20.0f && tCorpo < 45.0f) {
        dados.tempCorporal         = tCorpo;
        ultimaTempCorporalValida   = tCorpo; // Atualiza fallback
    } else {
        // MLX não é crítico — usa última leitura válida como fallback
        dados.tempCorporal = ultimaTempCorporalValida;
        Serial.printf("[AVISO] MLX90614: leitura suspeita (%.1f). Usando fallback: %.1f\n",
                      tCorpo, ultimaTempCorporalValida);
    }


    // 3. Lógica de Presença (PIR + Validação Térmica)
    bool pirAtivo = (digitalRead(PIN_PIR) == HIGH);
    
    // Presença confirmada por PIR OU por temperatura corporal elevada (>31°C).
    // A validação dupla reduz falsos negativos (pessoa parada não ativa o PIR).
    dados.presenca = pirAtivo || (dados.tempCorporal > 31.0f);

    return leituraOk;
}