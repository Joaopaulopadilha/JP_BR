#ifndef PRIMITIVOS_HPP
#define PRIMITIVOS_HPP

#include <d3d11.h>

// Estrutura de Vértice usada pelos primitivos
struct VertexPrimitivo {
    float x, y, z;      // Posição
    float r, g, b, a;   // Cor
};

// Cria os buffers (Vertex e Index) para um CUBO COLORIDO
// Retorna true se sucesso
// Parâmetros:
//   dev: O Device DirectX (vem de janela.hpp)
//   outVB: Ponteiro para receber o Vertex Buffer criado
//   outIB: Ponteiro para receber o Index Buffer criado
static bool primitivos_criar_cubo(ID3D11Device* dev, ID3D11Buffer** outVB, ID3D11Buffer** outIB) {
    if (!dev) return false;

    // 1. Definição dos 24 Vértices (4 por face para cores sólidas)
    // Layout: X, Y, Z,   R, G, B, A
    VertexPrimitivo vertices[] = {
        // Frente (Vermelho)
        {-1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 0.0f, 1.0f},
        {-1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 0.0f, 1.0f},
        { 1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 0.0f, 1.0f},

        // Trás (Verde)
        {-1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 0.0f, 1.0f},
        { 1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f, 1.0f},
        {-1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f, 1.0f},

        // Topo (Azul)
        {-1.0f,  1.0f, -1.0f,   0.0f, 0.0f, 1.0f, 1.0f},
        {-1.0f,  1.0f,  1.0f,   0.0f, 0.0f, 1.0f, 1.0f},
        { 1.0f,  1.0f,  1.0f,   0.0f, 0.0f, 1.0f, 1.0f},
        { 1.0f,  1.0f, -1.0f,   0.0f, 0.0f, 1.0f, 1.0f},

        // Base (Amarelo)
        {-1.0f, -1.0f, -1.0f,   1.0f, 1.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f, -1.0f,   1.0f, 1.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 0.0f, 1.0f},
        {-1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 0.0f, 1.0f},

        // Esquerda (Magenta)
        {-1.0f, -1.0f,  1.0f,   1.0f, 0.0f, 1.0f, 1.0f},
        {-1.0f,  1.0f,  1.0f,   1.0f, 0.0f, 1.0f, 1.0f},
        {-1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 1.0f, 1.0f},

        // Direita (Ciano)
        { 1.0f, -1.0f, -1.0f,   0.0f, 1.0f, 1.0f, 1.0f},
        { 1.0f,  1.0f, -1.0f,   0.0f, 1.0f, 1.0f, 1.0f},
        { 1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 1.0f, 1.0f},
        { 1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 1.0f, 1.0f},
    };

    // 2. Definição dos Índices (12 triângulos)
    unsigned short indices[] = {
        0, 1, 2,  0, 2, 3,       // Frente
        4, 5, 6,  4, 6, 7,       // Trás
        8, 9, 10, 8, 10, 11,     // Topo
        12, 13, 14, 12, 14, 15,  // Base
        16, 17, 18, 16, 18, 19,  // Esquerda
        20, 21, 22, 20, 22, 23   // Direita
    };

    // 3. Criar Vertex Buffer
    D3D11_BUFFER_DESC bd = {0};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VertexPrimitivo) * 24; // 24 vértices
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA initData = {0};
    initData.pSysMem = vertices;
    
    if (FAILED(dev->CreateBuffer(&bd, &initData, outVB))) return false;

    // 4. Criar Index Buffer
    bd.ByteWidth = sizeof(unsigned short) * 36; // 36 índices
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    
    if (FAILED(dev->CreateBuffer(&bd, &initData, outIB))) return false;

    return true;
}

#endif