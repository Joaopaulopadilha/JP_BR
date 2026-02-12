// etiquetas.hpp
// Lógica de etiquetas (labels) para o wrapper ImGui da JPLang

#ifndef ETIQUETAS_HPP
#define ETIQUETAS_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "imgui.h"

// === ESTRUTURA DE ETIQUETA ===
struct Etiqueta {
    int id;
    std::string texto;
    float x, y;
    
    // Cor da fonte (0-255)
    int corR = 0;
    int corG = 0;
    int corB = 0;
    
    // Fonte
    std::string fonte = "";
    int tamanhoFonte = 16;
};

// === ESTADO GLOBAL DAS ETIQUETAS ===
static std::vector<std::unique_ptr<Etiqueta>> g_etiquetas;
static std::mutex g_etiquetasMutex;
static int g_nextEtiquetaId = 1;

// === FUNÇÕES AUXILIARES ===

// Encontra etiqueta por ID
static Etiqueta* encontrarEtiqueta(int id) {
    for (auto& etiqueta : g_etiquetas) {
        if (etiqueta->id == id) {
            return etiqueta.get();
        }
    }
    return nullptr;
}

// Limpa todas as etiquetas
static void limparEtiquetas() {
    std::lock_guard<std::mutex> lock(g_etiquetasMutex);
    g_etiquetas.clear();
    g_nextEtiquetaId = 1;
}

// Cria uma nova etiqueta
static int criarEtiqueta(const std::string& texto, float x, float y) {
    auto etiqueta = std::make_unique<Etiqueta>();
    int id = g_nextEtiquetaId++;
    etiqueta->id = id;
    etiqueta->texto = texto;
    etiqueta->x = x;
    etiqueta->y = y;
    
    {
        std::lock_guard<std::mutex> lock(g_etiquetasMutex);
        g_etiquetas.push_back(std::move(etiqueta));
    }
    
    return id;
}

// Define cor da etiqueta
static bool etiquetaCor(int id, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_etiquetasMutex);
    Etiqueta* etiqueta = encontrarEtiqueta(id);
    if (!etiqueta) return false;
    
    etiqueta->corR = r;
    etiqueta->corG = g;
    etiqueta->corB = b;
    
    return true;
}

// Define fonte da etiqueta
static bool etiquetaFonte(int id, const std::string& fonte, int tamanho) {
    std::lock_guard<std::mutex> lock(g_etiquetasMutex);
    Etiqueta* etiqueta = encontrarEtiqueta(id);
    if (!etiqueta) return false;
    
    etiqueta->fonte = fonte;
    etiqueta->tamanhoFonte = tamanho;
    
    return true;
}

// Atualiza o texto da etiqueta
static bool etiquetaTexto(int id, const std::string& texto) {
    std::lock_guard<std::mutex> lock(g_etiquetasMutex);
    Etiqueta* etiqueta = encontrarEtiqueta(id);
    if (!etiqueta) return false;
    
    etiqueta->texto = texto;
    
    return true;
}

// Desenha todas as etiquetas (chamado na thread de renderização)
static void desenharEtiquetas() {
    std::lock_guard<std::mutex> lock(g_etiquetasMutex);
    
    for (auto& etiqueta : g_etiquetas) {
        ImGui::SetNextWindowPos(ImVec2(etiqueta->x, etiqueta->y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
        
        // Janela invisível para a etiqueta
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        // Cor do texto
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
            etiqueta->corR / 255.0f,
            etiqueta->corG / 255.0f,
            etiqueta->corB / 255.0f,
            1.0f
        ));
        
        std::string windowName = "##lbl" + std::to_string(etiqueta->id);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMouseInputs);
        
        ImGui::Text("%s", etiqueta->texto.c_str());
        
        ImGui::End();
        
        // Pop das cores e estilos
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}

#endif // ETIQUETAS_HPP