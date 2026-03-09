// barras.hpp
// Lógica de barras de progresso/gráfico para o wrapper ImGui da JPLang

#ifndef BARRAS_HPP
#define BARRAS_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include "imgui.h"

// === ESTRUTURA DE BARRA ===
struct Barra {
    int id;
    float x, y, largura, altura;
    float valorMin, valorMax;
    float valorAtual;
    std::string orientacao; // "h" horizontal, "v" vertical
    
    // Cor da barra preenchida (0-255)
    int corR = 0;
    int corG = 150;
    int corB = 255;
    
    // Cor do fundo (0-255)
    int corFundoR = 60;
    int corFundoG = 60;
    int corFundoB = 60;
    
    // Cor da borda
    int corBordaR = 100;
    int corBordaG = 100;
    int corBordaB = 100;
};

// === ESTADO GLOBAL DAS BARRAS ===
static std::vector<std::unique_ptr<Barra>> g_barras;
static std::mutex g_barrasMutex;
static int g_nextBarraId = 1;

// === FUNÇÕES AUXILIARES ===

// Encontra barra por ID
static Barra* encontrarBarra(int id) {
    for (auto& barra : g_barras) {
        if (barra->id == id) {
            return barra.get();
        }
    }
    return nullptr;
}

// Limpa todas as barras
static void limparBarras() {
    std::lock_guard<std::mutex> lock(g_barrasMutex);
    g_barras.clear();
    g_nextBarraId = 1;
}

// Cria uma nova barra
static int criarBarra(float x, float y, float largura, float altura, float valorMin, float valorMax, const std::string& orientacao) {
    auto barra = std::make_unique<Barra>();
    int id = g_nextBarraId++;
    barra->id = id;
    barra->x = x;
    barra->y = y;
    barra->largura = largura;
    barra->altura = altura;
    barra->valorMin = valorMin;
    barra->valorMax = valorMax;
    barra->valorAtual = valorMin;
    barra->orientacao = orientacao;
    
    {
        std::lock_guard<std::mutex> lock(g_barrasMutex);
        g_barras.push_back(std::move(barra));
    }
    
    return id;
}

// Define o valor atual da barra
static bool barraValor(int id, float valor) {
    std::lock_guard<std::mutex> lock(g_barrasMutex);
    Barra* barra = encontrarBarra(id);
    if (!barra) return false;
    
    // Clamp do valor entre min e max
    barra->valorAtual = std::max(barra->valorMin, std::min(barra->valorMax, valor));
    return true;
}

// Retorna o valor atual da barra
static float barraValorAtual(int id) {
    std::lock_guard<std::mutex> lock(g_barrasMutex);
    Barra* barra = encontrarBarra(id);
    if (!barra) return 0.0f;
    
    return barra->valorAtual;
}

// Define cor da barra preenchida
static bool barraCor(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_barrasMutex);
    Barra* barra = encontrarBarra(id);
    if (!barra) return false;
    
    barra->corR = r;
    barra->corG = g;
    barra->corB = b;
    
    return true;
}

// Define cor do fundo da barra
static bool barraCorFundo(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_barrasMutex);
    Barra* barra = encontrarBarra(id);
    if (!barra) return false;
    
    barra->corFundoR = r;
    barra->corFundoG = g;
    barra->corFundoB = b;
    
    return true;
}

// Desenha todas as barras (chamado na thread de renderização)
static void desenharBarras() {
    std::lock_guard<std::mutex> lock(g_barrasMutex);
    
    for (auto& barra : g_barras) {
        // Calcula a porcentagem
        float range = barra->valorMax - barra->valorMin;
        float porcento = 0.0f;
        if (range > 0) {
            porcento = (barra->valorAtual - barra->valorMin) / range;
        }
        porcento = std::max(0.0f, std::min(1.0f, porcento));
        
        // Posição da janela
        ImGui::SetNextWindowPos(ImVec2(barra->x, barra->y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(barra->largura + 4, barra->altura + 4), ImGuiCond_Always);
        
        // Janela invisível
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        std::string windowName = "##barra" + std::to_string(barra->id);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMouseInputs);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        // Cores
        ImU32 corFundo = IM_COL32(barra->corFundoR, barra->corFundoG, barra->corFundoB, 255);
        ImU32 corBarra = IM_COL32(barra->corR, barra->corG, barra->corB, 255);
        ImU32 corBorda = IM_COL32(barra->corBordaR, barra->corBordaG, barra->corBordaB, 255);
        
        // Desenha fundo
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + barra->largura, pos.y + barra->altura),
            corFundo
        );
        
        // Desenha barra preenchida
        if (barra->orientacao == "h" || barra->orientacao == "H") {
            // Horizontal: preenche da esquerda para direita
            float preenchimento = barra->largura * porcento;
            drawList->AddRectFilled(
                pos,
                ImVec2(pos.x + preenchimento, pos.y + barra->altura),
                corBarra
            );
        } else {
            // Vertical: preenche de baixo para cima
            float preenchimento = barra->altura * porcento;
            drawList->AddRectFilled(
                ImVec2(pos.x, pos.y + barra->altura - preenchimento),
                ImVec2(pos.x + barra->largura, pos.y + barra->altura),
                corBarra
            );
        }
        
        // Desenha borda
        drawList->AddRect(
            pos,
            ImVec2(pos.x + barra->largura, pos.y + barra->altura),
            corBorda
        );
        
        ImGui::End();
        
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(2);
    }
}

#endif // BARRAS_HPP