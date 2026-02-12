// player.hpp
// Sistema de players para JPGame - Suporta cor sólida ou textura (PNG/JPG)

#ifndef PLAYER_HPP
#define PLAYER_HPP

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
extern ID3D11RenderTargetView* g_mainRenderTargetView;

// Forward declaration de texturas.hpp
struct Textura;
int carregarTextura(const std::string& caminho);
Textura* getTextura(int id);
ID3D11VertexShader* getTexturaVertexShader();
ID3D11PixelShader* getTexturaPixelShader();
ID3D11InputLayout* getTexturaInputLayout();
ID3D11SamplerState* getTexturaSampler();
ID3D11BlendState* getTexturaBlendState();

// === ESTRUTURA DO PLAYER ===
struct Player {
    int id;
    float x, y;                     // Posição atual
    int largura, altura;            // Tamanho do quadrado
    float r, g, b;                  // Cor (0.0 - 1.0) - usado se não tem textura
    float velX, velY;               // Velocidade em pixels
    char teclaW, teclaA, teclaS, teclaD;  // Teclas de movimento
    bool ativo;                     // Se está visível/ativo
    
    // Textura (se usar imagem em vez de cor)
    bool usaTextura;
    int texturaId;
    
    // Espelhamento
    bool espelharH;                 // Espelhar horizontalmente
    bool espelharV;                 // Espelhar verticalmente
    
    // Hitbox para colisão
    bool temHitbox;
    float hitboxLargura, hitboxAltura;
    
    // Recursos D3D11
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* constantBuffer;
};

// === ESTADO GLOBAL DOS PLAYERS ===
static std::map<int, Player*> g_players;
static std::vector<int> g_playersParaDesenhar;
static int g_nextPlayerId = 100;  // Começa em 100 para não conflitar com janela (1)
static std::mutex g_playerMutex;

// Shaders para renderizar players (cor sólida)
static ID3D11VertexShader* g_playerVertexShader = nullptr;
static ID3D11PixelShader* g_playerPixelShader = nullptr;
static ID3D11InputLayout* g_playerInputLayout = nullptr;
static bool g_playerShadersInitialized = false;

// Estrutura do constant buffer (cor sólida)
struct PlayerConstantBuffer {
    float posX, posY, width, height;    // 16 bytes
    float screenW, screenH, pad1, pad2; // 16 bytes
    float r, g, b, a;                   // 16 bytes
};

// Estrutura do constant buffer (textura)
struct PlayerTexturaConstantBuffer {
    float posX, posY, width, height;    // 16 bytes
    float screenW, screenH, pad1, pad2; // 16 bytes
};

// Shader code para players (cor sólida)
static const char* g_playerVertexShaderCode = R"(
cbuffer PlayerCB : register(b0) {
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

static const char* g_playerPixelShaderCode = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

// === FUNÇÕES INTERNAS ===

static bool InitPlayerShaders() {
    if (g_playerShadersInitialized) return true;
    if (!g_pd3dDevice) return false;
    
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr;
    
    hr = D3DCompile(g_playerVertexShaderCode, strlen(g_playerVertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_playerVertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    hr = D3DCompile(g_playerPixelShaderCode, strlen(g_playerPixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        vsBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_playerPixelShader);
    psBlob->Release();
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = g_pd3dDevice->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_playerInputLayout);
    vsBlob->Release();
    
    if (FAILED(hr)) return false;
    
    g_playerShadersInitialized = true;
    return true;
}

static bool CreatePlayerBuffers(Player* player) {
    if (!g_pd3dDevice) return false;
    
    // Vértices com UV para textura (posição + UV)
    // UV normal: (0,0) top-left, (1,1) bottom-right
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
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;
    
    HRESULT hr = g_pd3dDevice->CreateBuffer(&vbDesc, &vbData, &player->vertexBuffer);
    if (FAILED(hr)) return false;
    
    // Constant buffer (usa o maior tamanho para compatibilidade)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(PlayerConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = g_pd3dDevice->CreateBuffer(&cbDesc, nullptr, &player->constantBuffer);
    if (FAILED(hr)) {
        player->vertexBuffer->Release();
        player->vertexBuffer = nullptr;
        return false;
    }
    
    return true;
}

// === FUNÇÕES PÚBLICAS ===

// Cria um novo player com COR SÓLIDA
int criarPlayer(int largura, int altura, int r, int g, int b) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    if (!InitPlayerShaders()) return 0;
    
    Player* player = new Player();
    player->id = g_nextPlayerId++;
    player->x = 0;
    player->y = 0;
    player->largura = largura;
    player->altura = altura;
    player->r = r / 255.0f;
    player->g = g / 255.0f;
    player->b = b / 255.0f;
    player->velX = 1.0f;
    player->velY = 1.0f;
    player->teclaW = 'W';
    player->teclaA = 'A';
    player->teclaS = 'S';
    player->teclaD = 'D';
    player->ativo = false;  // Inativo até posicionar
    player->usaTextura = false;
    player->texturaId = 0;
    player->espelharH = false;
    player->espelharV = false;
    player->temHitbox = false;
    player->hitboxLargura = 0;
    player->hitboxAltura = 0;
    player->vertexBuffer = nullptr;
    player->constantBuffer = nullptr;
    
    if (!CreatePlayerBuffers(player)) {
        delete player;
        return 0;
    }
    
    g_players[player->id] = player;
    return player->id;
}

// Cria um novo player com TEXTURA (imagem PNG/JPG)
int criarPlayerTextura(const std::string& caminho, int largura, int altura) {
    // Carrega a textura primeiro
    int texturaId = carregarTextura(caminho);
    if (texturaId == 0) return 0;
    
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    if (!InitPlayerShaders()) return 0;
    
    Player* player = new Player();
    player->id = g_nextPlayerId++;
    player->x = 0;
    player->y = 0;
    player->largura = largura;
    player->altura = altura;
    player->r = 1.0f;
    player->g = 1.0f;
    player->b = 1.0f;
    player->velX = 1.0f;
    player->velY = 1.0f;
    player->teclaW = 'W';
    player->teclaA = 'A';
    player->teclaS = 'S';
    player->teclaD = 'D';
    player->ativo = false;  // Inativo até posicionar
    player->usaTextura = true;
    player->texturaId = texturaId;
    player->espelharH = false;
    player->espelharV = false;
    player->temHitbox = false;
    player->hitboxLargura = 0;
    player->hitboxAltura = 0;
    player->vertexBuffer = nullptr;
    player->constantBuffer = nullptr;
    
    if (!CreatePlayerBuffers(player)) {
        delete player;
        return 0;
    }
    
    g_players[player->id] = player;
    return player->id;
}

// Posiciona e ativa o player
bool playerPosicionar(int playerId, float x, float y) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    player->x = x;
    player->y = y;
    player->ativo = true;
    
    return true;
}

// Desativa o player
bool playerEncerrar(int playerId) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    player->ativo = false;
    
    return true;
}

// Configura teclas de movimento
bool playerMover(int playerId, const std::string& teclaW, const std::string& teclaA, const std::string& teclaS, const std::string& teclaD) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    
    if (!teclaW.empty()) player->teclaW = toupper(teclaW[0]);
    if (!teclaA.empty()) player->teclaA = toupper(teclaA[0]);
    if (!teclaS.empty()) player->teclaS = toupper(teclaS[0]);
    if (!teclaD.empty()) player->teclaD = toupper(teclaD[0]);
    
    return true;
}

// Define velocidade
bool playerVelocidade(int playerId, float velX, float velY) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    player->velX = velX;
    player->velY = velY;
    
    return true;
}

// Obtém posição X do player
float playerGetX(int playerId) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return 0;
    
    return it->second->x;
}

// Obtém posição Y do player
float playerGetY(int playerId) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return 0;
    
    return it->second->y;
}

// Processa input do player
static void UpdatePlayerInput(Player* player) {
    if (!player->ativo) return;
    
    if (GetAsyncKeyState(player->teclaW) & 0x8000) {
        player->y -= player->velY;
    }
    if (GetAsyncKeyState(player->teclaS) & 0x8000) {
        player->y += player->velY;
    }
    if (GetAsyncKeyState(player->teclaA) & 0x8000) {
        player->x -= player->velX;
    }
    if (GetAsyncKeyState(player->teclaD) & 0x8000) {
        player->x += player->velX;
    }
    
    // Limita aos bounds da janela
    if (player->x < 0) player->x = 0;
    if (player->y < 0) player->y = 0;
    if (player->x + player->largura > g_windowWidth) {
        player->x = (float)(g_windowWidth - player->largura);
    }
    if (player->y + player->altura > g_windowHeight) {
        player->y = (float)(g_windowHeight - player->altura);
    }
}

// Marca player para desenhar
bool marcarPlayerParaDesenhar(int playerId) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    if (!player->ativo) return true;
    
    UpdatePlayerInput(player);
    
    g_playersParaDesenhar.push_back(playerId);
    
    return true;
}

// Desenha um player com COR SÓLIDA
static void DesenharPlayerCor(Player* player) {
    if (!g_pd3dDeviceContext || !player->vertexBuffer || !player->constantBuffer) return;
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = g_pd3dDeviceContext->Map(player->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        PlayerConstantBuffer* cb = (PlayerConstantBuffer*)mapped.pData;
        cb->posX = player->x;
        cb->posY = player->y;
        cb->width = (float)player->largura;
        cb->height = (float)player->altura;
        cb->screenW = (float)g_windowWidth;
        cb->screenH = (float)g_windowHeight;
        cb->pad1 = 0.0f;
        cb->pad2 = 0.0f;
        cb->r = player->r;
        cb->g = player->g;
        cb->b = player->b;
        cb->a = 1.0f;
        g_pd3dDeviceContext->Unmap(player->constantBuffer, 0);
    }
    
    g_pd3dDeviceContext->VSSetShader(g_playerVertexShader, nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(g_playerPixelShader, nullptr, 0);
    g_pd3dDeviceContext->IASetInputLayout(g_playerInputLayout);
    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &player->constantBuffer);
    
    // Stride de 2 floats (só posição, sem UV)
    UINT stride = sizeof(float) * 4;  // Usa 4 porque o buffer tem UV também
    UINT offset = 0;
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &player->vertexBuffer, &stride, &offset);
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    g_pd3dDeviceContext->Draw(6, 0);
}

// Desenha um player com TEXTURA
static void DesenharPlayerTextura(Player* player) {
    if (!g_pd3dDeviceContext || !player->vertexBuffer || !player->constantBuffer) return;
    
    Textura* tex = getTextura(player->texturaId);
    if (!tex || !tex->shaderResourceView) return;
    
    // Atualiza vertex buffer com UVs espelhadas se necessário
    D3D11_MAPPED_SUBRESOURCE mappedVB;
    HRESULT hr = g_pd3dDeviceContext->Map(player->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB);
    if (SUCCEEDED(hr)) {
        float* vertices = (float*)mappedVB.pData;
        
        // Calcula UVs baseado no espelhamento
        float u0 = player->espelharH ? 1.0f : 0.0f;
        float u1 = player->espelharH ? 0.0f : 1.0f;
        float v0 = player->espelharV ? 1.0f : 0.0f;
        float v1 = player->espelharV ? 0.0f : 1.0f;
        
        // Vértice 0: pos(0,0) uv(u0,v0)
        vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = u0; vertices[3] = v0;
        // Vértice 1: pos(1,0) uv(u1,v0)
        vertices[4] = 1.0f; vertices[5] = 0.0f; vertices[6] = u1; vertices[7] = v0;
        // Vértice 2: pos(0,1) uv(u0,v1)
        vertices[8] = 0.0f; vertices[9] = 1.0f; vertices[10] = u0; vertices[11] = v1;
        // Vértice 3: pos(1,0) uv(u1,v0)
        vertices[12] = 1.0f; vertices[13] = 0.0f; vertices[14] = u1; vertices[15] = v0;
        // Vértice 4: pos(1,1) uv(u1,v1)
        vertices[16] = 1.0f; vertices[17] = 1.0f; vertices[18] = u1; vertices[19] = v1;
        // Vértice 5: pos(0,1) uv(u0,v1)
        vertices[20] = 0.0f; vertices[21] = 1.0f; vertices[22] = u0; vertices[23] = v1;
        
        g_pd3dDeviceContext->Unmap(player->vertexBuffer, 0);
    }
    
    // Atualiza constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = g_pd3dDeviceContext->Map(player->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        PlayerTexturaConstantBuffer* cb = (PlayerTexturaConstantBuffer*)mapped.pData;
        cb->posX = player->x;
        cb->posY = player->y;
        cb->width = (float)player->largura;
        cb->height = (float)player->altura;
        cb->screenW = (float)g_windowWidth;
        cb->screenH = (float)g_windowHeight;
        cb->pad1 = 0.0f;
        cb->pad2 = 0.0f;
        g_pd3dDeviceContext->Unmap(player->constantBuffer, 0);
    }
    
    // Configura shaders de textura
    g_pd3dDeviceContext->VSSetShader(getTexturaVertexShader(), nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(getTexturaPixelShader(), nullptr, 0);
    g_pd3dDeviceContext->IASetInputLayout(getTexturaInputLayout());
    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &player->constantBuffer);
    
    // Configura textura e sampler
    g_pd3dDeviceContext->PSSetShaderResources(0, 1, &tex->shaderResourceView);
    ID3D11SamplerState* sampler = getTexturaSampler();
    g_pd3dDeviceContext->PSSetSamplers(0, 1, &sampler);
    
    // Ativa alpha blending para transparência
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->OMSetBlendState(getTexturaBlendState(), blendFactor, 0xffffffff);
    
    // Stride de 4 floats (posição + UV)
    UINT stride = sizeof(float) * 4;
    UINT offset = 0;
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &player->vertexBuffer, &stride, &offset);
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    g_pd3dDeviceContext->Draw(6, 0);
    
    // Desativa blend state após desenhar
    g_pd3dDeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

// Desenha um player (detecta se usa textura ou cor)
static void DesenharPlayer(Player* player) {
    if (!player->ativo) return;
    
    if (player->usaTextura) {
        DesenharPlayerTextura(player);
    } else {
        DesenharPlayerCor(player);
    }
}

// Desenha todos os players
void desenharTodosPlayers() {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    for (int playerId : g_playersParaDesenhar) {
        auto it = g_players.find(playerId);
        if (it != g_players.end()) {
            DesenharPlayer(it->second);
        }
    }
    
    g_playersParaDesenhar.clear();
}

// Atualiza a textura de um player existente
bool playerAtualizarTextura(int playerId, const std::string& caminho) {
    // Carrega a nova textura primeiro (fora do lock)
    int novaTexturaId = carregarTextura(caminho);
    if (novaTexturaId == 0) return false;
    
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    player->usaTextura = true;
    player->texturaId = novaTexturaId;
    
    return true;
}

// Atualiza a textura e o tamanho de um player existente
bool playerAtualizarTexturaComTamanho(int playerId, const std::string& caminho, int largura, int altura) {
    // Carrega a nova textura primeiro (fora do lock)
    int novaTexturaId = carregarTextura(caminho);
    if (novaTexturaId == 0) return false;
    
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    player->usaTextura = true;
    player->texturaId = novaTexturaId;
    player->largura = largura;
    player->altura = altura;
    
    return true;
}

// Espelha o player horizontalmente e/ou verticalmente
bool playerEspelhar(int playerId, bool horizontal, bool vertical) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    auto it = g_players.find(playerId);
    if (it == g_players.end()) return false;
    
    Player* player = it->second;
    player->espelharH = horizontal;
    player->espelharV = vertical;
    
    return true;
}

// Verifica se uma tecla está pressionada
bool teclaPressionada(const std::string& tecla) {
    if (tecla.empty()) return false;
    
    char c = toupper(tecla[0]);
    return (GetAsyncKeyState(c) & 0x8000) != 0;
}

// Verifica se é player
bool isPlayer(int id) {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    return g_players.find(id) != g_players.end();
}

// Obtém ponteiro para player (usado por colisoes.hpp)
Player* getPlayer(int id) {
    auto it = g_players.find(id);
    if (it == g_players.end()) return nullptr;
    return it->second;
}

// Limpa todos os players
void cleanupPlayers() {
    std::lock_guard<std::mutex> lock(g_playerMutex);
    
    for (auto& pair : g_players) {
        Player* player = pair.second;
        if (player->vertexBuffer) player->vertexBuffer->Release();
        if (player->constantBuffer) player->constantBuffer->Release();
        delete player;
    }
    g_players.clear();
    
    if (g_playerVertexShader) { g_playerVertexShader->Release(); g_playerVertexShader = nullptr; }
    if (g_playerPixelShader) { g_playerPixelShader->Release(); g_playerPixelShader = nullptr; }
    if (g_playerInputLayout) { g_playerInputLayout->Release(); g_playerInputLayout = nullptr; }
    g_playerShadersInitialized = false;
}

#endif // PLAYER_HPP