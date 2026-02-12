// inputs.hpp
// Lógica de campos de input para o wrapper ImGui da JPLang

#ifndef INPUTS_HPP
#define INPUTS_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "imgui.h"

// === ESTRUTURA DE INPUT ===
struct Input {
    int id;
    std::string placeholder;
    char buffer[256] = {0};
    float x, y, largura, altura;
    bool focado = false;
    
    // Cor de fundo (0-255)
    int corFundoR = 255;
    int corFundoG = 255;
    int corFundoB = 255;
    
    // Cor da fonte (0-255)
    int corFonteR = 0;
    int corFonteG = 0;
    int corFonteB = 0;
    
    // Cor da borda
    int corBordaR = 150;
    int corBordaG = 150;
    int corBordaB = 150;
};

// === ESTADO GLOBAL DOS INPUTS ===
static std::vector<std::unique_ptr<Input>> g_inputs;
static std::mutex g_inputsMutex;
static int g_nextInputId = 1;

// === FUNÇÕES AUXILIARES ===

// Encontra input por ID
static Input* encontrarInput(int id) {
    for (auto& input : g_inputs) {
        if (input->id == id) {
            return input.get();
        }
    }
    return nullptr;
}

// Limpa todos os inputs
static void limparInputs() {
    std::lock_guard<std::mutex> lock(g_inputsMutex);
    g_inputs.clear();
    g_nextInputId = 1;
}

// Cria um novo input
static int criarInput(const std::string& placeholder, float x, float y, float largura, float altura) {
    auto input = std::make_unique<Input>();
    int id = g_nextInputId++;
    input->id = id;
    input->placeholder = placeholder;
    input->x = x;
    input->y = y;
    input->largura = largura;
    input->altura = altura;
    memset(input->buffer, 0, sizeof(input->buffer));
    
    {
        std::lock_guard<std::mutex> lock(g_inputsMutex);
        g_inputs.push_back(std::move(input));
    }
    
    return id;
}

// Obtém o valor do input
static std::string inputValor(int id) {
    std::lock_guard<std::mutex> lock(g_inputsMutex);
    Input* input = encontrarInput(id);
    if (!input) return "";
    
    return std::string(input->buffer);
}

// Define o valor do input
static bool inputDefinirValor(int id, const std::string& valor) {
    std::lock_guard<std::mutex> lock(g_inputsMutex);
    Input* input = encontrarInput(id);
    if (!input) return false;
    
    strncpy(input->buffer, valor.c_str(), sizeof(input->buffer) - 1);
    input->buffer[sizeof(input->buffer) - 1] = '\0';
    
    return true;
}

// Define cor de fundo do input
static bool inputCorFundo(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_inputsMutex);
    Input* input = encontrarInput(id);
    if (!input) return false;
    
    input->corFundoR = r;
    input->corFundoG = g;
    input->corFundoB = b;
    
    return true;
}

// Define cor da fonte do input
static bool inputCorFonte(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_inputsMutex);
    Input* input = encontrarInput(id);
    if (!input) return false;
    
    input->corFonteR = r;
    input->corFonteG = g;
    input->corFonteB = b;
    
    return true;
}

// Desenha todos os inputs (chamado na thread de renderização)
static void desenharInputs() {
    std::lock_guard<std::mutex> lock(g_inputsMutex);
    
    for (auto& input : g_inputs) {
        ImGui::SetNextWindowPos(ImVec2(input->x, input->y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(input->largura + 10, input->altura + 10), ImGuiCond_Always);
        
        // Janela invisível para o input
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        // Cores do input
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(
            input->corFundoR / 255.0f,
            input->corFundoG / 255.0f,
            input->corFundoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(
            std::min(255, input->corFundoR + 10) / 255.0f,
            std::min(255, input->corFundoG + 10) / 255.0f,
            std::min(255, input->corFundoB + 10) / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(
            input->corFundoR / 255.0f,
            input->corFundoG / 255.0f,
            input->corFundoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
            input->corFonteR / 255.0f,
            input->corFonteG / 255.0f,
            input->corFonteB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(
            input->corBordaR / 255.0f,
            input->corBordaG / 255.0f,
            input->corBordaB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
        
        // Borda do input
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, (input->altura - 20) / 2));
        
        std::string windowName = "##input" + std::to_string(input->id);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoNavInputs);
        
        std::string inputLabel = "##inputfield" + std::to_string(input->id);
        ImGui::SetNextItemWidth(input->largura);
        
        // Se focado, não mostra placeholder
        const char* hint = input->focado ? "" : input->placeholder.c_str();
        
        ImGui::InputTextWithHint(inputLabel.c_str(), hint, input->buffer, sizeof(input->buffer));
        
        // Atualiza estado de foco
        input->focado = ImGui::IsItemActive();
        
        ImGui::End();
        
        // Pop das cores e estilos
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(7);
    }
}

#endif // INPUTS_HPP