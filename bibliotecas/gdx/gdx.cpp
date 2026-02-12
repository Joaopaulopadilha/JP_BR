// gdx.cpp
// Biblioteca Gráfica para JPLang - Win32/X11 + OpenGL + ImGui

// STB Image
#define STB_IMAGE_IMPLEMENTATION
#include "src/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "src/stb_image_resize2.h"

// ImGui
#include "src/imgui.cpp"
#include "src/imgui_draw.cpp"
#include "src/imgui_tables.cpp"
#include "src/imgui_widgets.cpp"

#include "gdx_defs.hpp"
#include "botao.hpp"
#include "etiqueta.hpp"
#include "janela.hpp"

static std::vector<Janela*> g_janelas;
static std::vector<Botao*> g_botoes;
static std::vector<Etiqueta*> g_etiquetas;

Janela* get_janela_atual() {
    if (g_janelas.empty()) return nullptr;
    return g_janelas.back();
}

// Implementação dos métodos de desenho da Janela
void Janela::desenhar_etiqueta(Etiqueta* lbl) {
    lbl->desenhar_imgui();
}

// Implementação de definir_imagem
bool Janela::definir_imagem(const std::string& caminho) {
    // Libera imagem anterior
    if (imagem_original) {
        free(imagem_original);
        imagem_original = nullptr;
    }
    if (textura_fundo != 0) {
        glDeleteTextures(1, &textura_fundo);
        textura_fundo = 0;
    }

    // Carrega nova imagem
    int canais;
    imagem_original = stbi_load(caminho.c_str(), &img_largura_original, &img_altura_original, &canais, 4);
    if (!imagem_original) {
        tem_imagem = false;
        return false;
    }

    img_canais = 4;
    tem_imagem = true;

    // Cria textura OpenGL
    glGenTextures(1, &textura_fundo);
    glBindTexture(GL_TEXTURE_2D, textura_fundo);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_largura_original, img_altura_original, 0, GL_RGBA, GL_UNSIGNED_BYTE, imagem_original);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

// =============================================================================
// FUNÇÕES EXPORTADAS (COM PREFIXO jp_)
// =============================================================================

// --- JANELA ---

JP_EXPORT JPValor jp_gdx_janela(JPValor* args, int numArgs) {
    std::string titulo = get_string(args, 0, numArgs);
    int w = (int)get_int(args, 1, numArgs);
    int h = (int)get_int(args, 2, numArgs);

    Janela* nova = new Janela(titulo, w, h);
    if (nova->aberta) {
        g_janelas.push_back(nova);
        return jp_int(g_janelas.size() - 1);
    }
    delete nova;
    return jp_int(-1);
}

JP_EXPORT JPValor jp_gdx_janela_exibir(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    if (id < 0 || id >= (int64_t)g_janelas.size()) return jp_bool(false);
    
    bool rodando = g_janelas[id]->processar_eventos();

    return jp_bool(rodando);
}

JP_EXPORT JPValor jp_gdx_janela_cor(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    int r = (int)get_int(args, 1, numArgs);
    int g = (int)get_int(args, 2, numArgs);
    int b = (int)get_int(args, 3, numArgs);

    if (id < 0 || id >= (int64_t)g_janelas.size()) return jp_bool(false);
    
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    g_janelas[id]->definir_cor(r, g, b);
    return jp_bool(true);
}

JP_EXPORT JPValor jp_gdx_janela_cor_atualizar(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    int r = (int)get_int(args, 1, numArgs);
    int g = (int)get_int(args, 2, numArgs);
    int b = (int)get_int(args, 3, numArgs);

    if (id < 0 || id >= (int64_t)g_janelas.size()) return jp_bool(false);
    
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    g_janelas[id]->definir_cor(r, g, b);
    return jp_bool(true);
}

JP_EXPORT JPValor jp_gdx_janela_imagem(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    std::string caminho = get_string(args, 1, numArgs);

    if (id < 0 || id >= (int64_t)g_janelas.size()) return jp_bool(false);
    
    return jp_bool(g_janelas[id]->definir_imagem(caminho));
}

JP_EXPORT JPValor jp_gdx_janela_imagem_atualizar(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    std::string caminho = get_string(args, 1, numArgs);

    if (id < 0 || id >= (int64_t)g_janelas.size()) return jp_bool(false);
    
    return jp_bool(g_janelas[id]->definir_imagem(caminho));
}

// --- BOTÃO ---

JP_EXPORT JPValor jp_gdx_botao(JPValor* args, int numArgs) {
    std::string titulo = get_string(args, 0, numArgs);
    int w = (int)get_int(args, 1, numArgs);
    int h = (int)get_int(args, 2, numArgs);
    int x = (int)get_int(args, 3, numArgs);
    int y = (int)get_int(args, 4, numArgs);

    Janela* win = get_janela_atual();
    if (!win) return jp_int(-1);

    Botao* btn = new Botao(x, y, w, h, titulo);
    btn->camada = 0;
    win->adicionar_botao(btn);
    g_botoes.push_back(btn);

    return jp_int(g_botoes.size() - 1);
}

JP_EXPORT JPValor jp_gdx_botao_camada_1(JPValor* args, int numArgs) {
    std::string titulo = get_string(args, 0, numArgs);
    int w = (int)get_int(args, 1, numArgs);
    int h = (int)get_int(args, 2, numArgs);
    int x = (int)get_int(args, 3, numArgs);
    int y = (int)get_int(args, 4, numArgs);

    Janela* win = get_janela_atual();
    if (!win) return jp_int(-1);

    Botao* btn = new Botao(x, y, w, h, titulo);
    btn->camada = 1;
    win->adicionar_botao(btn);
    g_botoes.push_back(btn);

    return jp_int(g_botoes.size() - 1);
}

JP_EXPORT JPValor jp_gdx_botao_camada_2(JPValor* args, int numArgs) {
    std::string titulo = get_string(args, 0, numArgs);
    int w = (int)get_int(args, 1, numArgs);
    int h = (int)get_int(args, 2, numArgs);
    int x = (int)get_int(args, 3, numArgs);
    int y = (int)get_int(args, 4, numArgs);

    Janela* win = get_janela_atual();
    if (!win) return jp_int(-1);

    Botao* btn = new Botao(x, y, w, h, titulo);
    btn->camada = 2;
    win->adicionar_botao(btn);
    g_botoes.push_back(btn);

    return jp_int(g_botoes.size() - 1);
}

JP_EXPORT JPValor jp_gdx_botao_clicado(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    if (id < 0 || id >= (int64_t)g_botoes.size()) return jp_bool(false);
    
    return jp_bool(g_botoes[id]->checar_clique());
}

JP_EXPORT JPValor jp_gdx_botao_cor(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    int r = (int)get_int(args, 1, numArgs);
    int g = (int)get_int(args, 2, numArgs);
    int b = (int)get_int(args, 3, numArgs);

    if (id < 0 || id >= (int64_t)g_botoes.size()) return jp_bool(false);
    
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    g_botoes[id]->definir_cor(r, g, b);
    return jp_bool(true);
}

JP_EXPORT JPValor jp_gdx_botao_fonte_cor(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    int r = (int)get_int(args, 1, numArgs);
    int g = (int)get_int(args, 2, numArgs);
    int b = (int)get_int(args, 3, numArgs);

    if (id < 0 || id >= (int64_t)g_botoes.size()) return jp_bool(false);
    
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    g_botoes[id]->definir_fonte_cor(r, g, b);
    return jp_bool(true);
}

JP_EXPORT JPValor jp_gdx_botao_canto_redondo(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    int raio = (int)get_int(args, 1, numArgs);

    if (id < 0 || id >= (int64_t)g_botoes.size()) return jp_bool(false);
    
    if (raio < 0) raio = 0;
    
    g_botoes[id]->definir_canto_redondo(raio);
    return jp_bool(true);
}

JP_EXPORT JPValor jp_gdx_botao_exibir(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    if (id < 0 || id >= (int64_t)g_botoes.size()) return jp_bool(false);
    
    return jp_bool(true);
}

// --- ETIQUETA ---

JP_EXPORT JPValor jp_gdx_etiqueta(JPValor* args, int numArgs) {
    std::string texto = get_string(args, 0, numArgs);
    int tam = (int)get_int(args, 1, numArgs);
    int x = (int)get_int(args, 2, numArgs);
    int y = (int)get_int(args, 3, numArgs);

    Janela* win = get_janela_atual();
    if (!win) return jp_int(-1);

    Etiqueta* lbl = new Etiqueta(x, y, tam, texto);
    win->adicionar_etiqueta(lbl);
    g_etiquetas.push_back(lbl);

    return jp_int(g_etiquetas.size() - 1);
}

JP_EXPORT JPValor jp_gdx_etiqueta_atualizar(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    std::string texto = get_string(args, 1, numArgs);
    int tam = (int)get_int(args, 2, numArgs);
    int x = (int)get_int(args, 3, numArgs);
    int y = (int)get_int(args, 4, numArgs);

    if (id < 0 || id >= (int64_t)g_etiquetas.size()) return jp_bool(false);
    
    Etiqueta* lbl = g_etiquetas[id];
    lbl->texto = texto;
    lbl->tamanho = tam;
    lbl->x = x;
    lbl->y = y;

    return jp_bool(true);
}

JP_EXPORT JPValor jp_gdx_etiqueta_exibir(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    if (id < 0 || id >= (int64_t)g_etiquetas.size()) return jp_bool(false);
    
    return jp_bool(true);
}