// jpgame.cpp
// Biblioteca de criação de janelas, jogos, players, sprites e colisões para JPLang

#include <windows.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include "janela.hpp"
#include "texturas.hpp"
#include "player.hpp"
#include "sprites.hpp"
#include "colisoes.hpp"

// === TIPOS JPLANG ===
struct Instancia;
using ValorVariant = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

struct Instancia {
    std::string nomeClasse;
    std::map<std::string, ValorVariant> propriedades;
};

// Tipos C puros para comunicação com TCC
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

// === CONVERSORES ===

// Converte JPValor (TCC) para ValorVariant (C++)
static ValorVariant jp_para_variant(const JPValor& jp) {
    switch (jp.tipo) {
        case JP_TIPO_INT:    return (int)jp.valor.inteiro;
        case JP_TIPO_DOUBLE: return jp.valor.decimal;
        case JP_TIPO_BOOL:   return (bool)jp.valor.booleano;
        case JP_TIPO_STRING: return jp.valor.texto ? std::string(jp.valor.texto) : std::string("");
        default:             return std::string("");
    }
}

// Converte ValorVariant (C++) para JPValor (TCC)
static JPValor variant_para_jp(const ValorVariant& var) {
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

// Converte array de JPValor para vector de ValorVariant
static std::vector<ValorVariant> jp_array_para_vector(JPValor* args, int numArgs) {
    std::vector<ValorVariant> result;
    result.reserve(numArgs);
    for (int i = 0; i < numArgs; i++) {
        result.push_back(jp_para_variant(args[i]));
    }
    return result;
}

// === HELPERS ===

static int get_int(const ValorVariant& v) {
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    if (std::holds_alternative<double>(v)) return (int)std::get<double>(v);
    return 0;
}

static double get_double(const ValorVariant& v) {
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<int>(v)) return (double)std::get<int>(v);
    return 0.0;
}

static std::string get_str(const ValorVariant& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
    return "";
}

// === IMPLEMENTAÇÕES - JANELA ===

// Cria e inicia janela
// jpgame.janela("titulo", largura, altura) -> retorna 1 se sucesso
static ValorVariant jpgame_janela_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return 0;
    
    std::string titulo = get_str(args[0]);
    int largura = get_int(args[1]);
    int altura = get_int(args[2]);
    
    return criarJanela(titulo, largura, altura);
}

// Define cor de fundo da janela (RGB 0-255)
// jpgame.janela_cor_fundo(janela, r, g, b)
static ValorVariant jpgame_janela_cor_fundo_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 4) return false;
    
    // args[0] é o ID da janela (ignorado por enquanto, só temos uma)
    int r = get_int(args[1]);
    int g = get_int(args[2]);
    int b = get_int(args[3]);
    
    return janelaCorFundo(r, g, b);
}

// Define imagem de fundo da janela
// jpgame.janela_imagem_fundo(janela, "caminho/imagem.png")
static ValorVariant jpgame_janela_imagem_fundo_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    
    // args[0] é o ID da janela (ignorado por enquanto, só temos uma)
    std::string caminho = get_str(args[1]);
    
    return janelaImagemFundo(caminho);
}

// Processa eventos e mantém a janela rodando
// jpgame.rodar(janela) -> retorna true se ainda está ativa
static ValorVariant jpgame_rodar_impl(const std::vector<ValorVariant>& args) {
    return janelaRodando();
}

// Fecha a janela
// jpgame.fechar(janela)
static ValorVariant jpgame_fechar_impl(const std::vector<ValorVariant>& args) {
    cleanupPlayers();
    cleanupSprites();
    cleanupTexturas();
    return fecharJanela();
}

// Verifica se uma tecla está pressionada
// jpgame.tecla_pressionada("w") -> verdadeiro/falso
static ValorVariant jpgame_tecla_pressionada_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    
    std::string tecla = get_str(args[0]);
    return teclaPressionada(tecla);
}

// === IMPLEMENTAÇÕES - PLAYER ===

// Cria um novo player com COR SÓLIDA
// jpgame.player(largura, altura, r, g, b)
static ValorVariant jpgame_player_cor_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return 0;
    
    int largura = get_int(args[0]);
    int altura = get_int(args[1]);
    int r = get_int(args[2]);
    int g = get_int(args[3]);
    int b = get_int(args[4]);
    
    return criarPlayer(largura, altura, r, g, b);
}

// Cria um novo player com TEXTURA
// jpgame.player_imagem("imagem.png", largura, altura)
static ValorVariant jpgame_player_textura_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return 0;
    
    std::string caminho = get_str(args[0]);
    int largura = get_int(args[1]);
    int altura = get_int(args[2]);
    
    return criarPlayerTextura(caminho, largura, altura);
}

// Atualiza a textura de um player existente
// jpgame.player_atualizar_textura(player, "nova_imagem.png")
// jpgame.player_atualizar_textura(player, "nova_imagem.png", largura, altura)
static ValorVariant jpgame_player_atualizar_textura_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    
    int playerId = get_int(args[0]);
    std::string caminho = get_str(args[1]);
    
    // Se passou largura e altura, atualiza também o tamanho
    if (args.size() >= 4) {
        int largura = get_int(args[2]);
        int altura = get_int(args[3]);
        return playerAtualizarTexturaComTamanho(playerId, caminho, largura, altura);
    }
    
    return playerAtualizarTextura(playerId, caminho);
}

// Espelha o player horizontalmente e/ou verticalmente
// jpgame.player_espelhar(player, horizontal, vertical)
static ValorVariant jpgame_player_espelhar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return false;
    
    int playerId = get_int(args[0]);
    bool horizontal = get_int(args[1]) != 0;
    bool vertical = get_int(args[2]) != 0;
    
    return playerEspelhar(playerId, horizontal, vertical);
}

// Posiciona e ativa o player
// jpgame.player_posicionar(player, x, y)
static ValorVariant jpgame_player_posicionar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return false;
    
    int playerId = get_int(args[0]);
    float x = (float)get_double(args[1]);
    float y = (float)get_double(args[2]);
    
    return playerPosicionar(playerId, x, y);
}

// Desativa o player
// jpgame.player_encerrar(player)
static ValorVariant jpgame_player_encerrar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    
    int playerId = get_int(args[0]);
    return playerEncerrar(playerId);
}

// Configura as teclas de movimento
// jpgame.player_mover(player, "w", "a", "s", "d")
static ValorVariant jpgame_player_mover_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return false;
    
    int playerId = get_int(args[0]);
    std::string teclaW = get_str(args[1]);
    std::string teclaA = get_str(args[2]);
    std::string teclaS = get_str(args[3]);
    std::string teclaD = get_str(args[4]);
    
    return playerMover(playerId, teclaW, teclaA, teclaS, teclaD);
}

// Define a velocidade do player
// jpgame.player_velocidade(player, velX, velY)
static ValorVariant jpgame_player_velocidade_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return false;
    
    int playerId = get_int(args[0]);
    float velX = (float)get_double(args[1]);
    float velY = (float)get_double(args[2]);
    
    return playerVelocidade(playerId, velX, velY);
}

// === IMPLEMENTAÇÕES - SPRITE ===

// Cria um novo sprite com COR SÓLIDA
// jpgame.sprite(largura, altura, r, g, b)
static ValorVariant jpgame_sprite_cor_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 5) return 0;
    
    int largura = get_int(args[0]);
    int altura = get_int(args[1]);
    int r = get_int(args[2]);
    int g = get_int(args[3]);
    int b = get_int(args[4]);
    
    return criarSprite(largura, altura, r, g, b);
}

// Cria um novo sprite com TEXTURA
// jpgame.sprite_imagem("imagem.png", largura, altura)
static ValorVariant jpgame_sprite_textura_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return 0;
    
    std::string caminho = get_str(args[0]);
    int largura = get_int(args[1]);
    int altura = get_int(args[2]);
    
    return criarSpriteTextura(caminho, largura, altura);
}

// Atualiza a textura de um sprite existente
// jpgame.sprite_atualizar_textura(sprite, "nova_imagem.png")
static ValorVariant jpgame_sprite_atualizar_textura_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    
    int spriteId = get_int(args[0]);
    std::string caminho = get_str(args[1]);
    
    return spriteAtualizarTextura(spriteId, caminho);
}

// Posiciona e ativa o sprite
// jpgame.sprite_posicionar(sprite, x, y)
static ValorVariant jpgame_sprite_posicionar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return false;
    
    int spriteId = get_int(args[0]);
    float x = (float)get_double(args[1]);
    float y = (float)get_double(args[2]);
    
    return spritePosicionar(spriteId, x, y);
}

// Desativa o sprite
// jpgame.sprite_encerrar(sprite)
static ValorVariant jpgame_sprite_encerrar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    
    int spriteId = get_int(args[0]);
    return spriteEncerrar(spriteId);
}

// === IMPLEMENTAÇÕES - COLISÃO ===

// Define hitbox retangular
// jpgame.colisao_retangulo(id, largura, altura)
static ValorVariant jpgame_colisao_retangulo_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return false;
    
    int id = get_int(args[0]);
    float largura = (float)get_double(args[1]);
    float altura = (float)get_double(args[2]);
    
    // Tenta como player primeiro, depois como sprite
    if (isPlayer(id)) {
        return colisaoRetanguloPlayer(id, largura, altura);
    }
    if (isSprite(id)) {
        return colisaoRetanguloSprite(id, largura, altura);
    }
    
    return false;
}

// Verifica colisão entre dois elementos
// jpgame.colidiu(id1, id2) -> verdadeiro/falso
static ValorVariant jpgame_colidiu_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return false;
    
    int id1 = get_int(args[0]);
    int id2 = get_int(args[1]);
    
    return colidiu(id1, id2);
}

// === IMPLEMENTAÇÕES - EXIBIR ===

// Função unificada de exibição
// jpgame.exibir(id) - detecta se é player, sprite ou janela
static ValorVariant jpgame_exibir_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 1) return false;
    
    int id = get_int(args[0]);
    
    // Verifica se é a janela (ID 1)
    if (id == 1 && isJanela(id)) {
        // 1. Limpa tela e desenha fundo
        if (!janelaExibir()) return false;
        
        // 2. Desenha todos os sprites marcados
        desenharTodosSprites();
        
        // 3. Desenha todos os players marcados
        desenharTodosPlayers();
        
        // 4. Apresenta o frame
        return janelaApresentar();
    }
    
    // Verifica se é um player
    if (isPlayer(id)) {
        return marcarPlayerParaDesenhar(id);
    }
    
    // Verifica se é um sprite
    if (isSprite(id)) {
        return marcarSpriteParaDesenhar(id);
    }
    
    return false;
}

// === EXPORTS PARA JPLANG ===
extern "C" {

    // === Versões C++ (interpretador) - JANELA ===
    
    __declspec(dllexport) ValorVariant jpgame_janela(const std::vector<ValorVariant>& args) {
        return jpgame_janela_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_janela_cor_fundo(const std::vector<ValorVariant>& args) {
        return jpgame_janela_cor_fundo_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_janela_imagem_fundo(const std::vector<ValorVariant>& args) {
        return jpgame_janela_imagem_fundo_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_rodar(const std::vector<ValorVariant>& args) {
        return jpgame_rodar_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_fechar(const std::vector<ValorVariant>& args) {
        return jpgame_fechar_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_tecla_pressionada(const std::vector<ValorVariant>& args) {
        return jpgame_tecla_pressionada_impl(args);
    }
    
    // === Versões C++ (interpretador) - PLAYER ===
    
    __declspec(dllexport) ValorVariant jpgame_player_cor(const std::vector<ValorVariant>& args) {
        return jpgame_player_cor_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_textura(const std::vector<ValorVariant>& args) {
        return jpgame_player_textura_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_atualizar_textura(const std::vector<ValorVariant>& args) {
        return jpgame_player_atualizar_textura_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_posicionar(const std::vector<ValorVariant>& args) {
        return jpgame_player_posicionar_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_encerrar(const std::vector<ValorVariant>& args) {
        return jpgame_player_encerrar_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_mover(const std::vector<ValorVariant>& args) {
        return jpgame_player_mover_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_velocidade(const std::vector<ValorVariant>& args) {
        return jpgame_player_velocidade_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_player_espelhar(const std::vector<ValorVariant>& args) {
        return jpgame_player_espelhar_impl(args);
    }
    
    // === Versões C++ (interpretador) - SPRITE ===
    
    __declspec(dllexport) ValorVariant jpgame_sprite_cor(const std::vector<ValorVariant>& args) {
        return jpgame_sprite_cor_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_sprite_textura(const std::vector<ValorVariant>& args) {
        return jpgame_sprite_textura_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_sprite_atualizar_textura(const std::vector<ValorVariant>& args) {
        return jpgame_sprite_atualizar_textura_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_sprite_posicionar(const std::vector<ValorVariant>& args) {
        return jpgame_sprite_posicionar_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_sprite_encerrar(const std::vector<ValorVariant>& args) {
        return jpgame_sprite_encerrar_impl(args);
    }
    
    // === Versões C++ (interpretador) - COLISÃO ===
    
    __declspec(dllexport) ValorVariant jpgame_colisao_retangulo(const std::vector<ValorVariant>& args) {
        return jpgame_colisao_retangulo_impl(args);
    }
    
    __declspec(dllexport) ValorVariant jpgame_colidiu(const std::vector<ValorVariant>& args) {
        return jpgame_colidiu_impl(args);
    }
    
    // === Versões C++ (interpretador) - EXIBIR ===
    
    __declspec(dllexport) ValorVariant jpgame_exibir(const std::vector<ValorVariant>& args) {
        return jpgame_exibir_impl(args);
    }

    // === Versões TCC (compilado) - JANELA ===
    
    __declspec(dllexport) JPValor jp_jpgame_janela(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_janela_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_janela_cor_fundo(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_janela_cor_fundo_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_janela_imagem_fundo(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_janela_imagem_fundo_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_rodar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_rodar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_fechar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_fechar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_tecla_pressionada(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_tecla_pressionada_impl(jp_array_para_vector(args, numArgs)));
    }
    
    // === Versões TCC (compilado) - PLAYER ===
    
    __declspec(dllexport) JPValor jp_jpgame_player_cor(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_cor_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_textura(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_textura_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_atualizar_textura(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_atualizar_textura_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_posicionar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_posicionar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_encerrar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_encerrar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_mover(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_mover_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_velocidade(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_velocidade_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_player_espelhar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_player_espelhar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    // === Versões TCC (compilado) - SPRITE ===
    
    __declspec(dllexport) JPValor jp_jpgame_sprite_cor(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_sprite_cor_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_sprite_textura(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_sprite_textura_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_sprite_atualizar_textura(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_sprite_atualizar_textura_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_sprite_posicionar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_sprite_posicionar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_sprite_encerrar(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_sprite_encerrar_impl(jp_array_para_vector(args, numArgs)));
    }
    
    // === Versões TCC (compilado) - COLISÃO ===
    
    __declspec(dllexport) JPValor jp_jpgame_colisao_retangulo(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_colisao_retangulo_impl(jp_array_para_vector(args, numArgs)));
    }
    
    __declspec(dllexport) JPValor jp_jpgame_colidiu(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_colidiu_impl(jp_array_para_vector(args, numArgs)));
    }
    
    // === Versões TCC (compilado) - EXIBIR ===
    
    __declspec(dllexport) JPValor jp_jpgame_exibir(JPValor* args, int numArgs) {
        return variant_para_jp(jpgame_exibir_impl(jp_array_para_vector(args, numArgs)));
    }

} // extern "C"