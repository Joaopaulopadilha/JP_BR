// imgui.cpp
// Wrapper Dear ImGui para JPLang - Multiplataforma (Windows: DirectX 11, Linux: OpenGL3 + GLFW)
// VERSÃO COM SUPORTE TCC: Exporta funções C para o runtime TCC + funções C++ para interpretador
//
// COMPILAÇÃO:
//   Windows: g++ -shared -o imgui.jpd imgui.cpp imgui/*.cpp imgui/backends/imgui_impl_win32.cpp imgui/backends/imgui_impl_dx11.cpp -ld3d11 -ldwmapi -O2
//   Linux:   g++ -shared -fPIC -o libimgui.jpd imgui.cpp imgui/*.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp -lglfw -lGL -ldl -O2

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// =============================================================================
// MACROS DE PORTABILIDADE
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_LINUX 0
    #define JP_EXPORT __declspec(dllexport)
    #ifdef _MSC_VER
        #pragma comment(lib, "d3d11.lib")
    #endif
#else
    #define JP_WINDOWS 0
    #define JP_LINUX 1
    #define JP_EXPORT __attribute__((visibility("default")))
#endif

#include "janela.hpp"
// =============================================================================
// TIPOS JPLANG (C++)
// =============================================================================
struct Instancia;
using Var = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

// =============================================================================
// TIPOS C PUROS (para comunicação com TCC)
// =============================================================================
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4,
    JP_TIPO_OBJETO = 5,
    JP_TIPO_LISTA = 6,
    JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
        void* lista;
        void* ponteiro;
    } valor;
} JPValor;

// =============================================================================
// CONVERSORES C <-> C++
// =============================================================================
static Var jp_para_variant(const JPValor& jp) {
    switch (jp.tipo) {
        case JP_TIPO_INT:
            return (int)jp.valor.inteiro;
        case JP_TIPO_DOUBLE:
            return jp.valor.decimal;
        case JP_TIPO_BOOL:
            return (bool)jp.valor.booleano;
        case JP_TIPO_STRING:
            return jp.valor.texto ? std::string(jp.valor.texto) : std::string("");
        default:
            return std::string("");
    }
}

static JPValor variant_para_jp(const Var& var) {
    JPValor jp;
    memset(&jp, 0, sizeof(jp));
    
    if (std::holds_alternative<int>(var)) {
        jp.tipo = JP_TIPO_INT;
        jp.valor.inteiro = std::get<int>(var);
    }
    else if (std::holds_alternative<double>(var)) {
        jp.tipo = JP_TIPO_DOUBLE;
        jp.valor.decimal = std::get<double>(var);
    }
    else if (std::holds_alternative<bool>(var)) {
        jp.tipo = JP_TIPO_BOOL;
        jp.valor.booleano = std::get<bool>(var) ? 1 : 0;
    }
    else if (std::holds_alternative<std::string>(var)) {
        jp.tipo = JP_TIPO_STRING;
        const std::string& s = std::get<std::string>(var);
        jp.valor.texto = (char*)malloc(s.size() + 1);
        if (jp.valor.texto) {
            memcpy(jp.valor.texto, s.c_str(), s.size() + 1);
        }
    }
    else {
        jp.tipo = JP_TIPO_NULO;
    }
    
    return jp;
}

static std::vector<Var> jp_array_para_vector(JPValor* args, int numArgs) {
    std::vector<Var> result;
    result.reserve(numArgs);
    for (int i = 0; i < numArgs; i++) {
        result.push_back(jp_para_variant(args[i]));
    }
    return result;
}

// =============================================================================
// HELPERS
// =============================================================================
static int get_int(const Var& v) {
    if (auto* i = std::get_if<int>(&v)) return *i;
    if (auto* d = std::get_if<double>(&v)) return (int)*d;
    return 0;
}

static std::string get_str(const Var& v) {
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    if (auto* i = std::get_if<int>(&v)) return std::to_string(*i);
    if (auto* d = std::get_if<double>(&v)) return std::to_string(*d);
    if (auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false";
    return "";
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================
extern "C" {

    // =========================================================================
    // FUNÇÕES ORIGINAIS (para interpretador JPLang)
    // =========================================================================

    // Cria a janela
    JP_EXPORT Var ig_janela(const std::vector<Var>& args) {
        if (args.size() < 3) return 0;
        std::string titulo = get_str(args[0]);
        int largura = get_int(args[1]);
        int altura = get_int(args[2]);
        return criarJanela(titulo, largura, altura);
    }

    // Define cor de fundo da janela (RGB 0-255)
    JP_EXPORT Var ig_fundo(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return janelaFundo(r, g, b);
    }

    // Exibe um frame da janela
    JP_EXPORT Var ig_exibir(const std::vector<Var>& args) {
        exibirJanela();
        return true;
    }

    // Cria um botão
    JP_EXPORT Var ig_botao(const std::vector<Var>& args) {
        if (args.size() < 6) return 0;
        std::string texto = get_str(args[1]);
        float x = (float)get_int(args[2]);
        float y = (float)get_int(args[3]);
        float largura = (float)get_int(args[4]);
        float altura = (float)get_int(args[5]);
        return criarBotao(texto, x, y, largura, altura);
    }

    // Verifica se botão foi clicado
    JP_EXPORT Var ig_clicado(const std::vector<Var>& args) {
        if (args.size() < 1) return false;
        int id = get_int(args[0]);
        return botaoClicado(id);
    }

    // Define cor de fundo do botão
    JP_EXPORT Var ig_botao_cor_fundo(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return botaoCorFundo(id, r, g, b);
    }

    // Define cor da fonte do botão
    JP_EXPORT Var ig_botao_cor_fonte(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return botaoCorFonte(id, r, g, b);
    }

    // Define fonte do botão
    JP_EXPORT Var ig_botao_fonte(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        std::string fonte = get_str(args[1]);
        return botaoFonte(id, fonte);
    }

    // Cria uma etiqueta
    JP_EXPORT Var ig_etiqueta(const std::vector<Var>& args) {
        if (args.size() < 4) return 0;
        std::string texto = get_str(args[1]);
        float x = (float)get_int(args[2]);
        float y = (float)get_int(args[3]);
        return criarEtiqueta(texto, x, y);
    }

    // Define cor da etiqueta
    JP_EXPORT Var ig_etiqueta_cor(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return etiquetaCor(id, r, g, b);
    }

    // Define fonte da etiqueta
    JP_EXPORT Var ig_etiqueta_fonte(const std::vector<Var>& args) {
        if (args.size() < 3) return false;
        int id = get_int(args[0]);
        std::string fonte = get_str(args[1]);
        int tamanho = get_int(args[2]);
        return etiquetaFonte(id, fonte, tamanho);
    }

    // Atualiza o texto da etiqueta
    JP_EXPORT Var ig_etiqueta_texto(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        std::string texto = get_str(args[1]);
        return etiquetaTexto(id, texto);
    }

    // Cria um campo de input
    JP_EXPORT Var ig_input(const std::vector<Var>& args) {
        if (args.size() < 6) return 0;
        std::string placeholder = get_str(args[1]);
        float x = (float)get_int(args[2]);
        float y = (float)get_int(args[3]);
        float largura = (float)get_int(args[4]);
        float altura = (float)get_int(args[5]);
        return criarInput(placeholder, x, y, largura, altura);
    }

    // Obtém o valor do input
    JP_EXPORT Var ig_input_valor(const std::vector<Var>& args) {
        if (args.size() < 1) return std::string("");
        int id = get_int(args[0]);
        return inputValor(id);
    }

    // Define o valor do input
    JP_EXPORT Var ig_input_definir(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        std::string valor = get_str(args[1]);
        return inputDefinirValor(id, valor);
    }

    // Define cor de fundo do input
    JP_EXPORT Var ig_input_cor_fundo(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return inputCorFundo(id, r, g, b);
    }

    // Define cor da fonte do input
    JP_EXPORT Var ig_input_cor_fonte(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return inputCorFonte(id, r, g, b);
    }

    // Cria um combobox
    JP_EXPORT Var ig_combobox(const std::vector<Var>& args) {
        if (args.size() < 6) return 0;
        std::string label = get_str(args[1]);
        float x = (float)get_int(args[2]);
        float y = (float)get_int(args[3]);
        float largura = (float)get_int(args[4]);
        float altura = (float)get_int(args[5]);
        return criarCombobox(label, x, y, largura, altura);
    }

    // Adiciona um item ao combobox
    JP_EXPORT Var ig_combobox_adicionar(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        std::string texto = get_str(args[1]);
        return comboboxAdicionar(id, texto);
    }

    // Retorna o índice selecionado do combobox
    JP_EXPORT Var ig_combobox_selecionado(const std::vector<Var>& args) {
        if (args.size() < 1) return -1;
        int id = get_int(args[0]);
        return comboboxSelecionado(id);
    }

    // Retorna o texto do item selecionado
    JP_EXPORT Var ig_combobox_valor(const std::vector<Var>& args) {
        if (args.size() < 1) return std::string("");
        int id = get_int(args[0]);
        return comboboxValor(id);
    }

    // Define qual item está selecionado por índice
    JP_EXPORT Var ig_combobox_definir(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        int indice = get_int(args[1]);
        return comboboxDefinir(id, indice);
    }

    // Limpa todos os itens do combobox
    JP_EXPORT Var ig_combobox_limpar(const std::vector<Var>& args) {
        if (args.size() < 1) return false;
        int id = get_int(args[0]);
        return comboboxLimpar(id);
    }

    // Define cor de fundo do combobox
    JP_EXPORT Var ig_combobox_cor_fundo(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return comboboxCorFundo(id, r, g, b);
    }

    // Define cor da fonte do combobox
    JP_EXPORT Var ig_combobox_cor_fonte(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return comboboxCorFonte(id, r, g, b);
    }

    // Cria uma barra de progresso/gráfico
    JP_EXPORT Var ig_barra(const std::vector<Var>& args) {
        if (args.size() < 8) return 0;
        float x = (float)get_int(args[1]);
        float y = (float)get_int(args[2]);
        float largura = (float)get_int(args[3]);
        float altura = (float)get_int(args[4]);
        float valorMin = (float)get_int(args[5]);
        float valorMax = (float)get_int(args[6]);
        std::string orientacao = get_str(args[7]);
        return criarBarra(x, y, largura, altura, valorMin, valorMax, orientacao);
    }

    // Define o valor atual da barra
    JP_EXPORT Var ig_barra_valor(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        float valor = (float)get_int(args[1]);
        return barraValor(id, valor);
    }

    // Retorna o valor atual da barra
    JP_EXPORT Var ig_barra_valor_atual(const std::vector<Var>& args) {
        if (args.size() < 1) return 0;
        int id = get_int(args[0]);
        return (int)barraValorAtual(id);
    }

    // Define cor da barra preenchida
    JP_EXPORT Var ig_barra_cor(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return barraCor(id, r, g, b);
    }

    // Define cor do fundo da barra
    JP_EXPORT Var ig_barra_cor_fundo(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return barraCorFundo(id, r, g, b);
    }

    // Ativa barra de título customizada
    JP_EXPORT Var ig_barra_titulo(const std::vector<Var>& args) {
        return janelaBarraTitulo();
    }

    // Define cor de fundo da barra de título
    JP_EXPORT Var ig_barra_titulo_cor(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return janelaBarraTituloCor(r, g, b);
    }

    // Define cor do texto do título
    JP_EXPORT Var ig_barra_titulo_texto_cor(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return janelaBarraTituloTextoCor(r, g, b);
    }

    // Define cor dos botões da barra de título
    JP_EXPORT Var ig_barra_titulo_botoes_cor(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return janelaBarraTituloBotoesCor(r, g, b);
    }

    // =========================================================================
    // FUNÇÕES DO GAUGE (medidor circular com ponteiro)
    // =========================================================================

    // Cria um gauge (medidor semicircular)
    JP_EXPORT Var ig_gauge(const std::vector<Var>& args) {
        if (args.size() < 6) return 0;
        float x = (float)get_int(args[1]);
        float y = (float)get_int(args[2]);
        float raio = (float)get_int(args[3]);
        float valorMin = (float)get_int(args[4]);
        float valorMax = (float)get_int(args[5]);
        return criarGauge(x, y, raio, valorMin, valorMax);
    }

    // Define o valor atual do gauge (move o ponteiro)
    JP_EXPORT Var ig_gauge_valor(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        float valor = (float)get_int(args[1]);
        return gaugeValor(id, valor);
    }

    // Retorna o valor atual do gauge
    JP_EXPORT Var ig_gauge_valor_atual(const std::vector<Var>& args) {
        if (args.size() < 1) return 0;
        int id = get_int(args[0]);
        return (int)gaugeValorAtual(id);
    }

    // Define cor do arco preenchido
    JP_EXPORT Var ig_gauge_cor(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return gaugeCor(id, r, g, b);
    }

    // Define cor do fundo do gauge
    JP_EXPORT Var ig_gauge_cor_fundo(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return gaugeCorFundo(id, r, g, b);
    }

    // Define cor do ponteiro
    JP_EXPORT Var ig_gauge_cor_ponteiro(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return gaugeCorPonteiro(id, r, g, b);
    }

    // Define cor do centro do ponteiro
    JP_EXPORT Var ig_gauge_cor_centro(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        int r = get_int(args[1]);
        int g = get_int(args[2]);
        int b = get_int(args[3]);
        return gaugeCorCentro(id, r, g, b);
    }

    // Define espessura do arco
    JP_EXPORT Var ig_gauge_espessura(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        float espessura = (float)get_int(args[1]);
        return gaugeEspessura(id, espessura);
    }

    // Define ângulos do arco (em graus)
    JP_EXPORT Var ig_gauge_angulos(const std::vector<Var>& args) {
        if (args.size() < 3) return false;
        int id = get_int(args[0]);
        float inicio = (float)get_int(args[1]);
        float fim = (float)get_int(args[2]);
        return gaugeAngulos(id, inicio, fim);
    }

    // Define configurações do ponteiro
    JP_EXPORT Var ig_gauge_ponteiro(const std::vector<Var>& args) {
        if (args.size() < 4) return false;
        int id = get_int(args[0]);
        float comprimento = (float)get_int(args[1]) / 100.0f;
        float largura = (float)get_int(args[2]);
        float centroRaio = (float)get_int(args[3]);
        return gaugePonteiro(id, comprimento, largura, centroRaio);
    }

    // Ativa/desativa arco preenchido
    JP_EXPORT Var ig_gauge_mostrar_arco(const std::vector<Var>& args) {
        if (args.size() < 2) return false;
        int id = get_int(args[0]);
        bool mostrar = get_int(args[1]) != 0;
        return gaugeMostrarArco(id, mostrar);
    }

    // =========================================================================
    // WRAPPERS C (para executáveis compilados com TCC)
    // =========================================================================

    JP_EXPORT JPValor jp_ig_janela(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_janela(vargs));
    }

    JP_EXPORT JPValor jp_ig_fundo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_fundo(vargs));
    }

    JP_EXPORT JPValor jp_ig_exibir(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_exibir(vargs));
    }

    JP_EXPORT JPValor jp_ig_botao(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_botao(vargs));
    }

    JP_EXPORT JPValor jp_ig_clicado(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_clicado(vargs));
    }

    JP_EXPORT JPValor jp_ig_botao_cor_fundo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_botao_cor_fundo(vargs));
    }

    JP_EXPORT JPValor jp_ig_botao_cor_fonte(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_botao_cor_fonte(vargs));
    }

    JP_EXPORT JPValor jp_ig_botao_fonte(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_botao_fonte(vargs));
    }

    JP_EXPORT JPValor jp_ig_etiqueta(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_etiqueta(vargs));
    }

    JP_EXPORT JPValor jp_ig_etiqueta_cor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_etiqueta_cor(vargs));
    }

    JP_EXPORT JPValor jp_ig_etiqueta_fonte(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_etiqueta_fonte(vargs));
    }

    JP_EXPORT JPValor jp_ig_etiqueta_texto(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_etiqueta_texto(vargs));
    }

    JP_EXPORT JPValor jp_ig_input(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_input(vargs));
    }

    JP_EXPORT JPValor jp_ig_input_valor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_input_valor(vargs));
    }

    JP_EXPORT JPValor jp_ig_input_definir(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_input_definir(vargs));
    }

    JP_EXPORT JPValor jp_ig_input_cor_fundo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_input_cor_fundo(vargs));
    }

    JP_EXPORT JPValor jp_ig_input_cor_fonte(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_input_cor_fonte(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_adicionar(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_adicionar(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_selecionado(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_selecionado(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_valor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_valor(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_definir(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_definir(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_limpar(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_limpar(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_cor_fundo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_cor_fundo(vargs));
    }

    JP_EXPORT JPValor jp_ig_combobox_cor_fonte(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_combobox_cor_fonte(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_valor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_valor(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_valor_atual(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_valor_atual(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_cor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_cor(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_cor_fundo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_cor_fundo(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_titulo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_titulo(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_titulo_cor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_titulo_cor(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_titulo_texto_cor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_titulo_texto_cor(vargs));
    }

    JP_EXPORT JPValor jp_ig_barra_titulo_botoes_cor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_barra_titulo_botoes_cor(vargs));
    }

    // =========================================================================
    // WRAPPERS C DO GAUGE (para executáveis compilados com TCC)
    // =========================================================================

    JP_EXPORT JPValor jp_ig_gauge(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_valor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_valor(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_valor_atual(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_valor_atual(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_cor(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_cor(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_cor_fundo(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_cor_fundo(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_cor_ponteiro(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_cor_ponteiro(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_cor_centro(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_cor_centro(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_espessura(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_espessura(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_angulos(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_angulos(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_ponteiro(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_ponteiro(vargs));
    }

    JP_EXPORT JPValor jp_ig_gauge_mostrar_arco(JPValor* args, int numArgs) {
        std::vector<Var> vargs = jp_array_para_vector(args, numArgs);
        return variant_para_jp(ig_gauge_mostrar_arco(vargs));
    }

} // extern "C"