// timer.hpp
// Sistema de tempo (FPS, cronômetros, timer global, wait) para engine sml3d

#pragma once

#include <chrono>
#include <thread>
#include <unordered_map>
#include <cmath>

// =============================================================================
// TIPOS
// =============================================================================

using SML3DClock = std::chrono::high_resolution_clock;
using SML3DTimePoint = SML3DClock::time_point;

// =============================================================================
// ESTADO GLOBAL DE TEMPO
// =============================================================================

static SML3DTimePoint g_timer_inicio;
static bool g_timer_iniciado = false;

// FPS
static float g_fps_atual = 0.0f;
static float g_fps_acumulador = 0.0f;
static int   g_fps_contagem = 0;
static float g_fps_ultimo_update = 0.0f;

// FPS limit
static int g_fps_limite = 0;  // 0 = sem limite (usa VSync)
static SML3DTimePoint g_frame_inicio;

// =============================================================================
// CRONÔMETROS INDEPENDENTES
// =============================================================================

struct SML3DTimer {
    SML3DTimePoint inicio;
    bool pausado = false;
    float tempo_pausado = 0.0f;  // Acumulado antes da pausa
    SML3DTimePoint momento_pausa;
};

static std::unordered_map<int, SML3DTimer> g_timers;
static int g_prox_timer_id = 1;

// =============================================================================
// INICIALIZAÇÃO — chamada automática na primeira consulta
// =============================================================================

static void inicializarTimer() {
    if (g_timer_iniciado) return;
    g_timer_inicio = SML3DClock::now();
    g_frame_inicio = g_timer_inicio;
    g_timer_iniciado = true;
}

// =============================================================================
// TIMER GLOBAL — tempo desde o início em segundos
// =============================================================================

static double obterTempoTotal() {
    inicializarTimer();
    std::chrono::duration<double> diff = SML3DClock::now() - g_timer_inicio;
    return diff.count();
}

// =============================================================================
// FPS — calcular a cada frame
// =============================================================================

static void atualizarFPS(float deltaTime) {
    inicializarTimer();

    g_fps_contagem++;
    g_fps_acumulador += deltaTime;

    // Atualizar FPS a cada 0.5 segundo (evita flutuação)
    if (g_fps_acumulador >= 0.5f) {
        g_fps_atual = (float)g_fps_contagem / g_fps_acumulador;
        g_fps_contagem = 0;
        g_fps_acumulador = 0.0f;
    }
}

static float obterFPS() {
    return g_fps_atual;
}

// =============================================================================
// FPS LIMIT — limitar framerate máximo
// =============================================================================

static bool g_vsync_dirty = false;  // Precisa atualizar VSync no próximo frame

static void definirFPSLimite(int limite) {
    g_fps_limite = limite;
    g_vsync_dirty = true;
}

// Chamar no INÍCIO do frame, com contexto OpenGL ativo
static void fpsLimiteInicioFrame() {
    if (g_vsync_dirty) {
        if (g_fps_limite > 0) {
            glfwSwapInterval(0);  // VSync off — FPS controlado pelo timer
        } else {
            glfwSwapInterval(1);  // VSync on — FPS controlado pelo monitor
        }
        g_vsync_dirty = false;
    }
    g_frame_inicio = SML3DClock::now();
}

// Chamar no FINAL do frame (depois do swap) — espera se necessário
static void fpsLimiteFinalFrame() {
    if (g_fps_limite <= 0) return;

    double tempo_alvo = 1.0 / (double)g_fps_limite;
    std::chrono::duration<double> elapsed = SML3DClock::now() - g_frame_inicio;
    double restante = tempo_alvo - elapsed.count();

    if (restante > 0.001) {
        // Sleep grosso pra a maior parte do tempo
        int sleep_ms = (int)(restante * 1000.0) - 1;
        if (sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
        // Busy-wait pro restante (precisão)
        while (true) {
            std::chrono::duration<double> now_elapsed = SML3DClock::now() - g_frame_inicio;
            if (now_elapsed.count() >= tempo_alvo) break;
        }
    }
}

// =============================================================================
// CRONÔMETROS INDEPENDENTES
// =============================================================================

// Criar cronômetro — começa contando imediatamente
static int criarCronometro() {
    inicializarTimer();
    int id = g_prox_timer_id++;
    SML3DTimer& t = g_timers[id];
    t.inicio = SML3DClock::now();
    t.pausado = false;
    t.tempo_pausado = 0.0f;
    return id;
}

// Tempo decorrido em segundos
static double cronometroElapsed(int id) {
    auto it = g_timers.find(id);
    if (it == g_timers.end()) return 0.0;

    SML3DTimer& t = it->second;
    if (t.pausado) {
        return (double)t.tempo_pausado;
    }

    std::chrono::duration<double> diff = SML3DClock::now() - t.inicio;
    return diff.count() + (double)t.tempo_pausado;
}

// Resetar cronômetro (volta a zero e continua contando)
static bool cronometroReset(int id) {
    auto it = g_timers.find(id);
    if (it == g_timers.end()) return false;

    it->second.inicio = SML3DClock::now();
    it->second.tempo_pausado = 0.0f;
    it->second.pausado = false;
    return true;
}

// Pausar cronômetro
static bool cronometroPausar(int id) {
    auto it = g_timers.find(id);
    if (it == g_timers.end()) return false;

    SML3DTimer& t = it->second;
    if (t.pausado) return true;  // Já pausado

    std::chrono::duration<double> diff = SML3DClock::now() - t.inicio;
    t.tempo_pausado += (float)diff.count();
    t.pausado = true;
    t.momento_pausa = SML3DClock::now();
    return true;
}

// Retomar cronômetro
static bool cronometroRetomar(int id) {
    auto it = g_timers.find(id);
    if (it == g_timers.end()) return false;

    SML3DTimer& t = it->second;
    if (!t.pausado) return true;  // Já rodando

    t.inicio = SML3DClock::now();
    t.pausado = false;
    return true;
}

// Destruir cronômetro
static void destruirCronometro(int id) {
    g_timers.erase(id);
}

// =============================================================================
// WAIT — pausar execução por X milissegundos
// =============================================================================

static void esperarMS(int ms) {
    if (ms <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirTodosTimers() {
    g_timers.clear();
    g_prox_timer_id = 1;
}