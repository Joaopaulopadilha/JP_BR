// janela.hpp
// Sistema de janelas DirectX 11 para JPGame - Renderização controlada pelo usuário

#ifndef JANELA_HPP
#define JANELA_HPP

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "src/stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// === DEBUG ===
#define DEBUG_MODE 0

static void DebugLog(const std::string& msg) {
    #if DEBUG_MODE
    MessageBoxA(nullptr, msg.c_str(), "JPGame Debug", MB_OK | MB_ICONINFORMATION);
    #endif
}

// === ESTADO GLOBAL DA JANELA (exportados para player.hpp) ===
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_hwnd = nullptr;
int g_windowWidth = 800;
int g_windowHeight = 600;

// Estado interno
static WNDCLASSEXW g_wc = {};
static std::atomic<bool> g_running{false};
static std::atomic<bool> g_shouldClose{false};
static std::atomic<bool> g_initialized{false};
static std::mutex g_deviceMutex;

// Cor de fundo padrão (cinza claro Win32)
static std::atomic<int> g_bgR{240};
static std::atomic<int> g_bgG{240};
static std::atomic<int> g_bgB{240};

// Imagem de fundo
static ID3D11ShaderResourceView* g_backgroundTexture = nullptr;
static ID3D11SamplerState* g_samplerState = nullptr;
static ID3D11VertexShader* g_vertexShader = nullptr;
static ID3D11PixelShader* g_pixelShader = nullptr;
static ID3D11InputLayout* g_inputLayout = nullptr;
static ID3D11Buffer* g_vertexBuffer = nullptr;
static ID3D11Buffer* g_indexBuffer = nullptr;
static std::atomic<bool> g_useBackgroundImage{false};
static int g_backgroundWidth = 0;
static int g_backgroundHeight = 0;

// Forward declarations
static void CreateRenderTarget();
static void CleanupRenderTarget();
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static bool CreateBackgroundQuad();
static void RenderBackground();

// Vertex structure
struct Vertex {
    float pos[3];
    float uv[2];
};

// Shader code
static const char* g_vertexShaderCode = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.uv = input.uv;
    return output;
}
)";

static const char* g_pixelShaderCode = R"(
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

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            g_windowWidth = LOWORD(lParam);
            g_windowHeight = HIWORD(lParam);
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_windowWidth, g_windowHeight, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_CLOSE:
        g_running = false;
        g_shouldClose = true;
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        exit(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, 
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_vertexBuffer) { g_vertexBuffer->Release(); g_vertexBuffer = nullptr; }
    if (g_indexBuffer) { g_indexBuffer->Release(); g_indexBuffer = nullptr; }
    if (g_inputLayout) { g_inputLayout->Release(); g_inputLayout = nullptr; }
    if (g_vertexShader) { g_vertexShader->Release(); g_vertexShader = nullptr; }
    if (g_pixelShader) { g_pixelShader->Release(); g_pixelShader = nullptr; }
    if (g_samplerState) { g_samplerState->Release(); g_samplerState = nullptr; }
    if (g_backgroundTexture) { g_backgroundTexture->Release(); g_backgroundTexture = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static bool CreateBackgroundQuad() {
    DebugLog("CreateBackgroundQuad: Iniciando...");
    
    // Compilar vertex shader
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    HRESULT hr = D3DCompile(g_vertexShaderCode, strlen(g_vertexShaderCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao compilar vertex shader");
        if (errorBlob) errorBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao criar vertex shader");
        vsBlob->Release();
        return false;
    }
    
    // Compilar pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(g_pixelShaderCode, strlen(g_pixelShaderCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao compilar pixel shader");
        if (errorBlob) errorBlob->Release();
        vsBlob->Release();
        return false;
    }
    
    hr = g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);
    psBlob->Release();
    
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao criar pixel shader");
        vsBlob->Release();
        return false;
    }
    
    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout);
    vsBlob->Release();
    
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao criar input layout");
        return false;
    }
    
    // Criar quad fullscreen
    Vertex vertices[] = {
        { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
        { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    };
    
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;
    
    hr = g_pd3dDevice->CreateBuffer(&vbDesc, &vbData, &g_vertexBuffer);
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao criar vertex buffer");
        return false;
    }
    
    // Indices
    unsigned short indices[] = { 0, 1, 2, 0, 2, 3 };
    
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;
    
    hr = g_pd3dDevice->CreateBuffer(&ibDesc, &ibData, &g_indexBuffer);
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao criar index buffer");
        return false;
    }
    
    // Sampler state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_samplerState);
    if (FAILED(hr)) {
        DebugLog("CreateBackgroundQuad: FALHOU ao criar sampler state");
        return false;
    }
    
    DebugLog("CreateBackgroundQuad: SUCESSO!");
    return true;
}

static void RenderBackground() {
    if (!g_useBackgroundImage || !g_backgroundTexture) return;
    
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    
    g_pd3dDeviceContext->IASetInputLayout(g_inputLayout);
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &g_vertexBuffer, &stride, &offset);
    g_pd3dDeviceContext->IASetIndexBuffer(g_indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    g_pd3dDeviceContext->VSSetShader(g_vertexShader, nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(g_pixelShader, nullptr, 0);
    g_pd3dDeviceContext->PSSetShaderResources(0, 1, &g_backgroundTexture);
    g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_samplerState);
    
    g_pd3dDeviceContext->DrawIndexed(6, 0, 0);
}

// === FUNÇÕES PÚBLICAS ===

// Cria janela
int criarJanela(const std::string& titulo, int largura, int altura) {
    // Se já tem uma janela ativa, não cria outra
    if (g_running) return 0;
    
    g_shouldClose = false;
    g_initialized = false;
    g_running = false;
    g_useBackgroundImage = false;
    g_windowWidth = largura;
    g_windowHeight = altura;
    
    // Converte título para wide string
    int len = MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, NULL, 0);
    std::wstring wtitulo(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, &wtitulo[0], len);
    
    // Cria janela Win32
    g_wc = { sizeof(g_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"JPGame", nullptr };
    RegisterClassExW(&g_wc);
    g_hwnd = CreateWindowW(g_wc.lpszClassName, wtitulo.c_str(), WS_OVERLAPPEDWINDOW, 
        100, 100, largura, altura, nullptr, nullptr, g_wc.hInstance, nullptr);

    // Inicializa DirectX
    if (!CreateDeviceD3D(g_hwnd)) {
        DebugLog("criarJanela: FALHOU ao criar device D3D");
        CleanupDeviceD3D();
        UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
        return 0;
    }

    // Cria recursos para background
    if (!CreateBackgroundQuad()) {
        DebugLog("criarJanela: FALHOU ao criar background quad");
        CleanupDeviceD3D();
        UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
        return 0;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    g_running = true;
    g_initialized = true;
    
    return 1;
}

// Define cor de fundo
bool janelaCorFundo(int r, int g, int b) {
    // Clamp valores entre 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    g_bgR = r;
    g_bgG = g;
    g_bgB = b;
    
    // Desabilita imagem de fundo quando cor é setada
    g_useBackgroundImage = false;
    
    return true;
}

// Define imagem de fundo
bool janelaImagemFundo(const std::string& caminho) {
    std::lock_guard<std::mutex> lock(g_deviceMutex);
    
    std::stringstream ss;
    ss << "janelaImagemFundo: Iniciando...\nCaminho: " << caminho;
    DebugLog(ss.str());
    
    if (!g_pd3dDevice) {
        DebugLog("janelaImagemFundo: ERRO - Device D3D nao existe!");
        return false;
    }
    
    if (!g_running) {
        DebugLog("janelaImagemFundo: ERRO - Janela nao esta rodando!");
        return false;
    }
    
    // Carrega imagem com stb_image
    int width, height, channels;
    unsigned char* data = stbi_load(caminho.c_str(), &width, &height, &channels, 4);
    
    if (!data) {
        // Tenta com caminho absoluto
        char fullPath[MAX_PATH];
        GetFullPathNameA(caminho.c_str(), MAX_PATH, fullPath, nullptr);
        
        ss.str("");
        ss << "janelaImagemFundo: Primeira tentativa falhou.\nTentando caminho absoluto:\n" << fullPath;
        DebugLog(ss.str());
        
        data = stbi_load(fullPath, &width, &height, &channels, 4);
        if (!data) {
            ss.str("");
            ss << "janelaImagemFundo: ERRO - Nao foi possivel carregar a imagem!\nCaminho relativo: " << caminho << "\nCaminho absoluto: " << fullPath;
            DebugLog(ss.str());
            return false;
        }
    }
    
    ss.str("");
    ss << "janelaImagemFundo: Imagem carregada!\nDimensoes: " << width << "x" << height << "\nCanais: " << channels;
    DebugLog(ss.str());
    
    g_backgroundWidth = width;
    g_backgroundHeight = height;
    
    // Libera textura anterior se existir
    if (g_backgroundTexture) {
        g_backgroundTexture->Release();
        g_backgroundTexture = nullptr;
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
    
    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &texture);
    
    stbi_image_free(data);
    
    if (FAILED(hr)) {
        ss.str("");
        ss << "janelaImagemFundo: ERRO ao criar textura D3D11!\nHRESULT: 0x" << std::hex << hr;
        DebugLog(ss.str());
        return false;
    }
    
    DebugLog("janelaImagemFundo: Textura D3D11 criada!");
    
    // Cria shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    
    hr = g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, &g_backgroundTexture);
    texture->Release();
    
    if (FAILED(hr)) {
        ss.str("");
        ss << "janelaImagemFundo: ERRO ao criar shader resource view!\nHRESULT: 0x" << std::hex << hr;
        DebugLog(ss.str());
        return false;
    }
    
    DebugLog("janelaImagemFundo: Shader resource view criada!");
    
    g_useBackgroundImage = true;
    
    DebugLog("janelaImagemFundo: SUCESSO! Imagem definida como fundo.");
    
    return true;
}

// Verifica se janela está rodando
bool janelaRodando() {
    return g_running.load();
}

// Fecha janela
bool fecharJanela() {
    if (!g_running) return false;
    
    g_shouldClose = true;
    g_running = false;
    
    // Cleanup
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
    
    g_hwnd = nullptr;
    
    return true;
}

// === FUNÇÕES PARA RENDERIZAÇÃO ===

// Exibe o frame completo (limpa, desenha fundo, apresenta)
// Esta função é chamada por jpgame.exibir(jogo)
bool janelaExibir() {
    if (!g_running.load() || !g_pd3dDeviceContext || !g_pSwapChain || !g_mainRenderTargetView) return false;
    
    // Processa mensagens da janela
    MSG msg;
    while (PeekMessage(&msg, g_hwnd, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Configura viewport
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)g_windowWidth;
    viewport.Height = (FLOAT)g_windowHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_pd3dDeviceContext->RSSetViewports(1, &viewport);

    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    
    // Limpa com cor de fundo
    float clear_color[4] = { 
        g_bgR.load() / 255.0f, 
        g_bgG.load() / 255.0f, 
        g_bgB.load() / 255.0f, 
        1.0f 
    };
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
    
    // Renderiza imagem de fundo se existir
    RenderBackground();
    
    return true;
}

// Apresenta o frame na tela (swap buffers)
// Chamado após desenhar todos os elementos
bool janelaApresentar() {
    if (!g_running.load() || !g_pSwapChain) return false;
    
    g_pSwapChain->Present(1, 0);
    return true;
}

// Verifica se é ID de janela (sempre 1 por enquanto)
bool isJanela(int id) {
    return id == 1 && g_running.load();
}

#endif // JANELA_HPP