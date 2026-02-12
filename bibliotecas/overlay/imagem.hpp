// imagem.hpp
// Funções de overlay de imagem para JPLang
#ifndef IMAGEM_HPP
#define IMAGEM_HPP

#include <iostream>

// Renderiza a imagem pré-carregada no overlay
static void renderizar_imagem(OverlayInfo& info) {
    if (!info.ativo || !info.hwnd || !info.imagemBitmap) return;
    
    // Debug: Descomente se quiser ver o spam de renderização
    // std::cout << "[Debug] Renderizando frame..." << std::endl;

    int largura = info.largura;
    int altura = info.altura;
    
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    // Configura o bitmap de buffer
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = largura;
    bmi.bmiHeader.biHeight = -altura; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    
    if (!hBitmap) {
        std::cout << "[Erro] Falha CreateDIBSection" << std::endl;
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Desenha com GDI+
    Gdiplus::Graphics graphics(hdcMem);
    
    // Desenha a imagem armazenada
    Gdiplus::Bitmap* img = (Gdiplus::Bitmap*)info.imagemBitmap;
    
    // Verifica se o bitmap é válido
    if (img->GetLastStatus() != Gdiplus::Ok) {
        std::cout << "[Erro] Bitmap invalido durante renderizacao" << std::endl;
    } else {
        graphics.DrawImage(img, 0, 0, largura, altura);
    }
    
    // Atualiza a janela transparente
    POINT ptSrc = {0, 0};
    POINT ptDst = {info.x, info.y};
    SIZE size = {largura, altura};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    
    if (!UpdateLayeredWindow(info.hwnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA)) {
         // std::cout << "[Erro] UpdateLayeredWindow falhou: " << GetLastError() << std::endl;
    }
    
    // Limpeza GDI
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

// --- Funções Exportadas ---
extern "C" {

    // overlay.imagem(caminho, x, y, largura, altura)
    JP_EXPORT JPValor jp_ov_imagem(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        
        if (numArgs < 5) return jp_int(0);
        
        iniciar_gdiplus();
        registrar_classe();
        
        std::string caminho = jp_get_string(args[0]);
        int x = (int)jp_get_int(args[1]);
        int y = (int)jp_get_int(args[2]);
        int reqLargura = (int)jp_get_int(args[3]);
        int reqAltura = (int)jp_get_int(args[4]);
        
        std::cout << "[1] Carregando: " << caminho << std::endl;

        // 1. Carregar imagem com STB
        int w, h, canais;
        unsigned char* dados = stbi_load(caminho.c_str(), &w, &h, &canais, 4);
        
        if (!dados) {
            std::cout << "[Erro] Falha ao carregar arquivo." << std::endl;
            return jp_int(0);
        }

        // 2. Redimensionar
        unsigned char* dadosFinais = dados;
        bool foiRedimensionado = false;

        if (w != reqLargura || h != reqAltura) {
            unsigned char* dadosResize = (unsigned char*)malloc(reqLargura * reqAltura * 4);
            if (dadosResize) {
                stbir_resize_uint8_linear(dados, w, h, 0, 
                                          dadosResize, reqLargura, reqAltura, 0, 
                                          STBIR_RGBA);
                dadosFinais = dadosResize;
                foiRedimensionado = true;
            } else {
                reqLargura = w;
                reqAltura = h;
            }
        }

        // 3. Converter RGBA -> BGRA
        int pixelsTotal = reqLargura * reqAltura;
        for (int i = 0; i < pixelsTotal; i++) {
            unsigned char r = dadosFinais[i * 4 + 0];
            unsigned char b = dadosFinais[i * 4 + 2];
            dadosFinais[i * 4 + 0] = b;
            dadosFinais[i * 4 + 2] = r;
        }

        // 4. Criar Bitmap Seguro (Deep Copy)
        // Criamos um bitmap temporário apontando para os dados brutos
        Gdiplus::Bitmap* bmpTemp = new Gdiplus::Bitmap(reqLargura, reqAltura, 4 * reqLargura, PixelFormat32bppARGB, dadosFinais);
        
        // Criamos o bitmap FINAL vazio, gerenciado pelo GDI+
        Gdiplus::Bitmap* bmpFinal = new Gdiplus::Bitmap(reqLargura, reqAltura, PixelFormat32bppARGB);
        
        // Desenhamos o temporário no final para garantir a cópia dos dados
        Gdiplus::Graphics g(bmpFinal);
        g.DrawImage(bmpTemp, 0, 0, reqLargura, reqAltura);
        
        // Agora podemos apagar tudo sem medo
        delete bmpTemp;
        if (foiRedimensionado) free(dadosFinais);
        stbi_image_free(dados);

        if (bmpFinal->GetLastStatus() != Gdiplus::Ok) {
            std::cout << "[Erro] Falha ao criar Bitmap Final." << std::endl;
            delete bmpFinal;
            return jp_int(0);
        }

        std::cout << "[2] Bitmap criado com sucesso na memoria." << std::endl;

        // 5. Criar Janela
        HWND hwnd = criar_janela_overlay(x, y, reqLargura, reqAltura);
        if (!hwnd) {
            std::cout << "[Erro] Falha ao criar janela." << std::endl;
            delete bmpFinal;
            return jp_int(0);
        }

        int handle = g_nextHandle++;
        
        OverlayInfo info;
        info.hwnd = hwnd;
        info.tipo = TipoOverlay::IMAGEM;
        info.x = x;
        info.y = y;
        info.largura = reqLargura;
        info.altura = reqAltura;
        info.ativo = true;
        info.imagemBitmap = (void*)bmpFinal;
        info.r = 255; info.g = 255; info.b = 255;
        
        g_overlays[handle] = info;
        
        std::cout << "[3] Overlay registrado. Handle: " << handle << std::endl;
        
        return jp_int(handle);
    }
}

#endif // IMAGEM_HPP