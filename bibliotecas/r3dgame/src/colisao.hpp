// src/colisao.hpp
// Sistema de Colisão AABB (Axis-Aligned Bounding Box) para r3dgame
// Com suporte a objetos sólidos (bloqueiam movimento)

#ifndef COLISAO_HPP
#define COLISAO_HPP

#include <map>
#include <cmath>

// ==========================================
// ESTRUTURAS
// ==========================================

struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

struct ColisaoPlayer {
    int playerId;
    bool ativo;
    bool solido;  // Se true, bloqueia outros
    float largura, altura, profundidade;
};

struct ColisaoObjeto {
    int objetoId;
    bool ativo;
    bool solido;  // Se true, bloqueia movimento do player
    float largura, altura, profundidade;
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, ColisaoPlayer*> g_colisaoPlayers;
static std::map<int, ColisaoObjeto*> g_colisaoObjetos;

// ==========================================
// FUNÇÕES DE AABB
// ==========================================

// Cria AABB a partir de posição central e dimensões
static AABB aabb_criar(float x, float y, float z, float largura, float altura, float profundidade) {
    AABB box;
    float hw = largura / 2.0f;
    float hh = altura / 2.0f;
    float hd = profundidade / 2.0f;
    
    box.minX = x - hw;
    box.maxX = x + hw;
    box.minY = y - hh;
    box.maxY = y + hh;
    box.minZ = z - hd;
    box.maxZ = z + hd;
    
    return box;
}

// Verifica se duas AABBs estão colidindo
static bool aabb_colide(const AABB& a, const AABB& b) {
    // Separação em qualquer eixo significa que não há colisão
    if (a.maxX < b.minX || a.minX > b.maxX) return false;
    if (a.maxY < b.minY || a.minY > b.maxY) return false;
    if (a.maxZ < b.minZ || a.minZ > b.maxZ) return false;
    
    // Se não há separação em nenhum eixo, há colisão
    return true;
}

// ==========================================
// FUNÇÕES DE COLISÃO PLAYER
// ==========================================

static bool colisao_player_definir(int playerId, float largura, float altura, float profundidade) {
    ColisaoPlayer* col = nullptr;
    
    auto it = g_colisaoPlayers.find(playerId);
    if (it != g_colisaoPlayers.end()) {
        col = it->second;
    } else {
        col = new ColisaoPlayer();
        col->playerId = playerId;
        col->solido = false;  // Padrão: não sólido
        g_colisaoPlayers[playerId] = col;
    }
    
    col->ativo = true;
    col->largura = largura;
    col->altura = altura;
    col->profundidade = profundidade;
    
    return true;
}

static bool colisao_player_solido(int playerId, bool solido) {
    auto it = g_colisaoPlayers.find(playerId);
    if (it != g_colisaoPlayers.end()) {
        it->second->solido = solido;
        return true;
    }
    
    // Se não existe, cria com tamanho padrão
    ColisaoPlayer* col = new ColisaoPlayer();
    col->playerId = playerId;
    col->ativo = true;
    col->solido = solido;
    col->largura = 1.0f;
    col->altura = 1.0f;
    col->profundidade = 1.0f;
    g_colisaoPlayers[playerId] = col;
    
    return true;
}

static ColisaoPlayer* colisao_player_get(int playerId) {
    auto it = g_colisaoPlayers.find(playerId);
    if (it != g_colisaoPlayers.end()) return it->second;
    return nullptr;
}

// ==========================================
// FUNÇÕES DE COLISÃO OBJETO
// ==========================================

static bool colisao_objeto_definir(int objetoId, float largura, float altura, float profundidade) {
    ColisaoObjeto* col = nullptr;
    
    auto it = g_colisaoObjetos.find(objetoId);
    if (it != g_colisaoObjetos.end()) {
        col = it->second;
    } else {
        col = new ColisaoObjeto();
        col->objetoId = objetoId;
        col->solido = false;  // Padrão: não sólido
        g_colisaoObjetos[objetoId] = col;
    }
    
    col->ativo = true;
    col->largura = largura;
    col->altura = altura;
    col->profundidade = profundidade;
    
    return true;
}

static bool colisao_objeto_solido(int objetoId, bool solido) {
    auto it = g_colisaoObjetos.find(objetoId);
    if (it != g_colisaoObjetos.end()) {
        it->second->solido = solido;
        return true;
    }
    
    // Se não existe, cria com tamanho padrão
    ColisaoObjeto* col = new ColisaoObjeto();
    col->objetoId = objetoId;
    col->ativo = true;
    col->solido = solido;
    col->largura = 1.0f;
    col->altura = 1.0f;
    col->profundidade = 1.0f;
    g_colisaoObjetos[objetoId] = col;
    
    return true;
}

static ColisaoObjeto* colisao_objeto_get(int objetoId) {
    auto it = g_colisaoObjetos.find(objetoId);
    if (it != g_colisaoObjetos.end()) return it->second;
    return nullptr;
}

// ==========================================
// FUNÇÕES DE VERIFICAÇÃO DE COLISÃO
// ==========================================

// Verifica colisão entre player e objeto
static bool colisao_player_objeto(int playerId, float playerX, float playerY, float playerZ,
                                   int objetoId, float objetoX, float objetoY, float objetoZ) {
    ColisaoPlayer* colPlayer = colisao_player_get(playerId);
    ColisaoObjeto* colObjeto = colisao_objeto_get(objetoId);
    
    if (!colPlayer || !colPlayer->ativo) return false;
    if (!colObjeto || !colObjeto->ativo) return false;
    
    AABB boxPlayer = aabb_criar(playerX, playerY, playerZ, 
                                 colPlayer->largura, colPlayer->altura, colPlayer->profundidade);
    AABB boxObjeto = aabb_criar(objetoX, objetoY, objetoZ,
                                 colObjeto->largura, colObjeto->altura, colObjeto->profundidade);
    
    return aabb_colide(boxPlayer, boxObjeto);
}

// Verifica colisão entre dois players
static bool colisao_player_player(int player1Id, float p1X, float p1Y, float p1Z,
                                   int player2Id, float p2X, float p2Y, float p2Z) {
    ColisaoPlayer* col1 = colisao_player_get(player1Id);
    ColisaoPlayer* col2 = colisao_player_get(player2Id);
    
    if (!col1 || !col1->ativo) return false;
    if (!col2 || !col2->ativo) return false;
    
    AABB box1 = aabb_criar(p1X, p1Y, p1Z, col1->largura, col1->altura, col1->profundidade);
    AABB box2 = aabb_criar(p2X, p2Y, p2Z, col2->largura, col2->altura, col2->profundidade);
    
    return aabb_colide(box1, box2);
}

// Verifica colisão entre dois objetos
static bool colisao_objeto_objeto(int obj1Id, float o1X, float o1Y, float o1Z,
                                   int obj2Id, float o2X, float o2Y, float o2Z) {
    ColisaoObjeto* col1 = colisao_objeto_get(obj1Id);
    ColisaoObjeto* col2 = colisao_objeto_get(obj2Id);
    
    if (!col1 || !col1->ativo) return false;
    if (!col2 || !col2->ativo) return false;
    
    AABB box1 = aabb_criar(o1X, o1Y, o1Z, col1->largura, col1->altura, col1->profundidade);
    AABB box2 = aabb_criar(o2X, o2Y, o2Z, col2->largura, col2->altura, col2->profundidade);
    
    return aabb_colide(box1, box2);
}

// ==========================================
// VERIFICAÇÃO DE MOVIMENTO (SÓLIDOS)
// ==========================================

// Verifica se o player pode se mover para a nova posição
// Retorna true se o movimento é permitido (não colide com sólidos)
static bool colisao_movimento_permitido(int playerId, float novoX, float novoY, float novoZ) {
    ColisaoPlayer* colPlayer = colisao_player_get(playerId);
    if (!colPlayer || !colPlayer->ativo) return true; // Sem colisão definida = pode mover
    
    AABB boxPlayer = aabb_criar(novoX, novoY, novoZ,
                                 colPlayer->largura, colPlayer->altura, colPlayer->profundidade);
    
    // Verifica contra todos os objetos sólidos
    for (auto& [objId, colObj] : g_colisaoObjetos) {
        if (!colObj->ativo || !colObj->solido) continue;
        
        // Precisa da posição do objeto - vem de objetos.hpp via extern
        // Como não temos acesso direto aqui, usamos uma abordagem diferente:
        // Armazenamos a posição na própria estrutura de colisão
    }
    
    return true; // Placeholder - a verificação real será feita em player.hpp
}

// Versão que recebe posições dos objetos diretamente
// Usada internamente pelo sistema de movimento
static bool colisao_verificar_solidos_objetos(int playerId, float novoX, float novoY, float novoZ,
                                               const std::map<int, std::tuple<float, float, float>>& objetosPos) {
    ColisaoPlayer* colPlayer = colisao_player_get(playerId);
    if (!colPlayer || !colPlayer->ativo) return true;
    
    // Usa uma pequena margem para evitar que colisão com chão bloqueie movimento XZ
    // O player está "em cima" do chão, não "dentro" dele
    float margemY = 0.01f;
    
    AABB boxPlayer = aabb_criar(novoX, novoY + margemY, novoZ,
                                 colPlayer->largura, colPlayer->altura - margemY * 2, colPlayer->profundidade);
    
    for (auto& [objId, pos] : objetosPos) {
        ColisaoObjeto* colObj = colisao_objeto_get(objId);
        if (!colObj || !colObj->ativo || !colObj->solido) continue;
        
        float objX = std::get<0>(pos);
        float objY = std::get<1>(pos);
        float objZ = std::get<2>(pos);
        
        AABB boxObj = aabb_criar(objX, objY, objZ,
                                  colObj->largura, colObj->altura, colObj->profundidade);
        
        if (aabb_colide(boxPlayer, boxObj)) {
            return false; // Colisão com sólido - movimento bloqueado
        }
    }
    
    return true; // Nenhuma colisão com sólidos
}

// ==========================================
// CLEANUP
// ==========================================

static void colisao_cleanup() {
    for (auto& [id, col] : g_colisaoPlayers) {
        delete col;
    }
    g_colisaoPlayers.clear();
    
    for (auto& [id, col] : g_colisaoObjetos) {
        delete col;
    }
    g_colisaoObjetos.clear();
}

#endif