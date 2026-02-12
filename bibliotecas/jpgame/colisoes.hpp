// colisoes.hpp
// Sistema de colisões para JPGame

#ifndef COLISOES_HPP
#define COLISOES_HPP

#include "player.hpp"
#include "sprites.hpp"

// === FUNÇÕES PARA DEFINIR HITBOX ===

// Define hitbox retangular para um player
bool colisaoRetanguloPlayer(int playerId, float largura, float altura) {
    Player* player = getPlayer(playerId);
    if (!player) return false;
    
    player->temHitbox = true;
    player->hitboxLargura = largura;
    player->hitboxAltura = altura;
    
    return true;
}

// Define hitbox retangular para um sprite
bool colisaoRetanguloSprite(int spriteId, float largura, float altura) {
    Sprite* sprite = getSprite(spriteId);
    if (!sprite) return false;
    
    sprite->temHitbox = true;
    sprite->hitboxLargura = largura;
    sprite->hitboxAltura = altura;
    
    return true;
}

// === ESTRUTURA DE HITBOX PARA CÁLCULO ===

struct Hitbox {
    float x, y;          // Posição
    float largura, altura; // Tamanho
    bool valido;
};

// Obtém hitbox de um elemento (player ou sprite)
static Hitbox obterHitbox(int id) {
    Hitbox hb = {0, 0, 0, 0, false};
    
    // Tenta como player
    Player* player = getPlayer(id);
    if (player && player->ativo) {
        hb.x = player->x;
        hb.y = player->y;
        if (player->temHitbox) {
            hb.largura = player->hitboxLargura;
            hb.altura = player->hitboxAltura;
        } else {
            // Usa tamanho visual se não tem hitbox definido
            hb.largura = (float)player->largura;
            hb.altura = (float)player->altura;
        }
        hb.valido = true;
        return hb;
    }
    
    // Tenta como sprite
    Sprite* sprite = getSprite(id);
    if (sprite && sprite->ativo) {
        hb.x = sprite->x;
        hb.y = sprite->y;
        if (sprite->temHitbox) {
            hb.largura = sprite->hitboxLargura;
            hb.altura = sprite->hitboxAltura;
        } else {
            // Usa tamanho visual se não tem hitbox definido
            hb.largura = (float)sprite->largura;
            hb.altura = (float)sprite->altura;
        }
        hb.valido = true;
        return hb;
    }
    
    return hb;
}

// === DETECÇÃO DE COLISÃO ===

// Verifica colisão AABB (Axis-Aligned Bounding Box) entre dois retângulos
static bool colisaoAABB(const Hitbox& a, const Hitbox& b) {
    return (a.x < b.x + b.largura &&
            a.x + a.largura > b.x &&
            a.y < b.y + b.altura &&
            a.y + a.altura > b.y);
}

// Verifica se dois elementos colidiram
// Funciona com qualquer combinação: player-player, player-sprite, sprite-sprite
bool colidiu(int id1, int id2) {
    Hitbox hb1 = obterHitbox(id1);
    Hitbox hb2 = obterHitbox(id2);
    
    // Se algum não é válido (não existe ou não está ativo), não há colisão
    if (!hb1.valido || !hb2.valido) return false;
    
    return colisaoAABB(hb1, hb2);
}

#endif // COLISOES_HPP
