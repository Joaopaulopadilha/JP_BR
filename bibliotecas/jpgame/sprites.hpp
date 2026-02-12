// sprites.hpp
// Sistema de sprites genéricos para JPGame - Suporta cor sólida ou textura (PNG/JPG)

#ifndef SPRITES_HPP
#define SPRITES_HPP

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <map>
#include <string>
#include <mutex>

// Variáveis globais do janela.hpp
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern int g_windowWidth;
extern int g_windowHeight;

// Forward declaration de texturas.hpp
struct Textura;
int carregarTextura(const std::string& caminho);
Textura* getTextura(int id);
ID3D11VertexShader* getTexturaVertexShader();
ID3D11PixelShader* getTexturaPixelShader();
ID3D11InputLayout* getTexturaInputLayout();
ID3D11SamplerState* getTexturaSampler();
ID3D11BlendState* getTexturaBlendState();

// === ESTRUTURA DO SPRITE ===
struct Sprite {
    int id;
    float x, y;                 // Posição atual
    int largura, altura;        // Tamanho do quadrado
    float r, g, b;              // Cor (0.0 - 1.0)
    bool ativo;                 // Se está visível/ativo
    
    // Textura (se usar imagem em vez de cor)
    bool usaTextura;
    int texturaId;
    
    // Hitbox para colisão (pode ser diferente do tamanho visual)
    bool temHitbox;
    float hitboxLargura, hitboxAltura;
    
    // Recursos D3D11
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* constantBuffer;
};

// === ESTADO GLOBAL DOS SPRITES ===
static std::map<int, Sprite*> g_sprites;
static std::vector<int> g_spritesParaDesenhar;
static int g_nextSpriteId = 1000;  // Começa em 1000 para não conflitar com janela (1) e players (100+)
static std::mutex g_spriteMutex;

// Shaders para renderizar sprites (cor sólida)
static ID3D11VertexShader* g_spriteVertexShader = nullptr;
static ID3D11PixelShader* g_spritePixelShader = nullptr;
static ID3D11InputLayout* g_spriteInputLayout = nullptr;
static bool g_spriteShadersInitialized = false;

// Estrutura do constant buffer (cor sólida)
struct SpriteConstantBuffer {
    float posX, posY, width, height;    // 16 bytes
    float screenW, screenH, pad1, pad2; // 16 bytes
    float r, g, b, a;                   // 16 bytes
};

// Estrutura do constant buffer (textura)
struct SpriteTexturaConstantBuffer {
    float posX, posY, width, height;    // 16 bytes
    float screenW, screenH, pad1, pad2; // 16 bytes
};

// Shader code para sprites (cor sólida)
static const char* g_spriteVertexShaderCode = R"(
cbuffer SpriteCB : register(b0) {
    float4 posSize;    // posX, posY, width, height
    float4 screen;     // screenW, screenH, pad, pad
    float4 color;      // r, g, b, a
};

struct VS_INPUT {
    float2 pos : POSITION;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    
    float posX = posSize.x;
    float posY = posSize.y;
    float width = posSize.z;
    float height = posSize.w;
    float screenW = screen.x;
    float screenH = screen.y;
    
    float x = ((posX + input.pos.x * width) / screenW) * 2.0f - 1.0f;
    float y = 1.0f - ((posY + input.pos.y * height) / screenH) * 2.0f;
    
    output.pos = float4(x, y, 0.0f, 1.0f);
    output.color = color;
    return output;
}
)";

static const char* g_spritePixelShaderCode = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

// === FUNÇÕES INTERNAS ===

static bool InitSpriteShaders() {
    if (g_spriteShadersInitialized) return true;
    if (!g_pd3dDevice) return false;
    
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr;
    
    // Compila vertex shader
    hr = D3DCompile(g_spriteVertexShaderCode, strlen(g_spriteVertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_spriteVertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    // Compila pixel shader
    hr = D3DCompile(g_spritePixelShaderCode, strlen(g_spritePixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        vsBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_spritePixelShader);
    psBlob->Release();
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = g_pd3dDevice->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_spriteInputLayout);
    vsBlob->Release();
    
    if (FAILED(hr)) return false;
    
    g_spriteShadersInitialized = true;
    return true;
}

static bool CreateSpriteBuffers(Sprite* sprite) {
    if (!g_pd3dDevice) return false;
    
    // Vértices com UV para textura (posição + UV)
    float vertices[] = {
        // pos.x, pos.y, uv.x, uv.y
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;
    
    HRESULT hr = g_pd3dDevice->CreateBuffer(&vbDesc, &vbData, &sprite->vertexBuffer);
    if (FAILED(hr)) return false;
    
    // Constant buffer (usa o maior tamanho para compatibilidade)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(SpriteConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = g_pd3dDevice->CreateBuffer(&cbDesc, nullptr, &sprite->constantBuffer);
    if (FAILED(hr)) {
        sprite->vertexBuffer->Release();
        sprite->vertexBuffer = nullptr;
        return false;
    }
    
    return true;
}

// === FUNÇÕES PÚBLICAS ===

// Cria um novo sprite com COR SÓLIDA
int criarSprite(int largura, int altura, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    if (!InitSpriteShaders()) return 0;
    
    Sprite* sprite = new Sprite();
    sprite->id = g_nextSpriteId++;
    sprite->x = 0;
    sprite->y = 0;
    sprite->largura = largura;
    sprite->altura = altura;
    sprite->r = r / 255.0f;
    sprite->g = g / 255.0f;
    sprite->b = b / 255.0f;
    sprite->ativo = false;  // Inativo até posicionar
    sprite->usaTextura = false;
    sprite->texturaId = 0;
    sprite->temHitbox = false;
    sprite->hitboxLargura = 0;
    sprite->hitboxAltura = 0;
    sprite->vertexBuffer = nullptr;
    sprite->constantBuffer = nullptr;
    
    if (!CreateSpriteBuffers(sprite)) {
        delete sprite;
        return 0;
    }
    
    g_sprites[sprite->id] = sprite;
    return sprite->id;
}

// Cria um novo sprite com TEXTURA (imagem PNG/JPG)
int criarSpriteTextura(const std::string& caminho, int largura, int altura) {
    // Carrega a textura primeiro
    int texturaId = carregarTextura(caminho);
    if (texturaId == 0) return 0;
    
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    if (!InitSpriteShaders()) return 0;
    
    Sprite* sprite = new Sprite();
    sprite->id = g_nextSpriteId++;
    sprite->x = 0;
    sprite->y = 0;
    sprite->largura = largura;
    sprite->altura = altura;
    sprite->r = 1.0f;
    sprite->g = 1.0f;
    sprite->b = 1.0f;
    sprite->ativo = false;  // Inativo até posicionar
    sprite->usaTextura = true;
    sprite->texturaId = texturaId;
    sprite->temHitbox = false;
    sprite->hitboxLargura = 0;
    sprite->hitboxAltura = 0;
    sprite->vertexBuffer = nullptr;
    sprite->constantBuffer = nullptr;
    
    if (!CreateSpriteBuffers(sprite)) {
        delete sprite;
        return 0;
    }
    
    g_sprites[sprite->id] = sprite;
    return sprite->id;
}

// Posiciona e ativa o sprite
bool spritePosicionar(int spriteId, float x, float y) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    auto it = g_sprites.find(spriteId);
    if (it == g_sprites.end()) return false;
    
    Sprite* sprite = it->second;
    sprite->x = x;
    sprite->y = y;
    sprite->ativo = true;
    
    return true;
}

// Desativa o sprite
bool spriteEncerrar(int spriteId) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    auto it = g_sprites.find(spriteId);
    if (it == g_sprites.end()) return false;
    
    Sprite* sprite = it->second;
    sprite->ativo = false;
    
    return true;
}

// Obtém posição X do sprite
float spriteGetX(int spriteId) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    auto it = g_sprites.find(spriteId);
    if (it == g_sprites.end()) return 0;
    
    return it->second->x;
}

// Obtém posição Y do sprite
float spriteGetY(int spriteId) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    auto it = g_sprites.find(spriteId);
    if (it == g_sprites.end()) return 0;
    
    return it->second->y;
}

// Marca sprite para desenhar
bool marcarSpriteParaDesenhar(int spriteId) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    auto it = g_sprites.find(spriteId);
    if (it == g_sprites.end()) return false;
    
    Sprite* sprite = it->second;
    if (!sprite->ativo) return true;
    
    g_spritesParaDesenhar.push_back(spriteId);
    
    return true;
}

// Desenha um sprite com COR SÓLIDA
static void DesenharSpriteCor(Sprite* sprite) {
    if (!g_pd3dDeviceContext || !sprite->vertexBuffer || !sprite->constantBuffer) return;
    
    // Atualiza constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = g_pd3dDeviceContext->Map(sprite->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        SpriteConstantBuffer* cb = (SpriteConstantBuffer*)mapped.pData;
        cb->posX = sprite->x;
        cb->posY = sprite->y;
        cb->width = (float)sprite->largura;
        cb->height = (float)sprite->altura;
        cb->screenW = (float)g_windowWidth;
        cb->screenH = (float)g_windowHeight;
        cb->pad1 = 0.0f;
        cb->pad2 = 0.0f;
        cb->r = sprite->r;
        cb->g = sprite->g;
        cb->b = sprite->b;
        cb->a = 1.0f;
        g_pd3dDeviceContext->Unmap(sprite->constantBuffer, 0);
    }
    
    // Configura pipeline
    g_pd3dDeviceContext->VSSetShader(g_spriteVertexShader, nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(g_spritePixelShader, nullptr, 0);
    g_pd3dDeviceContext->IASetInputLayout(g_spriteInputLayout);
    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &sprite->constantBuffer);
    
    UINT stride = sizeof(float) * 4;  // Usa 4 porque o buffer tem UV também
    UINT offset = 0;
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &sprite->vertexBuffer, &stride, &offset);
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    g_pd3dDeviceContext->Draw(6, 0);
}

// Desenha um sprite com TEXTURA
static void DesenharSpriteTextura(Sprite* sprite) {
    if (!g_pd3dDeviceContext || !sprite->vertexBuffer || !sprite->constantBuffer) return;
    
    Textura* tex = getTextura(sprite->texturaId);
    if (!tex || !tex->shaderResourceView) return;
    
    // Atualiza constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = g_pd3dDeviceContext->Map(sprite->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        SpriteTexturaConstantBuffer* cb = (SpriteTexturaConstantBuffer*)mapped.pData;
        cb->posX = sprite->x;
        cb->posY = sprite->y;
        cb->width = (float)sprite->largura;
        cb->height = (float)sprite->altura;
        cb->screenW = (float)g_windowWidth;
        cb->screenH = (float)g_windowHeight;
        cb->pad1 = 0.0f;
        cb->pad2 = 0.0f;
        g_pd3dDeviceContext->Unmap(sprite->constantBuffer, 0);
    }
    
    // Configura shaders de textura
    g_pd3dDeviceContext->VSSetShader(getTexturaVertexShader(), nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(getTexturaPixelShader(), nullptr, 0);
    g_pd3dDeviceContext->IASetInputLayout(getTexturaInputLayout());
    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &sprite->constantBuffer);
    
    // Configura textura e sampler
    g_pd3dDeviceContext->PSSetShaderResources(0, 1, &tex->shaderResourceView);
    ID3D11SamplerState* sampler = getTexturaSampler();
    g_pd3dDeviceContext->PSSetSamplers(0, 1, &sampler);
    
    // Ativa alpha blending para transparência
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->OMSetBlendState(getTexturaBlendState(), blendFactor, 0xffffffff);
    
    UINT stride = sizeof(float) * 4;  // posição + UV
    UINT offset = 0;
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &sprite->vertexBuffer, &stride, &offset);
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    g_pd3dDeviceContext->Draw(6, 0);
    
    // Desativa blend state após desenhar
    g_pd3dDeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

// Desenha um sprite (detecta se usa textura ou cor)
static void DesenharSprite(Sprite* sprite) {
    if (!sprite->ativo) return;
    
    if (sprite->usaTextura) {
        DesenharSpriteTextura(sprite);
    } else {
        DesenharSpriteCor(sprite);
    }
}

// Desenha todos os sprites marcados
void desenharTodosSprites() {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    for (int spriteId : g_spritesParaDesenhar) {
        auto it = g_sprites.find(spriteId);
        if (it != g_sprites.end()) {
            DesenharSprite(it->second);
        }
    }
    
    g_spritesParaDesenhar.clear();
}

// Atualiza a textura de um sprite existente
bool spriteAtualizarTextura(int spriteId, const std::string& caminho) {
    // Carrega a nova textura primeiro (fora do lock)
    int novaTexturaId = carregarTextura(caminho);
    if (novaTexturaId == 0) return false;
    
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    auto it = g_sprites.find(spriteId);
    if (it == g_sprites.end()) return false;
    
    Sprite* sprite = it->second;
    sprite->usaTextura = true;
    sprite->texturaId = novaTexturaId;
    
    return true;
}

// Verifica se é um sprite
bool isSprite(int id) {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    return g_sprites.find(id) != g_sprites.end();
}

// Obtém ponteiro para sprite (usado por colisoes.hpp)
Sprite* getSprite(int id) {
    auto it = g_sprites.find(id);
    if (it == g_sprites.end()) return nullptr;
    return it->second;
}

// Limpa todos os sprites
void cleanupSprites() {
    std::lock_guard<std::mutex> lock(g_spriteMutex);
    
    for (auto& pair : g_sprites) {
        Sprite* sprite = pair.second;
        if (sprite->vertexBuffer) sprite->vertexBuffer->Release();
        if (sprite->constantBuffer) sprite->constantBuffer->Release();
        delete sprite;
    }
    g_sprites.clear();
    
    if (g_spriteVertexShader) { g_spriteVertexShader->Release(); g_spriteVertexShader = nullptr; }
    if (g_spritePixelShader) { g_spritePixelShader->Release(); g_spritePixelShader = nullptr; }
    if (g_spriteInputLayout) { g_spriteInputLayout->Release(); g_spriteInputLayout = nullptr; }
    g_spriteShadersInitialized = false;
}

#endif // SPRITES_HPP