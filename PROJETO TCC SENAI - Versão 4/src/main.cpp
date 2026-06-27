#include <Arduino.h>
#include <Wire.h>

#include "Config.h"
#include "Sensores.h"
#include "FuzzyLogica.h"
#include "Atuadores.h"
#include "Interface.h"
#include "IoT.h"  

// ============================================================
// VARIÁVEIS GLOBAIS
// ============================================================
static DadosSensores dadosAtuais;
static ResultadoFuzzy  ultimoResultado = {0.0f, 0, false, 0.0f, 0.0f};

static unsigned long ultimaLeituraSensores = 0;
static unsigned long tsUltimaPresenca  = 0; // Timestamp da última detecção
static unsigned long tsUltimoFuzzy     = 0;

// Velocidade desejada pelo Fuzzy — persistida entre chamadas de controlarVentilador()
static VelocidadeVentilador velDesejada = DESLIGADO;

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Sistema Inteligente de Climatização ===");

    // I2C centralizado — antes de qualquer módulo que use o barramento
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000); // 400kHz (Fast Mode) — suportado por OLED e MLX90614
   
    // 1. Inicializa o Setpoint padrão (Fundamental para o cálculo do erro Fuzzy)
    //dadosAtuais.setpoint = 24.0;
    dadosAtuais.setpoint = SETPOINT_DEF; 

    // 2. Inicialização dos módulos
    setupSensores();    // Inicia DHT22, MLX90614 e PIR
    setupAtuadores();   // Inicia Relés e IR Whirlpool/Consul
    setupFuzzy();       // Inicia Motor Fuzzy com 15 regras
    setupInterface();   // Inicia o OLED e o Receptor IR

    // Compartilha ponteiro com o módulo IoT para que callbacks RPC
    // (ex: setSetpoint remoto) alterem diretamente dadosAtuais
    dadosGlobais = &dadosAtuais;
    setupIoT();        // Wi-Fi + MQTT ThingsBoard

    Serial.println("[System] Setup concluído. Iniciando monitoramento...");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
    unsigned long agora = millis();


    // ---1. CONECTIVIDADE IoT ───────────────────────────────────────────────
    // mqtt.loop() interno processa RPC recebidos do ThingsBoard — pode
    // alterar dadosAtuais.setpoint via callback; por isso vem primeiro.
    loopIoT(dadosAtuais, ultimoResultado);


    // --- 2. INTERAÇÃO COM O USUÁRIO - Controle Remoto IR ---
    // A função retorna 'true' se o usuário mudar o Setpoint ou alternar o QR Code
    if (processarComandosUsuario(dadosAtuais)) {
        
        atualizarDisplay(dadosAtuais); 

        // Reage ao novo setpoint só se o sistema estiver ativo
        if (sistemaLigado && dadosAtuais.presenca) {
            ResultadoFuzzy res = processarFuzzy(dadosAtuais);
            if (res.valido) {
                ultimoResultado = res;
                enviarComandoAC(res.temperaturaAC, true);
                velDesejada = (VelocidadeVentilador)res.estagioVentilador;
            }
        }
    }        

    // ── 3. TRANSIÇÃO GRADUAL DO VENTILADOR (não-bloqueante) ───────────────
    // Chamado a cada ciclo — avança UM estágio se o intervalo passou
    if (sistemaLigado) controlarVentilador(velDesejada);

    // ── 4. CICLO DE LEITURA DOS SENSORES ──────────────────────────────────

    // Só executa leitura e controle se o usuário ligou o sistema via botão *
    if (!sistemaLigado) return;

    if (agora - ultimaLeituraSensores >= INTERVALO_LEITURA) {
        ultimaLeituraSensores = agora;

        // A. Tenta ler os dados físicos dos sensores
        if (lerSensores(dadosAtuais)) {
            
            // B. // Atualiza timestamp de presença se alguém foi detectado
            if (dadosAtuais.presenca) {
                tsUltimaPresenca = agora;
            } 
            
            // Se o PIR não detectar ninguém por 15 minutos, força modo ECO.
            // Evita que uma detecção antiga mantenha o sistema ligado indefinidamente.
            bool presencaEfetiva = dadosAtuais.presenca || ((agora - tsUltimaPresenca) < TIMEOUT_PRESENCA);

            if(presencaEfetiva){
              // Executa o Fuzzy no intervalo configurado
                if(agora - tsUltimoFuzzy >= INTERVALO_FUZZY){
                    tsUltimoFuzzy = agora;
                    
                    ResultadoFuzzy res = processarFuzzy(dadosAtuais);
                    if (res.valido){
                        ultimoResultado = res;
                        enviarComandoAC(res.temperaturaAC, true);
                        velDesejada = (VelocidadeVentilador)res.estagioVentilador;
                    }
                    else{
                        Serial.println("[AVISO] Resultado Fuzzy inválido — mantendo estado atual.");
                    }
                }
            }
            else {
                // Modo ECO: ninguém detectado por TIMEOUT_PRESENCA
                Serial.println("[ECO] Ausência prolongada. Desligando sistema...");
                enviarComandoAC(SETPOINT_DEF, false); // false = Comando de desligar
                controlarVentilador(DESLIGADO);
            }

            // C. Atualiza o OLED com a temperatura atual do ambiente e do corpo
            atualizarDisplay(dadosAtuais); 

        } else {
             Serial.println("[ERRO] Falha crítica nos sensores — mantendo estado anterior.");
        }
    }
}