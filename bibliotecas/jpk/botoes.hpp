// botoes.hpp
// Lógica de botões para o wrapper ImGui da JPLang

#ifndef BOTOES_HPP
#define BOTOES_HPP

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include "imgui.h"

// === ESTRUTURA DE BOTÃO ===
struct Botao {
    int id;
    std::string texto;
    float x, y, largura, altura;
    std::atomic<bool> clicado{false};
    
    // Cores (0-255)
    int corFundoR = 41;
    int corFundoG = 74;
    int corFundoB = 122;
    
    int corFundoHoverR = 51;
    int corFundoHoverG = 92;
    int corFundoHoverB = 153;
    
    int corFundoAtivoR = 31;
    int corFundoAtivoG = 56;
    int corFundoAtivoB = 92;
    
    int corFonteR = 255;
    int corFonteG = 255;
    int corFonteB = 255;
    
    std::string fonte = "";
};

// === ESTADO GLOBAL DOS BOTÕES ===
static std::vector<std::unique_ptr<Botao>> g_botoes;
static std::mutex g_botoesMutex;
static int g_nextBotaoId = 1;

// === FUNÇÕES AUXILIARES ===

// Encontra botão por ID
static Botao* encontrarBotao(int id) {
    for (auto& botao : g_botoes) {
        if (botao->id == id) {
            return botao.get();
        }
    }
    return nullptr;
}

// Limpa todos os botões
static void limparBotoes() {
    std::lock_guard<std::mutex> lock(g_botoesMutex);
    g_botoes.clear();
    g_nextBotaoId = 1;
}

// Cria um novo botão
static int criarBotao(const std::string& texto, float x, float y, float largura, float altura) {
    auto botao = std::make_unique<Botao>();
    int id = g_nextBotaoId++;
    botao->id = id;
    botao->texto = texto;
    botao->x = x;
    botao->y = y;
    botao->largura = largura;
    botao->altura = altura;
    
    {
        std::lock_guard<std::mutex> lock(g_botoesMutex);
        g_botoes.push_back(std::move(botao));
    }
    
    return id;
}

// Define cor de fundo do botão
static bool botaoCorFundo(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_botoesMutex);
    Botao* botao = encontrarBotao(id);
    if (!botao) return false;
    
    botao->corFundoR = r;
    botao->corFundoG = g;
    botao->corFundoB = b;
    
    // Calcula hover (mais claro) e ativo (mais escuro) automaticamente
    botao->corFundoHoverR = std::min(255, r + 25);
    botao->corFundoHoverG = std::min(255, g + 25);
    botao->corFundoHoverB = std::min(255, b + 25);
    
    botao->corFundoAtivoR = std::max(0, r - 25);
    botao->corFundoAtivoG = std::max(0, g - 25);
    botao->corFundoAtivoB = std::max(0, b - 25);
    
    return true;
}

// Define cor da fonte do botão
static bool botaoCorFonte(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_botoesMutex);
    Botao* botao = encontrarBotao(id);
    if (!botao) return false;
    
    botao->corFonteR = r;
    botao->corFonteG = g;
    botao->corFonteB = b;
    
    return true;
}

// Define fonte do botão
static bool botaoFonte(int id, const std::string& fonte) {
    std::lock_guard<std::mutex> lock(g_botoesMutex);
    Botao* botao = encontrarBotao(id);
    if (!botao) return false;
    
    botao->fonte = fonte;
    
    return true;
}

// Verifica se botão foi clicado (consome o clique)
static bool botaoClicado(int id) {
    std::lock_guard<std::mutex> lock(g_botoesMutex);
    Botao* botao = encontrarBotao(id);
    if (!botao) return false;
    
    return botao->clicado.exchange(false);
}

// Desenha todos os botões (chamado na thread de renderização)
static void desenharBotoes() {
    std::lock_guard<std::mutex> lock(g_botoesMutex);
    
    for (auto& botao : g_botoes) {
        ImGui::SetNextWindowPos(ImVec2(botao->x, botao->y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(botao->largura, botao->altura), ImGuiCond_Always);
        
        // Janela invisível para o botão
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        // Cores do botão
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
            botao->corFundoR / 255.0f,
            botao->corFundoG / 255.0f,
            botao->corFundoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
            botao->corFundoHoverR / 255.0f,
            botao->corFundoHoverG / 255.0f,
            botao->corFundoHoverB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(
            botao->corFundoAtivoR / 255.0f,
            botao->corFundoAtivoG / 255.0f,
            botao->corFundoAtivoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
            botao->corFonteR / 255.0f,
            botao->corFonteG / 255.0f,
            botao->corFonteB / 255.0f,
            1.0f
        ));
        
        std::string windowName = "##btn" + std::to_string(botao->id);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground);
        
        if (ImGui::Button(botao->texto.c_str(), ImVec2(botao->largura, botao->altura))) {
            botao->clicado = true;
        }
        
        ImGui::End();
        
        // Pop das cores e estilos
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
    }
}

#endif // BOTOES_HPP