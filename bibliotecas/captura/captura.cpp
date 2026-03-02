/*  captura.cpp
 *  Biblioteca estática (obj) para JPLang – captura de tela.
 */
#include <windows.h>
#include <gdiplus.h>
#include <cstdint>
#include <cstdio>

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "user32.lib")

using namespace Gdiplus;

// ---------------------------------------------------------------------
//  Variáveis globais de inicialização do GDI+
static bool gdiInit  = false;
static ULONG_PTR gdiToken = 0;

// ---------------------------------------------------------------------
//  GDI+ helper – pega o CLSID do encoder PNG
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // número de encoders
    UINT  size = 0;         // tamanho do array em bytes

    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pCodecInfo = (ImageCodecInfo*)malloc(size);
    if (!pCodecInfo) return -1;

    GetImageEncoders(num, size, pCodecInfo);

    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(pCodecInfo[i].MimeType, format) == 0)
        {
            *pClsid = pCodecInfo[i].Clsid;
            free(pCodecInfo);
            return 0;            // sucesso
        }
    }

    free(pCodecInfo);
    return -1;                    // não encontrado
}

// ---------------------------------------------------------------------
//  Captura de tela – exposta para JPLang
extern "C" __stdcall void cap_capturar(const char* caminho,
                                       int x, int y,
                                       int largura, int altura)
{
    /* GDI+ inicializa apenas na primeira chamada */
    if (!gdiInit)
    {
        GdiplusStartupInput  input;
        GdiplusStartupOutput output;
        GdiplusStartup(&gdiToken, &input, &output);
        gdiInit = true;
    }

    /* Garantir que a captura considere DPI (caso o Windows esteja em “Scaling”) */
    SetProcessDPIAware();

    /* ---------- 1 – Preparar DCs ------------- */
    HDC hScreenDC = GetDC(nullptr);
    HDC hMemDC    = CreateCompatibleDC(hScreenDC);

    /* ---------- 2 – Bitmap compatível  -------- */
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, largura, altura);
    SelectObject(hMemDC, hBitmap);

    /* ---------- 3 – BitBlt (captura) --------- */
    BitBlt(hMemDC, 0, 0, largura, altura,
           hScreenDC, x, y, SRCCOPY | CAPTUREBLT);

    /* ---------- 4 – Wrap em GDI+ e salva PNG */
    Bitmap bitmap(hBitmap, nullptr);      // bitmap GDI+

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) == 0)
    {
        WCHAR wPath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, caminho, -1, wPath, MAX_PATH);
        bitmap.Save(wPath, &pngClsid, nullptr);
    }

    /* ---------- 5 – Limpeza --------------- */
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);
}
