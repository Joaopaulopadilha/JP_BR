// texturas.hpp
// Sistema de texturas para JPGame - Carrega e gerencia imagens PNG/JPG com suporte a transparência

#ifndef TEXTURAS_HPP
#define TEXTURAS_HPP

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <map>
#include <string>
#include <mutex>

// stb_image já foi incluído com STB_IMAGE_IMPLEMENTATION no janela.hpp
// Aqui só declaramos as funções que precisamos (extern)
extern "C" {
    unsigned char* stbi_load(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
}

// Variáveis globais do janela.hpp
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern int g_windowWidth;
extern int g_windowHeight;

// === ESTRUTURA DE TEXTURA ===
struct Textura {
    int id;
    std::string caminho;
    int larguraOriginal, alturaOriginal;  // Tamanho original da imagem
    ID3D11ShaderResourceView* shaderResourceView;
    ID3D11Texture2D* texture2D;
};

// === ESTADO GLOBAL DAS TEXTURAS ===
static std::map<int, Textura*> g_texturas;
static std::map<std::string, int> g_texturasPorCaminho;  // Cache por caminho
static int g_nextTexturaId = 10000;  // Começa em 10000 para não conflitar
static std::mutex g_texturaMutex;

// Shaders para renderizar texturas (com alpha)
static ID3D11VertexShader* g_texturaVertexShader = nullptr;
static ID3D11PixelShader* g_texturaPixelShader = nullptr;
static ID3D11InputLayout* g_texturaInputLayout = nullptr;
static ID3D11SamplerState* g_texturaSampler = nullptr;
static ID3D11BlendState* g_texturaBlendState = nullptr;
static bool g_texturaShadersInitialized = false;

// Estrutura do constant buffer para texturas
struct TexturaConstantBuffer {
    float posX, posY, width, height;    // 16 bytes
    float screenW, screenH, pad1, pad2; // 16 bytes
};

// Shader code para texturas com alpha
static const char* g_texturaVertexShaderCode = R"(
cbuffer TexturaCB : register(b0) {
    float4 posSize;    // posX, posY, width, height
    float4 screen;     // screenW, screenH, pad, pad
};

struct VS_INPUT {
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
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
    output.uv = input.uv;
    return output;
}
)";

static const char* g_texturaPixelShaderCode = R"(
Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target {
    return tex.Sample(samplerState, input.uv);
}
)";

// === FUNÇÕES INTERNAS ===

static bool InitTexturaShaders() {
    if (g_texturaShadersInitialized) return true;
    if (!g_pd3dDevice) return false;
    
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr;
    
    // Compila vertex shader
    hr = D3DCompile(g_texturaVertexShaderCode, strlen(g_texturaVertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_texturaVertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    // Compila pixel shader
    hr = D3DCompile(g_texturaPixelShaderCode, strlen(g_texturaPixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        vsBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_texturaPixelShader);
    psBlob->Release();
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    // Input layout (posição + UV)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_texturaInputLayout);
    vsBlob->Release();
    
    if (FAILED(hr)) return false;
    
    // Sampler state
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
    hr = g_pd3dDevice->CreateSamplerState(&samplerDesc, &g_texturaSampler);
    if (FAILED(hr)) return false;
    
    // Blend state para transparência (alpha blending)
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    hr = g_pd3dDevice->CreateBlendState(&blendDesc, &g_texturaBlendState);
    if (FAILED(hr)) return false;
    
    g_texturaShadersInitialized = true;
    return true;
}

// === FUNÇÕES PÚBLICAS ===

// Carrega uma textura de arquivo (PNG, JPG, etc)
// Retorna ID da textura ou 0 se falhar
int carregarTextura(const std::string& caminho) {
    std::lock_guard<std::mutex> lock(g_texturaMutex);
    
    if (!InitTexturaShaders()) return 0;
    if (!g_pd3dDevice) return 0;
    
    // Verifica se já está em cache
    auto cacheIt = g_texturasPorCaminho.find(caminho);
    if (cacheIt != g_texturasPorCaminho.end()) {
        return cacheIt->second;
    }
    
    // Carrega imagem com stb_image (força 4 canais para RGBA)
    int width, height, channels;
    unsigned char* data = stbi_load(caminho.c_str(), &width, &height, &channels, 4);
    
    if (!data) {
        // Tenta com caminho absoluto
        char fullPath[MAX_PATH];
        GetFullPathNameA(caminho.c_str(), MAX_PATH, fullPath, nullptr);
        data = stbi_load(fullPath, &width, &height, &channels, 4);
        
        if (!data) {
            return 0;
        }
    }
    
    // Cria textura D3D11
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;
    initData.SysMemPitch = width * 4;
    
    ID3D11Texture2D* texture2D = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &texture2D);
    
    stbi_image_free(data);
    
    if (FAILED(hr)) {
        return 0;
    }
    
    // Cria shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    
    ID3D11ShaderResourceView* shaderResourceView = nullptr;
    hr = g_pd3dDevice->CreateShaderResourceView(texture2D, &srvDesc, &shaderResourceView);
    
    if (FAILED(hr)) {
        texture2D->Release();
        return 0;
    }
    
    // Cria estrutura de textura
    Textura* textura = new Textura();
    textura->id = g_nextTexturaId++;
    textura->caminho = caminho;
    textura->larguraOriginal = width;
    textura->alturaOriginal = height;
    textura->texture2D = texture2D;
    textura->shaderResourceView = shaderResourceView;
    
    g_texturas[textura->id] = textura;
    g_texturasPorCaminho[caminho] = textura->id;
    
    return textura->id;
}

// Obtém textura por ID
Textura* getTextura(int id) {
    auto it = g_texturas.find(id);
    if (it == g_texturas.end()) return nullptr;
    return it->second;
}

// Verifica se é uma textura válida
bool isTextura(int id) {
    return g_texturas.find(id) != g_texturas.end();
}

// Obtém shaders e estados para renderização
ID3D11VertexShader* getTexturaVertexShader() { return g_texturaVertexShader; }
ID3D11PixelShader* getTexturaPixelShader() { return g_texturaPixelShader; }
ID3D11InputLayout* getTexturaInputLayout() { return g_texturaInputLayout; }
ID3D11SamplerState* getTexturaSampler() { return g_texturaSampler; }
ID3D11BlendState* getTexturaBlendState() { return g_texturaBlendState; }

// Limpa todas as texturas
void cleanupTexturas() {
    std::lock_guard<std::mutex> lock(g_texturaMutex);
    
    for (auto& pair : g_texturas) {
        Textura* tex = pair.second;
        if (tex->shaderResourceView) tex->shaderResourceView->Release();
        if (tex->texture2D) tex->texture2D->Release();
        delete tex;
    }
    g_texturas.clear();
    g_texturasPorCaminho.clear();
    
    if (g_texturaVertexShader) { g_texturaVertexShader->Release(); g_texturaVertexShader = nullptr; }
    if (g_texturaPixelShader) { g_texturaPixelShader->Release(); g_texturaPixelShader = nullptr; }
    if (g_texturaInputLayout) { g_texturaInputLayout->Release(); g_texturaInputLayout = nullptr; }
    if (g_texturaSampler) { g_texturaSampler->Release(); g_texturaSampler = nullptr; }
    if (g_texturaBlendState) { g_texturaBlendState->Release(); g_texturaBlendState = nullptr; }
    g_texturaShadersInitialized = false;
}

#endif // TEXTURAS_HPP