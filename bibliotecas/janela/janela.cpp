// janela.cpp
// Biblioteca simples de janelas para JPLang (Windows)

#include <windows.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unordered_map>

// === TIPOS ABI-COMPATÍVEIS ===
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
    } valor;
} JPValor;

#define EXPORTAR extern "C" __declspec(dllexport)

// === HELPERS ===
static int jp_get_int(const JPValor& v) {
    if (v.tipo == JP_TIPO_INT) return (int)v.valor.inteiro;
    if (v.tipo == JP_TIPO_DOUBLE) return (int)v.valor.decimal;
    return 0;
}

static std::string jp_get_str(const JPValor& v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto) return std::string(v.valor.texto);
    return "";
}

static JPValor jp_int(int64_t i) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_INT;
    v.valor.inteiro = i;
    return v;
}

static JPValor jp_bool(bool b) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_BOOL;
    v.valor.booleano = b ? 1 : 0;
    return v;
}

// === GERENCIADOR DE JANELAS ===
static std::unordered_map<std::string, HWND> janelas;
static bool classeRegistrada = false;
static const char* CLASSE_JANELA = "JPLangJanela";

// Callback da janela
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Registra a classe da janela (uma vez só)
static bool registrarClasse() {
    if (classeRegistrada) return true;
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASSE_JANELA;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (RegisterClassA(&wc)) {
        classeRegistrada = true;
        return true;
    }
    return false;
}

// =============================================================================
// === FUNÇÕES EXPORTADAS ===
// =============================================================================

// jan_criar(titulo, largura, altura) - Cria uma janela
EXPORTAR JPValor jp_jan_criar(JPValor* args, int n) {
    if (n < 3) return jp_bool(false);
    
    std::string titulo = jp_get_str(args[0]);
    int largura = jp_get_int(args[1]);
    int altura = jp_get_int(args[2]);
    
    if (!registrarClasse()) return jp_bool(false);
    
    // Calcula tamanho incluindo bordas
    RECT rect = {0, 0, largura, altura};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND hwnd = CreateWindowA(
        CLASSE_JANELA,
        titulo.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!hwnd) return jp_bool(false);
    
    janelas[titulo] = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    return jp_bool(true);
}

// jan_processar() - Processa mensagens, retorna falso se deve fechar
EXPORTAR JPValor jp_jan_processar(JPValor* args, int n) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return jp_bool(false);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return jp_bool(true);
}

// jan_aberta(titulo) - Verifica se janela ainda está aberta
EXPORTAR JPValor jp_jan_aberta(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    
    std::string titulo = jp_get_str(args[0]);
    auto it = janelas.find(titulo);
    
    if (it == janelas.end()) return jp_bool(false);
    
    if (!IsWindow(it->second)) {
        janelas.erase(it);
        return jp_bool(false);
    }
    
    return jp_bool(true);
}

// jan_fechar(titulo) - Fecha uma janela
EXPORTAR JPValor jp_jan_fechar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    
    std::string titulo = jp_get_str(args[0]);
    auto it = janelas.find(titulo);
    
    if (it != janelas.end() && IsWindow(it->second)) {
        DestroyWindow(it->second);
        janelas.erase(it);
        return jp_bool(true);
    }
    
    return jp_bool(false);
}

// jan_esperar(ms) - Espera alguns milissegundos
EXPORTAR JPValor jp_jan_esperar(JPValor* args, int n) {
    int ms = 16; // ~60fps por padrão
    if (n > 0) ms = jp_get_int(args[0]);
    Sleep(ms);
    return jp_bool(true);
}
