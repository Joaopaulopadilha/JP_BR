// botao.hpp
// Classe Botao para GDX - ImGui (Multiplataforma)

#ifndef BOTAO_HPP
#define BOTAO_HPP

#include "gdx_defs.hpp"

class Botao {
public:
    int id;
    std::string texto;
    int x, y, largura, altura;
    bool clicado;

    // Cor de fundo (RGB 0-255)
    int cor_r, cor_g, cor_b;
    bool cor_personalizada;

    // Cor da fonte (RGB 0-255)
    int fonte_r, fonte_g, fonte_b;
    bool fonte_cor_personalizada;

    // Cantos arredondados
    int raio_canto;
    bool canto_arredondado;

    // Camada (0 = fundo, 1 = meio, 2 = topo)
    int camada;

    // Estado visual
    bool pressionado;
    bool hover;

    // Contador global de IDs
    static int g_contador_id;

    Botao(int _x, int _y, int _w, int _h, std::string _texto) {
        x = _x;
        y = _y;
        largura = _w;
        altura = _h;
        texto = _texto;
        clicado = false;
        pressionado = false;
        hover = false;
        
        // Cor padrão (cinza claro)
        cor_r = 221;
        cor_g = 221;
        cor_b = 221;
        cor_personalizada = false;

        // Cor da fonte padrão (preto)
        fonte_r = 0;
        fonte_g = 0;
        fonte_b = 0;
        fonte_cor_personalizada = false;

        // Sem cantos arredondados por padrão
        raio_canto = 0;
        canto_arredondado = false;

        // Camada padrão
        camada = 0;
        
        id = ++g_contador_id;
    }

    // --- COR DO BOTÃO ---
    void definir_cor(int r, int g, int b) {
        cor_r = r;
        cor_g = g;
        cor_b = b;
        cor_personalizada = true;
    }

    // --- COR DA FONTE ---
    void definir_fonte_cor(int r, int g, int b) {
        fonte_r = r;
        fonte_g = g;
        fonte_b = b;
        fonte_cor_personalizada = true;
    }

    // --- CANTOS ARREDONDADOS ---
    void definir_canto_redondo(int raio) {
        raio_canto = raio;
        canto_arredondado = (raio > 0);
    }

    // --- CAMADA ---
    void definir_camada(int c) {
        if (c < 0) c = 0;
        if (c > 2) c = 2;
        camada = c;
    }

    // Calcula cor escurecida para efeito de pressionado
    void obter_cor_pressionada(int& r, int& g, int& b) {
        r = (int)(cor_r * 0.8);
        g = (int)(cor_g * 0.8);
        b = (int)(cor_b * 0.8);
    }

    // Calcula cor clara para efeito de hover
    void obter_cor_hover(int& r, int& g, int& b) {
        r = std::min(255, (int)(cor_r * 1.1));
        g = std::min(255, (int)(cor_g * 1.1));
        b = std::min(255, (int)(cor_b * 1.1));
    }

    // Verifica se um ponto está dentro do botão
    bool contem_ponto(int px, int py) {
        return (px >= x && px < x + largura && py >= y && py < y + altura);
    }

    // --- DESENHO COM IMGUI (usando ImDrawList para controle total) ---
    void desenhar_imgui(std::vector<Botao*>* todos_botoes = nullptr) {
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImGuiIO& io = ImGui::GetIO();
        
        // Posições
        ImVec2 p_min((float)x, (float)y);
        ImVec2 p_max((float)(x + largura), (float)(y + altura));
        
        // Verifica mouse
        int mx = (int)io.MousePos.x;
        int my = (int)io.MousePos.y;
        bool mouse_sobre = contem_ponto(mx, my);
        
        // Verifica se há botão de camada superior cobrindo este ponto
        bool coberto = false;
        if (todos_botoes && mouse_sobre) {
            for (Botao* outro : *todos_botoes) {
                if (outro != this && outro->camada > this->camada && outro->contem_ponto(mx, my)) {
                    coberto = true;
                    break;
                }
            }
        }
        
        // Se coberto, ignora interação
        if (coberto) {
            mouse_sobre = false;
        }
        
        // Atualiza estados
        bool mouse_down = io.MouseDown[0];
        
        if (mouse_sobre) {
            hover = true;
            if (mouse_down) {
                pressionado = true;
            } else if (pressionado) {
                // Mouse foi solto sobre o botão = clique
                clicado = true;
                pressionado = false;
            }
        } else {
            hover = false;
            if (!mouse_down) {
                pressionado = false;
            }
        }
        
        // Calcula cor baseada no estado
        int r = cor_r, g = cor_g, b = cor_b;
        if (!cor_personalizada) {
            r = g = b = 221;
        }
        
        if (pressionado && hover) {
            obter_cor_pressionada(r, g, b);
        } else if (hover) {
            obter_cor_hover(r, g, b);
        }
        
        // Desenha fundo do botão
        float rounding = canto_arredondado ? (float)raio_canto : 4.0f;
        draw_list->AddRectFilled(p_min, p_max, rgb_para_imu32(r, g, b), rounding);
        
        // Desenha borda
        int br = (int)(r * 0.7);
        int bg = (int)(g * 0.7);
        int bb = (int)(b * 0.7);
        draw_list->AddRect(p_min, p_max, rgb_para_imu32(br, bg, bb), rounding);
        
        // Desenha texto centralizado
        int tr = fonte_r, tg = fonte_g, tb = fonte_b;
        if (!fonte_cor_personalizada) {
            tr = tg = tb = 0;
        }
        
        ImVec2 text_size = ImGui::CalcTextSize(texto.c_str());
        float text_x = x + (largura - text_size.x) / 2.0f;
        float text_y = y + (altura - text_size.y) / 2.0f;
        
        if (pressionado && hover) {
            text_x += 1;
            text_y += 1;
        }
        
        draw_list->AddText(ImVec2(text_x, text_y), rgb_para_imu32(tr, tg, tb), texto.c_str());
    }

    // --- VERIFICAÇÃO DE CLIQUE ---
    bool checar_clique() {
        if (clicado) {
            clicado = false;
            return true;
        }
        return false;
    }
};

int Botao::g_contador_id = 1000;

#endif