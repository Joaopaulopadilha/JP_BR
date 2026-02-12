// gauges.hpp
// Lógica de medidores circulares (gauges) com ponteiro para o wrapper ImGui da JPLang

#ifndef GAUGES_HPP
#define GAUGES_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <cmath>
#include "imgui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// === ESTRUTURA DE GAUGE ===
struct Gauge {
    int id;
    float x, y;
    float raio;
    float valorMin, valorMax;
    float valorAtual;
    float espessura = 10.0f;
    
    // Ângulos em graus (0 = direita, sentido horário)
    // Padrão: semicírculo superior (180° a 0°)
    float anguloInicio = 180.0f;
    float anguloFim = 0.0f;
    
    // Cor do arco preenchido (0-255)
    int corR = 0;
    int corG = 150;
    int corB = 255;
    
    // Cor do fundo (0-255)
    int corFundoR = 60;
    int corFundoG = 60;
    int corFundoB = 60;
    
    // Cor do ponteiro (0-255)
    int corPonteiroR = 255;
    int corPonteiroG = 50;
    int corPonteiroB = 50;
    
    // Cor do centro do ponteiro
    int corCentroR = 80;
    int corCentroG = 80;
    int corCentroB = 80;
    
    // Configurações do ponteiro
    float ponteiroComprimento = 0.85f; // Percentual do raio
    float ponteiroLargura = 3.0f;
    float centroRaio = 8.0f;
    
    // Mostrar arco preenchido além do ponteiro
    bool mostrarArcoPreenchido = true;
};

// === ESTADO GLOBAL DOS GAUGES ===
static std::vector<std::unique_ptr<Gauge>> g_gauges;
static std::mutex g_gaugesMutex;
static int g_nextGaugeId = 1;

// === FUNÇÕES AUXILIARES ===

// Encontra gauge por ID
static Gauge* encontrarGauge(int id) {
    for (auto& gauge : g_gauges) {
        if (gauge->id == id) {
            return gauge.get();
        }
    }
    return nullptr;
}

// Limpa todos os gauges
static void limparGauges() {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    g_gauges.clear();
    g_nextGaugeId = 1;
}

// Converte graus para radianos
static float grausParaRadianos(float graus) {
    return graus * (float)M_PI / 180.0f;
}

// Cria um novo gauge
static int criarGauge(float x, float y, float raio, float valorMin, float valorMax) {
    auto gauge = std::make_unique<Gauge>();
    int id = g_nextGaugeId++;
    gauge->id = id;
    gauge->x = x;
    gauge->y = y;
    gauge->raio = raio;
    gauge->valorMin = valorMin;
    gauge->valorMax = valorMax;
    gauge->valorAtual = valorMin;
    gauge->espessura = raio * 0.1f; // 10% do raio por padrão
    
    {
        std::lock_guard<std::mutex> lock(g_gaugesMutex);
        g_gauges.push_back(std::move(gauge));
    }
    
    return id;
}

// Define o valor atual do gauge
static bool gaugeValor(int id, float valor) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    // Clamp do valor entre min e max
    gauge->valorAtual = std::max(gauge->valorMin, std::min(gauge->valorMax, valor));
    return true;
}

// Retorna o valor atual do gauge
static float gaugeValorAtual(int id) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return 0.0f;
    
    return gauge->valorAtual;
}

// Define cor do arco preenchido
static bool gaugeCor(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->corR = r;
    gauge->corG = g;
    gauge->corB = b;
    
    return true;
}

// Define cor do fundo do gauge
static bool gaugeCorFundo(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->corFundoR = r;
    gauge->corFundoG = g;
    gauge->corFundoB = b;
    
    return true;
}

// Define cor do ponteiro
static bool gaugeCorPonteiro(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->corPonteiroR = r;
    gauge->corPonteiroG = g;
    gauge->corPonteiroB = b;
    
    return true;
}

// Define cor do centro do ponteiro
static bool gaugeCorCentro(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->corCentroR = r;
    gauge->corCentroG = g;
    gauge->corCentroB = b;
    
    return true;
}

// Define espessura do arco
static bool gaugeEspessura(int id, float espessura) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->espessura = espessura;
    return true;
}

// Define ângulos do arco (em graus)
static bool gaugeAngulos(int id, float inicio, float fim) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->anguloInicio = inicio;
    gauge->anguloFim = fim;
    return true;
}

// Define configurações do ponteiro
static bool gaugePonteiro(int id, float comprimento, float largura, float centroRaio) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->ponteiroComprimento = comprimento;
    gauge->ponteiroLargura = largura;
    gauge->centroRaio = centroRaio;
    return true;
}

// Ativa/desativa arco preenchido
static bool gaugeMostrarArco(int id, bool mostrar) {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    Gauge* gauge = encontrarGauge(id);
    if (!gauge) return false;
    
    gauge->mostrarArcoPreenchido = mostrar;
    return true;
}

// Desenha um arco
static void desenharArco(ImDrawList* drawList, ImVec2 centro, float raio, float espessura, 
                         float anguloInicio, float anguloFim, ImU32 cor, int segmentos = 32) {
    float inicioRad = grausParaRadianos(anguloInicio);
    float fimRad = grausParaRadianos(anguloFim);
    
    // Ajusta para sentido horário
    if (fimRad < inicioRad) {
        fimRad += 2.0f * (float)M_PI;
    }
    
    float deltaAngulo = (fimRad - inicioRad) / (float)segmentos;
    
    for (int i = 0; i < segmentos; i++) {
        float a1 = inicioRad + deltaAngulo * i;
        float a2 = inicioRad + deltaAngulo * (i + 1);
        
        // Pontos externos
        ImVec2 p1Ext(centro.x + cosf(a1) * raio, centro.y + sinf(a1) * raio);
        ImVec2 p2Ext(centro.x + cosf(a2) * raio, centro.y + sinf(a2) * raio);
        
        // Pontos internos
        float raioInt = raio - espessura;
        ImVec2 p1Int(centro.x + cosf(a1) * raioInt, centro.y + sinf(a1) * raioInt);
        ImVec2 p2Int(centro.x + cosf(a2) * raioInt, centro.y + sinf(a2) * raioInt);
        
        // Desenha quadrilátero
        drawList->AddQuadFilled(p1Ext, p2Ext, p2Int, p1Int, cor);
    }
}

// Desenha todos os gauges (chamado na thread de renderização)
static void desenharGauges() {
    std::lock_guard<std::mutex> lock(g_gaugesMutex);
    
    for (auto& gauge : g_gauges) {
        // Calcula a porcentagem
        float range = gauge->valorMax - gauge->valorMin;
        float porcento = 0.0f;
        if (range > 0) {
            porcento = (gauge->valorAtual - gauge->valorMin) / range;
        }
        porcento = std::max(0.0f, std::min(1.0f, porcento));
        
        // Centro do gauge
        ImVec2 centro(gauge->x + gauge->raio, gauge->y + gauge->raio);
        
        // Tamanho da janela
        float tamanho = gauge->raio * 2 + 20;
        
        // Posição da janela
        ImGui::SetNextWindowPos(ImVec2(gauge->x - 10, gauge->y - 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(tamanho + 20, tamanho + 20), ImGuiCond_Always);
        
        // Janela invisível
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        std::string windowName = "##gauge" + std::to_string(gauge->id);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMouseInputs);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Cores
        ImU32 corFundo = IM_COL32(gauge->corFundoR, gauge->corFundoG, gauge->corFundoB, 255);
        ImU32 corArco = IM_COL32(gauge->corR, gauge->corG, gauge->corB, 255);
        ImU32 corPonteiro = IM_COL32(gauge->corPonteiroR, gauge->corPonteiroG, gauge->corPonteiroB, 255);
        ImU32 corCentro = IM_COL32(gauge->corCentroR, gauge->corCentroG, gauge->corCentroB, 255);
        
        // Desenha arco de fundo (trilha completa)
        desenharArco(drawList, centro, gauge->raio, gauge->espessura, 
                     gauge->anguloInicio, gauge->anguloFim, corFundo);
        
        // Desenha arco preenchido (se ativado)
        if (gauge->mostrarArcoPreenchido && porcento > 0.01f) {
            // Calcula ângulo atual baseado na porcentagem
            float rangeAngulo = gauge->anguloFim - gauge->anguloInicio;
            if (rangeAngulo < 0) rangeAngulo += 360.0f;
            
            float anguloAtual = gauge->anguloInicio + rangeAngulo * porcento;
            
            desenharArco(drawList, centro, gauge->raio, gauge->espessura, 
                         gauge->anguloInicio, anguloAtual, corArco);
        }
        
        // Calcula ângulo do ponteiro
        float rangeAngulo = gauge->anguloFim - gauge->anguloInicio;
        if (rangeAngulo < 0) rangeAngulo += 360.0f;
        float anguloPonteiro = gauge->anguloInicio + rangeAngulo * porcento;
        float anguloRad = grausParaRadianos(anguloPonteiro);
        
        // Comprimento do ponteiro
        float comprimento = gauge->raio * gauge->ponteiroComprimento;
        
        // Ponta do ponteiro
        ImVec2 ponta(
            centro.x + cosf(anguloRad) * comprimento,
            centro.y + sinf(anguloRad) * comprimento
        );
        
        // Base do ponteiro (lado oposto, menor)
        float baseComprimento = gauge->centroRaio * 0.5f;
        ImVec2 base(
            centro.x - cosf(anguloRad) * baseComprimento,
            centro.y - sinf(anguloRad) * baseComprimento
        );
        
        // Pontos laterais do ponteiro (para formar triângulo)
        float anguloPerp = anguloRad + (float)M_PI / 2.0f;
        float larguraBase = gauge->ponteiroLargura;
        
        ImVec2 esquerda(
            centro.x + cosf(anguloPerp) * larguraBase,
            centro.y + sinf(anguloPerp) * larguraBase
        );
        ImVec2 direita(
            centro.x - cosf(anguloPerp) * larguraBase,
            centro.y - sinf(anguloPerp) * larguraBase
        );
        
        // Desenha ponteiro (triângulo)
        drawList->AddTriangleFilled(ponta, esquerda, direita, corPonteiro);
        
        // Desenha linha fina até a base para dar mais forma
        drawList->AddLine(esquerda, base, corPonteiro, 2.0f);
        drawList->AddLine(direita, base, corPonteiro, 2.0f);
        drawList->AddTriangleFilled(esquerda, direita, base, corPonteiro);
        
        // Desenha centro (círculo)
        drawList->AddCircleFilled(centro, gauge->centroRaio, corCentro);
        drawList->AddCircle(centro, gauge->centroRaio, corPonteiro, 0, 2.0f);
        
        ImGui::End();
        
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(2);
    }
}

#endif // GAUGES_HPP
