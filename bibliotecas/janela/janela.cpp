// janela.cpp
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

struct Linha {
    int64_t id_janela;
    int x1, y1, x2, y2;
    int r, g, b; // Cores RGB
};

static std::unordered_map<int64_t, Linha> g_linhas;
static int64_t g_prox_id_linha = 1;
static int64_t g_prox_id_janela = 1;

#ifdef _WIN32
// =============================================================================
// WINDOWS (GDI32 com Double Buffering)
// =============================================================================
#include <windows.h>
#define JP_EXPORT extern "C" __declspec(dllexport)

struct JanelaDados { HWND hwnd; std::vector<int64_t> linhas; };
static std::unordered_map<int64_t, JanelaDados> g_janelas;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1; // Ignora apagar o fundo diretamente para evitar que a tela pisque
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Descobrir o ID desta janela
            int64_t id_janela_atual = 0;
            for (auto& par : g_janelas) {
                if (par.second.hwnd == hwnd) { id_janela_atual = par.first; break; }
            }

            // DOUBLE BUFFERING: Desenhar na memória antes de mandar pra tela
            RECT rect;
            GetClientRect(hwnd, &rect);
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(memDC, memBitmap);

            // Pintar fundo de branco
            HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &rect, bg);
            DeleteObject(bg);

            // Desenhar todas as linhas da janela
            if (id_janela_atual != 0) {
                for (int64_t id_linha : g_janelas[id_janela_atual].linhas) {
                    Linha& l = g_linhas[id_linha];
                    // Criar caneta com a espessura 3 e a cor RGB escolhida
                    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(l.r, l.g, l.b));
                    HPEN oldPen = (HPEN)SelectObject(memDC, hPen);
                    
                    MoveToEx(memDC, l.x1, l.y1, NULL);
                    LineTo(memDC, l.x2, l.y2);
                    
                    SelectObject(memDC, oldPen);
                    DeleteObject(hPen);
                }
            }
            
            // Transferir o desenho da memória para a tela de uma vez só
            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
            
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
        case WM_DESTROY: exit(0); return 0;
        default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

JP_EXPORT int64_t jn_criar(int64_t titulo_ptr, int64_t largura, int64_t altura) {
    const char* titulo = (const char*)titulo_ptr; if (!titulo) titulo = "Janela";
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const wchar_t CLASS_NAME[] = L"JPLangJanelaClass";

    static bool classe_registrada = false;
    if (!classe_registrada) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        classe_registrada = true;
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, titulo, -1, nullptr, 0);
    std::wstring wtitulo(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, titulo, -1, &wtitulo[0], len);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, wtitulo.c_str(), WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, (int)largura, (int)altura,
                                NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    int64_t id_janela = g_prox_id_janela++;
    g_janelas[id_janela] = { hwnd, {} };
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    return id_janela;
}

JP_EXPORT int64_t jn_desenhar_linha(int64_t id_janela, int64_t x1, int64_t y1, int64_t x2, int64_t y2, int64_t r, int64_t g, int64_t b) {
    if (g_janelas.find(id_janela) == g_janelas.end()) return 0;
    int64_t id_linha = g_prox_id_linha++;
    g_linhas[id_linha] = { id_janela, (int)x1, (int)y1, (int)x2, (int)y2, (int)r, (int)g, (int)b };
    g_janelas[id_janela].linhas.push_back(id_linha);
    InvalidateRect(g_janelas[id_janela].hwnd, NULL, FALSE);
    return id_linha;
}

JP_EXPORT int64_t jn_mover_linha(int64_t id_linha, int64_t x1, int64_t y1, int64_t x2, int64_t y2) {
    if (g_linhas.find(id_linha) != g_linhas.end()) {
        g_linhas[id_linha].x1 = (int)x1; g_linhas[id_linha].y1 = (int)y1;
        g_linhas[id_linha].x2 = (int)x2; g_linhas[id_linha].y2 = (int)y2;
        int64_t id_janela = g_linhas[id_linha].id_janela;
        InvalidateRect(g_janelas[id_janela].hwnd, NULL, FALSE);
    }
    return 1;
}

JP_EXPORT int64_t jn_exibir(int64_t id_janela) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) exit(0);
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    Sleep(1);
    return 0;
}

#else
// =============================================================================
// LINUX (X11 com Double Buffering via Pixmap)
// =============================================================================
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <time.h>
#include <unistd.h>
#define JP_EXPORT extern "C" __attribute__((visibility("default")))

struct JanelaDados { Window win; GC gc; std::vector<int64_t> linhas; };
static std::unordered_map<int64_t, JanelaDados> g_janelas;
static Display* g_display = nullptr;
static Atom g_wm_delete_window = 0;

JP_EXPORT int64_t jn_criar(int64_t titulo_ptr, int64_t largura, int64_t altura) {
    const char* titulo = (const char*)titulo_ptr; if (!titulo) titulo = "Janela";
    if (!g_display) {
        g_display = XOpenDisplay(NULL);
        if (!g_display) return 0;
        g_wm_delete_window = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    }
    int screen = DefaultScreen(g_display);
    Window root = RootWindow(g_display, screen);
    Window win = XCreateSimpleWindow(g_display, root, 0, 0, largura, altura, 1, BlackPixel(g_display, screen), WhitePixel(g_display, screen));

    Atom net_wm_name = XInternAtom(g_display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(g_display, "UTF8_STRING", False);
    XChangeProperty(g_display, win, net_wm_name, utf8_string, 8, PropModeReplace, (const unsigned char*)titulo, strlen(titulo));

    XSetWMProtocols(g_display, win, &g_wm_delete_window, 1);
    XSelectInput(g_display, win, ExposureMask | KeyPressMask | StructureNotifyMask);

    GC gc = XCreateGC(g_display, win, 0, NULL);
    XMapWindow(g_display, win);
    XFlush(g_display);

    int64_t id_janela = g_prox_id_janela++;
    g_janelas[id_janela] = { win, gc, {} };
    return id_janela;
}

JP_EXPORT int64_t jn_desenhar_linha(int64_t id_janela, int64_t x1, int64_t y1, int64_t x2, int64_t y2, int64_t r, int64_t g, int64_t b) {
    if (g_janelas.find(id_janela) == g_janelas.end()) return 0;
    int64_t id_linha = g_prox_id_linha++;
    g_linhas[id_linha] = { id_janela, (int)x1, (int)y1, (int)x2, (int)y2, (int)r, (int)g, (int)b };
    g_janelas[id_janela].linhas.push_back(id_linha);
    
    // Força o evento Expose
    XClearArea(g_display, g_janelas[id_janela].win, 0, 0, 0, 0, True);
    return id_linha;
}

JP_EXPORT int64_t jn_mover_linha(int64_t id_linha, int64_t x1, int64_t y1, int64_t x2, int64_t y2) {
    if (g_linhas.find(id_linha) != g_linhas.end()) {
        g_linhas[id_linha].x1 = (int)x1; g_linhas[id_linha].y1 = (int)y1;
        g_linhas[id_linha].x2 = (int)x2; g_linhas[id_linha].y2 = (int)y2;
        int64_t id_janela = g_linhas[id_linha].id_janela;
        XClearArea(g_display, g_janelas[id_janela].win, 0, 0, 0, 0, True);
    }
    return 1;
}

JP_EXPORT int64_t jn_exibir(int64_t id_janela) {
    if (!g_display) return 0;
    while (XPending(g_display) > 0) {
        XEvent event;
        XNextEvent(g_display, &event);
        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == g_wm_delete_window) {
            XCloseDisplay(g_display); exit(0);
        }
        else if (event.type == Expose) {
            for (auto& par : g_janelas) {
                if (par.second.win == event.xexpose.window) {
                    int screen = DefaultScreen(g_display);
                    XWindowAttributes wa;
                    XGetWindowAttributes(g_display, par.second.win, &wa);
                    
                    // DOUBLE BUFFERING: Cria um Pixmap (buffer de memória)
                    Pixmap buffer = XCreatePixmap(g_display, par.second.win, wa.width, wa.height, wa.depth);
                    
                    // Preenche o buffer de branco
                    XSetForeground(g_display, par.second.gc, WhitePixel(g_display, screen));
                    XFillRectangle(g_display, buffer, par.second.gc, 0, 0, wa.width, wa.height);

                    Colormap cmap = DefaultColormap(g_display, screen);
                    XSetLineAttributes(g_display, par.second.gc, 3, LineSolid, CapRound, JoinRound);

                    // Desenha as linhas da janela no buffer
                    for (int64_t id_linha : par.second.linhas) {
                        Linha& l = g_linhas[id_linha];
                        XColor color;
                        color.red = l.r * 257; color.green = l.g * 257; color.blue = l.b * 257;
                        color.flags = DoRed | DoGreen | DoBlue;
                        XAllocColor(g_display, cmap, &color);
                        XSetForeground(g_display, par.second.gc, color.pixel);
                        XDrawLine(g_display, buffer, par.second.gc, l.x1, l.y1, l.x2, l.y2);
                    }
                    
                    // Copia o buffer pra tela numa tacada só
                    XCopyArea(g_display, buffer, par.second.win, par.second.gc, 0, 0, wa.width, wa.height, 0, 0);
                    XFreePixmap(g_display, buffer);
                    break;
                }
            }
        }
    }
    struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 1000000;
    nanosleep(&ts, NULL);
    return 0;
}
#endif