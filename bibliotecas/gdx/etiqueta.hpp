// etiqueta.hpp
// Classe Etiqueta para GDX - ImGui (Multiplataforma)

#ifndef ETIQUETA_HPP
#define ETIQUETA_HPP

#include "gdx_defs.hpp"

class Etiqueta {
public:
    int id;
    std::string texto;
    int x, y;
    int tamanho;

    // Cor do texto (RGB 0-255)
    int cor_r, cor_g, cor_b;
    bool cor_personalizada;

    // Contador global de IDs
    static int g_contador_lbl;

    Etiqueta(int _x, int _y, int _tamanho, std::string _texto) {
        x = _x;
        y = _y;
        tamanho = _tamanho;
        texto = _texto;
        
        // Cor padrão (preto)
        cor_r = 0;
        cor_g = 0;
        cor_b = 0;
        cor_personalizada = false;
        
        id = ++g_contador_lbl;
    }

    ~Etiqueta() {
        // Nada a liberar
    }

    // --- COR DO TEXTO ---
    void definir_cor(int r, int g, int b) {
        cor_r = r;
        cor_g = g;
        cor_b = b;
        cor_personalizada = true;
    }

    // --- DESENHO COM IMGUI ---
    void desenhar_imgui() {
        // Configura posição
        ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
        
        // Janela invisível
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                                  ImGuiWindowFlags_NoResize | 
                                  ImGuiWindowFlags_NoMove | 
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoBackground |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_AlwaysAutoResize;

        char window_id[64];
        snprintf(window_id, sizeof(window_id), "##lbl_%d", id);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(window_id, nullptr, flags);
        ImGui::PopStyleVar();
        
        // Escala da fonte
        float escala = tamanho / 14.0f;
        if (escala < 0.5f) escala = 0.5f;
        if (escala > 4.0f) escala = 4.0f;
        
        ImGui::SetWindowFontScale(escala);
        
        // Cor do texto
        if (cor_personalizada) {
            ImGui::PushStyleColor(ImGuiCol_Text, rgb_para_imvec4(cor_r, cor_g, cor_b));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, rgb_para_imvec4(0, 0, 0));
        }
        
        ImGui::TextUnformatted(texto.c_str());
        
        ImGui::PopStyleColor(1);
        
        ImGui::End();
    }
};

int Etiqueta::g_contador_lbl = 2000;

#endif