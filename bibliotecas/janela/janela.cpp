/*****************************************************
 *  janela.cpp – Biblioteca “janela” para JPLang
 *
 *  Exporta:
 *      jn_criar          (const char* titulo, int larg, int alt)
 *      jn_exibir
 *      jn_botao          (const char* texto, int x, int y)
 *      jn_botao_clicado
 *      jn_destroy_vm
 *
 *  Compilação (MinGW‑g++):
 *      g++ -c -O3 -DUNICODE -D_UNICODE \
 *          -o bibliotecas/janela/janela.obj \
 *          bibliotecas/janela/janela.cpp
 *****************************************************/

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

extern "C" {

    /* ---------- Dados estáticos ---------- */
    static HINSTANCE hInst      = nullptr;   // instância do módulo
    static HWND     hWnd       = nullptr;   // janela principal
    static HWND     hWndButton = nullptr;   // botão (só 1)
    static bool     buttonClicked = false;  // flag de clique

    /* ID interno do botão (necessário para o WM_COMMAND) */
    static const int  BUTTON_ID = 101;

    /* Classe de janela (Unicode) */
    static const wchar_t WINDOW_CLASS[] = L"JN_WINDOW_CLASS";

    /* ---------- Converte UTF‑8 → UTF‑16 ---------- */
    static std::wstring utf8_to_wide(const char* utf8)
    {
        if (!utf8) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        if (len == 0) return L"";
        std::vector<wchar_t> buf(len);
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf.data(), len);
        return std::wstring(buf.data());
    }

    /* ---------- Função interna: encerra a VM ---------- */
    int64_t jn_destroy_vm()
    {
        ExitProcess(0);               // encerra todo o processo
        return 0;                     // nunca chega aqui
    }

    /* ---------- Window Procedure ---------- */
    LRESULT CALLBACK JNWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg) {
            case WM_DESTROY:                     // janela fechada
                PostQuitMessage(0);              // envia WM_QUIT
                jn_destroy_vm();                 // encerra a VM imediatamente
                return 0;

            case WM_COMMAND:                     // botões, menus, etc.
                if (HIWORD(wParam) == BN_CLICKED &&
                    LOWORD(wParam) == BUTTON_ID) {
                    buttonClicked = true;
                    return 0;
                }
                break;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    /* ---------- Cria a janela principal ---------- */
    int64_t jn_criar(const char* titulo, int larg, int alt)
    {
        if (!hInst)
            hInst = GetModuleHandle(nullptr);

        if (!hWnd) {  // registrar classe (somente na primeira chamada)
            WNDCLASSEXW wc{};
            wc.cbSize        = sizeof(WNDCLASSEXW);
            wc.style         = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = JNWndProc;
            wc.cbClsExtra    = 0;
            wc.cbWndExtra    = 0;
            wc.hInstance     = hInst;
            wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
            wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
            wc.lpszMenuName  = nullptr;
            wc.lpszClassName = WINDOW_CLASS;
            wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

            if (!RegisterClassExW(&wc))
                return -1;   // erro de registro
        }

        std::wstring wTitle = utf8_to_wide(titulo);
        if (wTitle.empty())
            return -1;   // falha de conversão

        hWnd = CreateWindowExW(
                    0,                     // estilos estendidos
                    WINDOW_CLASS,          // classe
                    wTitle.c_str(),        // título
                    WS_OVERLAPPEDWINDOW,   // estilo da janela
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    larg, alt,
                    nullptr, nullptr, hInst, nullptr);

        if (!hWnd)
            return -1;   // falha na criação

        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        return 0;   // OK
    }

    /* ---------- Loop de mensagens ---------- */
    int64_t jn_exibir()
    {
        if (!hWnd)
            return -1;   // janela não criada

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)      // janela fechada
                return 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return 0;   // janela continua aberta
    }

    /* ---------- Cria um botão dentro da janela ---------- */
    int64_t jn_botao(const char* texto, int x, int y)
    {
        if (!hWnd)
            return -1;   // janela não existe

        std::wstring wTxt = utf8_to_wide(texto);
        if (wTxt.empty())
            return -1;   // falha de conversão

        const int btnW = 100;
        const int btnH = 30;

        hWndButton = CreateWindowExW(
                          WS_EX_CLIENTEDGE,
                          L"BUTTON",              // classe padrão do botão
                          wTxt.c_str(),
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          x, y, btnW, btnH,
                          hWnd,                   // pai
                          (HMENU)(DWORD_PTR)BUTTON_ID, // ID
                          hInst,
                          nullptr);

        if (!hWndButton)
            return -1;   // falha na criação

        return 0;   // OK
    }

    /* ---------- Botão clicado? ---------- */
    int64_t jn_botao_clicado()
    {
        if (buttonClicked) {
            buttonClicked = false;   // limpa a flag
            return 1;                // foi clicado
        }
        return 0;                    // não foi clicado
    }
}   // extern "C"
