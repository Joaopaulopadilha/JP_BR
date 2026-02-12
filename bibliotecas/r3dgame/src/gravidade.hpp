// src/gravidade.hpp
// Sistema de Gravidade e Pulo para r3dgame

#ifndef GRAVIDADE_HPP
#define GRAVIDADE_HPP

#include <map>
#include <cmath>
#include "colisao.hpp"
#include "objetos.hpp"

// ==========================================
// ESTRUTURAS
// ==========================================

struct GravidadePlayer {
    int playerId;
    bool ativo;
    float forca;        // Força da gravidade (aceleração por frame)
    float velocidadeY;  // Velocidade vertical atual
    float maxVelocidade; // Velocidade máxima de queda
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, GravidadePlayer*> g_gravidadePlayers;

// ==========================================
// FUNÇÕES PÚBLICAS
// ==========================================

// Define gravidade para um player
static bool gravidade_player_definir(int playerId, float forca) {
    GravidadePlayer* grav = nullptr;
    
    auto it = g_gravidadePlayers.find(playerId);
    if (it != g_gravidadePlayers.end()) {
        grav = it->second;
    } else {
        grav = new GravidadePlayer();
        grav->playerId = playerId;
        grav->velocidadeY = 0;
        grav->maxVelocidade = 1.0f; // Velocidade máxima de queda
        g_gravidadePlayers[playerId] = grav;
    }
    
    grav->ativo = true;
    grav->forca = forca;
    
    return true;
}

// Retorna estrutura de gravidade do player
static GravidadePlayer* gravidade_player_get(int playerId) {
    auto it = g_gravidadePlayers.find(playerId);
    if (it != g_gravidadePlayers.end()) return it->second;
    return nullptr;
}

// Aplica impulso para cima (pular)
static bool gravidade_player_pular(int playerId, float forca) {
    GravidadePlayer* grav = gravidade_player_get(playerId);
    
    // Se não tem gravidade definida, cria com gravidade padrão
    if (!grav) {
        gravidade_player_definir(playerId, 0.01f);
        grav = gravidade_player_get(playerId);
    }
    
    if (!grav) return false;
    
    // Aplica impulso para cima (positivo no Y)
    grav->velocidadeY = forca;
    
    return true;
}

// Verifica se player está no chão (colidindo com sólido abaixo)
static bool gravidade_player_no_chao(int playerId, float playerX, float playerY, float playerZ) {
    ColisaoPlayer* colPlayer = colisao_player_get(playerId);
    if (!colPlayer || !colPlayer->ativo) return false;
    
    // Verifica um pouco abaixo do player
    float checkY = playerY - 0.05f;
    
    // Cria AABB do player na posição de verificação
    AABB boxPlayer = aabb_criar(playerX, checkY, playerZ,
                                 colPlayer->largura, colPlayer->altura, colPlayer->profundidade);
    
    // Verifica contra todos os objetos sólidos
    for (auto& [objId, colObj] : g_colisaoObjetos) {
        if (!colObj || !colObj->ativo || !colObj->solido) continue;
        
        // Pega posição do objeto
        Objeto* obj = objeto_get(objId);
        if (!obj) continue;
        
        AABB boxObj = aabb_criar(obj->x, obj->y, obj->z,
                                  colObj->largura, colObj->altura, colObj->profundidade);
        
        if (aabb_colide(boxPlayer, boxObj)) {
            return true; // Está no chão
        }
    }
    
    return false; // Não está no chão
}

// Processa gravidade e retorna o delta Y a ser aplicado
// Também verifica colisão com chão para parar a queda
static float gravidade_processar(int playerId, float playerX, float playerY, float playerZ) {
    GravidadePlayer* grav = gravidade_player_get(playerId);
    if (!grav || !grav->ativo) return 0;
    
    // Aplica aceleração da gravidade (diminui velocidade Y)
    grav->velocidadeY -= grav->forca;
    
    // Limita velocidade máxima de queda
    if (grav->velocidadeY < -grav->maxVelocidade) {
        grav->velocidadeY = -grav->maxVelocidade;
    }
    
    // Calcula nova posição Y
    float novoY = playerY + grav->velocidadeY;
    
    // Verifica colisão com objetos sólidos abaixo (se caindo)
    if (grav->velocidadeY < 0) {
        ColisaoPlayer* colPlayer = colisao_player_get(playerId);
        if (colPlayer && colPlayer->ativo) {
            AABB boxPlayer = aabb_criar(playerX, novoY, playerZ,
                                         colPlayer->largura, colPlayer->altura, colPlayer->profundidade);
            
            for (auto& [objId, colObj] : g_colisaoObjetos) {
                if (!colObj || !colObj->ativo || !colObj->solido) continue;
                
                Objeto* obj = objeto_get(objId);
                if (!obj) continue;
                
                AABB boxObj = aabb_criar(obj->x, obj->y, obj->z,
                                          colObj->largura, colObj->altura, colObj->profundidade);
                
                if (aabb_colide(boxPlayer, boxObj)) {
                    // Colidiu com chão - para a queda
                    grav->velocidadeY = 0;
                    
                    // Ajusta posição para ficar em cima do objeto
                    float topoObjeto = obj->y + (colObj->altura / 2.0f);
                    float basePlayer = colPlayer->altura / 2.0f;
                    return (topoObjeto + basePlayer) - playerY;
                }
            }
        }
    }
    
    // Verifica colisão com teto (se subindo)
    if (grav->velocidadeY > 0) {
        ColisaoPlayer* colPlayer = colisao_player_get(playerId);
        if (colPlayer && colPlayer->ativo) {
            AABB boxPlayer = aabb_criar(playerX, novoY, playerZ,
                                         colPlayer->largura, colPlayer->altura, colPlayer->profundidade);
            
            for (auto& [objId, colObj] : g_colisaoObjetos) {
                if (!colObj || !colObj->ativo || !colObj->solido) continue;
                
                Objeto* obj = objeto_get(objId);
                if (!obj) continue;
                
                AABB boxObj = aabb_criar(obj->x, obj->y, obj->z,
                                          colObj->largura, colObj->altura, colObj->profundidade);
                
                if (aabb_colide(boxPlayer, boxObj)) {
                    // Colidiu com teto - para a subida
                    grav->velocidadeY = 0;
                    return 0;
                }
            }
        }
    }
    
    return grav->velocidadeY;
}

// Cleanup
static void gravidade_cleanup() {
    for (auto& [id, grav] : g_gravidadePlayers) {
        delete grav;
    }
    g_gravidadePlayers.clear();
}

#endif