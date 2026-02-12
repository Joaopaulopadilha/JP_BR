// cvn.cpp
// Biblioteca de visão computacional para JPLang (Portável Windows/Linux)

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// Plataforma
#ifdef _WIN32
    #include <windows.h>
    #define EXPORTAR extern "C" __declspec(dllexport)
    #define DORMIR(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define EXPORTAR extern "C" __attribute__((visibility("default")))
    #define DORMIR(ms) usleep((ms) * 1000)
#endif

#include "imagem.hpp"
#include "janela.hpp"
#include "filtros.hpp"
#include "captura.hpp"
#include "camera.hpp"
#include "fonte.hpp"

// Forward declaration
struct Instancia;
using Var = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

// === TIPOS ABI-COMPATÍVEIS (C puro) ===
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

// === HELPERS PARA JPValor ===
static int jp_get_int(const JPValor& v) {
    switch (v.tipo) {
        case JP_TIPO_INT: return (int)v.valor.inteiro;
        case JP_TIPO_DOUBLE: return (int)v.valor.decimal;
        default: return 0;
    }
}

static float jp_get_float(const JPValor& v) {
    switch (v.tipo) {
        case JP_TIPO_DOUBLE: return (float)v.valor.decimal;
        case JP_TIPO_INT: return (float)v.valor.inteiro;
        default: return 0.0f;
    }
}

static std::string jp_get_str(const JPValor& v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto) return std::string(v.valor.texto);
    if (v.tipo == JP_TIPO_INT) return std::to_string(v.valor.inteiro);
    if (v.tipo == JP_TIPO_DOUBLE) return std::to_string(v.valor.decimal);
    if (v.tipo == JP_TIPO_BOOL) return v.valor.booleano ? "true" : "false";
    return "";
}

// === HELPERS PARA CRIAR JPValor ===
static JPValor jp_int(int64_t i) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_INT;
    v.valor.inteiro = i;
    return v;
}

static JPValor jp_double(double d) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_DOUBLE;
    v.valor.decimal = d;
    return v;
}

static JPValor jp_bool(bool b) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_BOOL;
    v.valor.booleano = b ? 1 : 0;
    return v;
}

static JPValor jp_string(const std::string& s) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_STRING;
    v.valor.texto = (char*)malloc(s.size() + 1);
    if (v.valor.texto) memcpy(v.valor.texto, s.c_str(), s.size() + 1);
    return v;
}

// === APLICA FILTROS EM IMAGEM ===
static unsigned char* aplicar_filtros(unsigned char* dados, int& largura, int& altura, int canais, 
                            const std::vector<cvn::ConfigFiltro>& filtros) {
    unsigned char* resultado = dados;
    
    for (const auto& f : filtros) {
        switch (f.tipo) {
            case cvn::TipoFiltro::CINZA:
                cvn::Filtros::cinza(resultado, largura, altura, canais);
                break;
            case cvn::TipoFiltro::INVERTER:
                cvn::Filtros::inverter(resultado, largura, altura, canais);
                break;
            case cvn::TipoFiltro::BRILHO:
                cvn::Filtros::brilho(resultado, largura, altura, canais, (int)f.valor1);
                break;
            case cvn::TipoFiltro::CONTRASTE:
                cvn::Filtros::contraste(resultado, largura, altura, canais, f.valor1);
                break;
            case cvn::TipoFiltro::LIMIAR:
                cvn::Filtros::limiar(resultado, largura, altura, canais, (int)f.valor1);
                break;
            case cvn::TipoFiltro::BLUR:
                cvn::Filtros::blur(resultado, largura, altura, canais, (int)f.valor1);
                break;
            case cvn::TipoFiltro::BORDAS:
                cvn::Filtros::bordas(resultado, largura, altura, canais);
                break;
            case cvn::TipoFiltro::SEPIA:
                cvn::Filtros::sepia(resultado, largura, altura, canais);
                break;
            case cvn::TipoFiltro::SATURACAO:
                cvn::Filtros::saturacao(resultado, largura, altura, canais, f.valor1);
                break;
            case cvn::TipoFiltro::FLIP_H:
                cvn::Filtros::flip_h(resultado, largura, altura, canais);
                break;
            case cvn::TipoFiltro::FLIP_V:
                cvn::Filtros::flip_v(resultado, largura, altura, canais);
                break;
            case cvn::TipoFiltro::REDIMENSIONAR: {
                int nova_largura = (int)f.valor1;
                int nova_altura = (int)f.valor2;
                unsigned char* novo = (unsigned char*)malloc(nova_largura * nova_altura * canais);
                if (novo) {
                    stbir_resize_uint8_linear(
                        resultado, largura, altura, largura * canais,
                        novo, nova_largura, nova_altura, nova_largura * canais,
                        STBIR_RGBA
                    );
                    if (resultado != dados) free(resultado);
                    resultado = novo;
                    largura = nova_largura;
                    altura = nova_altura;
                }
                break;
            }
            default:
                break;
        }
    }
    return resultado;
}

// === CAPTURA FRAME DA FONTE ===
static unsigned char* capturar_frame(cvn::Fonte* fonte, int& largura, int& altura) {
    if (!fonte || !fonte->valida) return nullptr;
    
    unsigned char* dados = nullptr;
    
    switch (fonte->tipo) {
        case cvn::TipoFonte::CAMERA: {
            dados = cvn::GerenciadorCameras::instancia().ler(fonte->indice, largura, altura);
            break;
        }
        case cvn::TipoFonte::TELA: {
            int mx, my, ml, ma;
            cvn::GerenciadorFontes::instancia().obter_monitor(fonte->indice, mx, my, ml, ma);
            dados = cvn::Captura::regiao(mx, my, ml, ma);
            largura = ml;
            altura = ma;
            break;
        }
        case cvn::TipoFonte::REGIAO: {
            int mx, my, ml, ma;
            cvn::GerenciadorFontes::instancia().obter_monitor(fonte->indice, mx, my, ml, ma);
            dados = cvn::Captura::regiao(mx + fonte->x, my + fonte->y, fonte->largura, fonte->altura);
            largura = fonte->largura;
            altura = fonte->altura;
            break;
        }
        default:
            return nullptr;
    }
    
    // Aplica filtros se houver
    if (dados && !fonte->filtros.empty()) {
        unsigned char* resultado = aplicar_filtros(dados, largura, altura, 4, fonte->filtros);
        if (resultado != dados) {
            free(dados);
            dados = resultado;
        }
    }
    
    return dados;
}

// === FUNÇÕES AUXILIARES ===
static int clonar_imagem(int id) {
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(id);
    if (!img || !img->valida) return -1;
    return cvn::GerenciadorImagens::instancia().criar(img->dados, img->largura, img->altura, img->canais);
}

// =============================================================================
// === EXPORTS COM INTERFACE JPValor ===
// =============================================================================

// === BÁSICOS ===

EXPORTAR JPValor jp_cvn_ler(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    std::string caminho = jp_get_str(args[0]);
    return jp_int(cvn::GerenciadorImagens::instancia().carregar(caminho));
}

EXPORTAR JPValor jp_cvn_existe(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    std::string caminho = jp_get_str(args[0]);
    return jp_bool(cvn::GerenciadorImagens::instancia().existe(caminho));
}

EXPORTAR JPValor jp_cvn_existe_imagem(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    std::string caminho = jp_get_str(args[0]);
    return jp_bool(cvn::GerenciadorImagens::instancia().existe_imagem(caminho));
}

EXPORTAR JPValor jp_cvn_tamanho_arquivo(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    std::string caminho = jp_get_str(args[0]);
    return jp_int(cvn::GerenciadorImagens::instancia().tamanho_arquivo(caminho));
}

EXPORTAR JPValor jp_cvn_tamanho(JPValor* args, int n) {
    if (n < 1) return jp_string("0,0");
    int img_id = jp_get_int(args[0]);
    return jp_string(cvn::GerenciadorImagens::instancia().tamanho(img_id));
}

EXPORTAR JPValor jp_cvn_redimensionar(JPValor* args, int n) {
    if (n < 3) return jp_int(-1);
    int img_id = jp_get_int(args[0]);
    int largura = jp_get_int(args[1]);
    int altura = jp_get_int(args[2]);
    return jp_int(cvn::GerenciadorImagens::instancia().redimensionar(img_id, largura, altura));
}

EXPORTAR JPValor jp_cvn_salvar(JPValor* args, int n) {
    if (n < 2) return jp_bool(false);
    int img_id = jp_get_int(args[0]);
    std::string caminho = jp_get_str(args[1]);
    return jp_bool(cvn::GerenciadorImagens::instancia().salvar(img_id, caminho));
}

EXPORTAR JPValor jp_cvn_exibir(JPValor* args, int n) {
    if (n < 2) return jp_bool(false);
    std::string titulo = jp_get_str(args[0]);
    int img_id = jp_get_int(args[1]);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(img_id);
    if (!img || !img->valida) return jp_bool(false);
    return jp_bool(cvn::GerenciadorJanelas::instancia().atualizar(titulo, img->dados, img->largura, img->altura));
}

EXPORTAR JPValor jp_cvn_esperar(JPValor* args, int n) {
    int ms = 0;
    if (n > 0) ms = jp_get_int(args[0]);
    return jp_int(cvn::GerenciadorJanelas::instancia().esperar(ms));
}

EXPORTAR JPValor jp_cvn_liberar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int img_id = jp_get_int(args[0]);
    cvn::GerenciadorImagens::instancia().liberar(img_id);
    return jp_bool(true);
}

EXPORTAR JPValor jp_cvn_fechar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    std::string titulo = jp_get_str(args[0]);
    return jp_bool(cvn::GerenciadorJanelas::instancia().fechar(titulo));
}

EXPORTAR JPValor jp_cvn_fechar_todas(JPValor* args, int n) {
    cvn::GerenciadorJanelas::instancia().fechar_todas();
    return jp_bool(true);
}

// === FONTES ===

EXPORTAR JPValor jp_cvn_listar_telas(JPValor* args, int n) {
    return jp_string(cvn::GerenciadorFontes::instancia().listar_telas());
}

EXPORTAR JPValor jp_cvn_camera(JPValor* args, int n) {
    int indice = 0;
    if (n > 0) indice = jp_get_int(args[0]);
    
    int cam_id = cvn::GerenciadorCameras::instancia().abrir(indice);
    if (cam_id < 0) return jp_int(-1);
    
    cvn::Fonte fonte = cvn::Fonte::criar_camera(cam_id);
    return jp_int(cvn::GerenciadorFontes::instancia().criar(fonte));
}

EXPORTAR JPValor jp_cvn_tela(JPValor* args, int n) {
    int indice = 0;
    if (n > 0) indice = jp_get_int(args[0]);
    
    cvn::Fonte fonte = cvn::Fonte::criar_tela(indice);
    return jp_int(cvn::GerenciadorFontes::instancia().criar(fonte));
}

EXPORTAR JPValor jp_cvn_regiao(JPValor* args, int n) {
    if (n < 5) return jp_int(-1);
    int monitor = jp_get_int(args[0]);
    int x = jp_get_int(args[1]);
    int y = jp_get_int(args[2]);
    int largura = jp_get_int(args[3]);
    int altura = jp_get_int(args[4]);
    
    cvn::Fonte fonte = cvn::Fonte::criar_regiao(monitor, x, y, largura, altura);
    return jp_int(cvn::GerenciadorFontes::instancia().criar(fonte));
}

EXPORTAR JPValor jp_cvn_listar_cameras(JPValor* args, int n) {
    return jp_string(cvn::GerenciadorCameras::instancia().listar());
}

EXPORTAR JPValor jp_cvn_reproduzir(JPValor* args, int n) {
    if (n < 3) return jp_bool(false);
    
    std::string titulo = jp_get_str(args[0]);
    int fonte_id = jp_get_int(args[1]);
    int ms = jp_get_int(args[2]);
    
    cvn::Fonte* fonte = cvn::GerenciadorFontes::instancia().obter(fonte_id);
    if (!fonte || !fonte->valida) return jp_bool(false);
    
    int tentativas = 0;
    int largura = 0, altura = 0;
    unsigned char* dados = nullptr;
    
    while (!dados && tentativas < 100) {
        dados = capturar_frame(fonte, largura, altura);
        if (!dados) {
            DORMIR(50);
            tentativas++;
        }
    }
    
    if (!dados) return jp_bool(false);
    
    cvn::GerenciadorJanelas::instancia().atualizar(titulo, dados, largura, altura);
    free(dados);
    
    while (true) {
        if (!cvn::GerenciadorJanelas::instancia().processar_mensagens(titulo)) {
            break;
        }
        
        dados = capturar_frame(fonte, largura, altura);
        if (dados) {
            cvn::GerenciadorJanelas::instancia().atualizar(titulo, dados, largura, altura);
            free(dados);
        }
        
        DORMIR(ms);
    }
    
    if (fonte->tipo == cvn::TipoFonte::CAMERA) {
        cvn::GerenciadorCameras::instancia().fechar(fonte->indice);
    }
    
    return jp_bool(true);
}

EXPORTAR JPValor jp_cvn_camera_fechar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int fonte_id = jp_get_int(args[0]);
    
    cvn::Fonte* fonte = cvn::GerenciadorFontes::instancia().obter(fonte_id);
    if (fonte && fonte->tipo == cvn::TipoFonte::CAMERA) {
        cvn::GerenciadorCameras::instancia().fechar(fonte->indice);
    }
    cvn::GerenciadorFontes::instancia().liberar(fonte_id);
    return jp_bool(true);
}

// === FILTROS EM FONTES ===

EXPORTAR JPValor jp_cvn_fonte_cinza(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::CINZA));
}

EXPORTAR JPValor jp_cvn_fonte_inverter(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::INVERTER));
}

EXPORTAR JPValor jp_cvn_fonte_brilho(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::BRILHO, jp_get_float(args[1])));
}

EXPORTAR JPValor jp_cvn_fonte_contraste(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::CONTRASTE, jp_get_float(args[1])));
}

EXPORTAR JPValor jp_cvn_fonte_blur(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::BLUR, jp_get_float(args[1])));
}

EXPORTAR JPValor jp_cvn_fonte_bordas(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::BORDAS));
}

EXPORTAR JPValor jp_cvn_fonte_sepia(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::SEPIA));
}

EXPORTAR JPValor jp_cvn_fonte_saturacao(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::SATURACAO, jp_get_float(args[1])));
}

EXPORTAR JPValor jp_cvn_fonte_flip_h(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::FLIP_H));
}

EXPORTAR JPValor jp_cvn_fonte_flip_v(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::FLIP_V));
}

EXPORTAR JPValor jp_cvn_fonte_redimensionar(JPValor* args, int n) {
    if (n < 3) return jp_int(-1);
    return jp_int(cvn::GerenciadorFontes::instancia().clonar_com_filtro(jp_get_int(args[0]), cvn::TipoFiltro::REDIMENSIONAR, jp_get_float(args[1]), jp_get_float(args[2])));
}

// === FILTROS EM IMAGENS ===

EXPORTAR JPValor jp_cvn_cinza(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::cinza(img->dados, img->largura, img->altura, img->canais);
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_inverter(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::inverter(img->dados, img->largura, img->altura, img->canais);
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_brilho(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::brilho(img->dados, img->largura, img->altura, img->canais, jp_get_int(args[1]));
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_contraste(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::contraste(img->dados, img->largura, img->altura, img->canais, jp_get_float(args[1]));
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_limiar(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::limiar(img->dados, img->largura, img->altura, img->canais, jp_get_int(args[1]));
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_blur(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::blur(img->dados, img->largura, img->altura, img->canais, jp_get_int(args[1]));
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_bordas(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::bordas(img->dados, img->largura, img->altura, img->canais);
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_sepia(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::sepia(img->dados, img->largura, img->altura, img->canais);
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_saturacao(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::saturacao(img->dados, img->largura, img->altura, img->canais, jp_get_float(args[1]));
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_flip_h(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::flip_h(img->dados, img->largura, img->altura, img->canais);
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_flip_v(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int novo_id = clonar_imagem(jp_get_int(args[0]));
    if (novo_id == -1) return jp_int(-1);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(novo_id);
    cvn::Filtros::flip_v(img->dados, img->largura, img->altura, img->canais);
    return jp_int(novo_id);
}

EXPORTAR JPValor jp_cvn_rotacionar(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int img_id = jp_get_int(args[0]);
    int graus = jp_get_int(args[1]);
    cvn::Imagem* img = cvn::GerenciadorImagens::instancia().obter(img_id);
    if (!img || !img->valida) return jp_int(-1);
    int largura = img->largura;
    int altura = img->altura;
    unsigned char* dados_novos = cvn::Filtros::rotacionar(img->dados, largura, altura, img->canais, graus);
    if (!dados_novos) return jp_int(-1);
    int novo_id = cvn::GerenciadorImagens::instancia().criar(dados_novos, largura, altura, img->canais);
    free(dados_novos);
    return jp_int(novo_id);
}

// === CAPTURA LEGADA ===

EXPORTAR JPValor jp_cvn_capturar_tela(JPValor* args, int n) {
    int largura, altura;
    unsigned char* dados = cvn::Captura::tela(largura, altura);
    if (!dados) return jp_int(-1);
    int id = cvn::GerenciadorImagens::instancia().criar(dados, largura, altura, 4);
    free(dados);
    return jp_int(id);
}

EXPORTAR JPValor jp_cvn_capturar_regiao(JPValor* args, int n) {
    if (n < 4) return jp_int(-1);
    int x = jp_get_int(args[0]);
    int y = jp_get_int(args[1]);
    int largura = jp_get_int(args[2]);
    int altura = jp_get_int(args[3]);
    unsigned char* dados = cvn::Captura::regiao(x, y, largura, altura);
    if (!dados) return jp_int(-1);
    int id = cvn::GerenciadorImagens::instancia().criar(dados, largura, altura, 4);
    free(dados);
    return jp_int(id);
}

EXPORTAR JPValor jp_cvn_capturar_janela(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    std::string titulo = jp_get_str(args[0]);
    int largura, altura;
    unsigned char* dados = cvn::Captura::janela(titulo, largura, altura);
    if (!dados) return jp_int(-1);
    int id = cvn::GerenciadorImagens::instancia().criar(dados, largura, altura, 4);
    free(dados);
    return jp_int(id);
}