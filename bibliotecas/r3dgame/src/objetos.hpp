// src/objetos.hpp
// Sistema de objetos/tiles para r3dgame

#ifndef OBJETOS_HPP
#define OBJETOS_HPP

#include <map>
#include <d3d11.h>
#include "matematica.hpp"

// ==========================================
// ESTRUTURAS
// ==========================================

struct VertexObjeto {
    float x, y, z;      // Posição
    float r, g, b, a;   // Cor
};

struct Objeto {
    int id;
    bool ativo;
    
    // Transformação
    float x, y, z;
    float scaleX, scaleY, scaleZ;
    float rotX, rotY, rotZ;
    
    // Cor (RGB 0-255)
    int r, g, b;
    
    // Buffers DirectX
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    bool buffersAtualizados;
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, Objeto*> g_objetos;
static int g_nextObjetoId = 1;

// ==========================================
// FUNÇÕES INTERNAS
// ==========================================

static void objeto_atualizar_buffers(Objeto* obj, ID3D11Device* dev) {
    if (!obj || !dev) return;
    
    if (obj->vertexBuffer) { obj->vertexBuffer->Release(); obj->vertexBuffer = nullptr; }
    if (obj->indexBuffer) { obj->indexBuffer->Release(); obj->indexBuffer = nullptr; }
    
    // Cor normalizada (0.0 - 1.0)
    float fR = obj->r / 255.0f;
    float fG = obj->g / 255.0f;
    float fB = obj->b / 255.0f;
    
    // Cria um Cubo Unitário (de -0.5 a 0.5)
    // A escala será aplicada via Matriz, não nos vértices, para ser mais rápido
    float s = 0.5f; 
    
    VertexObjeto vertices[] = {
        // Frente
        {-s, -s, -s, fR, fG, fB, 1.0f}, {-s,  s, -s, fR, fG, fB, 1.0f},
        { s,  s, -s, fR, fG, fB, 1.0f}, { s, -s, -s, fR, fG, fB, 1.0f},
        // Trás
        {-s, -s,  s, fR, fG, fB, 1.0f}, { s, -s,  s, fR, fG, fB, 1.0f},
        { s,  s,  s, fR, fG, fB, 1.0f}, {-s,  s,  s, fR, fG, fB, 1.0f},
        // Topo
        {-s,  s, -s, fR, fG, fB, 1.0f}, {-s,  s,  s, fR, fG, fB, 1.0f},
        { s,  s,  s, fR, fG, fB, 1.0f}, { s,  s, -s, fR, fG, fB, 1.0f},
        // Base
        {-s, -s, -s, fR, fG, fB, 1.0f}, { s, -s, -s, fR, fG, fB, 1.0f},
        { s, -s,  s, fR, fG, fB, 1.0f}, {-s, -s,  s, fR, fG, fB, 1.0f},
        // Esquerda
        {-s, -s,  s, fR, fG, fB, 1.0f}, {-s,  s,  s, fR, fG, fB, 1.0f},
        {-s,  s, -s, fR, fG, fB, 1.0f}, {-s, -s, -s, fR, fG, fB, 1.0f},
        // Direita
        { s, -s, -s, fR, fG, fB, 1.0f}, { s,  s, -s, fR, fG, fB, 1.0f},
        { s,  s,  s, fR, fG, fB, 1.0f}, { s, -s,  s, fR, fG, fB, 1.0f},
    };

    unsigned short indices[] = {
        0,1,2, 0,2,3,     4,5,6, 4,6,7,     8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
    };

    D3D11_BUFFER_DESC bd = {0};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VertexObjeto) * 24;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {0};
    initData.pSysMem = vertices;
    dev->CreateBuffer(&bd, &initData, &obj->vertexBuffer);

    bd.ByteWidth = sizeof(unsigned short) * 36;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    dev->CreateBuffer(&bd, &initData, &obj->indexBuffer);
    
    obj->buffersAtualizados = true;
}

// ==========================================
// FUNÇÕES PÚBLICAS
// ==========================================

static int objeto_criar(float x, float y, float z) {
    Objeto* obj = new Objeto();
    obj->id = g_nextObjetoId++;
    obj->ativo = true;
    obj->x = x; obj->y = y; obj->z = z;
    obj->scaleX = 1.0f; obj->scaleY = 1.0f; obj->scaleZ = 1.0f;
    obj->rotX = 0; obj->rotY = 0; obj->rotZ = 0;
    // Cor padrão: Cinza Claro
    obj->r = 200; obj->g = 200; obj->b = 200;
    
    obj->vertexBuffer = nullptr;
    obj->indexBuffer = nullptr;
    obj->buffersAtualizados = false;
    
    g_objetos[obj->id] = obj;
    return obj->id;
}

// Retorna ponteiro para objeto pelo ID (usado pela colisão)
static Objeto* objeto_get(int id) {
    auto it = g_objetos.find(id);
    if (it != g_objetos.end()) return it->second;
    return nullptr;
}

static bool objeto_escala(int id, float sx, float sy, float sz) {
    if (g_objetos.find(id) == g_objetos.end()) return false;
    g_objetos[id]->scaleX = sx;
    g_objetos[id]->scaleY = sy;
    g_objetos[id]->scaleZ = sz;
    return true;
}

static bool objeto_cor(int id, int r, int g, int b) {
    if (g_objetos.find(id) == g_objetos.end()) return false;
    g_objetos[id]->r = r;
    g_objetos[id]->g = g;
    g_objetos[id]->b = b;
    g_objetos[id]->buffersAtualizados = false; // Recriar buffers com nova cor
    return true;
}

static void objetos_renderizar_todos(ID3D11Device* dev, ID3D11DeviceContext* ctx, 
                                     ID3D11Buffer* constantBuffer, Mat4x4 view, Mat4x4 proj) {
    UINT stride = sizeof(VertexObjeto);
    UINT offset = 0;
    
    struct CBMatrices {
        Mat4x4 world;
        Mat4x4 view;
        Mat4x4 proj;
    };

    for (auto& [id, obj] : g_objetos) {
        if (!obj->ativo) continue;
        
        if (!obj->buffersAtualizados) objeto_atualizar_buffers(obj, dev);
        if (!obj->vertexBuffer) continue;

        ctx->IASetVertexBuffers(0, 1, &obj->vertexBuffer, &stride, &offset);
        ctx->IASetIndexBuffer(obj->indexBuffer, DXGI_FORMAT_R16_UINT, 0);

        // Matriz World: Escala -> Rotação -> Translação
        Mat4x4 mScale = mat_scale(obj->scaleX, obj->scaleY, obj->scaleZ);
        Mat4x4 mRotX = mat_rotX(obj->rotX);
        Mat4x4 mRotY = mat_rotY(obj->rotY);
        Mat4x4 mRotZ = mat_rotZ(obj->rotZ);
        Mat4x4 mTrans = mat_translation(obj->x, obj->y, obj->z);
        
        Mat4x4 rot = mat_mul(mRotX, mat_mul(mRotY, mRotZ));
        Mat4x4 world = mat_mul(mScale, mat_mul(rot, mTrans));

        CBMatrices cb;
        cb.world = mat_transpose(world);
        cb.view = mat_transpose(view);
        cb.proj = mat_transpose(proj);

        ctx->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, &constantBuffer);
        ctx->DrawIndexed(36, 0, 0);
    }
}

static void objetos_cleanup() {
    for (auto& [id, obj] : g_objetos) {
        if (obj->vertexBuffer) obj->vertexBuffer->Release();
        if (obj->indexBuffer) obj->indexBuffer->Release();
        delete obj;
    }
    g_objetos.clear();
}

#endif