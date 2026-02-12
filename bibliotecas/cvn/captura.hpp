// captura.hpp
// Módulo de captura de tela para biblioteca CVN (Portável Windows/Linux)

#ifndef CVN_CAPTURA_HPP
#define CVN_CAPTURA_HPP

#include <string>
#include <cstdlib>
#include <cstring>

// === PLATAFORMA WINDOWS ===
#ifdef _WIN32

#include <windows.h>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace cvn {

class Captura {
public:
    // Captura tela inteira
    static unsigned char* tela(int& largura, int& altura) {
        largura = GetSystemMetrics(SM_CXSCREEN);
        altura = GetSystemMetrics(SM_CYSCREEN);
        return capturar_regiao_interna(0, 0, largura, altura);
    }
    
    // Captura região específica
    static unsigned char* regiao(int x, int y, int largura, int altura) {
        if (largura <= 0 || altura <= 0) return nullptr;
        return capturar_regiao_interna(x, y, largura, altura);
    }
    
    // Captura janela pelo título
    static unsigned char* janela(const std::string& titulo, int& largura, int& altura) {
        int len = MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, NULL, 0);
        std::wstring wtitulo(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, titulo.c_str(), -1, &wtitulo[0], len);
        
        HWND hwnd = FindWindowW(NULL, wtitulo.c_str());
        if (!hwnd) {
            largura = 0;
            altura = 0;
            return nullptr;
        }
        
        RECT rect;
        GetWindowRect(hwnd, &rect);
        
        largura = rect.right - rect.left;
        altura = rect.bottom - rect.top;
        
        if (largura <= 0 || altura <= 0) return nullptr;
        
        return capturar_janela_interna(hwnd, largura, altura);
    }

private:
    static unsigned char* capturar_regiao_interna(int x, int y, int largura, int altura) {
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, largura, altura);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        
        BitBlt(hdcMem, 0, 0, largura, altura, hdcScreen, x, y, SRCCOPY);
        
        unsigned char* dados = extrair_pixels(hdcMem, hBitmap, largura, altura);
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        return dados;
    }
    
    static unsigned char* capturar_janela_interna(HWND hwnd, int largura, int altura) {
        HDC hdcWindow = GetWindowDC(hwnd);
        HDC hdcMem = CreateCompatibleDC(hdcWindow);
        
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, largura, altura);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        
        BitBlt(hdcMem, 0, 0, largura, altura, hdcWindow, 0, 0, SRCCOPY);
        
        unsigned char* dados = extrair_pixels(hdcMem, hBitmap, largura, altura);
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWindow);
        
        return dados;
    }
    
    static unsigned char* extrair_pixels(HDC hdcMem, HBITMAP hBitmap, int largura, int altura) {
        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = largura;
        bi.biHeight = -altura;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        
        int tamanho = largura * altura * 4;
        unsigned char* temp = (unsigned char*)malloc(tamanho);
        unsigned char* dados = (unsigned char*)malloc(tamanho);
        
        if (!temp || !dados) {
            if (temp) free(temp);
            if (dados) free(dados);
            return nullptr;
        }
        
        GetDIBits(hdcMem, hBitmap, 0, altura, temp, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        
        // BGRA para RGBA
        for (int i = 0; i < largura * altura; i++) {
            dados[i * 4 + 0] = temp[i * 4 + 2];
            dados[i * 4 + 1] = temp[i * 4 + 1];
            dados[i * 4 + 2] = temp[i * 4 + 0];
            dados[i * 4 + 3] = 255;
        }
        
        free(temp);
        return dados;
    }
};

} // namespace cvn

// === PLATAFORMA LINUX ===
#else

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace cvn {

class Captura {
private:
    static Display* obter_display() {
        static Display* display = nullptr;
        if (!display) {
            display = XOpenDisplay(nullptr);
        }
        return display;
    }

public:
    // Captura tela inteira
    static unsigned char* tela(int& largura, int& altura) {
        Display* display = obter_display();
        if (!display) {
            largura = 0;
            altura = 0;
            return nullptr;
        }
        
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);
        
        largura = DisplayWidth(display, screen);
        altura = DisplayHeight(display, screen);
        
        return capturar_janela_interna(display, root, 0, 0, largura, altura);
    }
    
    // Captura região específica
    static unsigned char* regiao(int x, int y, int largura, int altura) {
        if (largura <= 0 || altura <= 0) return nullptr;
        
        Display* display = obter_display();
        if (!display) return nullptr;
        
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);
        
        return capturar_janela_interna(display, root, x, y, largura, altura);
    }
    
    // Captura janela pelo título
    static unsigned char* janela(const std::string& titulo, int& largura, int& altura) {
        Display* display = obter_display();
        if (!display) {
            largura = 0;
            altura = 0;
            return nullptr;
        }
        
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);
        
        Window janela_encontrada = buscar_janela_por_titulo(display, root, titulo);
        if (!janela_encontrada) {
            largura = 0;
            altura = 0;
            return nullptr;
        }
        
        XWindowAttributes attrs;
        XGetWindowAttributes(display, janela_encontrada, &attrs);
        
        largura = attrs.width;
        altura = attrs.height;
        
        if (largura <= 0 || altura <= 0) return nullptr;
        
        return capturar_janela_interna(display, janela_encontrada, 0, 0, largura, altura);
    }

private:
    static Window buscar_janela_por_titulo(Display* display, Window root, const std::string& titulo) {
        Window parent, *filhos = nullptr;
        unsigned int num_filhos;
        
        if (XQueryTree(display, root, &root, &parent, &filhos, &num_filhos)) {
            for (unsigned int i = 0; i < num_filhos; i++) {
                char* nome = nullptr;
                if (XFetchName(display, filhos[i], &nome) && nome) {
                    if (titulo == nome) {
                        Window resultado = filhos[i];
                        XFree(nome);
                        if (filhos) XFree(filhos);
                        return resultado;
                    }
                    XFree(nome);
                }
                
                // Busca recursiva
                Window encontrada = buscar_janela_por_titulo(display, filhos[i], titulo);
                if (encontrada) {
                    if (filhos) XFree(filhos);
                    return encontrada;
                }
            }
            if (filhos) XFree(filhos);
        }
        return 0;
    }
    
    static unsigned char* capturar_janela_interna(Display* display, Window window, int x, int y, int largura, int altura) {
        XImage* ximage = XGetImage(display, window, x, y, largura, altura, AllPlanes, ZPixmap);
        if (!ximage) return nullptr;
        
        unsigned char* dados = (unsigned char*)malloc(largura * altura * 4);
        if (!dados) {
            XDestroyImage(ximage);
            return nullptr;
        }
        
        // Converte para RGBA
        for (int py = 0; py < altura; py++) {
            for (int px = 0; px < largura; px++) {
                unsigned long pixel = XGetPixel(ximage, px, py);
                int idx = (py * largura + px) * 4;
                
                // Extrai componentes (assumindo 24/32 bits)
                dados[idx + 0] = (pixel >> 16) & 0xFF; // R
                dados[idx + 1] = (pixel >> 8) & 0xFF;  // G
                dados[idx + 2] = pixel & 0xFF;         // B
                dados[idx + 3] = 255;                  // A
            }
        }
        
        XDestroyImage(ximage);
        return dados;
    }
};

} // namespace cvn

#endif // _WIN32

#endif // CVN_CAPTURA_HPP