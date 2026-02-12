// src/janela.hpp
// Gerenciamento de janela e DirectX para r3dgame
// Atualizado com Sistema de Câmera e Objetos
// Corrigido redimensionamento de janela

#ifndef JANELA_HPP
#define JANELA_HPP

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <map>
#include <string>

#include "matematica.hpp"
#include "primitivos.hpp"
#include "player.hpp"
#include "objetos.hpp"
#include "camera.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")

// ==========================================
// ESTRUTURAS
// ==========================================

struct CBMatrices {
    Mat4x4 world;
    Mat4x4 view;
    Mat4x4 proj;
};

struct Cubo {
    int id;
    float x, y, z;
    float rotX, rotY, rotZ;
    float scale;
    bool ativo;
};

struct Jogo {
    int id;
    bool running;
    
    // Janela e DirectX
    HWND hwnd;
    ID3D11Device* dev;
    ID3D11DeviceContext* ctx;
    IDXGISwapChain* swap;
    ID3D11RenderTargetView* rtv;
    ID3D11DepthStencilView* dsv;
    
    // Pipeline
    ID3D11Buffer* cuboVB;
    ID3D11Buffer* cuboIB;
    ID3D11Buffer* constantBuffer;
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    ID3D11InputLayout* layout;
    
    // Matrizes Principais
    Mat4x4 view;
    Mat4x4 proj;
    
    // Configurações
    int bgR, bgG, bgB;
    int width, height;
    
    // Flag para redimensionamento pendente
    bool needsResize;
    int newWidth, newHeight;
    
    // Legado (Cubos primitivos)
    std::map<int, Cubo*> cubos;
    int nextCuboId;
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, Jogo*> g_jogos;
static int g_nextJogoId = 1;

static const char* g_shaderCode = R"(
cbuffer MatrixBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
};
struct VS_INPUT {
    float3 pos : POSITION;
    float4 color : COLOR;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = mul(float4(input.pos, 1.0f), world);
    output.pos = mul(output.pos, view);
    output.pos = mul(output.pos, projection);
    output.color = input.color;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

// ==========================================
// FUNÇÃO DE REDIMENSIONAMENTO
// ==========================================

static void janela_redimensionar(Jogo* jogo, int newWidth, int newHeight) {
    if (!jogo || !jogo->dev || !jogo->ctx || !jogo->swap) return;
    if (newWidth <= 0 || newHeight <= 0) return;
    
    // Liberar recursos antigos
    if (jogo->rtv) { jogo->rtv->Release(); jogo->rtv = nullptr; }
    if (jogo->dsv) { jogo->dsv->Release(); jogo->dsv = nullptr; }
    
    // Redimensionar SwapChain
    HRESULT hr = jogo->swap->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return;
    
    // Recriar Render Target View
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = jogo->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return;
    
    hr = jogo->dev->CreateRenderTargetView(pBackBuffer, NULL, &jogo->rtv);
    pBackBuffer->Release();
    if (FAILED(hr)) return;
    
    // Recriar Depth Buffer
    D3D11_TEXTURE2D_DESC dsd = {0};
    dsd.Width = newWidth;
    dsd.Height = newHeight;
    dsd.MipLevels = 1;
    dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count = 1;
    dsd.Usage = D3D11_USAGE_DEFAULT;
    dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    
    ID3D11Texture2D* pDepthBuffer = nullptr;
    hr = jogo->dev->CreateTexture2D(&dsd, NULL, &pDepthBuffer);
    if (FAILED(hr)) return;
    
    hr = jogo->dev->CreateDepthStencilView(pDepthBuffer, NULL, &jogo->dsv);
    pDepthBuffer->Release();
    if (FAILED(hr)) return;
    
    // Bind novos render targets
    jogo->ctx->OMSetRenderTargets(1, &jogo->rtv, jogo->dsv);
    
    // Atualizar Viewport
    D3D11_VIEWPORT vp = {0};
    vp.Width = (float)newWidth;
    vp.Height = (float)newHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    jogo->ctx->RSSetViewports(1, &vp);
    
    // Atualizar dimensões
    jogo->width = newWidth;
    jogo->height = newHeight;
    
    // Recalcular matriz de projeção com novo aspect ratio
    float aspect = (float)newWidth / (float)newHeight;
    jogo->proj = mat_perspective(PI / 4.0f, aspect, 0.1f, 100.0f);
}

// ==========================================
// WINDOW PROCEDURE
// ==========================================

static LRESULT CALLBACK R3DGame_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_SIZE: {
            // Encontrar o jogo correspondente a esta janela
            for (auto& [id, jogo] : g_jogos) {
                if (jogo->hwnd == hwnd) {
                    int newWidth = LOWORD(lParam);
                    int newHeight = HIWORD(lParam);
                    
                    // Evitar redimensionar para tamanho zero (minimizar)
                    if (newWidth > 0 && newHeight > 0) {
                        // Marcar para redimensionar no próximo frame
                        jogo->needsResize = true;
                        jogo->newWidth = newWidth;
                        jogo->newHeight = newHeight;
                    }
                    break;
                }
            }
            return 0;
        }
        
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            for (auto& [id, jogo] : g_jogos) {
                if (jogo->hwnd == hwnd) {
                    jogo->running = false;
                    break;
                }
            }
            PostQuitMessage(0);
            exit(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ==========================================
// FUNÇÕES DE PIPELINE
// ==========================================

static bool janela_init_pipeline(Jogo* jogo) {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    // Compila Vertex Shader
    D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (errorBlob) { errorBlob->Release(); return false; }
    jogo->dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &jogo->vs);

    // Compila Pixel Shader
    ID3DBlob* psBlob = nullptr;
    D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (errorBlob) { errorBlob->Release(); vsBlob->Release(); return false; }
    jogo->dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &jogo->ps);
    psBlob->Release();

    // Layout de Entrada
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    jogo->dev->CreateInputLayout(ied, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &jogo->layout);
    vsBlob->Release();

    // Constant Buffer (Matrizes)
    D3D11_BUFFER_DESC bd = {0};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CBMatrices);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    jogo->dev->CreateBuffer(&bd, nullptr, &jogo->constantBuffer);

    // Primitivos Legados
    primitivos_criar_cubo(jogo->dev, &jogo->cuboVB, &jogo->cuboIB);
    
    return true;
}

// ==========================================
// FUNÇÕES PÚBLICAS
// ==========================================

static int janela_criar(const char* titulo, int w, int h) {
    Jogo* jogo = new Jogo();
    jogo->id = g_nextJogoId++;
    jogo->running = false;
    jogo->width = w;
    jogo->height = h;
    jogo->nextCuboId = 1;
    jogo->bgR = 25; jogo->bgG = 25; jogo->bgB = 51; // Cor padrão
    jogo->needsResize = false;
    jogo->newWidth = w;
    jogo->newHeight = h;
    
    // Registrar classe
    WNDCLASS wc = {0};
    wc.lpfnWndProc = R3DGame_WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "R3DGame_Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    RegisterClass(&wc);

    // Criar janela
    jogo->hwnd = CreateWindow("R3DGame_Class", titulo, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              100, 100, w, h, NULL, NULL, wc.hInstance, NULL);
    if (!jogo->hwnd) { delete jogo; return 0; }

    // SwapChain
    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = w;
    scd.BufferDesc.Height = h;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = jogo->hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                                               D3D11_SDK_VERSION, &scd, &jogo->swap, &jogo->dev, NULL, &jogo->ctx);
    if (FAILED(hr)) { DestroyWindow(jogo->hwnd); delete jogo; return 0; }

    // Render Target
    ID3D11Texture2D* pBackBuffer;
    jogo->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    jogo->dev->CreateRenderTargetView(pBackBuffer, NULL, &jogo->rtv);
    pBackBuffer->Release();

    // Depth Buffer
    D3D11_TEXTURE2D_DESC dsd = {0};
    dsd.Width = w; dsd.Height = h; dsd.MipLevels = 1; dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count = 1; dsd.Usage = D3D11_USAGE_DEFAULT;
    dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2D* pDepthBuffer;
    jogo->dev->CreateTexture2D(&dsd, NULL, &pDepthBuffer);
    jogo->dev->CreateDepthStencilView(pDepthBuffer, NULL, &jogo->dsv);
    pDepthBuffer->Release();

    jogo->ctx->OMSetRenderTargets(1, &jogo->rtv, jogo->dsv);

    // Viewport
    D3D11_VIEWPORT vp = {0};
    vp.Width = (float)w; vp.Height = (float)h; vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    jogo->ctx->RSSetViewports(1, &vp);

    // Câmera Inicial (Usando sistema novo)
    camera_fixa(jogo->id, 0, 5, -10, 0, 0, 0);
    jogo->view = camera_atualizar_matriz(jogo->id);
    
    // Projeção
    jogo->proj = mat_perspective(PI / 4.0f, (float)w/h, 0.1f, 100.0f);

    // Pipeline
    if (!janela_init_pipeline(jogo)) { delete jogo; return 0; }

    jogo->running = true;
    g_jogos[jogo->id] = jogo;
    return jogo->id;
}

static Jogo* janela_get(int id) {
    auto it = g_jogos.find(id);
    if (it != g_jogos.end()) return it->second;
    return nullptr;
}

static void janela_cor_fundo(Jogo* jogo, int r, int g, int b) {
    if (!jogo) return;
    jogo->bgR = r; jogo->bgG = g; jogo->bgB = b;
}

// Redireciona para o sistema de câmera fixa (para compatibilidade)
static void janela_camera(Jogo* jogo, float x, float y, float z) {
    if (!jogo) return;
    camera_fixa(jogo->id, x, y, z, 0, 0, 0);
}

// ==========================================
// LOOP DE RENDERIZAÇÃO
// ==========================================

static bool janela_renderizar(Jogo* jogo) {
    if (!jogo || !jogo->running || !IsWindow(jogo->hwnd)) return false;

    // =============================================
    // TRATAR REDIMENSIONAMENTO PENDENTE
    // =============================================
    if (jogo->needsResize) {
        janela_redimensionar(jogo, jogo->newWidth, jogo->newHeight);
        jogo->needsResize = false;
    }

    // =============================================
    // ORDEM CORRETA DE ATUALIZAÇÃO:
    // 1. Câmera processa mouse e DEFINE rotação do player
    // 2. Player processa input e se move NA DIREÇÃO CORRETA
    // =============================================
    
    // 1. Atualizar Câmera PRIMEIRO (define rotY do player baseado no mouse)
    jogo->view = camera_atualizar_matriz(jogo->id);
    
    // 2. Atualizar Players DEPOIS (movimento usa rotY já atualizado)
    player_atualizar_todos();

    // 3. Limpar Tela
    float cor[] = {jogo->bgR / 255.0f, jogo->bgG / 255.0f, jogo->bgB / 255.0f, 1.0f};
    jogo->ctx->ClearRenderTargetView(jogo->rtv, cor);
    jogo->ctx->ClearDepthStencilView(jogo->dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 4. Configurar Pipeline
    jogo->ctx->IASetInputLayout(jogo->layout);
    jogo->ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    jogo->ctx->VSSetShader(jogo->vs, NULL, 0);
    jogo->ctx->PSSetShader(jogo->ps, NULL, 0);

    // 5. Renderizar OBJETOS (Cenário)
    objetos_renderizar_todos(jogo->dev, jogo->ctx, jogo->constantBuffer, jogo->view, jogo->proj);

    // 6. Renderizar PLAYERS
    player_renderizar_todos(jogo->dev, jogo->ctx, jogo->constantBuffer, jogo->view, jogo->proj);

    // 7. Renderizar CUBOS LEGADOS (Se houver)
    UINT stride = sizeof(VertexPrimitivo);
    UINT offset = 0;
    jogo->ctx->IASetVertexBuffers(0, 1, &jogo->cuboVB, &stride, &offset);
    jogo->ctx->IASetIndexBuffer(jogo->cuboIB, DXGI_FORMAT_R16_UINT, 0);

    for (auto const& [id, cubo] : jogo->cubos) {
        if (!cubo->ativo) continue;

        Mat4x4 mScale = mat_scale(cubo->scale, cubo->scale, cubo->scale);
        Mat4x4 mRot = mat_mul(mat_rotX(cubo->rotX), mat_mul(mat_rotY(cubo->rotY), mat_rotZ(cubo->rotZ)));
        Mat4x4 mTrans = mat_translation(cubo->x, cubo->y, cubo->z);
        Mat4x4 world = mat_mul(mScale, mat_mul(mRot, mTrans));

        CBMatrices cb;
        cb.world = mat_transpose(world);
        cb.view = mat_transpose(jogo->view);
        cb.proj = mat_transpose(jogo->proj);
        
        jogo->ctx->UpdateSubresource(jogo->constantBuffer, 0, NULL, &cb, 0, 0);
        jogo->ctx->VSSetConstantBuffers(0, 1, &jogo->constantBuffer);
        jogo->ctx->DrawIndexed(36, 0, 0);
    }

    // 8. Apresentar
    jogo->swap->Present(1, 0);

    // 9. Mensagens do Windows
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            jogo->running = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return jogo->running;
}

// ==========================================
// FUNÇÕES LEGADO / ÚTEIS
// ==========================================

static int janela_criar_cubo(Jogo* jogo, float x, float y, float z) {
    if (!jogo) return 0;
    Cubo* c = new Cubo();
    c->id = jogo->nextCuboId++; c->x = x; c->y = y; c->z = z;
    c->rotX = 0; c->rotY = 0; c->rotZ = 0; c->scale = 1.0f; c->ativo = true;
    jogo->cubos[c->id] = c; return c->id;
}

static bool janela_mover_cubo(Jogo* jogo, int cuboId, float x, float y, float z) {
    if (!jogo) return false;
    if (jogo->cubos.count(cuboId)) {
        jogo->cubos[cuboId]->x = x; jogo->cubos[cuboId]->y = y; jogo->cubos[cuboId]->z = z; return true;
    } return false;
}

static bool janela_rotacionar_cubo(Jogo* jogo, int cuboId, float rx, float ry, float rz) {
    if (!jogo) return false;
    if (jogo->cubos.count(cuboId)) {
        jogo->cubos[cuboId]->rotX += rx; jogo->cubos[cuboId]->rotY += ry; jogo->cubos[cuboId]->rotZ += rz; return true;
    } return false;
}

static double janela_get_cubo_x(Jogo* jogo, int cuboId) {
    return (jogo && jogo->cubos.count(cuboId)) ? jogo->cubos[cuboId]->x : 0.0;
}

static double janela_get_cubo_z(Jogo* jogo, int cuboId) {
    return (jogo && jogo->cubos.count(cuboId)) ? jogo->cubos[cuboId]->z : 0.0;
}

static bool janela_tecla(const std::string& k) {
    if (k.empty()) return false;
    
    // Converter para maiúsculo para comparação
    std::string tecla = k;
    for (auto& c : tecla) c = toupper(c);
    
    // Teclas especiais
    int vk = 0;
    if (tecla == "SPACE" || tecla == "ESPACO" || tecla == "ESPAÇO") vk = VK_SPACE;
    else if (tecla == "SHIFT") vk = VK_SHIFT;
    else if (tecla == "CTRL" || tecla == "CONTROL") vk = VK_CONTROL;
    else if (tecla == "ENTER") vk = VK_RETURN;
    else if (tecla == "ESC" || tecla == "ESCAPE") vk = VK_ESCAPE;
    else if (tecla == "UP" || tecla == "CIMA") vk = VK_UP;
    else if (tecla == "DOWN" || tecla == "BAIXO") vk = VK_DOWN;
    else if (tecla == "LEFT" || tecla == "ESQUERDA") vk = VK_LEFT;
    else if (tecla == "RIGHT" || tecla == "DIREITA") vk = VK_RIGHT;
    else vk = (int)tecla[0]; // Caractere simples (A-Z, 0-9)
    
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

#endif