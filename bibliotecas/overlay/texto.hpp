// texto.hpp
#ifndef TEXTO_HPP
#define TEXTO_HPP

static void renderizar_texto(OverlayInfo& info) {
    if (!info.ativo || !info.hwnd) return;
    std::wstring textoW = utf8_to_wide(info.texto);
    
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    Gdiplus::Font font(L"Consolas", (Gdiplus::REAL)info.tamanho, Gdiplus::FontStyleBold);
    Gdiplus::Graphics gTemp(hdcMem);
    Gdiplus::RectF bounds;
    gTemp.MeasureString(textoW.c_str(), -1, &font, Gdiplus::PointF(0, 0), &bounds);
    
    int largura = (int)bounds.Width + 20;
    int altura = (int)bounds.Height + 10;
    if (largura < 50) largura = 50;
    if (altura < 20) altura = 20;
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = largura;
    bmi.bmiHeader.biHeight = -altura;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    memset(bits, 0, largura * altura * 4);
    
    Gdiplus::Graphics graphics(hdcMem);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    
    Gdiplus::SolidBrush brushSombra(Gdiplus::Color(180, 0, 0, 0));
    graphics.DrawString(textoW.c_str(), -1, &font, Gdiplus::PointF(2, 2), &brushSombra);
    
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, info.r, info.g, info.b));
    graphics.DrawString(textoW.c_str(), -1, &font, Gdiplus::PointF(0, 0), &brush);
    
    POINT ptSrc = {0, 0};
    POINT ptDst = {info.x, info.y};
    SIZE size = {largura, altura};
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
    // overlay.texto(texto, x, y, tamanho, r, g, b)
    JP_EXPORT JPValor jp_ov_texto(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 7) return jp_int(0);
        
        iniciar_gdiplus();
        registrar_classe();
        
        std::string texto = jp_get_string(args[0]);
        int x = (int)jp_get_int(args[1]);
        int y = (int)jp_get_int(args[2]);
        int tamanho = (int)jp_get_int(args[3]);
        int r = (int)jp_get_int(args[4]);
        int g = (int)jp_get_int(args[5]);
        int b = (int)jp_get_int(args[6]);
        
        HWND hwnd = criar_janela_overlay(x, y, 400, 50);
        if (!hwnd) return jp_int(0);
        
        int handle = g_nextHandle++;
        OverlayInfo info;
        info.hwnd = hwnd;
        info.tipo = TipoOverlay::TEXTO;
        info.texto = texto;
        info.x = x; info.y = y;
        info.tamanho = tamanho;
        info.r = r; info.g = g; info.b = b;
        info.ativo = true;
        info.largura = 0; info.altura = 0; info.espessura = 0;
        
        g_overlays[handle] = info;
        return jp_int(handle);
    }

    // overlay.atualizar(handle, novo_texto)
    JP_EXPORT JPValor jp_ov_atualizar(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 2) return jp_int(0);

        int handle = (int)jp_get_int(args[0]);
        std::string novoTexto = jp_get_string(args[1]);
        
        auto it = g_overlays.find(handle);
        if (it != g_overlays.end()) {
            it->second.texto = novoTexto;
            return jp_int(1);
        }
        return jp_int(0);
    }
}
#endif