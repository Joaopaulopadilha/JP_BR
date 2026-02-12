// retangulo.hpp
#ifndef RETANGULO_HPP
#define RETANGULO_HPP

static void renderizar_retangulo(OverlayInfo& info) {
    if (!info.ativo || !info.hwnd) return;
    
    int largura = info.largura;
    int altura = info.altura;
    int espessura = info.espessura;
    
    int bmpLargura = largura + espessura * 2;
    int bmpAltura = altura + espessura * 2;
    
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmpLargura;
    bmi.bmiHeader.biHeight = -bmpAltura;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    memset(bits, 0, bmpLargura * bmpAltura * 4);
    
    Gdiplus::Graphics graphics(hdcMem);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    
    Gdiplus::Pen pen(Gdiplus::Color(255, info.r, info.g, info.b), (Gdiplus::REAL)espessura);
    float offset = espessura / 2.0f;
    graphics.DrawRectangle(&pen, offset, offset, (Gdiplus::REAL)(largura), (Gdiplus::REAL)(altura));
    
    POINT ptSrc = {0, 0};
    POINT ptDst = {info.x - espessura / 2, info.y - espessura / 2};
    SIZE size = {bmpLargura, bmpAltura};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    
    UpdateLayeredWindow(info.hwnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

extern "C" {
    // overlay.retangulo(x, y, largura, altura, espessura, r, g, b)
    JP_EXPORT JPValor jp_ov_retangulo(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 8) return jp_int(0);
        
        iniciar_gdiplus();
        registrar_classe();
        
        int x = (int)jp_get_int(args[0]);
        int y = (int)jp_get_int(args[1]);
        int largura = (int)jp_get_int(args[2]);
        int altura = (int)jp_get_int(args[3]);
        int espessura = (int)jp_get_int(args[4]);
        int r = (int)jp_get_int(args[5]);
        int g = (int)jp_get_int(args[6]);
        int b = (int)jp_get_int(args[7]);
        
        if (espessura < 1) espessura = 1;
        
        HWND hwnd = criar_janela_overlay(x, y, largura + espessura * 2, altura + espessura * 2);
        if (!hwnd) return jp_int(0);
        
        int handle = g_nextHandle++;
        OverlayInfo info;
        info.hwnd = hwnd;
        info.tipo = TipoOverlay::RETANGULO;
        info.x = x; info.y = y;
        info.largura = largura; info.altura = altura;
        info.espessura = espessura;
        info.r = r; info.g = g; info.b = b;
        info.ativo = true;
        info.texto = ""; info.tamanho = 0;
        
        g_overlays[handle] = info;
        return jp_int(handle);
    }

    // overlay.redimensionar(handle, largura, altura)
    JP_EXPORT JPValor jp_ov_redimensionar(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 3) return jp_int(0);

        int handle = (int)jp_get_int(args[0]);
        int largura = (int)jp_get_int(args[1]);
        int altura = (int)jp_get_int(args[2]);
        
        auto it = g_overlays.find(handle);
        if (it != g_overlays.end()) {
            it->second.largura = largura;
            it->second.altura = altura;
            return jp_int(1);
        }
        return jp_int(0);
    }
}
#endif