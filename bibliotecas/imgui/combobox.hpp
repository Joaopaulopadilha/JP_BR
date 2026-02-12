// combobox.hpp
// Lógica de combobox para o wrapper ImGui da JPLang

#ifndef COMBOBOX_HPP
#define COMBOBOX_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "imgui.h"

// === ESTRUTURA DE COMBOBOX ===
struct Combobox {
    int id;
    std::string label;
    float x, y, largura, altura;
    
    std::vector<std::string> itens;
    int selecionado = 0;
    
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

// === ESTADO GLOBAL DOS COMBOBOXES ===
static std::vector<std::unique_ptr<Combobox>> g_comboboxes;
static std::mutex g_comboboxesMutex;
static int g_nextComboboxId = 1;

// === FUNÇÕES AUXILIARES ===

// Encontra combobox por ID
static Combobox* encontrarCombobox(int id) {
    for (auto& combobox : g_comboboxes) {
        if (combobox->id == id) {
            return combobox.get();
        }
    }
    return nullptr;
}

// Limpa todos os comboboxes
static void limparComboboxes() {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    g_comboboxes.clear();
    g_nextComboboxId = 1;
}

// Cria um novo combobox
static int criarCombobox(const std::string& label, float x, float y, float largura, float altura) {
    auto combobox = std::make_unique<Combobox>();
    int id = g_nextComboboxId++;
    combobox->id = id;
    combobox->label = label;
    combobox->x = x;
    combobox->y = y;
    combobox->largura = largura;
    combobox->altura = altura;
    combobox->selecionado = 0;
    
    {
        std::lock_guard<std::mutex> lock(g_comboboxesMutex);
        g_comboboxes.push_back(std::move(combobox));
    }
    
    return id;
}

// Adiciona um item ao combobox
static bool comboboxAdicionar(int id, const std::string& texto) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return false;
    
    combobox->itens.push_back(texto);
    return true;
}

// Retorna o índice selecionado
static int comboboxSelecionado(int id) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return -1;
    
    return combobox->selecionado;
}

// Retorna o texto do item selecionado
static std::string comboboxValor(int id) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return "";
    
    if (combobox->selecionado >= 0 && combobox->selecionado < (int)combobox->itens.size()) {
        return combobox->itens[combobox->selecionado];
    }
    return "";
}

// Define qual item está selecionado por índice
static bool comboboxDefinir(int id, int indice) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return false;
    
    if (indice >= 0 && indice < (int)combobox->itens.size()) {
        combobox->selecionado = indice;
        return true;
    }
    return false;
}

// Limpa todos os itens do combobox
static bool comboboxLimpar(int id) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return false;
    
    combobox->itens.clear();
    combobox->selecionado = 0;
    return true;
}

// Define cor de fundo do combobox
static bool comboboxCorFundo(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return false;
    
    combobox->corFundoR = r;
    combobox->corFundoG = g;
    combobox->corFundoB = b;
    
    return true;
}

// Define cor da fonte do combobox
static bool comboboxCorFonte(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    Combobox* combobox = encontrarCombobox(id);
    if (!combobox) return false;
    
    combobox->corFonteR = r;
    combobox->corFonteG = g;
    combobox->corFonteB = b;
    
    return true;
}

// Desenha todos os comboboxes (chamado na thread de renderização)
static void desenharComboboxes() {
    std::lock_guard<std::mutex> lock(g_comboboxesMutex);
    
    for (auto& combobox : g_comboboxes) {
        ImGui::SetNextWindowPos(ImVec2(combobox->x, combobox->y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(combobox->largura + 10, combobox->altura + 10), ImGuiCond_Always);
        
        // Janela invisível para o combobox
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        // Cores do combobox
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(
            combobox->corFundoR / 255.0f,
            combobox->corFundoG / 255.0f,
            combobox->corFundoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(
            std::min(255, combobox->corFundoR + 10) / 255.0f,
            std::min(255, combobox->corFundoG + 10) / 255.0f,
            std::min(255, combobox->corFundoB + 10) / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(
            combobox->corFundoR / 255.0f,
            combobox->corFundoG / 255.0f,
            combobox->corFundoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
            combobox->corFonteR / 255.0f,
            combobox->corFonteG / 255.0f,
            combobox->corFonteB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(
            combobox->corBordaR / 255.0f,
            combobox->corBordaG / 255.0f,
            combobox->corBordaB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
            combobox->corFundoR / 255.0f,
            combobox->corFundoG / 255.0f,
            combobox->corFundoB / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
            std::min(255, combobox->corFundoR + 20) / 255.0f,
            std::min(255, combobox->corFundoG + 20) / 255.0f,
            std::min(255, combobox->corFundoB + 20) / 255.0f,
            1.0f
        ));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.31f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
        
        // Borda do combobox
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, (combobox->altura - 20) / 2));
        
        std::string windowName = "##combobox" + std::to_string(combobox->id);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground);
        
        std::string comboLabel = "##combo" + std::to_string(combobox->id);
        ImGui::SetNextItemWidth(combobox->largura);
        
        // Preview text
        const char* previewText = combobox->label.c_str();
        if (combobox->selecionado >= 0 && combobox->selecionado < (int)combobox->itens.size()) {
            previewText = combobox->itens[combobox->selecionado].c_str();
        }
        
        if (ImGui::BeginCombo(comboLabel.c_str(), previewText)) {
            for (int i = 0; i < (int)combobox->itens.size(); i++) {
                bool isSelected = (combobox->selecionado == i);
                if (ImGui::Selectable(combobox->itens[i].c_str(), isSelected)) {
                    combobox->selecionado = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::End();
        
        // Pop das cores e estilos
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(11);
    }
}

#endif // COMBOBOX_HPP