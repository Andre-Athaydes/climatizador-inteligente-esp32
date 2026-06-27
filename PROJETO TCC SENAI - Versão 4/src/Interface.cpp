#include "Interface.h"
#include "Config.h"
#include "Atuadores.h" 

// Instâncias REAIS (aquelas que o 'extern' no .h aponta)
static Adafruit_SSD1306 display(128, 64, &Wire, -1);
IRrecv irRecv(PIN_IR_RECV);
decode_results irResults;

// ── Estado global do sistema ──────────────────────────────────
bool sistemaLigado = false; // Começa DESLIGADO — usuário precisa pressionar * para ligar
static bool exibindoQR = false;

// ============================================================
// SETUP
// ============================================================

// Inicializa o hardware da interface
void setupInterface() {
    // Inicializa OLED (0x3C é o endereço padrão da maioria dos displays)
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("Erro: OLED não encontrado!");
    }
    else{ 
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("CLIMATIZADOR INTELIGENTE");
    display.setCursor(22, 36);
    display.println("* para ligar");
    display.display();
    Serial.println("[OK] OLED SSD1306 detectado em 0x3C.");
    }

    irRecv.setUnknownThreshold(12); // Rejeita ruídos menores que 12 pulsos
    irRecv.enableIRIn(); // Liga o receptor infravermelho
    Serial.println("[System] Módulo de Interface online. Sistema aguardando botão *.");
}

// ============================================================
// PROCESSAMENTO DO CONTROLE REMOTO
// ============================================================
bool processarComandosUsuario(DadosSensores &dados) {

    if (!irRecv.decode(&irResults)) return false;

    // Ignora repetições (botão mantido pressionado)
        if (irResults.repeat) {
        irRecv.resume();
        return false;
    }

    uint32_t val = (uint32_t)irResults.value;
    bool houveMudanca = false;


    switch (val) {
        
        // Botão * (Asterisco) -> Primeira pressão: liga tudo (presença forçada para sair do ECO).
        // Segunda pressão: desliga AC e ventilador imediatamente e suspende
        // todo o processamento Fuzzy até nova pressão.
        case 0xFF6897:
            sistemaLigado = !sistemaLigado;
            houveMudanca  = true;

           if (sistemaLigado) {
                dados.presenca = true; // Garante saída imediata do modo ECO
                Serial.println("[UI] * pressionado → SISTEMA LIGADO");
            } else {
                // Desliga os atuadores imediatamente, sem esperar o ciclo Fuzzy
                enviarComandoAC(dados.setpoint, false);
                controlarVentilador(DESLIGADO);
                dados.presenca = false;
                Serial.println("[UI] * pressionado → SISTEMA DESLIGADO");
            }
            break;

        // Botão Seta ↑ : aumenta setpoint em 0.5°C
        case 0xFF18E7: // Use seus códigos reais capturados com o receptor
            
            if (sistemaLigado) {
                dados.setpoint = fmin(dados.setpoint + 0.5f, (float)SETPOINT_MAX);
                houveMudanca   = true;
                Serial.printf("[UI] Setpoint: %.1f C\n", dados.setpoint);
            } else {
                Serial.println("[UI] Sistema desligado — botão ↑ ignorado.");
            }
            break;        
        
        // Botão Seta ↓: diminui setpoint em 0.5°C
        case 0xFF4AB5:

            if (sistemaLigado) {
                dados.setpoint = fmax(dados.setpoint - 0.5f, (float)SETPOINT_MIN);
                houveMudanca   = true;
                Serial.printf("[UI] Setpoint: %.1f C\n", dados.setpoint);
            } else {
                Serial.println("[UI] Sistema desligado — botão ↓ ignorado.");
            }
            break;
            
        // Botão OK/Menu: alterna entre tela de dados e QR Code
        // Funciona independente do estado do sistema
        case 0xFF38C7:
            exibindoQR = !exibindoQR;
            houveMudanca = true;
            Serial.printf("[UI] QR Code: %s\n", exibindoQR ? "ON" : "OFF");
            break;
 
        default:
            // Exibe código desconhecido para facilitar o mapeamento de novos botões
            Serial.printf("[UI] Botão não mapeado: 0x%08X\n", val);
            break;
    }

    irRecv.resume(); // Prepara para receber o próximo sinal
    return houveMudanca; 
}


// ============================================================
// ATUALIZAÇÃO DO DISPLAY
// ============================================================
void atualizarDisplay(const DadosSensores &dados) {
    display.clearDisplay();
    
    if (exibindoQR) {
        mostrarQRCode();
        display.display();
        return;
    } 
    
    display.setTextSize(1);
    
    if (!sistemaLigado){

        // ── Tela de sistema desligado ──────────────────────────────────
        display.setCursor(22, 10);
        display.println("** DESLIGADO **");
        display.setCursor(10, 28);
        display.println("Pressione * para");
        display.setCursor(22, 40);
        display.println("ligar o sistema");
        display.setCursor(0, 56);
        display.printf("Set: %.1fC", dados.setpoint);
    } else {

        // ── Tela de dados normal ───────────────────────────────────────
        display.setCursor(0, 0);
        display.println("--- STATUS SISTEMA ---");
       
        display.setCursor(0, 12);
        display.printf("Amb: %.1f C\n", dados.tempAmbiente);
        display.printf("Umid: %.1f %%\n", dados.umidade);
        display.printf("Corpo   : %.1f C\n", dados.tempCorporal);
        display.printf("Setpoint: %.1f C\n", dados.setpoint);
        
        display.setCursor(0, 56);
        display.print(dados.presenca ? "[ OCUPADO ]" : "[  VAZIO  ]");
    }
    
    display.display();
}


// ============================================================
// QR CODE
// ============================================================

void mostrarQRCode() {
    QRCode qrcode;
    const uint8_t versao = 3;
    size_t bufSize = qrcode_getBufferSize(versao);
 
    uint8_t *qrcodeData = (uint8_t*)malloc(bufSize);
    if (qrcodeData == nullptr) {
        Serial.println("[ERRO] QR Code: falha na alocação de memória.");
        display.setCursor(0, 28);
        display.println("Sem memoria p/ QR");
        display.display();
        return;
    }
 
    qrcode_initText(&qrcode, qrcodeData, versao, 0, "https://thingsboard.cloud/dashboards/groups/b0eb0780-3d10-11f1-9c01-37a7c07792f2/af367250-3d13-11f1-8ebb-d54a2d348b45");
 
    // Centraliza no OLED 128×64
    // Cada módulo QR desenhado em 2×2 pixels
    int tamanho    = qrcode.size * 2;           // tamanho total em pixels
    int offset_x   = (128 - tamanho) / 2;
    int offset_y   = (64  - tamanho) / 2 + 6;  // +6 para dar espaço ao título
 
    display.setCursor((128 - 13*6) / 2, 0);    // Centraliza título
    display.print("ACESSO REMOTO");
 
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                display.fillRect(x * 2 + offset_x,
                                 y * 2 + offset_y,
                                 2, 2,
                                 SSD1306_WHITE);
            }
        }
    }
 
    free(qrcodeData); // OBRIGATÓRIO: libera a heap após uso
}