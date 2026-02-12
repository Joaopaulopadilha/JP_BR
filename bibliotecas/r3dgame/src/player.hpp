// src/player.hpp
// Gerenciamento de players para r3dgame
// Movimento relativo à rotação do player (compatível com câmera third-person)
// Com verificação de colisão contra objetos sólidos
// Gravidade e pulo integrados

#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <map>
#include <tuple>
#include <cmath>
#include <d3d11.h>
#include "matematica.hpp"
#include "input.hpp"
#include "colisao.hpp"
#include "objetos.hpp"
#include "gravidade.hpp"

// ==========================================
// ESTRUTURAS
// ==========================================

struct VertexPlayer {
    float x, y, z;      // Posição
    float r, g, b, a;   // Cor
};

struct FaceCor {
    int r, g, b;
};

struct Player {
    int id;
    bool ativo;
    
    // Posição
    float x, y, z;
    
    // Tamanho
    float largura, altura, profundidade;
    
    // Rotação
    float rotX, rotY, rotZ;
    
    // Cores das 6 faces (RGB 0-255)
    FaceCor corFrente;
    FaceCor corTras;
    FaceCor corTopo;
    FaceCor corBase;
    FaceCor corEsquerda;
    FaceCor corDireita;
    
    // Buffers DirectX (cada player tem seu próprio para cores individuais)
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    bool buffersAtualizados;
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, Player*> g_players;
static int g_nextPlayerId = 1;

// ==========================================
// FUNÇÕES INTERNAS
// ==========================================

static void player_atualizar_buffers(Player* p, ID3D11Device* dev) {
    if (!p || !dev) return;
    
    // Liberar buffers antigos se existirem
    if (p->vertexBuffer) { p->vertexBuffer->Release(); p->vertexBuffer = nullptr; }
    if (p->indexBuffer) { p->indexBuffer->Release(); p->indexBuffer = nullptr; }
    
    // Metade das dimensões para criar vértices centrados
    float hw = p->largura / 2.0f;
    float hh = p->altura / 2.0f;
    float hd = p->profundidade / 2.0f;
    
    // Converter cores para 0.0-1.0
    float fR = p->corFrente.r / 255.0f, fG = p->corFrente.g / 255.0f, fB = p->corFrente.b / 255.0f;
    float tR = p->corTras.r / 255.0f, tG = p->corTras.g / 255.0f, tB = p->corTras.b / 255.0f;
    float toR = p->corTopo.r / 255.0f, toG = p->corTopo.g / 255.0f, toB = p->corTopo.b / 255.0f;
    float baR = p->corBase.r / 255.0f, baG = p->corBase.g / 255.0f, baB = p->corBase.b / 255.0f;
    float eR = p->corEsquerda.r / 255.0f, eG = p->corEsquerda.g / 255.0f, eB = p->corEsquerda.b / 255.0f;
    float dR = p->corDireita.r / 255.0f, dG = p->corDireita.g / 255.0f, dB = p->corDireita.b / 255.0f;
    
    // 24 vértices (4 por face para cores sólidas)
    VertexPlayer vertices[] = {
        // Frente (Z negativo)
        {-hw, -hh, -hd,  fR, fG, fB, 1.0f},
        {-hw,  hh, -hd,  fR, fG, fB, 1.0f},
        { hw,  hh, -hd,  fR, fG, fB, 1.0f},
        { hw, -hh, -hd,  fR, fG, fB, 1.0f},

        // Trás (Z positivo)
        {-hw, -hh,  hd,  tR, tG, tB, 1.0f},
        { hw, -hh,  hd,  tR, tG, tB, 1.0f},
        { hw,  hh,  hd,  tR, tG, tB, 1.0f},
        {-hw,  hh,  hd,  tR, tG, tB, 1.0f},

        // Topo (Y positivo)
        {-hw,  hh, -hd,  toR, toG, toB, 1.0f},
        {-hw,  hh,  hd,  toR, toG, toB, 1.0f},
        { hw,  hh,  hd,  toR, toG, toB, 1.0f},
        { hw,  hh, -hd,  toR, toG, toB, 1.0f},

        // Base (Y negativo)
        {-hw, -hh, -hd,  baR, baG, baB, 1.0f},
        { hw, -hh, -hd,  baR, baG, baB, 1.0f},
        { hw, -hh,  hd,  baR, baG, baB, 1.0f},
        {-hw, -hh,  hd,  baR, baG, baB, 1.0f},

        // Esquerda (X negativo)
        {-hw, -hh,  hd,  eR, eG, eB, 1.0f},
        {-hw,  hh,  hd,  eR, eG, eB, 1.0f},
        {-hw,  hh, -hd,  eR, eG, eB, 1.0f},
        {-hw, -hh, -hd,  eR, eG, eB, 1.0f},

        // Direita (X positivo)
        { hw, -hh, -hd,  dR, dG, dB, 1.0f},
        { hw,  hh, -hd,  dR, dG, dB, 1.0f},
        { hw,  hh,  hd,  dR, dG, dB, 1.0f},
        { hw, -hh,  hd,  dR, dG, dB, 1.0f},
    };

    // Índices (12 triângulos = 36 índices)
    unsigned short indices[] = {
        0, 1, 2,  0, 2, 3,       // Frente
        4, 5, 6,  4, 6, 7,       // Trás
        8, 9, 10, 8, 10, 11,     // Topo
        12, 13, 14, 12, 14, 15,  // Base
        16, 17, 18, 16, 18, 19,  // Esquerda
        20, 21, 22, 20, 22, 23   // Direita
    };

    // Criar Vertex Buffer
    D3D11_BUFFER_DESC bd = {0};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VertexPlayer) * 24;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA initData = {0};
    initData.pSysMem = vertices;
    dev->CreateBuffer(&bd, &initData, &p->vertexBuffer);

    // Criar Index Buffer
    bd.ByteWidth = sizeof(unsigned short) * 36;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    dev->CreateBuffer(&bd, &initData, &p->indexBuffer);
    
    p->buffersAtualizados = true;
}

// Coleta posições de todos os objetos para verificação de colisão
static std::map<int, std::tuple<float, float, float>> player_coletar_posicoes_objetos() {
    std::map<int, std::tuple<float, float, float>> posicoes;
    
    for (auto& [id, obj] : g_objetos) {
        if (obj && obj->ativo) {
            posicoes[id] = std::make_tuple(obj->x, obj->y, obj->z);
        }
    }
    
    return posicoes;
}

// ==========================================
// FUNÇÕES PÚBLICAS
// ==========================================

static int player_criar(float x, float y, float z) {
    Player* p = new Player();
    p->id = g_nextPlayerId++;
    p->ativo = true;
    
    // Posição
    p->x = x;
    p->y = y;
    p->z = z;
    
    // Tamanho padrão
    p->largura = 1.0f;
    p->altura = 1.0f;
    p->profundidade = 1.0f;
    
    // Rotação
    p->rotX = 0;
    p->rotY = 0;
    p->rotZ = 0;
    
    // Cores padrão (cada face uma cor diferente)
    p->corFrente = {255, 0, 0};       // Vermelho
    p->corTras = {0, 255, 0};         // Verde
    p->corTopo = {0, 0, 255};         // Azul
    p->corBase = {255, 255, 0};       // Amarelo
    p->corEsquerda = {255, 0, 255};   // Magenta
    p->corDireita = {0, 255, 255};    // Ciano
    
    // Buffers
    p->vertexBuffer = nullptr;
    p->indexBuffer = nullptr;
    p->buffersAtualizados = false;
    
    g_players[p->id] = p;
    return p->id;
}

static Player* player_get(int id) {
    auto it = g_players.find(id);
    if (it != g_players.end()) return it->second;
    return nullptr;
}

static bool player_tamanho(int id, float largura, float altura, float profundidade) {
    Player* p = player_get(id);
    if (!p) return false;
    
    p->largura = largura;
    p->altura = altura;
    p->profundidade = profundidade;
    p->buffersAtualizados = false;
    
    return true;
}

static bool player_cores(int id, 
    int fR, int fG, int fB,     // Frente
    int tR, int tG, int tB,     // Trás
    int toR, int toG, int toB,  // Topo
    int baR, int baG, int baB,  // Base
    int eR, int eG, int eB,     // Esquerda
    int dR, int dG, int dB)     // Direita
{
    Player* p = player_get(id);
    if (!p) return false;
    
    p->corFrente = {fR, fG, fB};
    p->corTras = {tR, tG, tB};
    p->corTopo = {toR, toG, toB};
    p->corBase = {baR, baG, baB};
    p->corEsquerda = {eR, eG, eB};
    p->corDireita = {dR, dG, dB};
    p->buffersAtualizados = false;
    
    return true;
}

static bool player_posicao(int id, float x, float y, float z) {
    Player* p = player_get(id);
    if (!p) return false;
    
    p->x = x;
    p->y = y;
    p->z = z;
    
    return true;
}

static bool player_rotacionar(int id, float rx, float ry, float rz) {
    Player* p = player_get(id);
    if (!p) return false;
    
    p->rotX += rx;
    p->rotY += ry;
    p->rotZ += rz;
    
    return true;
}

static void player_renderizar(Player* p, ID3D11Device* dev, ID3D11DeviceContext* ctx, 
                               ID3D11Buffer* constantBuffer, Mat4x4 view, Mat4x4 proj) {
    if (!p || !p->ativo || !dev || !ctx) return;
    
    // Atualizar buffers se necessário
    if (!p->buffersAtualizados) {
        player_atualizar_buffers(p, dev);
    }
    
    if (!p->vertexBuffer || !p->indexBuffer) return;
    
    // Configurar buffers
    UINT stride = sizeof(VertexPlayer);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &p->vertexBuffer, &stride, &offset);
    ctx->IASetIndexBuffer(p->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    
    // Calcular matriz world
    Mat4x4 mRotX = mat_rotX(p->rotX);
    Mat4x4 mRotY = mat_rotY(p->rotY);
    Mat4x4 mRotZ = mat_rotZ(p->rotZ);
    Mat4x4 mTrans = mat_translation(p->x, p->y, p->z);
    Mat4x4 rot = mat_mul(mRotX, mat_mul(mRotY, mRotZ));
    Mat4x4 world = mat_mul(rot, mTrans);
    
    // Estrutura para constant buffer
    struct CBMatrices {
        Mat4x4 world;
        Mat4x4 view;
        Mat4x4 proj;
    };
    
    CBMatrices cb;
    cb.world = mat_transpose(world);
    cb.view = mat_transpose(view);
    cb.proj = mat_transpose(proj);
    
    ctx->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);
    ctx->VSSetConstantBuffers(0, 1, &constantBuffer);
    
    // Desenhar
    ctx->DrawIndexed(36, 0, 0);
}

static void player_renderizar_todos(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                                     ID3D11Buffer* constantBuffer, Mat4x4 view, Mat4x4 proj) {
    for (auto& [id, p] : g_players) {
        player_renderizar(p, dev, ctx, constantBuffer, view, proj);
    }
}

static void player_cleanup() {
    for (auto& [id, p] : g_players) {
        if (p->vertexBuffer) p->vertexBuffer->Release();
        if (p->indexBuffer) p->indexBuffer->Release();
        delete p;
    }
    g_players.clear();
    input_cleanup();
    gravidade_cleanup();
}

// Atualiza player (processa input, gravidade e move)
static void player_atualizar(int id) {
    Player* p = player_get(id);
    if (!p || !p->ativo) return;
    
    // =============================================
    // 1. PROCESSA GRAVIDADE (movimento Y automático)
    // =============================================
    float deltaY = gravidade_processar(id, p->x, p->y, p->z);
    if (deltaY != 0) {
        p->y += deltaY;
    }
    
    // =============================================
    // 2. PROCESSA INPUT DE MOVIMENTO (XZ apenas)
    // =============================================
    if (input_player_tem_input(id)) {
        float dx, dy, dz;
        input_processar_player(id, &dx, &dy, &dz);
        
        // Ignora dy do input - gravidade controla Y
        // dy agora não é usado (removido do player_mover)
        
        // =============================================
        // MOVIMENTO RELATIVO À ROTAÇÃO DO PLAYER
        // =============================================
        float sinY = sin(p->rotY);
        float cosY = cos(p->rotY);
        
        // Frente/Trás (Z no espaço local -> X,Z no espaço mundo)
        float moveX = dz * sinY;
        float moveZ = dz * cosY;
        
        // Strafe Esquerda/Direita (X no espaço local -> X,Z no espaço mundo)
        moveX += dx * cosY;
        moveZ += -dx * sinY;
        
        // =============================================
        // VERIFICAÇÃO DE COLISÃO COM SÓLIDOS (XZ)
        // =============================================
        auto objetosPos = player_coletar_posicoes_objetos();
        
        bool podeMovX = true;
        bool podeMovZ = true;
        
        ColisaoPlayer* colPlayer = colisao_player_get(id);
        if (colPlayer && colPlayer->ativo) {
            // Testa movimento em X
            if (moveX != 0) {
                if (!colisao_verificar_solidos_objetos(id, p->x + moveX, p->y, p->z, objetosPos)) {
                    podeMovX = false;
                }
            }
            
            // Testa movimento em Z
            if (moveZ != 0) {
                if (!colisao_verificar_solidos_objetos(id, p->x, p->y, p->z + moveZ, objetosPos)) {
                    podeMovZ = false;
                }
            }
        }
        
        // Aplica movimento apenas nos eixos permitidos
        if (podeMovX) p->x += moveX;
        if (podeMovZ) p->z += moveZ;
    }
}

// Atualiza todos os players
static void player_atualizar_todos() {
    for (auto& [id, p] : g_players) {
        player_atualizar(id);
    }
}

#endif