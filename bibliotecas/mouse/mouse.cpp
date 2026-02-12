// mouse.cpp
// Biblioteca de controle do mouse para JPLang
// Compatível com Windows e Linux (X11)
//
// COMPILAÇÃO:
//   Windows: g++ -shared -o bibliotecas/mouse/mouse.jpd bibliotecas/mouse/mouse.cpp -O3 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/mouse/libmouse.jpd bibliotecas/mouse/mouse.cpp -O3 -lX11 -lXtst

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// =============================================================================
// MACROS DE PORTABILIDADE E INCLUDES ESPECÍFICOS
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <windows.h>
    
    // Helper de sleep
    static void ms_sleep(int ms) { Sleep(ms); }

#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <unistd.h>
    #include <X11/Xlib.h>
    #include <X11/extensions/XTest.h>

    // Helper de sleep
    static void ms_sleep(int ms) { usleep(ms * 1000); }

    // Gerenciador de Display X11 (Singleton simples)
    static Display* _x_display = NULL;
    static Display* get_display() {
        if (!_x_display) {
            _x_display = XOpenDisplay(NULL);
        }
        return _x_display;
    }
#endif

// =============================================================================
// TIPOS OBRIGATÓRIOS JPLANG
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
        void* objeto;
    } valor;
} JPValor;

// =============================================================================
// HELPERS INTERNOS
// =============================================================================

static inline JPValor jp_int(int64_t i) {
    JPValor v; v.tipo = JP_TIPO_INT; v.valor.inteiro = i; return v;
}

static inline JPValor jp_nulo() {
    JPValor v; v.tipo = JP_TIPO_NULO; v.valor.inteiro = 0; return v;
}

static inline int64_t jp_get_int(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return 0;
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int64_t)args[idx].valor.decimal;
    return 0;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// Retorna posição X do mouse
JP_EXPORT JPValor jp_ms_posicaox(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        POINT p;
        GetCursorPos(&p);
        return jp_int(p.x);
    #else
        Display* d = get_display();
        if (!d) return jp_int(0);
        Window root = DefaultRootWindow(d);
        Window ret_root, ret_child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(d, root, &ret_root, &ret_child, &root_x, &root_y, &win_x, &win_y, &mask);
        return jp_int(root_x);
    #endif
}

// Retorna posição Y do mouse
JP_EXPORT JPValor jp_ms_posicaoy(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        POINT p;
        GetCursorPos(&p);
        return jp_int(p.y);
    #else
        Display* d = get_display();
        if (!d) return jp_int(0);
        Window root = DefaultRootWindow(d);
        Window ret_root, ret_child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(d, root, &ret_root, &ret_child, &root_x, &root_y, &win_x, &win_y, &mask);
        return jp_int(root_y);
    #endif
}

// Move o mouse para uma posição absoluta (X, Y)
JP_EXPORT JPValor jp_ms_absmover(JPValor* args, int numArgs) {
    int x = (int)jp_get_int(args, 0, numArgs);
    int y = (int)jp_get_int(args, 1, numArgs);

    #if JP_WINDOWS
        SetCursorPos(x, y);
    #else
        Display* d = get_display();
        if (d) {
            XWarpPointer(d, None, DefaultRootWindow(d), 0, 0, 0, 0, x, y);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Move o mouse relativamente a partir da posição atual
JP_EXPORT JPValor jp_ms_relmover(JPValor* args, int numArgs) {
    int dx = (int)jp_get_int(args, 0, numArgs);
    int dy = (int)jp_get_int(args, 1, numArgs);

    #if JP_WINDOWS
        POINT p;
        GetCursorPos(&p);
        SetCursorPos(p.x + dx, p.y + dy);
    #else
        Display* d = get_display();
        if (d) {
            // Em X11, XWarpPointer com src_w=None e dest_w=None move relativo
            XWarpPointer(d, None, None, 0, 0, 0, 0, dx, dy);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Clique esquerdo
JP_EXPORT JPValor jp_ms_clique(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        INPUT input[2] = {};
        input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            XTestFakeButtonEvent(d, 1, True, CurrentTime);  // 1 = Esquerdo Down
            XTestFakeButtonEvent(d, 1, False, CurrentTime); // 1 = Esquerdo Up
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Clique direito
JP_EXPORT JPValor jp_ms_clique_dir(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        INPUT input[2] = {};
        input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(2, input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            XTestFakeButtonEvent(d, 3, True, CurrentTime);  // 3 = Direito
            XTestFakeButtonEvent(d, 3, False, CurrentTime);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Clique do meio
JP_EXPORT JPValor jp_ms_clique_meio(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        INPUT input[2] = {};
        input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        SendInput(2, input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            XTestFakeButtonEvent(d, 2, True, CurrentTime);  // 2 = Meio
            XTestFakeButtonEvent(d, 2, False, CurrentTime);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Duplo clique
JP_EXPORT JPValor jp_ms_duplo_clique(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        INPUT input[4] = {};
        input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        input[2].type = INPUT_MOUSE; input[2].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        input[3].type = INPUT_MOUSE; input[3].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(4, input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            XTestFakeButtonEvent(d, 1, True, CurrentTime);
            XTestFakeButtonEvent(d, 1, False, CurrentTime);
            XTestFakeButtonEvent(d, 1, True, CurrentTime);
            XTestFakeButtonEvent(d, 1, False, CurrentTime);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Segurar botão esquerdo
JP_EXPORT JPValor jp_ms_segurar(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            XTestFakeButtonEvent(d, 1, True, CurrentTime);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Soltar botão esquerdo
JP_EXPORT JPValor jp_ms_soltar(JPValor* args, int numArgs) {
    #if JP_WINDOWS
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            XTestFakeButtonEvent(d, 1, False, CurrentTime);
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Scroll vertical (positivo = cima, negativo = baixo)
JP_EXPORT JPValor jp_ms_scroll(JPValor* args, int numArgs) {
    int quantidade = (int)jp_get_int(args, 0, numArgs);
    
    #if JP_WINDOWS
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = quantidade * WHEEL_DELTA;
        SendInput(1, &input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            int button = (quantidade > 0) ? 4 : 5; // 4=Up, 5=Down
            int clicks = abs(quantidade);
            // No Linux, scroll geralmente é um clique por "tick", então repetimos
            for(int i=0; i<clicks; i++) {
                XTestFakeButtonEvent(d, button, True, CurrentTime);
                XTestFakeButtonEvent(d, button, False, CurrentTime);
            }
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Scroll horizontal
JP_EXPORT JPValor jp_ms_scroll_h(JPValor* args, int numArgs) {
    int quantidade = (int)jp_get_int(args, 0, numArgs);
    
    #if JP_WINDOWS
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        input.mi.mouseData = quantidade * WHEEL_DELTA;
        SendInput(1, &input, sizeof(INPUT));
    #else
        Display* d = get_display();
        if (d) {
            int button = (quantidade > 0) ? 7 : 6; // 6=Left, 7=Right (geralmente)
            int clicks = abs(quantidade);
            for(int i=0; i<clicks; i++) {
                XTestFakeButtonEvent(d, button, True, CurrentTime);
                XTestFakeButtonEvent(d, button, False, CurrentTime);
            }
            XFlush(d);
        }
    #endif
    return jp_int(1);
}

// Mover suavemente absoluto
// ms_absmover_passo(destX, destY, pixels_por_step, ms_delay)
JP_EXPORT JPValor jp_ms_absmover_passo(JPValor* args, int numArgs) {
    int destX = (int)jp_get_int(args, 0, numArgs);
    int destY = (int)jp_get_int(args, 1, numArgs);
    int passo = (int)jp_get_int(args, 2, numArgs);
    int tempo_ms = (int)jp_get_int(args, 3, numArgs);
    
    if (passo < 1) passo = 1;
    if (tempo_ms < 1) tempo_ms = 1;
    
    // Obter posição atual
    int inicioX = 0, inicioY = 0;
    
    #if JP_WINDOWS
        POINT p; GetCursorPos(&p);
        inicioX = p.x; inicioY = p.y;
    #else
        Display* d = get_display();
        if (!d) return jp_int(0);
        Window root = DefaultRootWindow(d);
        Window r, c; int rx, ry, wx, wy; unsigned int m;
        XQueryPointer(d, root, &r, &c, &rx, &ry, &wx, &wy, &m);
        inicioX = rx; inicioY = ry;
    #endif

    // Calcular trajetória
    int dx = destX - inicioX;
    int dy = destY - inicioY;
    
    double dist = std::sqrt((double)(dx * dx + dy * dy));
    int total_passos = (int)(dist / passo);
    if (total_passos < 1) total_passos = 1;
    
    double stepX = (double)dx / total_passos;
    double stepY = (double)dy / total_passos;
    
    for (int i = 1; i <= total_passos; i++) {
        int novoX = inicioX + (int)(stepX * i);
        int novoY = inicioY + (int)(stepY * i);
        
        #if JP_WINDOWS
            SetCursorPos(novoX, novoY);
        #else
            XWarpPointer(d, None, DefaultRootWindow(d), 0, 0, 0, 0, novoX, novoY);
            XFlush(d);
        #endif
        
        ms_sleep(tempo_ms);
    }
    
    // Garantir posição final exata
    #if JP_WINDOWS
        SetCursorPos(destX, destY);
    #else
        XWarpPointer(d, None, DefaultRootWindow(d), 0, 0, 0, 0, destX, destY);
        XFlush(d);
    #endif
    
    return jp_int(1);
}

// Mover suavemente relativo
// ms_relmover_passo(dx, dy, passo, tempo_ms)
JP_EXPORT JPValor jp_ms_relmover_passo(JPValor* args, int numArgs) {
    int dx = (int)jp_get_int(args, 0, numArgs);
    int dy = (int)jp_get_int(args, 1, numArgs);
    int passo = (int)jp_get_int(args, 2, numArgs);
    int tempo_ms = (int)jp_get_int(args, 3, numArgs);

    // Obter posição atual para calcular destino absoluto
    int atualX = 0, atualY = 0;
    
    #if JP_WINDOWS
        POINT p; GetCursorPos(&p);
        atualX = p.x; atualY = p.y;
    #else
        Display* d = get_display();
        if (!d) return jp_int(0);
        Window root = DefaultRootWindow(d);
        Window r, c; int rx, ry, wx, wy; unsigned int m;
        XQueryPointer(d, root, &r, &c, &rx, &ry, &wx, &wy, &m);
        atualX = rx; atualY = ry;
    #endif
    
    // Chama a função absoluta com o destino calculado
    // Precisamos reconstruir um array de args para chamar a função interna ou duplicar lógica
    // Vamos duplicar lógica simplificada para evitar overhead de criação de JPValor
    
    int destX = atualX + dx;
    int destY = atualY + dy;
    
    // Reusa a lógica da função acima chamando recursivamente via ponteiro de args?
    // Melhor fazer direto:
    JPValor novosArgs[4];
    novosArgs[0] = jp_int(destX);
    novosArgs[1] = jp_int(destY);
    novosArgs[2] = jp_int(passo);
    novosArgs[3] = jp_int(tempo_ms);
    
    return jp_ms_absmover_passo(novosArgs, 4);
}

// Arrastar (drag and drop) de A para B
JP_EXPORT JPValor jp_ms_arrastar(JPValor* args, int numArgs) {
    int x1 = (int)jp_get_int(args, 0, numArgs);
    int y1 = (int)jp_get_int(args, 1, numArgs);
    int x2 = (int)jp_get_int(args, 2, numArgs);
    int y2 = (int)jp_get_int(args, 3, numArgs);
    
    // Move para inicio
    JPValor moveArgs[2] = { jp_int(x1), jp_int(y1) };
    jp_ms_absmover(moveArgs, 2);
    ms_sleep(50);
    
    // Segura
    jp_ms_segurar(NULL, 0);
    ms_sleep(50);
    
    // Move para fim
    moveArgs[0] = jp_int(x2); 
    moveArgs[1] = jp_int(y2);
    jp_ms_absmover(moveArgs, 2);
    ms_sleep(50);
    
    // Solta
    jp_ms_soltar(NULL, 0);
    
    return jp_int(1);
}