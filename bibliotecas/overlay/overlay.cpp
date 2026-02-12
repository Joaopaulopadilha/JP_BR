// overlay.cpp
// Biblioteca de overlay para JPLang (Suporte a Texto, Formas e Imagens)
// Compile: g++ -shared -o bibliotecas/overlay/overlay.jpd bibliotecas/overlay/overlay.cpp -lgdi32 -lgdiplus -mwindows -O3

#include <vector>
#include <string>
#include <map>
#include <windows.h>
#include <gdiplus.h>
#include <mutex>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")

// === STB IMAGE IMPLEMENTATION ===
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"

// ==========================================
// === PADRÃO JPLANG (Tipos Obrigatórios) ===
// ==========================================

#define JP_EXPORT __declspec(dllexport)

typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4,
    JP_TIPO_OBJETO = 5
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
    } valor;
} JPValor;

static inline JPValor jp_int(int64_t i) {
    JPValor v; v.tipo = JP_TIPO_INT; v.valor.inteiro = i; return v;
}
static inline int64_t jp_get_int(JPValor v) {
    if (v.tipo == JP_TIPO_INT) return v.valor.inteiro;
    if (v.tipo == JP_TIPO_DOUBLE) return (int64_t)v.valor.decimal;
    return 0;
}
static inline std::string jp_get_string(JPValor v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto) return std::string(v.valor.texto);
    return "";
}

// ==========================================
// === LÓGICA INTERNA DO OVERLAY ===
// ==========================================

enum class TipoOverlay { TEXTO, RETANGULO, BARRA, IMAGEM };

struct OverlayInfo {
    HWND hwnd;
    TipoOverlay tipo;
    int x, y;
    int r, g, b;
    bool ativo;
    // Texto
    std::string texto;
    int tamanho;
    // Retângulo / Barra / Imagem
    int largura, altura;
    int espessura;
    // Barra
    int valor_min, valor_max, valor_atual;
    char orientacao;
    // Imagem (Ponteiro para Gdiplus::Bitmap)
    void* imagemBitmap;
};

// Globais
static std::map<int, OverlayInfo> g_overlays;
static int g_nextHandle = 1;
static ULONG_PTR g_gdiplusToken = 0;
static bool g_gdiplusIniciado = false;
static std::mutex g_mutex;
static bool g_classeRegistrada = false;

// Converte UTF-8 para Wide String (Windows)
static std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

static void iniciar_gdiplus() {
    if (!g_gdiplusIniciado) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
        g_gdiplusIniciado = true;
    }
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY: return 0;
        case WM_NCHITTEST: return HTCLIENT;
        default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static void registrar_classe() {
    if (g_classeRegistrada) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"JPLangOverlay";
    RegisterClassExW(&wc);
    g_classeRegistrada = true;
}

static HWND criar_janela_overlay(int x, int y, int largura, int altura) {
    return CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"JPLangOverlay", L"", WS_POPUP, x, y, largura, altura,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
}

// === INCLUDES DOS MÓDULOS ===
// Nota: Os arquivos .hpp já contêm definições de funções (redimensionar, texto, barra, etc)
#include "texto.hpp"
#include "retangulo.hpp"
#include "barra.hpp"
#include "imagem.hpp"

static void renderizar_overlay(OverlayInfo& info) {
    switch (info.tipo) {
        case TipoOverlay::TEXTO: renderizar_texto(info); break;
        case TipoOverlay::RETANGULO: renderizar_retangulo(info); break;
        case TipoOverlay::BARRA: renderizar_barra(info); break;
        case TipoOverlay::IMAGEM: renderizar_imagem(info); break;
    }
}

// ==========================================
// === FUNÇÕES EXPORTADAS ===
// ==========================================

extern "C" {

    JP_EXPORT JPValor jp_ov_exibir(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 1) return jp_int(0);

        int handle = (int)jp_get_int(args[0]);
        auto it = g_overlays.find(handle);
        
        if (it != g_overlays.end() && it->second.ativo) {
            OverlayInfo& info = it->second;
            if (!IsWindowVisible(info.hwnd)) ShowWindow(info.hwnd, SW_SHOWNOACTIVATE);
            
            renderizar_overlay(info);
            
            MSG msg;
            while (PeekMessage(&msg, info.hwnd, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            return jp_int(1);
        }
        return jp_int(0);
    }

    JP_EXPORT JPValor jp_ov_fechar(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 1) return jp_int(0);

        int handle = (int)jp_get_int(args[0]);
        auto it = g_overlays.find(handle);
        if (it != g_overlays.end()) {
            if (it->second.hwnd) DestroyWindow(it->second.hwnd);
            
            // LIMPEZA DE MEMÓRIA DA IMAGEM
            if (it->second.tipo == TipoOverlay::IMAGEM && it->second.imagemBitmap) {
                delete (Gdiplus::Bitmap*)it->second.imagemBitmap;
                it->second.imagemBitmap = nullptr;
            }

            g_overlays.erase(it);
            return jp_int(1);
        }
        return jp_int(0);
    }

    JP_EXPORT JPValor jp_ov_fechar_todos(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto& par : g_overlays) {
            if (par.second.hwnd) DestroyWindow(par.second.hwnd);
            
            // LIMPEZA DE MEMÓRIA
            if (par.second.tipo == TipoOverlay::IMAGEM && par.second.imagemBitmap) {
                delete (Gdiplus::Bitmap*)par.second.imagemBitmap;
                par.second.imagemBitmap = nullptr;
            }
        }
        g_overlays.clear();
        return jp_int(1);
    }

    JP_EXPORT JPValor jp_ov_mover(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 3) return jp_int(0);
        
        int handle = (int)jp_get_int(args[0]);
        int x = (int)jp_get_int(args[1]);
        int y = (int)jp_get_int(args[2]);
        
        auto it = g_overlays.find(handle);
        if (it != g_overlays.end()) {
            it->second.x = x;
            it->second.y = y;
            return jp_int(1);
        }
        return jp_int(0);
    }
    
    JP_EXPORT JPValor jp_ov_cor(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 4) return jp_int(0);
        
        int handle = (int)jp_get_int(args[0]);
        int r = (int)jp_get_int(args[1]);
        int g = (int)jp_get_int(args[2]);
        int b = (int)jp_get_int(args[3]);
        
        auto it = g_overlays.find(handle);
        if (it != g_overlays.end()) {
            it->second.r = r;
            it->second.g = g;
            it->second.b = b;
            return jp_int(1);
        }
        return jp_int(0);
    }

    // REMOVIDO: jp_ov_redimensionar já está em retangulo.hpp
}