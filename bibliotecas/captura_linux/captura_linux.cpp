// captura.cpp
// Biblioteca nativa para JPLang — captura de tela no Linux via X11/Xlib + stb_image_write (PNG)

/*
 *  Compilação:
 *    g++ -c -O2 -o bibliotecas/captura_linux/captura_linux.o bibliotecas/captura_linux/captura_linux.cpp -I.
 *
 *  Dependências de sistema:
 *    sudo apt install libx11-dev
 *
 *  O arquivo stb_image_write.h deve estar no mesmo diretório.
 *  Baixar de: https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
 */

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <cstdint>
#include <cstring>

// =====================================================================
// cap_capturar(caminho, x, y, largura, altura)
//
// Captura uma região da tela e salva como PNG.
//   caminho  — path do arquivo de saída (ex: "captura.png")
//   x, y     — coordenadas do canto superior esquerdo
//   largura  — largura em pixels
//   altura   — altura em pixels
//
// Se x=0, y=0, largura=0, altura=0 → captura a tela inteira.
// =====================================================================

extern "C" void cap_capturar(const char* caminho,
                              int x, int y,
                              int largura, int altura)
{
    // Abrir conexão com o display X11
    Display* display = XOpenDisplay(nullptr);
    if (!display) return;

    Window root = DefaultRootWindow(display);
    int screen = DefaultScreen(display);

    // Se largura/altura == 0, capturar tela inteira
    if (largura <= 0 || altura <= 0) {
        largura = DisplayWidth(display, screen);
        altura  = DisplayHeight(display, screen);
        x = 0;
        y = 0;
    }

    // Capturar a imagem da tela via XGetImage
    XImage* img = XGetImage(display, root,
                            x, y,
                            (unsigned int)largura,
                            (unsigned int)altura,
                            AllPlanes, ZPixmap);

    if (!img) {
        XCloseDisplay(display);
        return;
    }

    // Converter XImage (BGRA) para buffer RGB para stb_image_write
    int w = img->width;
    int h = img->height;
    unsigned char* rgb = (unsigned char*)malloc((size_t)w * (size_t)h * 3);

    if (!rgb) {
        XDestroyImage(img);
        XCloseDisplay(display);
        return;
    }

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            unsigned long pixel = XGetPixel(img, px, py);
            size_t idx = ((size_t)py * (size_t)w + (size_t)px) * 3;
            rgb[idx + 0] = (unsigned char)((pixel >> 16) & 0xFF); // R
            rgb[idx + 1] = (unsigned char)((pixel >>  8) & 0xFF); // G
            rgb[idx + 2] = (unsigned char)((pixel >>  0) & 0xFF); // B
        }
    }

    // Salvar como PNG
    stbi_write_png(caminho, w, h, 3, rgb, w * 3);

    // Limpeza
    free(rgb);
    XDestroyImage(img);
    XCloseDisplay(display);
}