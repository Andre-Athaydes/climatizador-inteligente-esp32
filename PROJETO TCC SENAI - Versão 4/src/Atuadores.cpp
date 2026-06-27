#include "Config.h"
#include "Atuadores.h"


// ============================================================
// INCLUDES CONDICIONAIS — seleção via Config.h
// ============================================================
// O compilador verifica Config.h para saber qual marca está ativa
#ifdef MARCA_WHIRLPOOL
    #include <ir_Whirlpool.h>
    static IRWhirlpoolAc ac_control(PIN_IR_SEND);
#endif

#ifdef MARCA_ELGIN
    #include <ir_Gree.h>
    static IRGreeAC ac_control(PIN_IR_SEND);
#endif

#ifdef MARCA_PHILCO
    // Philco usa protocolo TCL112AC (OEM TCL/Funiki — padrão Brasil)
    // Compatível com: PH9000FM4, PH12000FM4, PH18000FM4, PH24000FM6, série QF
    #include <ir_Tcl.h>
    static IRTcl112Ac ac_control(PIN_IR_SEND);
#endif


// ============================================================
// VARIÁVEIS DE ESTADO DO AC
// ============================================================
bool acLigadoFisicamente = false;
float ultimaTempAC        = -1.0f; // -1 força envio na primeira chamada
VelocidadeVentilador velocidadeAtual = DESLIGADO;
const int TEMPO_TRANSICAO = 2000;
// Espelho inteiro de velocidadeAtual — usado pelo IoT.cpp para telemetria
// sem criar dependência de enum entre módulos desacoplados
uint8_t velocidadeAtualInt  = 0;

// ============================================================
// BLOCO 1: LÓGICA DO AR-CONDICIONADO (INFRAVERMELHO)
// ============================================================

void enviarComandoAC(float tempFuzzy, bool queroLigar) {
    
    // 1. GERENCIAMENTO DE LIGA/DESLIGA
    if (queroLigar != acLigadoFisicamente) {
        
        #ifdef MARCA_WHIRLPOOL
            // Whirlpool/Consul usa o bit de Toggle
            ac_control.setPowerToggle(true);
            ac_control.send();
            ac_control.setPowerToggle(false); 
        #endif

        #ifdef MARCA_ELGIN
            // Elgin possui comandos dedicados de ON/OFF
            if (queroLigar) ac_control.on();
            else ac_control.off();
            ac_control.send();
        #endif

        #ifdef MARCA_PHILCO
            if (queroLigar) ac_control.on();
            else            ac_control.off();
            ac_control.send();
        #endif        
        
        acLigadoFisicamente = queroLigar;
        Serial.printf("[AC] Energia: %s\n", queroLigar ? "LIGADO" : "DESLIGADO");

        if (!queroLigar) return;
        delay(500); // Aguarda estabilização do hardware do AC
    }
    
    if (!acLigadoFisicamente) return;
    
    // 2. GERENCIAMENTO DE TEMPERATURA

    // Ajuste de limites conforme a marca selecionada
    // Clamp de segurança antes do cast para int
    #ifdef MARCA_WHIRLPOOL
        tempFuzzy = constrain(tempFuzzy, 18.0f, 30.0f);
    #endif
    #ifdef MARCA_ELGIN
        tempFuzzy = constrain(tempFuzzy, 16.0f, 30.0f);
    #endif

    #ifdef MARCA_PHILCO
        // TCL112AC: range 16–31°C conforme especificação do protocolo
        tempFuzzy = constrain(tempFuzzy, 16.0f, 31.0f);
    #endif
        
    int tempAlvo = (int)round(tempFuzzy);
    
    // Só envia sinal IR se a temperatura realmente mudou — evita flood de IR
    if (tempAlvo != (int)ultimaTempAC) {
        
        #ifdef MARCA_WHIRLPOOL
            ac_control.setMode(kWhirlpoolAcCool);
            ac_control.setFan(kWhirlpoolAcFanAuto);
            ac_control.setTemp(tempAlvo);
            ac_control.setPowerToggle(false); 
            Serial.printf("[AC Whirlpool/Consul] Ajustado para %d C\n", tempAlvo);
        #endif

        #ifdef MARCA_ELGIN
            ac_control.setMode(kGreeCool);
            ac_control.setFan(kGreeFanAuto);
            ac_control.setTemp(tempAlvo);
            Serial.printf("[AC Elgin/Gree] Ajustado para %d C\n", tempAlvo);
        #endif
        #ifdef MARCA_PHILCO
        // TCL112AC: modo Frio, ventilador automático, swing vertical ligado
            ac_control.setMode(kTcl112AcCool);
            ac_control.setFan(kTcl112AcFanAuto);
            ac_control.setTemp(tempAlvo);
            //ac_control.setSwingVertical(kTcl112AcSwingVOn);  // Swing padrão para melhor distribuição
            ac_control.setTurbo(false);
            ac_control.setEcono(false);
            Serial.printf("[AC Philco/TCL112AC] Ajustado para %d C\n", tempAlvo);
        #endif
        
        ac_control.send(); 
        ultimaTempAC = (float)tempAlvo;
    }
}

// ============================================================
// BLOCO 2: LÓGICA DO VENTILADOR (RELÉS)
// ============================================================

void desligarRelesImediato() {
    // HIGH = relé desarmado em módulos Active-Low com optoacoplador
    digitalWrite(RELAY_FAN_V1, HIGH); 
    digitalWrite(RELAY_FAN_V2, HIGH);
    digitalWrite(RELAY_FAN_V3, HIGH);
}
// Define fisicamente qual relé deve estar ativo
static void setFisicoVentilador(VelocidadeVentilador vel) {
    desligarRelesImediato();
    delay(150); // Anti-arco: garante que o contato anterior abriu antes de fechar outro

    switch(vel) {
        case BAIXA:  digitalWrite(RELAY_FAN_V1, LOW); break;
        case MEDIA:  digitalWrite(RELAY_FAN_V2, LOW); break;
        case ALTA:   digitalWrite(RELAY_FAN_V3, LOW); break;
        case DESLIGADO: break;
    }
    velocidadeAtual = vel;
    velocidadeAtualInt = (uint8_t)vel; // Mantém espelho para telemetria
    Serial.printf("[Vent] Estágio físico: %d\n", (int)vel);
}

void controlarVentilador(VelocidadeVentilador velDesejada) {
    if (velDesejada == velocidadeAtual) return;


    static unsigned long tsUltimaTransicao = 0;
    const unsigned long  INTERVALO_TRANSICAO = 1500; // ms entre estágios

    unsigned long agora = millis();
    if (agora - tsUltimaTransicao < INTERVALO_TRANSICAO) return; // Ainda aguardando

    // Avança UM estágio em direção ao alvo

    if ((int)velocidadeAtual < (int)velDesejada) {
        setFisicoVentilador((VelocidadeVentilador)((int)velocidadeAtual + 1));
    } else {
        setFisicoVentilador((VelocidadeVentilador)((int)velocidadeAtual - 1));
    }
 
    tsUltimaTransicao = agora;

}

// ============================================================
// BLOCO 3: SETUP
// ============================================================

void setupAtuadores() {
    pinMode(RELAY_FAN_V1, OUTPUT);
    pinMode(RELAY_FAN_V2, OUTPUT);
    pinMode(RELAY_FAN_V3, OUTPUT);
    desligarRelesImediato(); // Garante estado seguro no boot
    
    ac_control.begin();
    Serial.println("[System] Módulo de Atuadores Online.");
}