#include "IoT.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================
//  IoT.cpp — Wi-Fi + ThingsBoard MQTT
//
//  CONFIGURAÇÕES QUE VOCÊ DEVE PREENCHER:
//    WIFI_SSID      → nome da sua rede Wi-Fi
//    WIFI_PASSWORD  → senha da sua rede Wi-Fi
//    TB_TOKEN       → Access Token do dispositivo no ThingsBoard
//    TB_HOST        → endereço do seu ThingsBoard
//                     (ex: "thingsboard.cloud" ou IP local "192.168.1.x")
//
//
//  CHAVES DE TELEMETRIA ENVIADAS:
//    tempAmbiente   → temperatura do DHT22 (°C)
//    umidade        → umidade relativa do DHT22 (%)
//    tempCorporal   → temperatura corporal/IR do MLX90614 (°C)
//    presenca       → booleano de ocupação (true/false)
//    setpoint       → setpoint configurado pelo usuário (°C)
//    acLigado       → estado do ar-condicionado (true/false)
//    tempAC         → temperatura que o AC está operando (°C)
//    velVentilador  → estágio do ventilador (0=off, 1, 2, 3)
//    rssi           → força do sinal Wi-Fi (dBm) para diagnóstico
//
//  TÓPICOS MQTT USADOS:
//    Publicação de telemetria : v1/devices/me/telemetry
//    Publicação de atributos  : v1/devices/me/attributes
//    Subscrição RPC           : v1/devices/me/rpc/request/+
//    Resposta RPC             : v1/devices/me/rpc/response/{id}
// ============================================================

// ─────────────────────────Credenciais ──────────────────────────────────────────
static const char* WIFI_SSID     = "André's Galaxy A23"; //Nome da rede wi-fi
static const char* WIFI_PASSWORD = "boah2193"; // Senha da rede wi-fi
static const char* TB_HOST       = "thingsboard.cloud";  // Servidor que irá receber os dados
static const char* TB_TOKEN      = "jJmQrjuP2JIDBK0vAYYu";   // Token do dispositivo
static const uint16_t TB_PORT    = 1883;                  // MQTT sem TLS

// ── Tópicos ThingsBoard ───────────────────────────────────────
static const char* TOPIC_TELEMETRY  = "v1/devices/me/telemetry";
static const char* TOPIC_ATTRIBUTES = "v1/devices/me/attributes";
static const char* TOPIC_RPC_SUB    = "v1/devices/me/rpc/request/+";


// ── Intervalos de tempo ───────────────────────────────────────
static const unsigned long INTERVALO_TELEMETRIA_MS = 10000UL; // 10 segundos
static const unsigned long INTERVALO_RECONEXAO_MS  =  5000UL; // tenta reconectar a cada 5s
static const unsigned long WIFI_TIMEOUT_MS         = 15000UL; // timeout da conexão Wi-Fi

// ── Clientes de rede ─────────────────────────────────────────
static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

// ── Timestamps internos ───────────────────────────────────────
static unsigned long tsUltimaTelemetria = 0;
static unsigned long tsUltimaReconexao  = 0;

// ── Ponteiro para a struct global do Main.cpp ─────────────────
DadosSensores *dadosGlobais = nullptr;

// ── Estado dos Atuadores (extern de Atuadores.cpp) ───────────────────
// Declaramos extern para não criar dependência de header circular
extern bool  acLigadoFisicamente;
extern float ultimaTempAC;
extern uint8_t velocidadeAtualInt; // ver nota em Atuadores.cpp

// ── Último ResultadoFuzzy recebido ────────────────────────────
// Armazenado para publicarTelemetria() acessar sem precisar de parâmetro extra — loopIoT() atualiza antes de publicar.
static ResultadoFuzzy ultimoResultado = {0.0f, 0, false};

// =============================================================
//  FUNÇÕES INTERNAS
// =============================================================

// ── Conecta ao Wi-Fi ─────────────────────────────────────────
static bool conectarWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.printf("[WiFi] Conectando a '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - inicio > WIFI_TIMEOUT_MS) {
            Serial.println("\n[WiFi] TIMEOUT — verifique SSID/senha.");
            return false;
        }
        delay(500);
        Serial.print('.');
    }

    Serial.printf("\n[WiFi] Conectado! IP: %s | RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
    return true;
}

// ── Callback MQTT — processa mensagens recebidas do ThingsBoard ──
// Suporta RPC Server-Side:
//   setSetpoint   → altera o setpoint remotamente
//   getStatus     → retorna estado atual como resposta RPC
static void callbackMQTT(char* topic, byte* payload, unsigned int length) {
    // Copia payload para string terminada em null
    char msg[256];
    length = min(length, (unsigned int)(sizeof(msg) - 1));
    memcpy(msg, payload, length);
    msg[length] = '\0';

    Serial.printf("[MQTT] RPC recebido → Tópico: %s | Payload: %s\n", topic, msg);

    // Extrai o requestId do tópico "v1/devices/me/rpc/request/{id}"
    String topicStr(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    String requestId = topicStr.substring(lastSlash + 1);

    // ── Parse manual de JSON mínimo (sem biblioteca extra) ───────
    // Formato esperado do ThingsBoard: {"method":"setSetpoint","params":{"value":22.5}}
    String payload_str(msg);

    char responseTopic[80];
    snprintf(responseTopic, sizeof(responseTopic),
             "v1/devices/me/rpc/response/%s", requestId.c_str());

    // Comando: setSetpoint
    if (payload_str.indexOf("setSetpoint") >= 0) {
        int vIdx = payload_str.indexOf("\"value\":");
        if (vIdx >= 0 && dadosGlobais != nullptr) {
            float novoSetpoint = payload_str.substring(vIdx + 8).toFloat();
            novoSetpoint = constrain(novoSetpoint, SETPOINT_MIN, SETPOINT_MAX);
            dadosGlobais->setpoint = novoSetpoint;

            char resp[64];
            snprintf(resp, sizeof(resp), "{\"setpoint\":%.1f}", novoSetpoint);
            mqtt.publish(responseTopic, resp);

            Serial.printf("[RPC] Setpoint remoto → %.1f C\n", novoSetpoint);
        }
        return;
    }

    // Comando: getStatus — retorna snapshot do estado atual
    if (payload_str.indexOf("getStatus") >= 0 && dadosGlobais != nullptr) {
        char resp[256];
        snprintf(resp, sizeof(resp),
                 "{\"tempAmbiente\":%.1f,\"umidade\":%.1f,"
                 "\"presenca\":%s,\"setpoint\":%.1f,"
                 "\"acLigado\":%s}",
                 dadosGlobais->tempAmbiente,
                 dadosGlobais->umidade,
                 dadosGlobais->presenca ? "true" : "false",
                 dadosGlobais->setpoint,
                 acLigadoFisicamente    ? "true" : "false");
        mqtt.publish(responseTopic, resp);
        Serial.println("[RPC] getStatus respondido.");
        return;
    }

    // Comando desconhecido — responde com erro
    mqtt.publish(responseTopic, "{\"error\":\"comando_desconhecido\"}");
}

// ── Conecta ao broker MQTT do ThingsBoard ────────────────────
static bool conectarMQTT() {
    if (mqtt.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    Serial.printf("[MQTT] Conectando a %s:%d com token %s...\n",
                  TB_HOST, TB_PORT, TB_TOKEN);

    // ThingsBoard usa o Access Token como username; password e clientId são vazios/livres
    String clientId = "ESP32_Climatizador_" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqtt.connect(clientId.c_str(), TB_TOKEN, "")) {
        Serial.println("[MQTT] Conectado ao ThingsBoard!");

        // Subscreve ao tópico de RPC para receber comandos remotos
        mqtt.subscribe(TOPIC_RPC_SUB);
        Serial.printf("[MQTT] Subscrito em: %s\n", TOPIC_RPC_SUB);

        // Publica atributos estáticos do dispositivo logo após conectar
        char atributos[256];
        snprintf(atributos, sizeof(atributos),
                 "{\"firmware\":\"v2.0\",\"marca_ac\":\"%s\","
                 "\"ssid\":\"%s\",\"ip\":\"%s\"}",
#ifdef MARCA_PHILCO
                 "Philco_TCL112AC",
#elif defined(MARCA_ELGIN)
                 "Elgin_Gree",
#elif defined(MARCA_WHIRLPOOL)
                 "Whirlpool_Consul",
#else
                 "Desconhecida",
#endif
                 WIFI_SSID,
                 WiFi.localIP().toString().c_str());

        mqtt.publish(TOPIC_ATTRIBUTES, atributos, true); // retain=true
        return true;
    }

    Serial.printf("[MQTT] Falha. Estado: %d — tentando em %lus\n",
                  mqtt.state(), INTERVALO_RECONEXAO_MS / 1000);
    return false;
}

// ── Publica telemetria no ThingsBoard ────────────────────────
static void publicarTelemetria(const DadosSensores &dados) {
    if (!mqtt.connected()) return;



    // erroFuzzy: calculado aqui (setpoint - tempAmbiente) para garantir que o valor publicado é 
    // sempre consistente com os dados do ciclo atual, independente de quando o Fuzzy foi 
    // processado pela última vez.
    float erroFuzzy = dados.setpoint - dados.tempAmbiente;

    // tendencia: vem do ResultadoFuzzy — é a variação entre leituras
    // calculada dentro de FuzzyLogica.cpp, reutilizamos sem recalcular.
    float tendencia = ultimoResultado.valido ? ultimoResultado.tendencia : 0.0f;

    // Monta JSON com todas as chaves de telemetria do sistema
    // Inclui RSSI para monitorar qualidade da conexão remotamente
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{"
             "\"tempAmbiente\":%.2f,"
             "\"umidade\":%.2f,"
             "\"tempCorporal\":%.2f,"
             "\"presenca\":%s,"
             "\"setpoint\":%.1f,"
             "\"erroFuzzy\":%.2f,"
             "\"tendencia\":%.2f,"
             "\"acLigado\":%s,"
             "\"tempAC\":%.1f,"
             "\"velVentilador\":%d,"
             "\"rssi\":%d"
             "}",
             dados.tempAmbiente,
             dados.umidade,
             dados.tempCorporal,
             dados.presenca ? "true" : "false",
             dados.setpoint,
             erroFuzzy,
             tendencia,
             acLigadoFisicamente ? "true" : "false",
             ultimaTempAC,
             (int)velocidadeAtualInt,
             (int)WiFi.RSSI());

    bool ok = mqtt.publish(TOPIC_TELEMETRY, payload);
    Serial.printf("[MQTT] Telemetria %s → %s\n",
                  ok ? "enviada" : "FALHOU",
                  payload);
}

// =============================================================
//  FUNÇÕES PÚBLICAS
// =============================================================

void setupIoT() {
    Serial.println("[IoT] Inicializando conectividade...");

    mqtt.setServer(TB_HOST, TB_PORT);
    mqtt.setCallback(callbackMQTT);

    // Buffer MQTT: 512 bytes cobre o payload de telemetria confortavelmente
    mqtt.setBufferSize(512);

    // Tenta conectar Wi-Fi no setup; se falhar, loopIoT tenta novamente
    if (conectarWiFi()) {
        conectarMQTT();
    }

    Serial.println("[IoT] Módulo IoT online.");
}

void loopIoT(const DadosSensores &dados, const ResultadoFuzzy &resultado) {
    unsigned long agora = millis();

    // Armazena o resultado para publicarTelemetria() acessar
    if (resultado.valido) {
        ultimoResultado = resultado;
    }

    // ── 1. Garante conexão Wi-Fi ────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        if (agora - tsUltimaReconexao >= INTERVALO_RECONEXAO_MS) {
            tsUltimaReconexao = agora;
            Serial.println("[WiFi] Desconectado — tentando reconectar...");
            conectarWiFi();
        }
        return; // Sem Wi-Fi, não há nada mais a fazer neste ciclo
    }

    // ── 2. Garante conexão MQTT ─────────────────────────────────────────
    if (!mqtt.connected()) {
        if (agora - tsUltimaReconexao >= INTERVALO_RECONEXAO_MS) {
            tsUltimaReconexao = agora;
            conectarMQTT();
        }
        return; // Sem MQTT, aguarda próximo ciclo
    }

    // ── 3. Processa mensagens recebidas (RPC, atributos) ────────────────
    mqtt.loop();

    // ── 4. Publica telemetria no intervalo configurado ──────────────────
    if (agora - tsUltimaTelemetria >= INTERVALO_TELEMETRIA_MS) {
        tsUltimaTelemetria = agora;
        publicarTelemetria(dados);
    }
}

bool mqttConectado() {
    return mqtt.connected();
}