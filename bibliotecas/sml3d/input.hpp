// input.hpp
// Sistema de input (teclado + mouse) para engine sml3d — leitura manual de teclas, botões e movimento do mouse

#pragma once

#include <cstring>
#include <cstdio>
#include <cmath>
#include <unordered_map>

#include "GLFW/glfw3.h"

// =============================================================================
// ESTADO DAS TECLAS
// =============================================================================

// Estado por tecla: pressionada agora, pressionou neste frame, soltou neste frame
struct SML3DTeclaEstado {
    bool pressionada = false;   // Está segurando agora
    bool down        = false;   // Acabou de pressionar (edge)
    bool up          = false;   // Acabou de soltar (edge)
};

// =============================================================================
// ESTADO DO MOUSE
// =============================================================================

struct SML3DMouseEstado {
    double x  = 0.0, y  = 0.0;   // Posição atual
    double px = 0.0, py = 0.0;   // Posição do frame anterior
    double dx = 0.0, dy = 0.0;   // Delta do frame

    double scroll_dx = 0.0;      // Scroll horizontal acumulado no frame
    double scroll_dy = 0.0;      // Scroll vertical acumulado no frame

    bool botoes[3]      = {};    // 0=esquerdo, 1=direito, 2=meio
    bool botoes_down[3] = {};    // Edge: acabou de clicar
    bool botoes_up[3]   = {};    // Edge: acabou de soltar

    bool primeiro_movimento = true;  // Pra evitar pulo no dx/dy inicial
};

// =============================================================================
// ESTADO GLOBAL DE INPUT
// =============================================================================

static std::unordered_map<int, SML3DTeclaEstado> g_teclas;          // GLFW_KEY_* -> estado
static SML3DMouseEstado g_mouse;
static GLFWwindow* g_input_janela = nullptr;  // Janela que recebe input

// =============================================================================
// MAPEAMENTO DE STRING -> GLFW_KEY
// =============================================================================

static int mapearTecla(const char* nome) {
    if (!nome || nome[0] == '\0') return -1;

    // Letras: "a"-"z" e "A"-"Z"
    if (nome[1] == '\0') {
        char c = nome[0];
        if (c >= 'a' && c <= 'z') return GLFW_KEY_A + (c - 'a');
        if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
    }

    // Teclas especiais (comparação case-insensitive simples)
    // Cria cópia lowercase pra comparar
    char lower[32] = {};
    for (int i = 0; i < 31 && nome[i]; i++) {
        char c = nome[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
    }

    if (strcmp(lower, "space")     == 0 || strcmp(lower, "espaco") == 0) return GLFW_KEY_SPACE;
    if (strcmp(lower, "enter")     == 0) return GLFW_KEY_ENTER;
    if (strcmp(lower, "escape")    == 0 || strcmp(lower, "esc") == 0) return GLFW_KEY_ESCAPE;
    if (strcmp(lower, "tab")       == 0) return GLFW_KEY_TAB;
    if (strcmp(lower, "backspace") == 0) return GLFW_KEY_BACKSPACE;
    if (strcmp(lower, "delete")    == 0) return GLFW_KEY_DELETE;
    if (strcmp(lower, "insert")    == 0) return GLFW_KEY_INSERT;

    // Setas
    if (strcmp(lower, "up")    == 0 || strcmp(lower, "cima")     == 0) return GLFW_KEY_UP;
    if (strcmp(lower, "down")  == 0 || strcmp(lower, "baixo")    == 0) return GLFW_KEY_DOWN;
    if (strcmp(lower, "left")  == 0 || strcmp(lower, "esquerda") == 0) return GLFW_KEY_LEFT;
    if (strcmp(lower, "right") == 0 || strcmp(lower, "direita")  == 0) return GLFW_KEY_RIGHT;

    // Modificadores
    if (strcmp(lower, "shift")    == 0 || strcmp(lower, "lshift") == 0) return GLFW_KEY_LEFT_SHIFT;
    if (strcmp(lower, "rshift")   == 0) return GLFW_KEY_RIGHT_SHIFT;
    if (strcmp(lower, "ctrl")     == 0 || strcmp(lower, "lctrl")  == 0) return GLFW_KEY_LEFT_CONTROL;
    if (strcmp(lower, "rctrl")    == 0) return GLFW_KEY_RIGHT_CONTROL;
    if (strcmp(lower, "alt")      == 0 || strcmp(lower, "lalt")   == 0) return GLFW_KEY_LEFT_ALT;
    if (strcmp(lower, "ralt")     == 0) return GLFW_KEY_RIGHT_ALT;

    // F1-F12
    if (lower[0] == 'f' && lower[1] >= '1' && lower[1] <= '9') {
        int num = 0;
        for (int i = 1; lower[i]; i++) num = num * 10 + (lower[i] - '0');
        if (num >= 1 && num <= 12) return GLFW_KEY_F1 + (num - 1);
    }

    return -1;  // Tecla desconhecida
}

// =============================================================================
// CALLBACKS GLFW
// =============================================================================

static void sml3d_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window; (void)scancode; (void)mods;

    if (key < 0) return;

    SML3DTeclaEstado& estado = g_teclas[key];

    if (action == GLFW_PRESS) {
        estado.pressionada = true;
        estado.down = true;
    } else if (action == GLFW_RELEASE) {
        estado.pressionada = false;
        estado.up = true;
    }
    // GLFW_REPEAT: mantém pressionada=true, não seta down de novo
}

static void sml3d_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window; (void)mods;

    if (button < 0 || button > 2) return;

    if (action == GLFW_PRESS) {
        g_mouse.botoes[button] = true;
        g_mouse.botoes_down[button] = true;
    } else if (action == GLFW_RELEASE) {
        g_mouse.botoes[button] = false;
        g_mouse.botoes_up[button] = true;
    }
}

static void sml3d_cursor_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;

    g_mouse.x = xpos;
    g_mouse.y = ypos;

    if (g_mouse.primeiro_movimento) {
        g_mouse.px = xpos;
        g_mouse.py = ypos;
        g_mouse.primeiro_movimento = false;
    }
}

static void sml3d_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;

    g_mouse.scroll_dx += xoffset;
    g_mouse.scroll_dy += yoffset;
}

// =============================================================================
// INICIALIZAÇÃO — chamar após criar a janela GLFW
// =============================================================================

static void inicializarInput(GLFWwindow* window) {
    g_input_janela = window;

    glfwSetKeyCallback(window, sml3d_key_callback);
    glfwSetMouseButtonCallback(window, sml3d_mouse_button_callback);
    glfwSetCursorPosCallback(window, sml3d_cursor_callback);
    glfwSetScrollCallback(window, sml3d_scroll_callback);

    // Posição inicial do mouse
    glfwGetCursorPos(window, &g_mouse.x, &g_mouse.y);
    g_mouse.px = g_mouse.x;
    g_mouse.py = g_mouse.y;
    g_mouse.primeiro_movimento = false;
}

// =============================================================================
// ATUALIZAR INPUT — duas fases por frame
// =============================================================================

// Fase 1: limpar edges e scroll do frame anterior — chamar ANTES do glfwPollEvents
static void prepararInput() {
    // Limpa edges do frame anterior (down/up são por frame)
    for (auto& [key, estado] : g_teclas) {
        estado.down = false;
        estado.up = false;
    }

    for (int i = 0; i < 3; i++) {
        g_mouse.botoes_down[i] = false;
        g_mouse.botoes_up[i] = false;
    }

    // Scroll é acumulado nos callbacks, resetado por frame
    g_mouse.scroll_dx = 0.0;
    g_mouse.scroll_dy = 0.0;
}

// Fase 2: calcular deltas do mouse — chamar DEPOIS do glfwPollEvents
static void finalizarInput() {
    g_mouse.dx = g_mouse.x - g_mouse.px;
    g_mouse.dy = g_mouse.y - g_mouse.py;
    g_mouse.px = g_mouse.x;
    g_mouse.py = g_mouse.y;
}

// =============================================================================
// CONSULTAS — TECLADO
// =============================================================================

// Tecla está pressionada agora?
static bool inputTeclaPressed(const char* nome) {
    int key = mapearTecla(nome);
    if (key < 0) return false;

    auto it = g_teclas.find(key);
    if (it == g_teclas.end()) return false;
    return it->second.pressionada;
}

// Tecla acabou de ser pressionada neste frame?
static bool inputTeclaDown(const char* nome) {
    int key = mapearTecla(nome);
    if (key < 0) return false;

    auto it = g_teclas.find(key);
    if (it == g_teclas.end()) return false;
    return it->second.down;
}

// Tecla acabou de ser solta neste frame?
static bool inputTeclaUp(const char* nome) {
    int key = mapearTecla(nome);
    if (key < 0) return false;

    auto it = g_teclas.find(key);
    if (it == g_teclas.end()) return false;
    return it->second.up;
}

// =============================================================================
// CONSULTAS — MOUSE
// =============================================================================

static double inputMouseX()       { return g_mouse.x; }
static double inputMouseY()       { return g_mouse.y; }
static double inputMouseDX()      { return g_mouse.dx; }
static double inputMouseDY()      { return g_mouse.dy; }
static double inputMouseScrollX() { return g_mouse.scroll_dx; }
static double inputMouseScrollY() { return g_mouse.scroll_dy; }

// Botão do mouse: 0=esquerdo, 1=direito, 2=meio
static bool inputMouseBotao(int botao) {
    if (botao < 0 || botao > 2) return false;
    return g_mouse.botoes[botao];
}

static bool inputMouseBotaoDown(int botao) {
    if (botao < 0 || botao > 2) return false;
    return g_mouse.botoes_down[botao];
}

static bool inputMouseBotaoUp(int botao) {
    if (botao < 0 || botao > 2) return false;
    return g_mouse.botoes_up[botao];
}

// =============================================================================
// CONTROLE DO CURSOR
// =============================================================================

static void inputMouseLock(bool lock) {
    if (!g_input_janela) return;

    if (lock) {
        glfwSetInputMode(g_input_janela, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(g_input_janela, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Reset pra evitar pulo no delta ao mudar modo
    g_mouse.primeiro_movimento = true;
}

// =============================================================================
// MOUSE GRAB — captura do mouse pra jogos
// =============================================================================

static bool g_mouse_grabbed = false;
static int  g_mouse_grab_toggle_key = -1;  // GLFW_KEY pra alternar grab
static bool g_mouse_grab_clique_captura = true;  // Clique na janela recaptura

static void inputMouseGrab(bool grab) {
    if (!g_input_janela) return;

    g_mouse_grabbed = grab;

    if (grab) {
        glfwSetInputMode(g_input_janela, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        // Ativar raw mouse motion se disponível (mais preciso pra jogos)
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(g_input_janela, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else {
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(g_input_janela, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
        glfwSetInputMode(g_input_janela, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Reset pra evitar pulo no delta
    g_mouse.primeiro_movimento = true;
}

static void inputMouseGrabToggleKey(const char* tecla) {
    g_mouse_grab_toggle_key = mapearTecla(tecla);
}

static bool inputMouseGrabbed() {
    return g_mouse_grabbed;
}

// Chamar por frame — verifica tecla de toggle e clique pra recapturar
static void atualizarMouseGrab() {
    if (g_mouse_grab_toggle_key < 0) return;

    // Tecla de toggle: se apertou, libera o mouse
    auto it = g_teclas.find(g_mouse_grab_toggle_key);
    if (it != g_teclas.end() && it->second.down) {
        if (g_mouse_grabbed) {
            inputMouseGrab(false);
            return;
        }
    }

    // Se mouse não está preso e clicou na janela, recaptura
    if (!g_mouse_grabbed && g_mouse_grab_clique_captura) {
        if (g_mouse.botoes_down[0]) {  // Clique esquerdo
            inputMouseGrab(true);
        }
    }
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirInput() {
    g_teclas.clear();
    g_mouse = SML3DMouseEstado{};
    g_input_janela = nullptr;
}