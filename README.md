# 🌡️ Climatizador Inteligente ESP32

Sistema embarcado de controle inteligente de **ar-condicionado e ventilador de mesa** baseado em **Lógica Fuzzy**, desenvolvido com ESP32 como parte de Trabalho de Conclusão de Curso.

---

## 📌 Descrição

O Climatizador Inteligente automatiza o controle simultâneo de um ar-condicionado e um ventilador de mesa, tomando decisões em tempo real com base em:

- 🌡️ Temperatura ambiente (DHT22)
- 💧 Umidade relativa do ar (DHT22)
- 🌡️ Temperatura corporal do usuário (MLX90614)
- 👤 Presença humana (PIR HC-SR501)

As decisões são processadas por um **motor de inferência fuzzy Mamdani com 15 regras**, embarcado diretamente no ESP32. Os dados são transmitidos em tempo real para a plataforma **ThingsBoard** via **MQTT**, com suporte a monitoramento remoto e comandos RPC.

---

## 🏗️ Arquitetura do sistema

```
Sensores (DHT22, MLX90614, PIR)
         │
         ▼
   ESP32 Mestre ──► Motor Fuzzy (15 regras)
         │                    │
         ├──► OLED SSD1306    ├──► LED IR local (AC próximo)
         ├──► WiFi / MQTT     └──► ESP-NOW ──► ESP32 Escravo ──► LED IR remoto
         └──► Controle Remoto IR (NEC)
```

---

## 🛠️ Hardware utilizado

| Componente | Função |
|---|---|
| ESP32 DevKit V1 (30 pinos) | Microcontrolador principal |
| Shield expansor de pinos | Facilita a conexão dos periféricos |
| DHT22 | Temperatura e umidade ambiente |
| MLX90614 | Temperatura corporal (sensor IR) |
| PIR HC-SR501 | Detecção de presença |
| OLED SSD1306 0,96" I2C | Display de status |
| Módulo relé 4 canais (optoacoplador) | Controle do ventilador (3 velocidades) |
| Módulo receptor IR (VS1838B) | Leitura do controle remoto genérico |
| LED IR 940nm | Emissão de comandos para o AC |
| Fonte ajustável para protoboard | Alimentação dos periféricos 5V |

### Marcas de ar-condicionado compatíveis

| Marca | Protocolo IR | Biblioteca |
|---|---|---|
| **Philco** (modelos FM4, QF) | TCL112AC | `ir_Tcl.h` |
| **Elgin** | Gree (OEM) | `ir_Gree.h` |
| Whirlpool / Consul | Whirlpool | `ir_Whirlpool.h` |

> A marca é selecionada em tempo de compilação via `#define` em `src/Config.h`.

---

## 📁 Estrutura do repositório

```
climatizador-inteligente-esp32/
├── src/
│   ├── main.cpp            # Loop principal e orquestração
│   ├── Config.h            # Pinagem, constantes e seleção de marca
│   ├── Sensores.h/.cpp     # DHT22, MLX90614, PIR
│   ├── FuzzyLogica.h/.cpp  # Motor fuzzy (15 regras, Mamdani + centroide)
│   ├── Atuadores.h/.cpp    # Relés (ventilador) + IR (ar-condicionado)
│   ├── Interface.h/.cpp    # OLED, controle remoto IR, liga/desliga
│   ├── IoT.h/.cpp          # WiFi, MQTT, ThingsBoard, RPC
│   └── EspNow.h/.cpp       # Comunicação ESP-NOW com o nó escravo
├── escravo/
│   └── EspNowEscravo.ino   # Firmware do ESP32 escravo (LED IR remoto)
└── platformio.ini          # Configuração PlatformIO com todas as dependências
```

---

## ⚙️ Pré-requisitos

- [VSCode](https://code.visualstudio.com/) + extensão [PlatformIO](https://platformio.org/)
- Conta gratuita no [ThingsBoard Cloud](https://thingsboard.cloud) (limite de 5 dispositivos)

Todas as bibliotecas são instaladas automaticamente pelo PlatformIO ao compilar:

```
crankyoldgit/IRremoteESP8266 @ ^2.9.0
beegee-tokyo/DHT sensor library for ESPx @ ^1.19
adafruit/Adafruit MLX90614 Library @ ^2.1.5
adafruit/Adafruit SSD1306 @ ^2.5.9
adafruit/Adafruit GFX Library @ ^1.11.9
ricmoo/QRCode @ ^0.0.1
zerokol/eFLL @ ^1.4.1
knolleary/PubSubClient @ ^2.8
```

---

## 🚀 Como usar

### 1. Clone o repositório

```bash
git clone https://github.com/andremartins/climatizador-inteligente-esp32.git
cd climatizador-inteligente-esp32
```

### 2. Configure as credenciais

Abra `src/Config.h` e selecione a marca do seu ar-condicionado:

```cpp
// Descomente APENAS UMA linha:
#define MARCA_PHILCO
// #define MARCA_ELGIN
// #define MARCA_WHIRLPOOL
```

Abra `src/IoT.cpp` e preencha suas credenciais:

```cpp
static const char* WIFI_SSID     = "SUA_REDE_WIFI";
static const char* WIFI_PASSWORD = "SUA_SENHA";
static const char* TB_TOKEN      = "SEU_TOKEN_THINGSBOARD";
```

### 3. Compile e envie

```bash
pio run --target upload
pio device monitor
```

### 4. (Opcional) ESP32 escravo — LED IR remoto via ESP-NOW

Se o LED IR estiver num segundo ESP32 próximo ao AC:

1. Carregue `escravo/EspNowEscravo.ino` no segundo ESP32
2. Anote o MAC Address impresso no Monitor Serial
3. Cole o MAC em `src/EspNow.cpp` na variável `MAC_ESCRAVO`
4. Em `src/Atuadores.cpp`, defina `USAR_LED_IR_REMOTO = true`
5. Recompile e carregue o firmware do mestre

---

## 🗺️ Mapeamento dos botões do controle remoto

| Botão | Código NEC | Ação |
|---|---|---|
| Seta ↑ | `0xFF18E7` | Aumenta setpoint +0,5°C |
| Seta ↓ | `0xFF4AB5` | Diminui setpoint -0,5°C |
| `*` (Asterisco) | `0xFF6897` | Liga / Desliga o sistema |
| `#` | `0xFF38C7` | Alterna OLED / QR Code ThingsBoard |

---

## 📡 Telemetria enviada ao ThingsBoard

| Chave | Descrição |
|---|---|
| `tempAmbiente` | Temperatura do DHT22 (°C) |
| `umidade` | Umidade relativa (%) |
| `tempCorporal` | Temperatura corporal MLX90614 (°C) |
| `presenca` | Ocupação detectada (true/false) |
| `setpoint` | Temperatura desejada (°C) |
| `erroFuzzy` | setpoint − tempAmbiente (°C) |
| `tendencia` | Variação entre leituras (°C) |
| `acLigado` | Estado do ar-condicionado |
| `tempAC` | Temperatura configurada no AC (°C) |
| `velVentilador` | Estágio do ventilador (0–3) |
| `rssi` | Qualidade do sinal WiFi (dBm) |

Telemetria publicada a cada **10 segundos** no tópico `v1/devices/me/telemetry`.

---

## 🔌 Pinagem principal (ESP32 DevKit V1)

| GPIO | Componente |
|---|---|
| 21 / 22 | I2C SDA / SCL (OLED + MLX90614) |
| 23 | DHT22 DATA |
| 34 | PIR HC-SR501 OUT |
| 16 | LED IR emissor |
| 17 | Receptor IR (VS1838B) |
| 32 / 33 / 25 | Relés V1 / V2 / V3 (ventilador) |

---

## 👤 Autor

**André Athaydes Martins**  
Trabalho de Conclusão de Curso — Engenharia / Tecnologia  
2025

---

## 🔗 Repositório relacionado

Simulação do controlador fuzzy em Python (Google Colab):  
👉 [climatizador-fuzzy-simulacao](https://github.com/andremartins/climatizador-fuzzy-simulacao)
EOF
echo "README firmware gerado"
Saída
