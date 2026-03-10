// contexto.hpp
// Gerenciamento de janela GLFW + contexto OpenGL 4.6 + loop de render para sml3d

#pragma once

#include <string>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "mesh.hpp"
#include "camera.hpp"
#include "input.hpp"
#include "texture.hpp"
#include "model.hpp"
#include "collision.hpp"
#include "gravity.hpp"
#include "timer.hpp"
#include "animation.hpp"

// =============================================================================
// ESTRUTURAS
// =============================================================================

struct SML3DJanela {
    GLFWwindow* glfw_window = nullptr;
    int largura = 0;
    int altura = 0;
    std::string titulo;
    float fundo_r = 0.1f;
    float fundo_g = 0.1f;
    float fundo_b = 0.1f;
    bool ativa = false;
};

// =============================================================================
// ESTADO GLOBAL
// =============================================================================

static std::unordered_map<int, SML3DJanela> g_janelas;
static int g_prox_id = 1;
static bool g_glfw_init = false;

// =============================================================================
// CALLBACKS
// =============================================================================

static void sml3d_erro_glfw(int codigo, const char* descricao) {
    fprintf(stderr, "[sml3d] Erro GLFW (%d): %s\n", codigo, descricao);
}

static void sml3d_framebuffer_resize(GLFWwindow* window, int largura, int altura) {
    glViewport(0, 0, largura, altura);
    // Atualiza dimensoes na struct
    for (auto& [id, jan] : g_janelas) {
        if (jan.glfw_window == window) {
            jan.largura = largura;
            jan.altura = altura;
            break;
        }
    }
}

// =============================================================================
// INICIALIZAÇÃO GLFW + GLAD
// =============================================================================

static bool inicializarGLFW() {
    if (g_glfw_init) return true;

    glfwSetErrorCallback(sml3d_erro_glfw);

    if (!glfwInit()) {
        fprintf(stderr, "[sml3d] Falha ao inicializar GLFW\n");
        return false;
    }

    g_glfw_init = true;
    return true;
}

// =============================================================================
// CRIAÇÃO DE JANELA
// =============================================================================

static int criarJanela(const std::string& titulo, int largura, int altura) {
    if (!inicializarGLFW()) return 0;

    // Hints OpenGL 4.6 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* win = glfwCreateWindow(largura, altura, titulo.c_str(), nullptr, nullptr);
    if (!win) {
        fprintf(stderr, "[sml3d] Falha ao criar janela '%s'\n", titulo.c_str());
        return 0;
    }

    glfwMakeContextCurrent(win);

    // Carrega funções OpenGL via GLAD (apenas na primeira janela)
    static bool glad_carregado = false;
    if (!glad_carregado) {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            fprintf(stderr, "[sml3d] Falha ao carregar GLAD\n");
            glfwDestroyWindow(win);
            return 0;
        }
        glad_carregado = true;
        fprintf(stdout, "[sml3d] OpenGL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    }

    glfwSetFramebufferSizeCallback(win, sml3d_framebuffer_resize);
    glfwSwapInterval(1); // VSync

    glViewport(0, 0, largura, altura);
    glEnable(GL_DEPTH_TEST);

    int id = g_prox_id++;
    SML3DJanela& jan = g_janelas[id];
    jan.glfw_window = win;
    jan.largura = largura;
    jan.altura = altura;
    jan.titulo = titulo;
    jan.ativa = true;

    // Registra callbacks de input na janela
    inicializarInput(win);

    return id;
}

// =============================================================================
// COR DE FUNDO
// =============================================================================

static bool janelaFundo(int id, int r, int g, int b) {
    auto it = g_janelas.find(id);
    if (it == g_janelas.end()) return false;

    it->second.fundo_r = r / 255.0f;
    it->second.fundo_g = g / 255.0f;
    it->second.fundo_b = b / 255.0f;
    return true;
}

// =============================================================================
// EXIBIR (FRAME) — poll events, limpa tela, render meshes, swap
// =============================================================================

static int exibirJanela(int id) {
    auto it = g_janelas.find(id);
    if (it == g_janelas.end()) return 0;

    SML3DJanela& jan = it->second;

    if (glfwWindowShouldClose(jan.glfw_window)) {
        // Limpa recursos da janela
        destruirAnimacoesJanela(id);
        destruirMeshesJanela(id);
        destruirCamerasJanela(id);
        destruirTodosShaders();
        destruirShaderSkinned();
        destruirInput();
        destruirPhysicsJanela(id);
        destruirCollisionPropsJanela(id);
        destruirTodasTexturas();
        destruirTodosTimers();

        jan.ativa = false;
        glfwDestroyWindow(jan.glfw_window);
        jan.glfw_window = nullptr;
        g_janelas.erase(it);

        // Se não sobrou nenhuma janela, termina tudo
        if (g_janelas.empty()) {
            glfwTerminate();
            g_glfw_init = false;
            exit(0); // Encerra a VM do JPLang
        }
        return 0; // Janela fechada
    }

    // Fase 1: limpar edges/scroll do frame anterior
    prepararInput();

    // Poll events: callbacks atualizam teclas, mouse, scroll
    glfwPollEvents();

    // Fase 2: calcular deltas do mouse (posição já atualizada pelo poll)
    finalizarInput();

    // Verificar toggle do mouse grab
    atualizarMouseGrab();

    glfwMakeContextCurrent(jan.glfw_window);

    // Marcar início do frame e aplicar VSync (precisa do contexto ativo)
    fpsLimiteInicioFrame();
    glClearColor(jan.fundo_r, jan.fundo_g, jan.fundo_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Câmera: usa a ativa da janela, ou fallback padrão
    float aspect = (jan.altura > 0) ? (float)jan.largura / (float)jan.altura : 1.0f;
    Mat4 view, projection;

    SML3DCamera* cam = obterCameraAtivaJanela(id);
    if (cam) {
        // Atualizar colisão de câmera antes de calcular a view
        auto itCamAtiva = g_camera_ativa_janela.find(id);
        if (itCamAtiva != g_camera_ativa_janela.end()) {
            atualizarCameraCollision(itCamAtiva->second);
        }
        view = cam->calcularView();
        projection = cam->calcularProjection(aspect);
    } else {
        projection = Mat4::perspectiva(45.0f * (float)M_PI / 180.0f, aspect, 0.1f, 1000.0f);
        view = Mat4::lookAt({0, 2, 5}, {0, 0, 0}, {0, 1, 0});
    }

    // Atualizar animações (bones) antes do render
    atualizarAnimacoes(g_delta_time);

    renderizarMeshes(id, view, projection);
    renderizarMeshesAnimadas(id, view, projection);

    glfwSwapBuffers(jan.glfw_window);

    // FPS: atualizar contagem e aplicar limite
    atualizarFPS(g_delta_time);
    fpsLimiteFinalFrame();

    return 1; // Janela ainda aberta
}

// =============================================================================
// CONSULTAS
// =============================================================================

static bool janelaAtiva(int id) {
    auto it = g_janelas.find(id);
    if (it == g_janelas.end()) return false;
    return it->second.ativa;
}

static int janelaLargura(int id) {
    auto it = g_janelas.find(id);
    if (it == g_janelas.end()) return 0;
    return it->second.largura;
}

static int janelaAltura(int id) {
    auto it = g_janelas.find(id);
    if (it == g_janelas.end()) return 0;
    return it->second.altura;
}