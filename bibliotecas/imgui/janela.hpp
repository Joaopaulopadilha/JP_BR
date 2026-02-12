// --- START OF FILE janela.hpp ---

// janela.hpp
// Lógica de criação e gerenciamento de janela para o wrapper ImGui da JPLang (Windows: DirectX 11, Linux: OpenGL3 + GLFW)

#ifndef JANELA_HPP
#define JANELA_HPP

#include <string>
#include <cstdlib>

#include "imgui.h"
#include "botoes.hpp"
#include "etiquetas.hpp"
#include "inputs.hpp"
#include "combobox.hpp"
#include "barras.hpp"
#include "gauges.hpp"
#include "titulo.hpp"

// === INCLUDES ESPECÍFICOS POR PLATAFORMA ===
#ifdef _WIN32
    #include <windows.h>
    #include <windowsx.h>
    #include <d3d11.h>
    #include "backends/imgui_impl_win32.h"
    #include "backends/imgui_impl_dx11.h"
#else
    #include <GLFW/glfw3.h>
    #include "backends/imgui_impl_glfw.h"
    #include "backends/imgui_impl_opengl3.h"
#endif

// === ESTADO GLOBAL DA JANELA ===
static bool g_initialized = false;
static std::string g_tituloJanela = "";

// Cor de fundo padrão (240, 240, 240)
static int g_bgR = 240;
static int g_bgG = 240;
static int g_bgB = 240;

// ============================================================================
// === IMPLEMENTAÇÃO WINDOWS (DirectX 11 + Win32) ===
// ============================================================================
#ifdef _WIN32

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND g_hwnd = nullptr;
static WNDCLASSEXW g_wc = {};

// Forward declarations
static void CreateRenderTarget();
static void CleanupRenderTarget();
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool g_shouldClose = false;

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_initialized) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }

    switch (msg) {
    // CORREÇÃO: Remove a área "non-client" padrão do Windows para que a janela ocupe tudo
    case WM_NCCALCSIZE:
        if (barraTituloAtiva() && wParam) {
            return 0; 
        }
        break;

    case WM_NCHITTEST:
        // Se a barra de título customizada está ativa, trata o resize nas bordas
        if (barraTituloAtiva()) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hWnd, &pt);
            
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            int largura = rc.right - rc.left;
            int altura = rc.bottom - rc.top;
            
            // Detecta bordas para resize
            bool esquerda = pt.x < BORDA_RESIZE;
            bool direita = pt.x >= largura - BORDA_RESIZE;
            bool topo = pt.y < BORDA_RESIZE;
            bool baixo = pt.y >= altura - BORDA_RESIZE;
            
            if (topo && esquerda) return HTTOPLEFT;
            if (topo && direita) return HTTOPRIGHT;
            if (baixo && esquerda) return HTBOTTOMLEFT;
            if (baixo && direita) return HTBOTTOMRIGHT;
            if (esquerda) return HTLEFT;
            if (direita) return HTRIGHT;
            if (topo) return HTTOP;
            if (baixo) return HTBOTTOM;
            
            // Permite interação com a barra de título pelo ImGui, mas se precisar 
            // mover pelo Windows nativo (HTCAPTION), a lógica estaria aqui.
            // Deixamos passar para que o código em titulo.hpp gerencie o arrasto.
        }
        break;

    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_CLOSE:
        g_shouldClose = true;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
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
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
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

// Cria a janela (Windows)
static int criarJanela(const std::string& titulo, int largura, int altura) {
    if (g_initialized) return 0;
    
    // Salva o título
    g_tituloJanela = titulo;
    
    // Reseta cor de fundo para padrão
    g_bgR = 240;
    g_bgG = 240;
    g_bgB = 240;
    g_shouldClose = false;
    
    // Limpa componentes anteriores
    limparBotoes();
    limparEtiquetas();
    limparInputs();
    limparComboboxes();
    limparBarras();
    limparGauges();
    
    // Converte título para wide string
    int len = MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, NULL, 0);
    std::wstring wtitulo(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, &wtitulo[0], len);
    
    // Cria janela Win32
    g_wc = { sizeof(g_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"JPLang ImGui", nullptr };
    RegisterClassExW(&g_wc);
    g_hwnd = CreateWindowW(g_wc.lpszClassName, wtitulo.c_str(), WS_OVERLAPPEDWINDOW, 100, 100, largura, altura, nullptr, nullptr, g_wc.hInstance, nullptr);

    // Inicializa DirectX
    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
        return 0;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    // Inicializa ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = NULL;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    g_initialized = true;
    return 1;
}

// Exibe um frame (Windows)
static void exibirJanela() {
    if (!g_initialized) return;
    
    // Processa eventos
    MSG msg;
    while (PeekMessage(&msg, g_hwnd, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Verifica se deve fechar
    if (g_shouldClose) {
        // Cleanup
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        DestroyWindow(g_hwnd);
        UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
        
        g_hwnd = nullptr;
        g_initialized = false;
        
        // Limpa componentes
        limparBotoes();
        limparEtiquetas();
        limparInputs();
        limparComboboxes();
        limparBarras();
        limparGauges();
        
        // Encerra o programa
        exit(0);
    }
    
    // Novo frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Desenha barra de título customizada (se ativa)
    desenharBarraTitulo(g_hwnd);

    // Desenha componentes
    desenharBotoes();
    desenharEtiquetas();
    desenharInputs();
    desenharComboboxes();
    desenharBarras();
    desenharGauges();

    // Renderiza
    ImGui::Render();
    
    float clear_color[4] = { 
        g_bgR / 255.0f, 
        g_bgG / 255.0f, 
        g_bgB / 255.0f, 
        1.0f 
    };
    
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

// ============================================================================
// === IMPLEMENTAÇÃO LINUX (OpenGL3 + GLFW) ===
// ============================================================================
#else

static GLFWwindow* g_window = nullptr;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Cria a janela (Linux)
static int criarJanela(const std::string& titulo, int largura, int altura) {
    if (g_initialized) return 0;
    
    // Salva o título
    g_tituloJanela = titulo;
    
    // Reseta cor de fundo para padrão
    g_bgR = 240;
    g_bgG = 240;
    g_bgB = 240;
    
    // Limpa componentes anteriores
    limparBotoes();
    limparEtiquetas();
    limparInputs();
    limparComboboxes();
    limparBarras();
    limparGauges();
    
    // Inicializa GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 0;
    }

    // Configura OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Cria janela GLFW
    g_window = glfwCreateWindow(largura, altura, titulo.c_str(), nullptr, nullptr);
    if (!g_window) {
        glfwTerminate();
        return 0;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1); // VSync

    // Inicializa ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = NULL;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    g_initialized = true;
    return 1;
}

// Exibe um frame (Linux)
static void exibirJanela() {
    if (!g_initialized) return;
    
    // Processa eventos
    glfwPollEvents();
    
    // Verifica se deve fechar
    if (glfwWindowShouldClose(g_window)) {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(g_window);
        glfwTerminate();
        
        g_window = nullptr;
        g_initialized = false;
        
        // Limpa componentes
        limparBotoes();
        limparEtiquetas();
        limparInputs();
        limparComboboxes();
        limparBarras();
        limparGauges();
        
        // Encerra o programa
        exit(0);
    }
    
    // Novo frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Desenha barra de título customizada (se ativa)
    desenharBarraTitulo(g_window);

    // Desenha componentes
    desenharBotoes();
    desenharEtiquetas();
    desenharInputs();
    desenharComboboxes();
    desenharBarras();
    desenharGauges();

    // Renderiza
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(g_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    glClearColor(
        g_bgR / 255.0f, 
        g_bgG / 255.0f, 
        g_bgB / 255.0f, 
        1.0f
    );
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_window);
}

#endif // _WIN32 / Linux

// ============================================================================
// === FUNÇÕES COMUNS ===
// ============================================================================

// Define cor de fundo da janela (RGB 0-255)
static bool janelaFundo(int r, int g, int b) {
    g_bgR = r;
    g_bgG = g;
    g_bgB = b;
    return true;
}

// Define cor de fundo da barra de título
static bool janelaBarraTituloCor(int r, int g, int b) {
    barraTituloCor(r, g, b);
    return true;
}

// Define cor do texto do título
static bool janelaBarraTituloTextoCor(int r, int g, int b) {
    barraTituloTextoCor(r, g, b);
    return true;
}

// Define cor dos botões da barra de título
static bool janelaBarraTituloBotoesCor(int r, int g, int b) {
    barraTituloBotoesCor(r, g, b);
    return true;
}

// Ativa barra de título customizada
#ifdef _WIN32
static bool janelaBarraTitulo() {
    if (!g_initialized) return false;
    ativarBarraTitulo(g_tituloJanela, 0, 0);
    removerBarraTituloPadrao(g_hwnd);
    return true;
}
#else
static bool janelaBarraTitulo() {
    if (!g_initialized) return false;
    ativarBarraTitulo(g_tituloJanela, 0, 0);
    removerBarraTituloPadrao(g_window);
    return true;
}
#endif

#endif // JANELA_HPP