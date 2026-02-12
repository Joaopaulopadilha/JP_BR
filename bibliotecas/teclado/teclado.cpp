// teclado.cpp
// Biblioteca nativa de controle de teclado para JPLang - Versão Unificada Windows/Linux
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/teclado/teclado.jpd bibliotecas/teclado/teclado.cpp -O3 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/teclado/libteclado.jpd bibliotecas/teclado/teclado.cpp -O3 -lX11 -lXtst

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <cctype>

// =============================================================================
// DETECÇÃO DE PLATAFORMA
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <windows.h>
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <X11/Xlib.h>
    #include <X11/keysym.h>
    #include <X11/extensions/XTest.h>
#endif

// =============================================================================
// TIPOS C PUROS (interface com JPLang)
// =============================================================================
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

// =============================================================================
// HELPERS
// =============================================================================
static JPValor jp_int(int v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static JPValor jp_string(const std::string& s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    r.valor.texto = (char*)malloc(s.size() + 1);
    if (r.valor.texto) {
        memcpy(r.valor.texto, s.c_str(), s.size() + 1);
    }
    return r;
}

static JPValor jp_bool(bool b) {
    JPValor r;
    r.tipo = JP_TIPO_BOOL;
    r.valor.booleano = b ? 1 : 0;
    return r;
}

static std::string jp_get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) {
        return std::string(args[idx].valor.texto);
    }
    if (args[idx].tipo == JP_TIPO_INT) {
        return std::to_string(args[idx].valor.inteiro);
    }
    return "";
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

static std::string to_lower(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = tolower((unsigned char)c);
    return result;
}

// =============================================================================
// IMPLEMENTAÇÃO WINDOWS
// =============================================================================
#if JP_WINDOWS

static std::unordered_map<std::string, WORD> teclas_especiais = {
    {"enter", VK_RETURN}, {"tab", VK_TAB}, {"space", VK_SPACE}, {"espaco", VK_SPACE},
    {"backspace", VK_BACK}, {"delete", VK_DELETE}, {"del", VK_DELETE},
    {"esc", VK_ESCAPE}, {"escape", VK_ESCAPE},
    {"up", VK_UP}, {"cima", VK_UP}, {"down", VK_DOWN}, {"baixo", VK_DOWN},
    {"left", VK_LEFT}, {"esquerda", VK_LEFT}, {"right", VK_RIGHT}, {"direita", VK_RIGHT},
    {"shift", VK_SHIFT}, {"ctrl", VK_CONTROL}, {"control", VK_CONTROL},
    {"alt", VK_MENU}, {"win", VK_LWIN}, {"windows", VK_LWIN},
    {"f1", VK_F1}, {"f2", VK_F2}, {"f3", VK_F3}, {"f4", VK_F4},
    {"f5", VK_F5}, {"f6", VK_F6}, {"f7", VK_F7}, {"f8", VK_F8},
    {"f9", VK_F9}, {"f10", VK_F10}, {"f11", VK_F11}, {"f12", VK_F12},
    {"home", VK_HOME}, {"end", VK_END}, {"pageup", VK_PRIOR}, {"pagedown", VK_NEXT},
    {"insert", VK_INSERT}, {"ins", VK_INSERT}, {"capslock", VK_CAPITAL},
    {"numlock", VK_NUMLOCK}, {"printscreen", VK_SNAPSHOT}, {"print", VK_SNAPSHOT},
    {"pause", VK_PAUSE},
    {"num0", VK_NUMPAD0}, {"num1", VK_NUMPAD1}, {"num2", VK_NUMPAD2},
    {"num3", VK_NUMPAD3}, {"num4", VK_NUMPAD4}, {"num5", VK_NUMPAD5},
    {"num6", VK_NUMPAD6}, {"num7", VK_NUMPAD7}, {"num8", VK_NUMPAD8},
    {"num9", VK_NUMPAD9}, {"num*", VK_MULTIPLY}, {"num+", VK_ADD},
    {"num-", VK_SUBTRACT}, {"num/", VK_DIVIDE}, {"num.", VK_DECIMAL}
};

static WORD obter_tecla_especial(const std::string& tecla) {
    std::string lower = to_lower(tecla);
    auto it = teclas_especiais.find(lower);
    return (it != teclas_especiais.end()) ? it->second : 0;
}

static void pressionar_vk(WORD vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.dwFlags = 0;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static void pressionar_char(wchar_t c) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0;
    inputs[0].ki.wScan = c;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0;
    inputs[1].ki.wScan = c;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

static void digitar_frase(const std::string& frase, int delay_ms) {
    int len = MultiByteToWideChar(CP_UTF8, 0, frase.c_str(), -1, NULL, 0);
    if (len <= 0) return;
    std::wstring wide(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, frase.c_str(), -1, &wide[0], len);
    int delay_real = (delay_ms > 0) ? delay_ms : 5;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (wchar_t c : wide) {
        if (c == 0) continue;
        pressionar_char(c);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_real));
    }
}

static void segurar_tecla_impl(const std::string& tecla) {
    WORD vk = obter_tecla_especial(tecla);
    if (vk == 0 && !tecla.empty()) vk = VkKeyScan(tecla[0]) & 0xFF;
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
}

static void soltar_tecla_impl(const std::string& tecla) {
    WORD vk = obter_tecla_especial(tecla);
    if (vk == 0 && !tecla.empty()) vk = VkKeyScan(tecla[0]) & 0xFF;
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

static std::string escutar_tecla_impl() {
    // Teclas com nomes padronizados (verificar primeiro para consistência)
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return "esc";
    if (GetAsyncKeyState(VK_RETURN) & 0x8000) return "enter";
    if (GetAsyncKeyState(VK_TAB) & 0x8000) return "tab";
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) return "space";
    if (GetAsyncKeyState(VK_BACK) & 0x8000) return "backspace";
    if (GetAsyncKeyState(VK_DELETE) & 0x8000) return "delete";
    if (GetAsyncKeyState(VK_UP) & 0x8000) return "up";
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) return "down";
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) return "left";
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) return "right";
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) return "shift";
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) return "ctrl";
    if (GetAsyncKeyState(VK_MENU) & 0x8000) return "alt";
    if (GetAsyncKeyState(VK_LWIN) & 0x8000) return "win";
    if (GetAsyncKeyState(VK_F1) & 0x8000) return "f1";
    if (GetAsyncKeyState(VK_F2) & 0x8000) return "f2";
    if (GetAsyncKeyState(VK_F3) & 0x8000) return "f3";
    if (GetAsyncKeyState(VK_F4) & 0x8000) return "f4";
    if (GetAsyncKeyState(VK_F5) & 0x8000) return "f5";
    if (GetAsyncKeyState(VK_F6) & 0x8000) return "f6";
    if (GetAsyncKeyState(VK_F7) & 0x8000) return "f7";
    if (GetAsyncKeyState(VK_F8) & 0x8000) return "f8";
    if (GetAsyncKeyState(VK_F9) & 0x8000) return "f9";
    if (GetAsyncKeyState(VK_F10) & 0x8000) return "f10";
    if (GetAsyncKeyState(VK_F11) & 0x8000) return "f11";
    if (GetAsyncKeyState(VK_F12) & 0x8000) return "f12";
    if (GetAsyncKeyState(VK_HOME) & 0x8000) return "home";
    if (GetAsyncKeyState(VK_END) & 0x8000) return "end";
    if (GetAsyncKeyState(VK_PRIOR) & 0x8000) return "pageup";
    if (GetAsyncKeyState(VK_NEXT) & 0x8000) return "pagedown";
    if (GetAsyncKeyState(VK_INSERT) & 0x8000) return "insert";
    if (GetAsyncKeyState(VK_CAPITAL) & 0x8000) return "capslock";
    if (GetAsyncKeyState(VK_NUMLOCK) & 0x8000) return "numlock";
    if (GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) return "printscreen";
    if (GetAsyncKeyState(VK_PAUSE) & 0x8000) return "pause";
    
    // Letras A-Z
    for (char c = 'A'; c <= 'Z'; c++) {
        if (GetAsyncKeyState(c) & 0x8000) return std::string(1, c);
    }
    
    // Números 0-9
    for (char c = '0'; c <= '9'; c++) {
        if (GetAsyncKeyState(c) & 0x8000) return std::string(1, c);
    }
    
    return "";
}

static bool tecla_pressionada_impl(const std::string& tecla) {
    WORD vk = obter_tecla_especial(tecla);
    if (vk == 0 && !tecla.empty()) vk = VkKeyScan(tecla[0]) & 0xFF;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// =============================================================================
// IMPLEMENTAÇÃO LINUX
// =============================================================================
#else

static Display* obter_display() {
    static Display* display = nullptr;
    if (!display) {
        display = XOpenDisplay(nullptr);
    }
    return display;
}

static std::unordered_map<std::string, KeySym> teclas_especiais = {
    {"enter", XK_Return}, {"tab", XK_Tab}, {"space", XK_space}, {"espaco", XK_space},
    {"backspace", XK_BackSpace}, {"delete", XK_Delete}, {"del", XK_Delete},
    {"esc", XK_Escape}, {"escape", XK_Escape},
    {"up", XK_Up}, {"cima", XK_Up}, {"down", XK_Down}, {"baixo", XK_Down},
    {"left", XK_Left}, {"esquerda", XK_Left}, {"right", XK_Right}, {"direita", XK_Right},
    {"shift", XK_Shift_L}, {"ctrl", XK_Control_L}, {"control", XK_Control_L},
    {"alt", XK_Alt_L}, {"win", XK_Super_L}, {"windows", XK_Super_L},
    {"f1", XK_F1}, {"f2", XK_F2}, {"f3", XK_F3}, {"f4", XK_F4},
    {"f5", XK_F5}, {"f6", XK_F6}, {"f7", XK_F7}, {"f8", XK_F8},
    {"f9", XK_F9}, {"f10", XK_F10}, {"f11", XK_F11}, {"f12", XK_F12},
    {"home", XK_Home}, {"end", XK_End}, {"pageup", XK_Page_Up}, {"pagedown", XK_Page_Down},
    {"insert", XK_Insert}, {"ins", XK_Insert}, {"capslock", XK_Caps_Lock},
    {"numlock", XK_Num_Lock}, {"printscreen", XK_Print}, {"print", XK_Print},
    {"pause", XK_Pause},
    {"num0", XK_KP_0}, {"num1", XK_KP_1}, {"num2", XK_KP_2},
    {"num3", XK_KP_3}, {"num4", XK_KP_4}, {"num5", XK_KP_5},
    {"num6", XK_KP_6}, {"num7", XK_KP_7}, {"num8", XK_KP_8},
    {"num9", XK_KP_9}, {"num*", XK_KP_Multiply}, {"num+", XK_KP_Add},
    {"num-", XK_KP_Subtract}, {"num/", XK_KP_Divide}, {"num.", XK_KP_Decimal}
};

static KeySym obter_tecla_especial(const std::string& tecla) {
    std::string lower = to_lower(tecla);
    auto it = teclas_especiais.find(lower);
    return (it != teclas_especiais.end()) ? it->second : 0;
}

static void pressionar_keysym(KeySym ks) {
    Display* display = obter_display();
    if (!display) return;
    KeyCode kc = XKeysymToKeycode(display, ks);
    if (kc == 0) return;
    XTestFakeKeyEvent(display, kc, True, 0);
    XTestFakeKeyEvent(display, kc, False, 0);
    XFlush(display);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static void digitar_frase(const std::string& frase, int delay_ms) {
    Display* display = obter_display();
    if (!display) return;
    int delay_real = (delay_ms > 0) ? delay_ms : 5;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (char c : frase) {
        KeySym ks = XStringToKeysym(std::string(1, c).c_str());
        if (ks == NoSymbol) {
            ks = c;
        }
        KeyCode kc = XKeysymToKeycode(display, ks);
        if (kc != 0) {
            bool need_shift = (isupper(c) || strchr("!@#$%^&*()_+{}|:\"<>?~", c));
            if (need_shift) {
                KeyCode shift_kc = XKeysymToKeycode(display, XK_Shift_L);
                XTestFakeKeyEvent(display, shift_kc, True, 0);
            }
            XTestFakeKeyEvent(display, kc, True, 0);
            XTestFakeKeyEvent(display, kc, False, 0);
            if (need_shift) {
                KeyCode shift_kc = XKeysymToKeycode(display, XK_Shift_L);
                XTestFakeKeyEvent(display, shift_kc, False, 0);
            }
            XFlush(display);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_real));
    }
}

static void segurar_tecla_impl(const std::string& tecla) {
    Display* display = obter_display();
    if (!display) return;
    KeySym ks = obter_tecla_especial(tecla);
    if (ks == 0 && !tecla.empty()) ks = tecla[0];
    KeyCode kc = XKeysymToKeycode(display, ks);
    if (kc != 0) {
        XTestFakeKeyEvent(display, kc, True, 0);
        XFlush(display);
    }
}

static void soltar_tecla_impl(const std::string& tecla) {
    Display* display = obter_display();
    if (!display) return;
    KeySym ks = obter_tecla_especial(tecla);
    if (ks == 0 && !tecla.empty()) ks = tecla[0];
    KeyCode kc = XKeysymToKeycode(display, ks);
    if (kc != 0) {
        XTestFakeKeyEvent(display, kc, False, 0);
        XFlush(display);
    }
}

static bool tecla_pressionada_keysym(Display* display, char* keys, KeySym ks) {
    KeyCode kc = XKeysymToKeycode(display, ks);
    return (kc != 0 && (keys[kc / 8] & (1 << (kc % 8))));
}

static std::string escutar_tecla_impl() {
    Display* display = obter_display();
    if (!display) return "";
    char keys[32];
    XQueryKeymap(display, keys);
    
    // Teclas com nomes padronizados (verificar primeiro para consistência)
    if (tecla_pressionada_keysym(display, keys, XK_Escape)) return "esc";
    if (tecla_pressionada_keysym(display, keys, XK_Return)) return "enter";
    if (tecla_pressionada_keysym(display, keys, XK_Tab)) return "tab";
    if (tecla_pressionada_keysym(display, keys, XK_space)) return "space";
    if (tecla_pressionada_keysym(display, keys, XK_BackSpace)) return "backspace";
    if (tecla_pressionada_keysym(display, keys, XK_Delete)) return "delete";
    if (tecla_pressionada_keysym(display, keys, XK_Up)) return "up";
    if (tecla_pressionada_keysym(display, keys, XK_Down)) return "down";
    if (tecla_pressionada_keysym(display, keys, XK_Left)) return "left";
    if (tecla_pressionada_keysym(display, keys, XK_Right)) return "right";
    if (tecla_pressionada_keysym(display, keys, XK_Shift_L) || 
        tecla_pressionada_keysym(display, keys, XK_Shift_R)) return "shift";
    if (tecla_pressionada_keysym(display, keys, XK_Control_L) || 
        tecla_pressionada_keysym(display, keys, XK_Control_R)) return "ctrl";
    if (tecla_pressionada_keysym(display, keys, XK_Alt_L) || 
        tecla_pressionada_keysym(display, keys, XK_Alt_R)) return "alt";
    if (tecla_pressionada_keysym(display, keys, XK_Super_L) || 
        tecla_pressionada_keysym(display, keys, XK_Super_R)) return "win";
    if (tecla_pressionada_keysym(display, keys, XK_F1)) return "f1";
    if (tecla_pressionada_keysym(display, keys, XK_F2)) return "f2";
    if (tecla_pressionada_keysym(display, keys, XK_F3)) return "f3";
    if (tecla_pressionada_keysym(display, keys, XK_F4)) return "f4";
    if (tecla_pressionada_keysym(display, keys, XK_F5)) return "f5";
    if (tecla_pressionada_keysym(display, keys, XK_F6)) return "f6";
    if (tecla_pressionada_keysym(display, keys, XK_F7)) return "f7";
    if (tecla_pressionada_keysym(display, keys, XK_F8)) return "f8";
    if (tecla_pressionada_keysym(display, keys, XK_F9)) return "f9";
    if (tecla_pressionada_keysym(display, keys, XK_F10)) return "f10";
    if (tecla_pressionada_keysym(display, keys, XK_F11)) return "f11";
    if (tecla_pressionada_keysym(display, keys, XK_F12)) return "f12";
    if (tecla_pressionada_keysym(display, keys, XK_Home)) return "home";
    if (tecla_pressionada_keysym(display, keys, XK_End)) return "end";
    if (tecla_pressionada_keysym(display, keys, XK_Page_Up)) return "pageup";
    if (tecla_pressionada_keysym(display, keys, XK_Page_Down)) return "pagedown";
    if (tecla_pressionada_keysym(display, keys, XK_Insert)) return "insert";
    if (tecla_pressionada_keysym(display, keys, XK_Caps_Lock)) return "capslock";
    if (tecla_pressionada_keysym(display, keys, XK_Num_Lock)) return "numlock";
    if (tecla_pressionada_keysym(display, keys, XK_Print)) return "printscreen";
    if (tecla_pressionada_keysym(display, keys, XK_Pause)) return "pause";
    
    // Letras a-z (retorna maiúscula)
    for (char c = 'a'; c <= 'z'; c++) {
        KeyCode kc = XKeysymToKeycode(display, c);
        if (kc != 0 && (keys[kc / 8] & (1 << (kc % 8)))) {
            return std::string(1, toupper(c));
        }
    }
    
    // Números 0-9
    for (char c = '0'; c <= '9'; c++) {
        KeyCode kc = XKeysymToKeycode(display, c);
        if (kc != 0 && (keys[kc / 8] & (1 << (kc % 8)))) {
            return std::string(1, c);
        }
    }
    
    return "";
}

static bool tecla_pressionada_impl(const std::string& tecla) {
    Display* display = obter_display();
    if (!display) return false;
    KeySym ks = obter_tecla_especial(tecla);
    if (ks == 0 && !tecla.empty()) ks = tecla[0];
    KeyCode kc = XKeysymToKeycode(display, ks);
    if (kc == 0) return false;
    char keys[32];
    XQueryKeymap(display, keys);
    return (keys[kc / 8] & (1 << (kc % 8))) != 0;
}

#endif

// =============================================================================
// FUNÇÕES EXPORTADAS (INTERFACE COMUM)
// =============================================================================

JP_EXPORT JPValor jp_tc_pressionar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    std::string texto = jp_get_string(args, 0);
    
#if JP_WINDOWS
    WORD vk = obter_tecla_especial(texto);
    if (vk != 0) {
        pressionar_vk(vk);
    } else {
        digitar_frase(texto, 0);
    }
#else
    KeySym ks = obter_tecla_especial(texto);
    if (ks != 0) {
        pressionar_keysym(ks);
    } else {
        digitar_frase(texto, 0);
    }
#endif
    return jp_int(0);
}

JP_EXPORT JPValor jp_tc_pressionar_passo(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_int(0);
    std::string frase = jp_get_string(args, 0);
    int delay = jp_get_int(args, 1);
    digitar_frase(frase, delay);
    return jp_int(0);
}

JP_EXPORT JPValor jp_tc_segurar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    segurar_tecla_impl(jp_get_string(args, 0));
    return jp_int(0);
}

JP_EXPORT JPValor jp_tc_soltar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    soltar_tecla_impl(jp_get_string(args, 0));
    return jp_int(0);
}

JP_EXPORT JPValor jp_tc_escutar(JPValor* args, int numArgs) {
    return jp_string(escutar_tecla_impl());
}

JP_EXPORT JPValor jp_tc_pressionada(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_bool(false);
    return jp_bool(tecla_pressionada_impl(jp_get_string(args, 0)));
}

JP_EXPORT JPValor jp_tc_combinacao(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    
    // Segura todas as teclas
    for (int i = 0; i < numArgs; i++) {
        segurar_tecla_impl(jp_get_string(args, i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Solta todas as teclas (ordem reversa)
    for (int i = numArgs - 1; i >= 0; i--) {
        soltar_tecla_impl(jp_get_string(args, i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return jp_int(0);
}