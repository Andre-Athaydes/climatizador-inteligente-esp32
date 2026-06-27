#include "FuzzyLogica.h"

// ============================================================
// INSTÂNCIA DO MOTOR FUZZY
// ============================================================

static Fuzzy *fuzzy = new Fuzzy();
float ultimaTempAmbiente = NAN; // Para cálculo da tendência


// ============================================================
// PONTEIROS AOS SETS — armazenados para uso nas regras
// Declarados aqui para que setupFuzzy() e as regras possam acessar
// sem recriar objetos (os objetos pertencem ao motor Fuzzy após addFuzzySet)
// ============================================================

// Entradas
static FuzzySet *negativo_quente, *zero_conforto, *positivo_frio;
static FuzzySet *diminuindo, *estavel, *aumentando;
static FuzzySet *seca, *ideal, *umida;
static FuzzySet *fria, *neutra, *quente;
 
// Saídas
static FuzzySet *frigido, *moderado, *economico;
static FuzzySet *v_off, *v1, *v2, *v3;

// ============================================================
// SETUP DO MOTOR FUZZY
// ============================================================

void setupFuzzy() {
    // --- ENTRADAS (Input) ---

    // Input 1: Erro (Setpoint - TempAmbiente)
    FuzzyInput *erro = new FuzzyInput(1);
    FuzzySet *negativo_quente = new FuzzySet(-5, -5, -2.5, -0.5);
    FuzzySet *zero_conforto = new FuzzySet(-1.5, 0, 0, 1.5);
    FuzzySet *positivo_frio = new FuzzySet(0.5, 2.5, 5, 5);
    erro->addFuzzySet(negativo_quente);
    erro->addFuzzySet(zero_conforto);
    erro->addFuzzySet(positivo_frio);
    fuzzy->addFuzzyInput(erro);

    // Input 2: Tendência (Variação entre leituras)
    FuzzyInput *tendencia = new FuzzyInput(2);
    FuzzySet *diminuindo = new FuzzySet(-2, -2, -0.8, -0.2);
    FuzzySet *estavel = new FuzzySet(-0.4, 0, 0, 0.4);
    FuzzySet *aumentando = new FuzzySet(0.2, 0.8, 2, 2);
    tendencia->addFuzzySet(diminuindo);
    tendencia->addFuzzySet(estavel);
    tendencia->addFuzzySet(aumentando);
    fuzzy->addFuzzyInput(tendencia);

    // Input 3: Umidade
    FuzzyInput *umidade = new FuzzyInput(3);
    FuzzySet *seca = new FuzzySet(0, 0, 30, 45);
    FuzzySet *ideal = new FuzzySet(40, 50, 50, 65);
    FuzzySet *umida = new FuzzySet(60, 80, 100, 100);
    umidade->addFuzzySet(seca);
    umidade->addFuzzySet(ideal);
    umidade->addFuzzySet(umida);
    fuzzy->addFuzzyInput(umidade);

    // Input 4: Temperatura Corporal (MLX90614)
    FuzzyInput *corpo = new FuzzyInput(4);
    FuzzySet *fria = new FuzzySet(30, 30, 32, 33);
    FuzzySet *neutra = new FuzzySet(32.5, 34, 34, 35.5);
    FuzzySet *quente = new FuzzySet(35, 36.5, 38, 38);
    corpo->addFuzzySet(fria);
    corpo->addFuzzySet(neutra);
    corpo->addFuzzySet(quente);
    fuzzy->addFuzzyInput(corpo);

    // --- SAÍDAS (Output) ---
    // Output 1: Temperatura do Ar Condicionado
    FuzzyOutput *ac = new FuzzyOutput(1);
    FuzzySet *frigido = new FuzzySet(16, 16, 18, 20);
    FuzzySet *moderado = new FuzzySet(19, 23, 23, 27);
    FuzzySet *economico = new FuzzySet(25, 28, 30, 30);
    ac->addFuzzySet(frigido);
    ac->addFuzzySet(moderado);
    ac->addFuzzySet(economico);
    fuzzy->addFuzzyOutput(ac);

    // Output 2: Velocidade do Ventilador
    FuzzyOutput *ventilador = new FuzzyOutput(2);
    FuzzySet *v_off = new FuzzySet(0, 0, 10, 20);
    FuzzySet *v1 = new FuzzySet(15, 35, 35, 55);
    FuzzySet *v2 = new FuzzySet(45, 65, 65, 85);
    FuzzySet *v3 = new FuzzySet(75, 90, 100, 100);
    ventilador->addFuzzySet(v_off);
    ventilador->addFuzzySet(v1);
    ventilador->addFuzzySet(v2);
    ventilador->addFuzzySet(v3);
    fuzzy->addFuzzyOutput(ventilador);

    // --- REGRAS (1 a 15) ---
    // Lambda ajustada para capturar o ponteiro 'fuzzy'

    int id = 1; // Contador sequencial de regras


    auto R = [&](FuzzyRuleAntecedent* ant,
                 FuzzySet* outAC,
                 FuzzySet* outVent) {
        FuzzyRuleConsequent* cons = new FuzzyRuleConsequent();
        if (outAC)   cons->addOutput(outAC);
        if (outVent) cons->addOutput(outVent);
        fuzzy->addFuzzyRule(new FuzzyRule(id++, ant, cons));
    };

    FuzzyRuleAntecedent *ant;

    // Definição das Regras
    // R1: Quente + Úmido → Frígido + V3
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(negativo_quente, umida);
    R(ant, frigido, v3);

    // R2. Quente + Umidade Ideal → Moderado + V2
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(negativo_quente, ideal);
    R(ant, moderado, v2);

    // R3. Quente + Corpo Quente → Frígido + V3
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(negativo_quente, quente);
    R(ant, frigido, v3);


    // R4: Quente + Temperatura Aumentando → Frígido + V2
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(negativo_quente, aumentando);
    R(ant, frigido, v2);
 
    // R5: Quente + Seco → Moderado + V1
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(negativo_quente, seca);
    R(ant, moderado, v1);
 
    // R6: Conforto + Estável + Corpo Neutro → Econômico + Desligado
    FuzzyRuleAntecedent *aux6 = new FuzzyRuleAntecedent();
    aux6->joinWithAND(zero_conforto, estavel);
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(aux6, neutra);
    R(ant, economico, v_off);
 
    // R7: Conforto + Úmido → Moderado + V1
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(zero_conforto, umida);
    R(ant, moderado, v1);
 
    // R8: Conforto + Corpo Quente → Econômico + V1
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(zero_conforto, quente);
    R(ant, economico, v1);
 
    // R9: Conforto + Temperatura Aumentando → Moderado + V1
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(zero_conforto, aumentando);
    R(ant, moderado, v1);
 
    // R10: Conforto + Seco → Econômico + Desligado
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(zero_conforto, seca);
    R(ant, economico, v_off);
 
    // R11: Frio (erro positivo) → Econômico + Desligado
    ant = new FuzzyRuleAntecedent(); ant->joinSingle(positivo_frio);
    R(ant, economico, v_off);
 
    // R12: Corpo Frio → Econômico + Desligado
    ant = new FuzzyRuleAntecedent(); ant->joinSingle(fria);
    R(ant, economico, v_off);
 
    // R13: Temperatura Diminuindo + Conforto → Econômico + Desligado
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(diminuindo, zero_conforto);
    R(ant, economico, v_off);
 
    // R14: Úmido + Corpo Quente → (sem AC) + V3
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(umida, quente);
    R(ant, nullptr, v3);
 
    // R15: Seco + Corpo Neutro → (sem AC) + Desligado
    ant = new FuzzyRuleAntecedent(); ant->joinWithAND(seca, neutra);
    R(ant, nullptr, v_off);
 
    Serial.println("[Fuzzy] Motor configurado com 15 regras.");
}

// ============================================================
// PROCESSAR FUZZY — retorna ResultadoFuzzy 
// ============================================================

ResultadoFuzzy processarFuzzy(DadosSensores &dados) {

    ResultadoFuzzy resultado = {dados.setpoint, 0, false, 0.0f, 0.0f};
    
    //1. Erro = diferença entre setpoint e temperatura atual
    float valorErro = dados.setpoint - dados.tempAmbiente;
    
    // 2. Tendência: variação entre leitura atual e anterio
    float valorTendencia = 0.0f;
    if (!isnan(ultimaTempAmbiente)) {
        valorTendencia = dados.tempAmbiente - ultimaTempAmbiente;
        // Clamp: tendências absurdas (ex: sensor desconectado) não alimentam o Fuzzy
        valorTendencia = constrain(valorTendencia, -2.0f, 2.0f);
    }
    ultimaTempAmbiente = dados.tempAmbiente;

    // 3. Alimentar entradas
    fuzzy->setInput(1, valorErro);
    fuzzy->setInput(2, valorTendencia);
    fuzzy->setInput(3, dados.umidade);
    fuzzy->setInput(4, dados.tempCorporal);

    // 4. Fuzzificação + Inferência
    fuzzy->fuzzify();

    // 5. Defuzzificação 
    float saidaAC = fuzzy->defuzzify(1);
    float saidaVent = fuzzy->defuzzify(2);

    // 6. Validação da saída do AC
    // Proteção: Se o AC retornar 0 (erro de regras), mantém no setpoint
    if (isnan(saidaAC) || saidaAC < 16.0f || saidaAC > 30.0f) {
        saidaAC = dados.setpoint; // Fallback seguro: mantém o setpoint atual
        Serial.println("[Fuzzy] AVISO: saída AC inválida — usando setpoint como fallback.");
    }

    // 7. Mapeia saída contínua do ventilador (0–100) para estágio discreto   
    uint8_t estagioVent;
    if (saidaVent < 20.0f) estagioVent = 0; // DESLIGADO
    else if (saidaVent < 50.0f) estagioVent = 1; // BAIXA
    else if (saidaVent < 80.0f) estagioVent = 2; // MEDIA
    else                        estagioVent = 3; // ALTA

    
    resultado.temperaturaAC      = saidaAC;
    resultado.estagioVentilador  = estagioVent;
    resultado.valido             = true;
    resultado.erroFuzzy          = valorErro; 
    resultado.tendencia          = valorTendencia;
 
    Serial.printf("[Fuzzy] Err:%.1f Tend:%.2f | AC:%.1fC Vent:%d\n",
                  valorErro, valorTendencia, saidaAC, estagioVent);
 
    return resultado;
}