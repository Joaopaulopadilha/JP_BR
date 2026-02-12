// janela.cpp
// Biblioteca JPLang – Cria e exibe uma janela Win32 simples
// ------------------------------------------

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#include <string>

// === EXPORT ===
#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// === TIPOS ===
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

// === HELPERS ===
static JPValor jp_int(int64_t v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}
static JPValor jp_double(double v) {
    JPValor r;
    r.tipo = JP_TIPO_DOUBLE;
    r.valor.decimal = v;
    return r;
}
static JPValor jp_string(const char* s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    size_t len = strlen(s);
    r.valor.texto = (char*)malloc(len + 1);
    if (r.valor.texto) memcpy(r.valor.texto, s, len + 1);
    return r;
}
static JPValor jp_bool(int v) {
    JPValor r;
    r.tipo = JP_TIPO_BOOL;
    r.valor.booleano = v;
    return r;
}
static JPValor jp_nulo() {
    JPValor r;
    r.tipo = JP_TIPO_NULO;
    r.valor.inteiro = 0;
    return r;
}
static int64_t get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int64_t)args[idx].valor.decimal;
    return 0;
}
static double get_double(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_DOUBLE) return args[idx].valor.decimal;
    if (args[idx].tipo == JP_TIPO_INT) return (double)args[idx].valor.inteiro;
    return 0.0;
}
static const char* get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto)
        return args[idx].valor.texto;
    return "";
}
static int get_bool(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_BOOL) return args[idx].valor.booleano;
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro != 0;
    return 0;
}

// === GLOBAIS ===
static const char* g_windowClassName = "JPLangWindowClass";
static bool g_classRegistered = false;
static HWND g_mainWindow = NULL;
static bool g_windowClosed = false;

// === WNDPROC ===
LRESULT CALLBACK JPLangWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            g_windowClosed = true;
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW+1));
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// === FUNÇÕES EXPORTADAS ===
JP_EXPORT JPValor janela(JPValor* args, int numArgs) {
    // Espera 3 argumentos: título, largura, altura
    if (numArgs < 3) return jp_nulo();

    const char* title = get_string(args, 0);
    int width  = (int)get_int(args, 1);
    int height = (int)get_int(args, 2);

    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Registrar classe de janela apenas uma vez
    if (!g_classRegistered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc   = JPLangWndProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = g_windowClassName;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);

        if (!RegisterClass(&wc)) return jp_nulo();
        g_classRegistered = true;
    }

    // Criar a janela
    HWND hwnd = CreateWindowEx(
        0,
        g_windowClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) return jp_nulo();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    g_mainWindow  = hwnd;
    g_windowClosed = false;

    return jp_int((int64_t)(uintptr_t)hwnd); // devolve o handle
}

JP_EXPORT JPValor exibir(JPValor* args, int numArgs) {
    // Espera 1 argumento: handle da janela
    if (numArgs < 1) return jp_nulo();

    int64_t handleInt = get_int(args, 0);
    HWND hwnd = (HWND)(uintptr_t)handleInt;

    // Processa mensagens (sem bloquear)
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Retorna 1 se ainda aberta, 0 se fechada
    return jp_bool(!g_windowClosed);
}
