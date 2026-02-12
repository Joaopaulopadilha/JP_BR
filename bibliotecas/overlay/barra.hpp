// barra.hpp
#ifndef BARRA_HPP
#define BARRA_HPP

static void renderizar_barra(OverlayInfo& info) {
    if (!info.ativo || !info.hwnd) return;
    
    int largura = info.largura;
    int altura = info.altura;
    int borda = 2;
    int bmpLargura = largura + borda * 2;
    int bmpAltura = altura + borda * 2;
    
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
    
    Gdiplus::Pen penBorda(Gdiplus::Color(200, 80, 80, 80), 1.0f);
    graphics.DrawRectangle(&penBorda, borda - 1, borda - 1, largura + 1, altura + 1);
    
    Gdiplus::SolidBrush brushFundo(Gdiplus::Color(150, 0, 0, 0));
    graphics.FillRectangle(&brushFundo, borda, borda, largura, altura);
    
    float range = (float)(info.valor_max - info.valor_min);
    float porcentagem = 0.0f;
    if (range > 0) {
        porcentagem = (float)(info.valor_atual - info.valor_min) / range;
        if (porcentagem < 0) porcentagem = 0;
        if (porcentagem > 1) porcentagem = 1;
    }
    
    Gdiplus::SolidBrush brushBarra(Gdiplus::Color(255, info.r, info.g, info.b));
    
    if (info.orientacao == 'v') {
        int preenchido = (int)(altura * porcentagem);
        int y_inicio = borda + (altura - preenchido);
        graphics.FillRectangle(&brushBarra, borda, y_inicio, largura, preenchido);
    } else {
        int preenchido = (int)(largura * porcentagem);
        graphics.FillRectangle(&brushBarra, borda, borda, preenchido, altura);
    }
    
    POINT ptSrc = {0, 0};
    POINT ptDst = {info.x - borda, info.y - borda};
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
    // overlay.barra(x, y, largura, altura, v_min, v_max, orientacao, r, g, b)
    JP_EXPORT JPValor jp_ov_barra(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 10) return jp_int(0);
        
        iniciar_gdiplus();
        registrar_classe();
        
        int x = (int)jp_get_int(args[0]);
        int y = (int)jp_get_int(args[1]);
        int largura = (int)jp_get_int(args[2]);
        int altura = (int)jp_get_int(args[3]);
        int valor_min = (int)jp_get_int(args[4]);
        int valor_max = (int)jp_get_int(args[5]);
        std::string orientacao_str = jp_get_string(args[6]);
        int r = (int)jp_get_int(args[7]);
        int g = (int)jp_get_int(args[8]);
        int b = (int)jp_get_int(args[9]);
        
        char orientacao = 'h';
        if (!orientacao_str.empty() && (orientacao_str[0] == 'v' || orientacao_str[0] == 'V')) {
            orientacao = 'v';
        }
        
        int borda = 2;
        HWND hwnd = criar_janela_overlay(x - borda, y - borda, largura + borda * 2, altura + borda * 2);
        if (!hwnd) return jp_int(0);
        
        int handle = g_nextHandle++;
        OverlayInfo info;
        info.hwnd = hwnd;
        info.tipo = TipoOverlay::BARRA;
        info.x = x; info.y = y;
        info.largura = largura; info.altura = altura;
        info.valor_min = valor_min; info.valor_max = valor_max;
        info.valor_atual = valor_min;
        info.orientacao = orientacao;
        info.r = r; info.g = g; info.b = b;
        info.ativo = true;
        info.texto = ""; info.tamanho = 0; info.espessura = 0;
        
        g_overlays[handle] = info;
        return jp_int(handle);
    }

    // overlay.valor(handle, valor)
    JP_EXPORT JPValor jp_ov_valor(JPValor* args, int numArgs) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (numArgs < 2) return jp_int(0);

        int handle = (int)jp_get_int(args[0]);
        int valor = (int)jp_get_int(args[1]);
        
        auto it = g_overlays.find(handle);
        if (it != g_overlays.end()) {
            it->second.valor_atual = valor;
            return jp_int(1);
        }
        return jp_int(0);
    }
}
#endif