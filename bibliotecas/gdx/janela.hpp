// janela.hpp
// Classe Janela para GDX - Win32/X11 + OpenGL + ImGui (Multiplataforma)

#ifndef JANELA_HPP
#define JANELA_HPP

#include "janela_def.hpp"

// Forward declarations
class Botao;
class Etiqueta;

// =============================================================================
// CLASSE JANELA
// =============================================================================
class Janela {
public:
    bool aberta;
    int largura;
    int altura;
    std::vector<Botao*> botoes;
    std::vector<Etiqueta*> etiquetas;

    // Cor de fundo (RGB normalizado 0-1)
    float cor_r, cor_g, cor_b;
    bool cor_personalizada;

    // Imagem de fundo
    unsigned char* imagem_original;
    int img_largura_original;
    int img_altura_original;
    int img_canais;
    bool tem_imagem;
    GLuint textura_fundo;

    // ImGui
    ImGuiContext* imgui_ctx;
    bool imgui_inicializado;

#if JP_WINDOWS
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    static bool classe_registrada;
#else
    Display* display;
    Window window;
    GLXContext glx_context;
    Colormap colormap;
    Atom wmDeleteMessage;
    int screen;
#endif

    // =========================================================================
    // CONSTRUTOR / DESTRUTOR
    // =========================================================================
    Janela(std::string titulo, int w, int h) {
        largura = w;
        altura = h;
        aberta = false;
        
        cor_r = 1.0f;
        cor_g = 1.0f;
        cor_b = 1.0f;
        cor_personalizada = false;
        
        imagem_original = nullptr;
        img_largura_original = 0;
        img_altura_original = 0;
        img_canais = 0;
        tem_imagem = false;
        textura_fundo = 0;
        
        imgui_ctx = nullptr;
        imgui_inicializado = false;

#if JP_WINDOWS
        hwnd = nullptr;
        hdc = nullptr;
        hglrc = nullptr;
        criar_janela_windows(titulo, w, h);
#else
        display = nullptr;
        window = 0;
        glx_context = nullptr;
        colormap = 0;
        criar_janela_linux(titulo, w, h);
#endif

        if (aberta) {
            inicializar_imgui();
        }
    }

    ~Janela() {
        if (textura_fundo != 0) {
            glDeleteTextures(1, &textura_fundo);
        }
        if (imagem_original) {
            free(imagem_original);
        }
        
        finalizar_imgui();

#if JP_WINDOWS
        if (hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hglrc);
        }
        if (hwnd && hdc) {
            ReleaseDC(hwnd, hdc);
        }
        if (hwnd) {
            DestroyWindow(hwnd);
        }
#else
        if (glx_context) {
            glXMakeCurrent(display, None, nullptr);
            glXDestroyContext(display, glx_context);
        }
        if (window) {
            XDestroyWindow(display, window);
        }
        if (colormap) {
            XFreeColormap(display, colormap);
        }
        if (display) {
            XCloseDisplay(display);
        }
#endif
    }

    // =========================================================================
    // CRIAÇÃO DA JANELA - WINDOWS
    // =========================================================================
#if JP_WINDOWS
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Janela* janela = (Janela*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        
        if (janela && janela->imgui_inicializado) {
            ImGuiIO& io = ImGui::GetIO();
            
            switch (msg) {
                case WM_MOUSEMOVE:
                    io.MousePos = ImVec2((float)LOWORD(lParam), (float)HIWORD(lParam));
                    break;
                case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
                    io.MouseDown[0] = true;
                    SetCapture(hwnd);
                    break;
                case WM_LBUTTONUP:
                    io.MouseDown[0] = false;
                    ReleaseCapture();
                    break;
                case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
                    io.MouseDown[1] = true;
                    break;
                case WM_RBUTTONUP:
                    io.MouseDown[1] = false;
                    break;
                case WM_MOUSEWHEEL:
                    io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                    break;
                case WM_KEYDOWN:
                case WM_KEYUP: {
                    bool down = (msg == WM_KEYDOWN);
                    ImGuiKey key = ImGui_ImplWin32_VirtualKeyToImGuiKey((int)wParam);
                    if (key != ImGuiKey_None) {
                        io.AddKeyEvent(key, down);
                    }
                    io.AddKeyEvent(ImGuiMod_Ctrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
                    io.AddKeyEvent(ImGuiMod_Shift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                    io.AddKeyEvent(ImGuiMod_Alt, (GetKeyState(VK_MENU) & 0x8000) != 0);
                    break;
                }
                case WM_CHAR:
                    if (wParam > 0 && wParam < 0x10000)
                        io.AddInputCharacterUTF16((unsigned short)wParam);
                    break;
            }
        }
        
        switch (msg) {
            case WM_CLOSE:
                if (janela) janela->aberta = false;
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                exit(0);
                return 0;
            case WM_SIZE:
                if (janela) {
                    janela->largura = LOWORD(lParam);
                    janela->altura = HIWORD(lParam);
                    glViewport(0, 0, janela->largura, janela->altura);
                }
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void criar_janela_windows(const std::string& titulo, int w, int h) {
        if (!classe_registrada) {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            wc.lpfnWndProc = WndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.lpszClassName = L"GDX_ImGui_Window";
            RegisterClassExW(&wc);
            classe_registrada = true;
        }

        std::wstring wtitulo = utf8_to_utf16(titulo);
        hwnd = CreateWindowExW(
            0, L"GDX_ImGui_Window", wtitulo.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, w, h,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );

        if (!hwnd) return;

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
        
        hdc = GetDC(hwnd);
        
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        
        int pf = ChoosePixelFormat(hdc, &pfd);
        SetPixelFormat(hdc, pf, &pfd);
        
        hglrc = wglCreateContext(hdc);
        wglMakeCurrent(hdc, hglrc);
        
        carregar_extensoes_opengl();
        if (wglSwapIntervalEXT) {
            wglSwapIntervalEXT(1);
        }

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        
        aberta = true;
    }
#endif

    // =========================================================================
    // CRIAÇÃO DA JANELA - LINUX
    // =========================================================================
#if !JP_WINDOWS
    void criar_janela_linux(const std::string& titulo, int w, int h) {
        display = XOpenDisplay(nullptr);
        if (!display) return;
        
        screen = DefaultScreen(display);
        
        static int visual_attribs[] = {
            GLX_RGBA,
            GLX_DEPTH_SIZE, 24,
            GLX_DOUBLEBUFFER,
            None
        };
        
        XVisualInfo* vi = glXChooseVisual(display, screen, visual_attribs);
        if (!vi) {
            XCloseDisplay(display);
            display = nullptr;
            return;
        }
        
        colormap = XCreateColormap(display, RootWindow(display, screen), vi->visual, AllocNone);
        
        XSetWindowAttributes swa = {};
        swa.colormap = colormap;
        swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                         ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                         StructureNotifyMask;
        
        window = XCreateWindow(
            display, RootWindow(display, screen),
            0, 0, w, h, 0,
            vi->depth, InputOutput, vi->visual,
            CWColormap | CWEventMask, &swa
        );
        
        // Título UTF-8
        Atom netWmName = XInternAtom(display, "_NET_WM_NAME", False);
        Atom utf8String = XInternAtom(display, "UTF8_STRING", False);
        if (netWmName != None && utf8String != None) {
            XChangeProperty(display, window, netWmName, utf8String, 8, PropModeReplace, 
                           (unsigned char*)titulo.c_str(), titulo.size());
        }
        XStoreName(display, window, titulo.c_str());
        
        wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wmDeleteMessage, 1);
        
        glx_context = glXCreateContext(display, vi, nullptr, GL_TRUE);
        glXMakeCurrent(display, window, glx_context);
        
        XFree(vi);
        XMapWindow(display, window);
        
        aberta = true;
    }
#endif

    // =========================================================================
    // IMGUI
    // =========================================================================
    void inicializar_imgui() {
        IMGUI_CHECKVERSION();
        imgui_ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(imgui_ctx);
        
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.DisplaySize = ImVec2((float)largura, (float)altura);
        
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 4.0f;
        style.FramePadding = ImVec2(8, 4);
        
        carregar_fonte_sistema(24.0f);
        criar_textura_fonte_imgui();
        
        imgui_inicializado = true;
    }

    void finalizar_imgui() {
        if (imgui_ctx) {
            ImGui::SetCurrentContext(imgui_ctx);
            ImGuiIO& io = ImGui::GetIO();
            ImTextureID texId = io.Fonts->TexRef.GetTexID();
            if (texId != ImTextureID_Invalid) {
                GLuint tex = (GLuint)(uintptr_t)texId;
                glDeleteTextures(1, &tex);
            }
            ImGui::DestroyContext(imgui_ctx);
            imgui_ctx = nullptr;
        }
        imgui_inicializado = false;
    }

    // =========================================================================
    // IMAGEM DE FUNDO
    // =========================================================================
    bool definir_imagem(const std::string& caminho);

    // =========================================================================
    // COR DE FUNDO
    // =========================================================================
    void definir_cor(int r, int g, int b) {
        cor_r = r / 255.0f;
        cor_g = g / 255.0f;
        cor_b = b / 255.0f;
        cor_personalizada = true;
    }

    // =========================================================================
    // BOTÕES E ETIQUETAS
    // =========================================================================
    void adicionar_botao(Botao* btn) {
        botoes.push_back(btn);
    }

    void desenhar_botao(Botao* btn) {
        btn->desenhar_imgui(&botoes);
    }

    void adicionar_etiqueta(Etiqueta* lbl) {
        etiquetas.push_back(lbl);
    }

    void atualizar_etiqueta(Etiqueta* lbl) {
        // Com ImGui não precisa fazer nada especial
    }

    void desenhar_etiqueta(Etiqueta* lbl);

    // =========================================================================
    // RENDERIZAÇÃO
    // =========================================================================
    void desenhar_imagem_fundo() {
        if (!tem_imagem || textura_fundo == 0) return;

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddImage(
            (ImTextureID)(intptr_t)textura_fundo,
            ImVec2(0, 0),
            ImVec2((float)largura, (float)altura)
        );
    }

    bool processar_eventos() {
        if (!aberta) return false;

#if JP_WINDOWS
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                aberta = false;
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        
        if (!aberta) return false;
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        largura = rect.right - rect.left;
        altura = rect.bottom - rect.top;
#else
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            
            ImGuiIO& io = ImGui::GetIO();
            
            switch (event.type) {
                case ClientMessage:
                    if ((Atom)event.xclient.data.l[0] == wmDeleteMessage) {
                        aberta = false;
                        exit(0);
                        return false;
                    }
                    break;
                case ConfigureNotify:
                    largura = event.xconfigure.width;
                    altura = event.xconfigure.height;
                    glViewport(0, 0, largura, altura);
                    break;
                case MotionNotify:
                    io.MousePos = ImVec2((float)event.xmotion.x, (float)event.xmotion.y);
                    break;
                case ButtonPress:
                    if (event.xbutton.button == 1) io.MouseDown[0] = true;
                    if (event.xbutton.button == 3) io.MouseDown[1] = true;
                    if (event.xbutton.button == 4) io.MouseWheel += 1.0f;
                    if (event.xbutton.button == 5) io.MouseWheel -= 1.0f;
                    break;
                case ButtonRelease:
                    if (event.xbutton.button == 1) io.MouseDown[0] = false;
                    if (event.xbutton.button == 3) io.MouseDown[1] = false;
                    break;
            }
        }
        
        if (!aberta) return false;
#endif

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)largura, (float)altura);
        io.DeltaTime = 1.0f / 60.0f;
        
        ImGui::NewFrame();

        if (tem_imagem) {
            desenhar_imagem_fundo();
        }

        // Desenha botões por camada (0, 1, 2) - camada 0 primeiro (fundo)
        for (int c = 0; c <= 2; c++) {
            for (Botao* btn : botoes) {
                if (btn->camada == c) {
                    desenhar_botao(btn);
                }
            }
        }
        
        for (Etiqueta* lbl : etiquetas) {
            desenhar_etiqueta(lbl);
        }

        ImGui::Render();
        
        glClearColor(cor_r, cor_g, cor_b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        renderizar_imgui_opengl(largura, altura);

#if JP_WINDOWS
        SwapBuffers(hdc);
#else
        glXSwapBuffers(display, window);
#endif

        return true;
    }
};

#if JP_WINDOWS
bool Janela::classe_registrada = false;
#endif

#endif