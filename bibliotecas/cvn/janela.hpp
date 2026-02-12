// janela.hpp
// Módulo de gerenciamento de janelas para biblioteca CVN (Portável Windows/Linux)

#ifndef CVN_JANELA_HPP
#define CVN_JANELA_HPP

#include <string>
#include <unordered_map>
#include <cstring>
#include <cstdlib>

// === PLATAFORMA WINDOWS ===
#ifdef _WIN32

#include <windows.h>

namespace cvn {

struct JanelaInfo {
    HWND hwnd;
    HDC hdcMem;
    HBITMAP hbitmapMem;
    HBITMAP hbitmapOld;
    int largura;
    int altura;
    bool executando;
};

class GerenciadorJanelas {
private:
    WNDCLASSEXW wc = {};
    bool classe_registrada = false;
    std::unordered_map<std::string, JanelaInfo> janelas;
    std::unordered_map<HWND, std::string> hwnd_para_titulo;

    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto& inst = instancia();
        
        switch (msg) {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hWnd, &ps);
                
                auto it = inst.hwnd_para_titulo.find(hWnd);
                if (it != inst.hwnd_para_titulo.end()) {
                    auto& info = inst.janelas[it->second];
                    if (info.hdcMem) {
                        BitBlt(hdc, 0, 0, info.largura, info.altura, info.hdcMem, 0, 0, SRCCOPY);
                    } else {
                        RECT rect;
                        GetClientRect(hWnd, &rect);
                        FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    }
                }
                
                EndPaint(hWnd, &ps);
            }
            return 0;
            
        case WM_DESTROY:
            {
                auto it = inst.hwnd_para_titulo.find(hWnd);
                if (it != inst.hwnd_para_titulo.end()) {
                    auto& info = inst.janelas[it->second];
                    info.executando = false;
                    if (info.hdcMem) {
                        SelectObject(info.hdcMem, info.hbitmapOld);
                        DeleteObject(info.hbitmapMem);
                        DeleteDC(info.hdcMem);
                    }
                    inst.janelas.erase(it->second);
                    inst.hwnd_para_titulo.erase(it);
                }
            }
            return 0;
            
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                auto it = inst.hwnd_para_titulo.find(hWnd);
                if (it != inst.hwnd_para_titulo.end()) {
                    inst.janelas[it->second].executando = false;
                }
            }
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    bool registrar_classe() {
        if (classe_registrada) return true;
        
        wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = L"CVN_JPLang";
        
        if (RegisterClassExW(&wc)) {
            classe_registrada = true;
            return true;
        }
        return false;
    }

public:
    static GerenciadorJanelas& instancia() {
        static GerenciadorJanelas inst;
        return inst;
    }

    JanelaInfo* obter_ou_criar(const std::string& titulo, int largura, int altura) {
        auto it = janelas.find(titulo);
        if (it != janelas.end()) {
            if (it->second.largura != largura || it->second.altura != altura) {
                redimensionar_buffer(&it->second, largura, altura);
            }
            return &it->second;
        }
        
        if (!registrar_classe()) return nullptr;
        
        int len = MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, NULL, 0);
        std::wstring wtitulo(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, &wtitulo[0], len);
        
        RECT rect = {0, 0, largura, altura};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        int janela_largura = rect.right - rect.left;
        int janela_altura = rect.bottom - rect.top;
        
        int tela_largura = GetSystemMetrics(SM_CXSCREEN);
        int tela_altura = GetSystemMetrics(SM_CYSCREEN);
        int pos_x = (tela_largura - janela_largura) / 2;
        int pos_y = (tela_altura - janela_altura) / 2;
        
        if (pos_x < 0) pos_x = 0;
        if (pos_y < 0) pos_y = 0;
        if (janela_largura > tela_largura) janela_largura = tela_largura;
        if (janela_altura > tela_altura) janela_altura = tela_altura;
        
        HWND hwnd = CreateWindowW(
            wc.lpszClassName, wtitulo.c_str(), WS_OVERLAPPEDWINDOW,
            pos_x, pos_y, janela_largura, janela_altura,
            nullptr, nullptr, wc.hInstance, nullptr
        );
        
        if (!hwnd) return nullptr;
        
        JanelaInfo info = {};
        info.hwnd = hwnd;
        info.largura = largura;
        info.altura = altura;
        info.executando = true;
        
        HDC hdcWnd = GetDC(hwnd);
        info.hdcMem = CreateCompatibleDC(hdcWnd);
        info.hbitmapMem = CreateCompatibleBitmap(hdcWnd, largura, altura);
        info.hbitmapOld = (HBITMAP)SelectObject(info.hdcMem, info.hbitmapMem);
        ReleaseDC(hwnd, hdcWnd);
        
        janelas[titulo] = info;
        hwnd_para_titulo[hwnd] = titulo;
        
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        
        return &janelas[titulo];
    }
    
    void redimensionar_buffer(JanelaInfo* info, int largura, int altura) {
        if (!info || !info->hwnd) return;
        
        if (info->hdcMem) {
            SelectObject(info->hdcMem, info->hbitmapOld);
            DeleteObject(info->hbitmapMem);
            DeleteDC(info->hdcMem);
        }
        
        HDC hdcWnd = GetDC(info->hwnd);
        info->hdcMem = CreateCompatibleDC(hdcWnd);
        info->hbitmapMem = CreateCompatibleBitmap(hdcWnd, largura, altura);
        info->hbitmapOld = (HBITMAP)SelectObject(info->hdcMem, info->hbitmapMem);
        ReleaseDC(info->hwnd, hdcWnd);
        
        info->largura = largura;
        info->altura = altura;
        
        RECT rect = {0, 0, largura, altura};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(info->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, 
                     SWP_NOMOVE | SWP_NOZORDER);
    }
    
    bool atualizar(const std::string& titulo, unsigned char* dados, int largura, int altura) {
        JanelaInfo* info = obter_ou_criar(titulo, largura, altura);
        if (!info) return false;
        
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = largura;
        bmi.bmiHeader.biHeight = -altura;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        unsigned char* bgra = (unsigned char*)malloc(largura * altura * 4);
        if (!bgra) return false;
        
        for (int i = 0; i < largura * altura; i++) {
            bgra[i * 4 + 0] = dados[i * 4 + 2];
            bgra[i * 4 + 1] = dados[i * 4 + 1];
            bgra[i * 4 + 2] = dados[i * 4 + 0];
            bgra[i * 4 + 3] = dados[i * 4 + 3];
        }
        
        SetDIBitsToDevice(info->hdcMem, 0, 0, largura, altura, 0, 0, 0, altura, bgra, &bmi, DIB_RGB_COLORS);
        free(bgra);
        
        InvalidateRect(info->hwnd, NULL, FALSE);
        UpdateWindow(info->hwnd);
        
        return true;
    }
    
    bool exibir(const std::string& titulo, unsigned char* dados, int largura, int altura, int img_id) {
        return atualizar(titulo, dados, largura, altura);
    }
    
    bool processar_mensagens(const std::string& titulo) {
        auto it = janelas.find(titulo);
        if (it == janelas.end()) return false;
        
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                it->second.executando = false;
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        return it->second.executando;
    }

    int esperar(int ms) {
        MSG msg;
        DWORD inicio = GetTickCount();
        
        while (true) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) return -1;
                if (msg.message == WM_KEYDOWN) return (int)msg.wParam;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            if (ms > 0) {
                DWORD elapsed = GetTickCount() - inicio;
                if (elapsed >= (DWORD)ms) return -1;
            }
            
            if (janelas.empty()) return -1;
            Sleep(1);
        }
        return -1;
    }

    bool fechar(const std::string& titulo) {
        auto it = janelas.find(titulo);
        if (it != janelas.end()) {
            DestroyWindow(it->second.hwnd);
            return true;
        }
        return false;
    }

    void fechar_todas() {
        for (auto& par : janelas) {
            if (par.second.hdcMem) {
                SelectObject(par.second.hdcMem, par.second.hbitmapOld);
                DeleteObject(par.second.hbitmapMem);
                DeleteDC(par.second.hdcMem);
            }
            DestroyWindow(par.second.hwnd);
        }
        janelas.clear();
        hwnd_para_titulo.clear();
    }

    bool tem_janelas() const { return !janelas.empty(); }

    ~GerenciadorJanelas() { fechar_todas(); }
};

} // namespace cvn

// === PLATAFORMA LINUX ===
#else

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <unistd.h>

namespace cvn {

struct JanelaInfo {
    Window window;
    GC gc;
    XImage* ximage;
    unsigned char* buffer;
    int largura;
    int altura;
    bool executando;
    Atom wm_delete;
};

class GerenciadorJanelas {
private:
    Display* display = nullptr;
    int screen = 0;
    std::unordered_map<std::string, JanelaInfo> janelas;
    std::unordered_map<Window, std::string> window_para_titulo;

    bool inicializar_display() {
        if (display) return true;
        display = XOpenDisplay(nullptr);
        if (display) {
            screen = DefaultScreen(display);
            return true;
        }
        return false;
    }

    unsigned long obter_tempo_ms() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }

public:
    static GerenciadorJanelas& instancia() {
        static GerenciadorJanelas inst;
        return inst;
    }

    JanelaInfo* obter_ou_criar(const std::string& titulo, int largura, int altura) {
        auto it = janelas.find(titulo);
        if (it != janelas.end()) {
            if (it->second.largura != largura || it->second.altura != altura) {
                redimensionar_buffer(&it->second, largura, altura);
            }
            return &it->second;
        }
        
        if (!inicializar_display()) return nullptr;
        
        int tela_largura = DisplayWidth(display, screen);
        int tela_altura = DisplayHeight(display, screen);
        int pos_x = (tela_largura - largura) / 2;
        int pos_y = (tela_altura - altura) / 2;
        if (pos_x < 0) pos_x = 0;
        if (pos_y < 0) pos_y = 0;
        
        Window root = RootWindow(display, screen);
        Window window = XCreateSimpleWindow(
            display, root,
            pos_x, pos_y, largura, altura, 1,
            BlackPixel(display, screen),
            BlackPixel(display, screen)
        );
        
        XStoreName(display, window, titulo.c_str());
        XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
        
        Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wm_delete, 1);
        
        GC gc = XCreateGC(display, window, 0, nullptr);
        
        JanelaInfo info = {};
        info.window = window;
        info.gc = gc;
        info.largura = largura;
        info.altura = altura;
        info.executando = true;
        info.wm_delete = wm_delete;
        info.buffer = (unsigned char*)malloc(largura * altura * 4);
        info.ximage = nullptr;
        
        if (info.buffer) {
            memset(info.buffer, 0, largura * altura * 4);
            Visual* visual = DefaultVisual(display, screen);
            int depth = DefaultDepth(display, screen);
            info.ximage = XCreateImage(display, visual, depth, ZPixmap, 0,
                                       (char*)info.buffer, largura, altura, 32, 0);
        }
        
        janelas[titulo] = info;
        window_para_titulo[window] = titulo;
        
        XMapWindow(display, window);
        XFlush(display);
        
        return &janelas[titulo];
    }
    
    void redimensionar_buffer(JanelaInfo* info, int largura, int altura) {
        if (!info || !display) return;
        
        if (info->ximage) {
            info->ximage->data = nullptr;
            XDestroyImage(info->ximage);
        }
        if (info->buffer) free(info->buffer);
        
        info->buffer = (unsigned char*)malloc(largura * altura * 4);
        if (info->buffer) {
            memset(info->buffer, 0, largura * altura * 4);
            Visual* visual = DefaultVisual(display, screen);
            int depth = DefaultDepth(display, screen);
            info->ximage = XCreateImage(display, visual, depth, ZPixmap, 0,
                                        (char*)info->buffer, largura, altura, 32, 0);
        }
        
        info->largura = largura;
        info->altura = altura;
        
        XResizeWindow(display, info->window, largura, altura);
    }
    
    bool atualizar(const std::string& titulo, unsigned char* dados, int largura, int altura) {
        JanelaInfo* info = obter_ou_criar(titulo, largura, altura);
        if (!info || !info->buffer || !info->ximage) return false;
        
        // Converte RGBA para BGRA (formato X11)
        for (int i = 0; i < largura * altura; i++) {
            info->buffer[i * 4 + 0] = dados[i * 4 + 2]; // B
            info->buffer[i * 4 + 1] = dados[i * 4 + 1]; // G
            info->buffer[i * 4 + 2] = dados[i * 4 + 0]; // R
            info->buffer[i * 4 + 3] = dados[i * 4 + 3]; // A
        }
        
        XPutImage(display, info->window, info->gc, info->ximage, 0, 0, 0, 0, largura, altura);
        XFlush(display);
        
        return true;
    }
    
    bool exibir(const std::string& titulo, unsigned char* dados, int largura, int altura, int img_id) {
        return atualizar(titulo, dados, largura, altura);
    }
    
    bool processar_mensagens(const std::string& titulo) {
        auto it = janelas.find(titulo);
        if (it == janelas.end()) return false;
        
        if (!display) return false;
        
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            
            if (event.type == ClientMessage) {
                auto wit = window_para_titulo.find(event.xclient.window);
                if (wit != window_para_titulo.end()) {
                    JanelaInfo& info = janelas[wit->second];
                    if ((Atom)event.xclient.data.l[0] == info.wm_delete) {
                        info.executando = false;
                    }
                }
            } else if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Escape) {
                    auto wit = window_para_titulo.find(event.xkey.window);
                    if (wit != window_para_titulo.end()) {
                        janelas[wit->second].executando = false;
                    }
                }
            } else if (event.type == Expose) {
                auto wit = window_para_titulo.find(event.xexpose.window);
                if (wit != window_para_titulo.end()) {
                    JanelaInfo& info = janelas[wit->second];
                    if (info.ximage) {
                        XPutImage(display, info.window, info.gc, info.ximage, 
                                  0, 0, 0, 0, info.largura, info.altura);
                    }
                }
            }
        }
        
        return it->second.executando;
    }

    int esperar(int ms) {
        if (!display) return -1;
        
        unsigned long inicio = obter_tempo_ms();
        
        while (true) {
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                
                if (event.type == KeyPress) {
                    KeySym key = XLookupKeysym(&event.xkey, 0);
                    return (int)key;
                } else if (event.type == ClientMessage) {
                    auto wit = window_para_titulo.find(event.xclient.window);
                    if (wit != window_para_titulo.end()) {
                        JanelaInfo& info = janelas[wit->second];
                        if ((Atom)event.xclient.data.l[0] == info.wm_delete) {
                            info.executando = false;
                            return -1;
                        }
                    }
                } else if (event.type == Expose) {
                    auto wit = window_para_titulo.find(event.xexpose.window);
                    if (wit != window_para_titulo.end()) {
                        JanelaInfo& info = janelas[wit->second];
                        if (info.ximage) {
                            XPutImage(display, info.window, info.gc, info.ximage, 
                                      0, 0, 0, 0, info.largura, info.altura);
                        }
                    }
                }
            }
            
            if (ms > 0) {
                unsigned long elapsed = obter_tempo_ms() - inicio;
                if (elapsed >= (unsigned long)ms) return -1;
            }
            
            if (janelas.empty()) return -1;
            usleep(1000);
        }
        return -1;
    }

    bool fechar(const std::string& titulo) {
        auto it = janelas.find(titulo);
        if (it != janelas.end() && display) {
            JanelaInfo& info = it->second;
            if (info.ximage) {
                info.ximage->data = nullptr;
                XDestroyImage(info.ximage);
            }
            if (info.buffer) free(info.buffer);
            XFreeGC(display, info.gc);
            XDestroyWindow(display, info.window);
            window_para_titulo.erase(info.window);
            janelas.erase(it);
            XFlush(display);
            return true;
        }
        return false;
    }

    void fechar_todas() {
        if (!display) return;
        for (auto& par : janelas) {
            JanelaInfo& info = par.second;
            if (info.ximage) {
                info.ximage->data = nullptr;
                XDestroyImage(info.ximage);
            }
            if (info.buffer) free(info.buffer);
            XFreeGC(display, info.gc);
            XDestroyWindow(display, info.window);
        }
        janelas.clear();
        window_para_titulo.clear();
        XFlush(display);
    }

    bool tem_janelas() const { return !janelas.empty(); }

    ~GerenciadorJanelas() {
        fechar_todas();
        if (display) {
            XCloseDisplay(display);
            display = nullptr;
        }
    }
};

} // namespace cvn

#endif // _WIN32

#endif // CVN_JANELA_HPP