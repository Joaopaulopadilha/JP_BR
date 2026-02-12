// bibliotecas/r3dgame/r3dgame.cpp
// Engine 3D para JPLang com DirectX 11
// Versão com Câmeras, Objetos, Colisão, Sólidos e Gravidade

#include <vector>
#include <map>
#include <string>
#include <variant>
#include <memory>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

// ==========================================
// INCLUDES DA ENGINE (PASTA SRC)
// ==========================================
// janela.hpp inclui todos os outros headers necessários
#include "src/janela.hpp"

// ==========================================
// 1. TIPOS JPLANG
// ==========================================

struct Instancia;
using ValorVariant = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

struct Instancia {
    std::string nomeClasse;
    std::map<std::string, ValorVariant> propriedades;
};

// Tipos C (DLL)
typedef enum {
    JP_TIPO_NULO = 0, JP_TIPO_INT = 1, JP_TIPO_DOUBLE = 2, JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4, JP_TIPO_OBJETO = 5, JP_TIPO_LISTA = 6, JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro; double decimal; char* texto; int booleano;
        void* objeto; void* lista; void* ponteiro;
    } valor;
} JPValor;

// ==========================================
// 2. CONVERSORES
// ==========================================

static ValorVariant jp_para_variant(const JPValor& jp) {
    switch (jp.tipo) {
        case JP_TIPO_INT: return (int)jp.valor.inteiro;
        case JP_TIPO_DOUBLE: return jp.valor.decimal;
        case JP_TIPO_BOOL: return (bool)jp.valor.booleano;
        case JP_TIPO_STRING: return jp.valor.texto ? std::string(jp.valor.texto) : std::string("");
        default: return 0;
    }
}

static JPValor variant_para_jp(const ValorVariant& var) {
    JPValor jp; memset(&jp, 0, sizeof(jp));
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
        std::string s = std::get<std::string>(var);
        jp.valor.texto = (char*)malloc(s.length() + 1);
        if (jp.valor.texto) strcpy(jp.valor.texto, s.c_str());
    }
    else { 
        jp.tipo = JP_TIPO_NULO; 
    }
    return jp;
}

static std::vector<ValorVariant> jp_array_para_vector(JPValor* args, int numArgs) {
    std::vector<ValorVariant> result;
    for (int i = 0; i < numArgs; i++) result.push_back(jp_para_variant(args[i]));
    return result;
}

// ==========================================
// 3. HELPERS
// ==========================================

static double get_double(const ValorVariant& v) {
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<int>(v)) return (double)std::get<int>(v);
    return 0.0;
}

static int get_int(const ValorVariant& v) {
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    if (std::holds_alternative<double>(v)) return (int)std::get<double>(v);
    return 0;
}

static std::string get_str(const ValorVariant& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    return "";
}

// ==========================================
// 4. IMPLEMENTAÇÕES (JANELA)
// ==========================================

static ValorVariant janela_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return 0;
    return janela_criar(get_str(args[0]).c_str(), get_int(args[1]), get_int(args[2]));
}

static ValorVariant janela_cor_fundo_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return false;
    janela_cor_fundo(jogo, get_int(args[1]), get_int(args[2]), get_int(args[3]));
    return true;
}

static ValorVariant exibir_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return false;
    return janela_renderizar(jogo);
}

static ValorVariant tecla_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    return janela_tecla(get_str(args[0]));
}

// ==========================================
// 5. IMPLEMENTAÇÕES (CÂMERA)
// ==========================================

static ValorVariant camera_fixa_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 7) return false;
    camera_fixa(get_int(args[0]), 
        (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]),
        (float)get_double(args[4]), (float)get_double(args[5]), (float)get_double(args[6]));
    return true;
}

static ValorVariant camera_orbital_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 6) return false;
    camera_orbital(get_int(args[0]),
        (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]),
        (float)get_double(args[4]), (float)get_double(args[5]));
    return true;
}

static ValorVariant camera_player_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return false;
    camera_player(get_int(args[0]), get_int(args[1]),
        (float)get_double(args[2]), (float)get_double(args[3]), (float)get_double(args[4]));
    return true;
}

static ValorVariant camera_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    camera_fixa(get_int(args[0]), (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]), 0, 0, 0);
    return true;
}

// ==========================================
// 6. IMPLEMENTAÇÕES (PLAYER)
// ==========================================

static ValorVariant player_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return 0;
    return player_criar((float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant player_tamanho_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return player_tamanho(get_int(args[0]), (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant player_cores_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 19) return false;
    return player_cores(get_int(args[0]),
        get_int(args[1]),  get_int(args[2]),  get_int(args[3]),
        get_int(args[4]),  get_int(args[5]),  get_int(args[6]),
        get_int(args[7]),  get_int(args[8]),  get_int(args[9]),
        get_int(args[10]), get_int(args[11]), get_int(args[12]),
        get_int(args[13]), get_int(args[14]), get_int(args[15]),
        get_int(args[16]), get_int(args[17]), get_int(args[18])
    );
}

static ValorVariant player_posicao_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return player_posicao(get_int(args[0]), (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant player_rotacionar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return player_rotacionar(get_int(args[0]), (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

// player_mover agora só aceita 4 teclas (XZ)
static ValorVariant player_mover_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return false;
    int id = get_int(args[0]);
    std::string cima = get_str(args[1]);
    std::string esquerda = get_str(args[2]);
    std::string baixo = get_str(args[3]);
    std::string direita = get_str(args[4]);
    return input_player_mover(id, cima, esquerda, baixo, direita);
}

// player_velocidade agora só aceita 2 valores (XZ)
static ValorVariant player_velocidade_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return false;
    return input_player_velocidade(get_int(args[0]), (float)get_double(args[1]), (float)get_double(args[2]));
}

static ValorVariant player_colisao_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return colisao_player_definir(get_int(args[0]), 
        (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant player_solido_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    int id = get_int(args[0]);
    return colisao_player_solido(id, true);
}

// ==========================================
// 7. IMPLEMENTAÇÕES (GRAVIDADE)
// ==========================================

// player_gravidade(player_id, forca)
static ValorVariant player_gravidade_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    return gravidade_player_definir(get_int(args[0]), (float)get_double(args[1]));
}

// player_pular(player_id, forca)
static ValorVariant player_pular_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    return gravidade_player_pular(get_int(args[0]), (float)get_double(args[1]));
}

// player_no_chao(player_id) -> verdadeiro/falso
static ValorVariant player_no_chao_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    int id = get_int(args[0]);
    Player* p = player_get(id);
    if (!p) return false;
    return gravidade_player_no_chao(id, p->x, p->y, p->z);
}

// ==========================================
// 8. IMPLEMENTAÇÕES (OBJETOS)
// ==========================================

static ValorVariant objeto_criar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return 0;
    return objeto_criar((float)get_double(args[0]), (float)get_double(args[1]), (float)get_double(args[2]));
}

static ValorVariant objeto_escala_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return objeto_escala(get_int(args[0]), (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant objeto_cor_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return objeto_cor(get_int(args[0]), get_int(args[1]), get_int(args[2]), get_int(args[3]));
}

static ValorVariant objeto_colisao_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    return colisao_objeto_definir(get_int(args[0]), 
        (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant objeto_solido_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    int id = get_int(args[0]);
    return colisao_objeto_solido(id, true);
}

// ==========================================
// 9. IMPLEMENTAÇÕES (COLISÃO)
// ==========================================

static ValorVariant colidiu_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    
    int id1 = get_int(args[0]);
    int id2 = get_int(args[1]);
    
    Player* player = player_get(id1);
    Objeto* objeto = objeto_get(id2);
    
    if (player && objeto) {
        return colisao_player_objeto(id1, player->x, player->y, player->z,
                                      id2, objeto->x, objeto->y, objeto->z);
    }
    
    Player* player2 = player_get(id2);
    if (player && player2) {
        return colisao_player_player(id1, player->x, player->y, player->z,
                                      id2, player2->x, player2->y, player2->z);
    }
    
    Objeto* objeto1 = objeto_get(id1);
    Objeto* objeto2 = objeto_get(id2);
    if (objeto1 && objeto2) {
        return colisao_objeto_objeto(id1, objeto1->x, objeto1->y, objeto1->z,
                                      id2, objeto2->x, objeto2->y, objeto2->z);
    }
    
    return false;
}

// ==========================================
// 10. IMPLEMENTAÇÕES LEGADO
// ==========================================

static ValorVariant cubo_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return 0;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return 0;
    return janela_criar_cubo(jogo, (float)get_double(args[1]), (float)get_double(args[2]), (float)get_double(args[3]));
}

static ValorVariant mover_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return false;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return false;
    return janela_mover_cubo(jogo, get_int(args[1]), (float)get_double(args[2]), (float)get_double(args[3]), (float)get_double(args[4]));
}

static ValorVariant rotacionar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return false;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return false;
    return janela_rotacionar_cubo(jogo, get_int(args[1]), (float)get_double(args[2]), (float)get_double(args[3]), (float)get_double(args[4]));
}

static ValorVariant get_x_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0.0;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return 0.0;
    return janela_get_cubo_x(jogo, get_int(args[1]));
}

static ValorVariant get_z_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0.0;
    Jogo* jogo = janela_get(get_int(args[0]));
    if (!jogo) return 0.0;
    return janela_get_cubo_z(jogo, get_int(args[1]));
}

// ==========================================
// 11. EXPORTS
// ==========================================

extern "C" {
    // === C++ (VM) ===
    __declspec(dllexport) ValorVariant r3dgame_janela(const std::vector<ValorVariant>& args) { return janela_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_janela_cor_fundo(const std::vector<ValorVariant>& args) { return janela_cor_fundo_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_exibir(const std::vector<ValorVariant>& args) { return exibir_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_tecla(const std::vector<ValorVariant>& args) { return tecla_impl(args); }
    
    // Câmera
    __declspec(dllexport) ValorVariant r3dgame_camera(const std::vector<ValorVariant>& args) { return camera_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_camera_fixa(const std::vector<ValorVariant>& args) { return camera_fixa_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_camera_orbital(const std::vector<ValorVariant>& args) { return camera_orbital_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_camera_player(const std::vector<ValorVariant>& args) { return camera_player_impl(args); }
    
    // Player
    __declspec(dllexport) ValorVariant r3dgame_player(const std::vector<ValorVariant>& args) { return player_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_tamanho(const std::vector<ValorVariant>& args) { return player_tamanho_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_cores(const std::vector<ValorVariant>& args) { return player_cores_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_posicao(const std::vector<ValorVariant>& args) { return player_posicao_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_rotacionar(const std::vector<ValorVariant>& args) { return player_rotacionar_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_mover(const std::vector<ValorVariant>& args) { return player_mover_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_velocidade(const std::vector<ValorVariant>& args) { return player_velocidade_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_colisao(const std::vector<ValorVariant>& args) { return player_colisao_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_solido(const std::vector<ValorVariant>& args) { return player_solido_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_gravidade(const std::vector<ValorVariant>& args) { return player_gravidade_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_pular(const std::vector<ValorVariant>& args) { return player_pular_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_player_no_chao(const std::vector<ValorVariant>& args) { return player_no_chao_impl(args); }
    
    // Objetos
    __declspec(dllexport) ValorVariant r3dgame_objeto(const std::vector<ValorVariant>& args) { return objeto_criar_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_objeto_escala(const std::vector<ValorVariant>& args) { return objeto_escala_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_objeto_cor(const std::vector<ValorVariant>& args) { return objeto_cor_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_objeto_colisao(const std::vector<ValorVariant>& args) { return objeto_colisao_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_objeto_solido(const std::vector<ValorVariant>& args) { return objeto_solido_impl(args); }
    
    // Colisão
    __declspec(dllexport) ValorVariant r3dgame_colidiu(const std::vector<ValorVariant>& args) { return colidiu_impl(args); }

    // Legado
    __declspec(dllexport) ValorVariant r3dgame_cubo(const std::vector<ValorVariant>& args) { return cubo_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_mover(const std::vector<ValorVariant>& args) { return mover_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_rotacionar(const std::vector<ValorVariant>& args) { return rotacionar_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_get_x(const std::vector<ValorVariant>& args) { return get_x_impl(args); }
    __declspec(dllexport) ValorVariant r3dgame_get_z(const std::vector<ValorVariant>& args) { return get_z_impl(args); }

    // === C (TCC) ===
    __declspec(dllexport) JPValor jp_r3dgame_janela(JPValor* args, int n) { return variant_para_jp(janela_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_janela_cor_fundo(JPValor* args, int n) { return variant_para_jp(janela_cor_fundo_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_exibir(JPValor* args, int n) { return variant_para_jp(exibir_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_tecla(JPValor* args, int n) { return variant_para_jp(tecla_impl(jp_array_para_vector(args, n))); }
    
    __declspec(dllexport) JPValor jp_r3dgame_camera(JPValor* args, int n) { return variant_para_jp(camera_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_camera_fixa(JPValor* args, int n) { return variant_para_jp(camera_fixa_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_camera_orbital(JPValor* args, int n) { return variant_para_jp(camera_orbital_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_camera_player(JPValor* args, int n) { return variant_para_jp(camera_player_impl(jp_array_para_vector(args, n))); }
    
    __declspec(dllexport) JPValor jp_r3dgame_player(JPValor* args, int n) { return variant_para_jp(player_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_tamanho(JPValor* args, int n) { return variant_para_jp(player_tamanho_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_cores(JPValor* args, int n) { return variant_para_jp(player_cores_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_posicao(JPValor* args, int n) { return variant_para_jp(player_posicao_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_rotacionar(JPValor* args, int n) { return variant_para_jp(player_rotacionar_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_mover(JPValor* args, int n) { return variant_para_jp(player_mover_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_velocidade(JPValor* args, int n) { return variant_para_jp(player_velocidade_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_colisao(JPValor* args, int n) { return variant_para_jp(player_colisao_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_solido(JPValor* args, int n) { return variant_para_jp(player_solido_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_gravidade(JPValor* args, int n) { return variant_para_jp(player_gravidade_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_pular(JPValor* args, int n) { return variant_para_jp(player_pular_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_player_no_chao(JPValor* args, int n) { return variant_para_jp(player_no_chao_impl(jp_array_para_vector(args, n))); }
    
    __declspec(dllexport) JPValor jp_r3dgame_objeto(JPValor* args, int n) { return variant_para_jp(objeto_criar_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_objeto_escala(JPValor* args, int n) { return variant_para_jp(objeto_escala_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_objeto_cor(JPValor* args, int n) { return variant_para_jp(objeto_cor_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_objeto_colisao(JPValor* args, int n) { return variant_para_jp(objeto_colisao_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_objeto_solido(JPValor* args, int n) { return variant_para_jp(objeto_solido_impl(jp_array_para_vector(args, n))); }
    
    __declspec(dllexport) JPValor jp_r3dgame_colidiu(JPValor* args, int n) { return variant_para_jp(colidiu_impl(jp_array_para_vector(args, n))); }

    __declspec(dllexport) JPValor jp_r3dgame_cubo(JPValor* args, int n) { return variant_para_jp(cubo_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_mover(JPValor* args, int n) { return variant_para_jp(mover_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_rotacionar(JPValor* args, int n) { return variant_para_jp(rotacionar_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_get_x(JPValor* args, int n) { return variant_para_jp(get_x_impl(jp_array_para_vector(args, n))); }
    __declspec(dllexport) JPValor jp_r3dgame_get_z(JPValor* args, int n) { return variant_para_jp(get_z_impl(jp_array_para_vector(args, n))); }
}