// jpk.cpp
// Biblioteca grafica JPK para JPLang 3.0 - Wrapper Dear ImGui - Convenção int64_t
//
// Todas as funções recebem args como int64_t direto e retornam int64_t.
// Strings são passadas como char* castado para int64_t.
// Strings retornadas usam buffer rotativo estático.
//
// COMPILAÇÃO:
//   Windows: g++ -shared -o bibliotecas\jpk\jpk.jpd bibliotecas\jpk\jpk.cpp bibliotecas\jpk\src\imgui.cpp bibliotecas\jpk\src\imgui_draw.cpp bibliotecas\jpk\src\imgui_tables.cpp bibliotecas\jpk\src\imgui_widgets.cpp bibliotecas\jpk\src\backends\imgui_impl_win32.cpp bibliotecas\jpk\src\backends\imgui_impl_dx11.cpp -Ibibliotecas\jpk\src -Ibibliotecas\jpk -ld3d11 -ld3dcompiler -ldwmapi -lgdi32 -O2 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/jpk/libjpk.jpd bibliotecas/jpk/jpk.cpp bibliotecas/jpk/src/imgui.cpp bibliotecas/jpk/src/imgui_draw.cpp bibliotecas/jpk/src/imgui_tables.cpp bibliotecas/jpk/src/imgui_widgets.cpp bibliotecas/jpk/src/backends/imgui_impl_glfw.cpp bibliotecas/jpk/src/backends/imgui_impl_opengl3.cpp -Ibibliotecas/jpk/src -Ibibliotecas/jpk -lglfw -lGL -ldl -O2

#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>

// =============================================================================
// PLATAFORMA E EXPORT
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#include "janela.hpp"

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================
static char str_bufs[8][4096];
static int str_buf_idx = 0;

static int64_t retorna_str(const char* s) {
    char* buf = str_bufs[str_buf_idx++ & 7];
    strncpy(buf, s, 4095);
    buf[4095] = '\0';
    return (int64_t)buf;
}

static int64_t retorna_str(const std::string& s) {
    return retorna_str(s.c_str());
}

// =============================================================================
// JANELA
// =============================================================================

// jpk_janela(titulo, largura, altura) -> inteiro (1=ok)
JP_EXPORT int64_t jpk_janela(int64_t titulo, int64_t largura, int64_t altura) {
    return criarJanela(std::string((const char*)titulo), (int)largura, (int)altura);
}

// jpk_fundo(janela_id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_fundo(int64_t id, int64_t r, int64_t g, int64_t b) {
    return janelaFundo((int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_exibir() -> inteiro
JP_EXPORT int64_t jpk_exibir() {
    exibirJanela();
    return 1;
}

// =============================================================================
// BOTÃO
// =============================================================================

// jpk_botao(janela_id, texto, x, y, largura, altura) -> inteiro (id)
JP_EXPORT int64_t jpk_botao(int64_t id, int64_t texto, int64_t x, int64_t y, int64_t largura, int64_t altura) {
    return criarBotao(std::string((const char*)texto), (float)x, (float)y, (float)largura, (float)altura);
}

// jpk_clicado(botao_id) -> inteiro (1/0)
JP_EXPORT int64_t jpk_clicado(int64_t id) {
    return botaoClicado((int)id) ? 1 : 0;
}

// jpk_botao_cor_fundo(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_botao_cor_fundo(int64_t id, int64_t r, int64_t g, int64_t b) {
    return botaoCorFundo((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_botao_cor_fonte(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_botao_cor_fonte(int64_t id, int64_t r, int64_t g, int64_t b) {
    return botaoCorFonte((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_botao_fonte(id, fonte) -> inteiro
JP_EXPORT int64_t jpk_botao_fonte(int64_t id, int64_t fonte) {
    return botaoFonte((int)id, std::string((const char*)fonte)) ? 1 : 0;
}

// =============================================================================
// ETIQUETA
// =============================================================================

// jpk_etiqueta(janela_id, texto, x, y) -> inteiro (id)
JP_EXPORT int64_t jpk_etiqueta(int64_t id, int64_t texto, int64_t x, int64_t y) {
    return criarEtiqueta(std::string((const char*)texto), (float)x, (float)y);
}

// jpk_etiqueta_cor(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_etiqueta_cor(int64_t id, int64_t r, int64_t g, int64_t b) {
    return etiquetaCor((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_etiqueta_fonte(id, fonte, tamanho) -> inteiro
JP_EXPORT int64_t jpk_etiqueta_fonte(int64_t id, int64_t fonte, int64_t tamanho) {
    return etiquetaFonte((int)id, std::string((const char*)fonte), (int)tamanho) ? 1 : 0;
}

// jpk_etiqueta_texto(id, texto) -> inteiro
JP_EXPORT int64_t jpk_etiqueta_texto(int64_t id, int64_t texto) {
    return etiquetaTexto((int)id, std::string((const char*)texto)) ? 1 : 0;
}

// =============================================================================
// INPUT
// =============================================================================

// jpk_input(janela_id, placeholder, x, y, largura, altura) -> inteiro (id)
JP_EXPORT int64_t jpk_input(int64_t id, int64_t placeholder, int64_t x, int64_t y, int64_t largura, int64_t altura) {
    return criarInput(std::string((const char*)placeholder), (float)x, (float)y, (float)largura, (float)altura);
}

// jpk_input_valor(id) -> texto
JP_EXPORT int64_t jpk_input_valor(int64_t id) {
    return retorna_str(inputValor((int)id));
}

// jpk_input_definir(id, valor) -> inteiro
JP_EXPORT int64_t jpk_input_definir(int64_t id, int64_t valor) {
    return inputDefinirValor((int)id, std::string((const char*)valor)) ? 1 : 0;
}

// jpk_input_cor_fundo(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_input_cor_fundo(int64_t id, int64_t r, int64_t g, int64_t b) {
    return inputCorFundo((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_input_cor_fonte(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_input_cor_fonte(int64_t id, int64_t r, int64_t g, int64_t b) {
    return inputCorFonte((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// =============================================================================
// COMBOBOX
// =============================================================================

// jpk_combobox(janela_id, label, x, y, largura, altura) -> inteiro (id)
JP_EXPORT int64_t jpk_combobox(int64_t id, int64_t label, int64_t x, int64_t y, int64_t largura, int64_t altura) {
    return criarCombobox(std::string((const char*)label), (float)x, (float)y, (float)largura, (float)altura);
}

// jpk_combobox_adicionar(id, texto) -> inteiro
JP_EXPORT int64_t jpk_combobox_adicionar(int64_t id, int64_t texto) {
    return comboboxAdicionar((int)id, std::string((const char*)texto)) ? 1 : 0;
}

// jpk_combobox_selecionado(id) -> inteiro (indice)
JP_EXPORT int64_t jpk_combobox_selecionado(int64_t id) {
    return comboboxSelecionado((int)id);
}

// jpk_combobox_valor(id) -> texto
JP_EXPORT int64_t jpk_combobox_valor(int64_t id) {
    return retorna_str(comboboxValor((int)id));
}

// jpk_combobox_definir(id, indice) -> inteiro
JP_EXPORT int64_t jpk_combobox_definir(int64_t id, int64_t indice) {
    return comboboxDefinir((int)id, (int)indice) ? 1 : 0;
}

// jpk_combobox_limpar(id) -> inteiro
JP_EXPORT int64_t jpk_combobox_limpar(int64_t id) {
    return comboboxLimpar((int)id) ? 1 : 0;
}

// jpk_combobox_cor_fundo(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_combobox_cor_fundo(int64_t id, int64_t r, int64_t g, int64_t b) {
    return comboboxCorFundo((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_combobox_cor_fonte(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_combobox_cor_fonte(int64_t id, int64_t r, int64_t g, int64_t b) {
    return comboboxCorFonte((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// =============================================================================
// BARRA DE PROGRESSO
// =============================================================================

// jpk_barra(janela_id, x, y, largura, altura, valorMin, valorMax, orientacao) -> inteiro (id)
JP_EXPORT int64_t jpk_barra(int64_t id, int64_t x, int64_t y, int64_t largura, int64_t altura, int64_t valorMin, int64_t valorMax, int64_t orientacao) {
    return criarBarra((float)x, (float)y, (float)largura, (float)altura, (float)valorMin, (float)valorMax, std::string((const char*)orientacao));
}

// jpk_barra_valor(id, valor) -> inteiro
JP_EXPORT int64_t jpk_barra_valor(int64_t id, int64_t valor) {
    return barraValor((int)id, (float)valor) ? 1 : 0;
}

// jpk_barra_valor_atual(id) -> inteiro
JP_EXPORT int64_t jpk_barra_valor_atual(int64_t id) {
    return (int64_t)barraValorAtual((int)id);
}

// jpk_barra_cor(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_barra_cor(int64_t id, int64_t r, int64_t g, int64_t b) {
    return barraCor((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_barra_cor_fundo(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_barra_cor_fundo(int64_t id, int64_t r, int64_t g, int64_t b) {
    return barraCorFundo((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// =============================================================================
// BARRA DE TÍTULO
// =============================================================================

// jpk_barra_titulo() -> inteiro
JP_EXPORT int64_t jpk_barra_titulo() {
    return janelaBarraTitulo() ? 1 : 0;
}

// jpk_barra_titulo_cor(janela_id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_barra_titulo_cor(int64_t id, int64_t r, int64_t g, int64_t b) {
    return janelaBarraTituloCor((int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_barra_titulo_texto_cor(janela_id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_barra_titulo_texto_cor(int64_t id, int64_t r, int64_t g, int64_t b) {
    return janelaBarraTituloTextoCor((int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_barra_titulo_botoes_cor(janela_id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_barra_titulo_botoes_cor(int64_t id, int64_t r, int64_t g, int64_t b) {
    return janelaBarraTituloBotoesCor((int)r, (int)g, (int)b) ? 1 : 0;
}

// =============================================================================
// GAUGE (MEDIDOR CIRCULAR)
// =============================================================================

// jpk_gauge(janela_id, x, y, raio, valorMin, valorMax) -> inteiro (id)
JP_EXPORT int64_t jpk_gauge(int64_t id, int64_t x, int64_t y, int64_t raio, int64_t valorMin, int64_t valorMax) {
    return criarGauge((float)x, (float)y, (float)raio, (float)valorMin, (float)valorMax);
}

// jpk_gauge_valor(id, valor) -> inteiro
JP_EXPORT int64_t jpk_gauge_valor(int64_t id, int64_t valor) {
    return gaugeValor((int)id, (float)valor) ? 1 : 0;
}

// jpk_gauge_valor_atual(id) -> inteiro
JP_EXPORT int64_t jpk_gauge_valor_atual(int64_t id) {
    return (int64_t)gaugeValorAtual((int)id);
}

// jpk_gauge_cor(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_gauge_cor(int64_t id, int64_t r, int64_t g, int64_t b) {
    return gaugeCor((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_gauge_cor_fundo(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_gauge_cor_fundo(int64_t id, int64_t r, int64_t g, int64_t b) {
    return gaugeCorFundo((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_gauge_cor_ponteiro(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_gauge_cor_ponteiro(int64_t id, int64_t r, int64_t g, int64_t b) {
    return gaugeCorPonteiro((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_gauge_cor_centro(id, r, g, b) -> inteiro
JP_EXPORT int64_t jpk_gauge_cor_centro(int64_t id, int64_t r, int64_t g, int64_t b) {
    return gaugeCorCentro((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// jpk_gauge_espessura(id, espessura) -> inteiro
JP_EXPORT int64_t jpk_gauge_espessura(int64_t id, int64_t espessura) {
    return gaugeEspessura((int)id, (float)espessura) ? 1 : 0;
}

// jpk_gauge_angulos(id, inicio, fim) -> inteiro
JP_EXPORT int64_t jpk_gauge_angulos(int64_t id, int64_t inicio, int64_t fim) {
    return gaugeAngulos((int)id, (float)inicio, (float)fim) ? 1 : 0;
}

// jpk_gauge_ponteiro(id, comprimento, largura, centroRaio) -> inteiro
JP_EXPORT int64_t jpk_gauge_ponteiro(int64_t id, int64_t comprimento, int64_t largura, int64_t centroRaio) {
    return gaugePonteiro((int)id, (float)comprimento / 100.0f, (float)largura, (float)centroRaio) ? 1 : 0;
}

// jpk_gauge_mostrar_arco(id, mostrar) -> inteiro
JP_EXPORT int64_t jpk_gauge_mostrar_arco(int64_t id, int64_t mostrar) {
    return gaugeMostrarArco((int)id, mostrar != 0) ? 1 : 0;
}