// src/input.hpp
// Gerenciamento de input e movimento para r3dgame
// Movimento horizontal apenas (XZ) - Gravidade controla Y

#ifndef INPUT_HPP
#define INPUT_HPP

#include <windows.h>
#include <string>
#include <map>
#include <algorithm>

// ==========================================
// ESTRUTURAS
// ==========================================

struct PlayerInput {
    int playerId;
    
    // Teclas de movimento (int para suportar Virtual Keys)
    int teclaCima;     // Z+
    int teclaEsquerda; // X-
    int teclaBaixo;    // Z-
    int teclaDireita;  // X+
    
    // Velocidade (unidades por frame)
    float velX;
    float velZ;
    
    bool ativo;
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, PlayerInput*> g_playerInputs;

// ==========================================
// FUNÇÕES INTERNAS
// ==========================================

// Converte string para código de tecla virtual (VK)
static int input_parse_tecla(std::string tecla) {
    if (tecla.empty()) return 0;
    
    // Converter para maiúsculo
    std::transform(tecla.begin(), tecla.end(), tecla.begin(), ::toupper);
    
    // Teclas especiais
    if (tecla == "SPACE" || tecla == "ESPACO" || tecla == "ESPAÇO") return VK_SPACE;
    if (tecla == "SHIFT") return VK_SHIFT;
    if (tecla == "CTRL" || tecla == "CONTROL") return VK_CONTROL;
    if (tecla == "ENTER") return VK_RETURN;
    if (tecla == "ESC" || tecla == "ESCAPE") return VK_ESCAPE;
    
    // Caracteres simples (A-Z, 0-9)
    return (int)tecla[0];
}

static bool input_tecla_pressionada(int tecla) {
    if (tecla == 0) return false;
    return (GetAsyncKeyState(tecla) & 0x8000) != 0;
}

// ==========================================
// FUNÇÕES PÚBLICAS
// ==========================================

// Configura teclas de movimento para um player (4 teclas - XZ apenas)
static bool input_player_mover(int playerId, 
                               const std::string& cima, const std::string& esquerda, 
                               const std::string& baixo, const std::string& direita) {
    PlayerInput* input = nullptr;
    
    auto it = g_playerInputs.find(playerId);
    if (it != g_playerInputs.end()) {
        input = it->second;
    } else {
        input = new PlayerInput();
        input->playerId = playerId;
        input->velX = 0.1f; 
        input->velZ = 0.1f;
        input->ativo = true;
        g_playerInputs[playerId] = input;
    }
    
    input->teclaCima = input_parse_tecla(cima);
    input->teclaEsquerda = input_parse_tecla(esquerda);
    input->teclaBaixo = input_parse_tecla(baixo);
    input->teclaDireita = input_parse_tecla(direita);
    
    return true;
}

// Define velocidade de movimento do player (XZ)
static bool input_player_velocidade(int playerId, float velX, float velZ) {
    PlayerInput* input = nullptr;
    auto it = g_playerInputs.find(playerId);
    if (it != g_playerInputs.end()) {
        input = it->second;
    } else {
        // Cria se não existir
        input = new PlayerInput();
        input->playerId = playerId;
        input->ativo = true;
        input->teclaCima = 0; input->teclaBaixo = 0;
        input->teclaEsquerda = 0; input->teclaDireita = 0;
        g_playerInputs[playerId] = input;
    }
    
    input->velX = velX;
    input->velZ = velZ;
    return true;
}

// Processa input e retorna movimento (dx, dy, dz)
// dy sempre retorna 0 - gravidade controla Y
static void input_processar_player(int playerId, float* dx, float* dy, float* dz) {
    *dx = 0; *dy = 0; *dz = 0;
    
    auto it = g_playerInputs.find(playerId);
    if (it == g_playerInputs.end()) return;
    
    PlayerInput* input = it->second;
    if (!input->ativo) return;
    
    // Movimento Z (Frente/Trás)
    if (input_tecla_pressionada(input->teclaCima))  *dz += input->velZ;
    if (input_tecla_pressionada(input->teclaBaixo)) *dz -= input->velZ;
    
    // Movimento X (Esquerda/Direita)
    if (input_tecla_pressionada(input->teclaEsquerda)) *dx -= input->velX;
    if (input_tecla_pressionada(input->teclaDireita))  *dx += input->velX;
    
    // Y sempre 0 - gravidade/pulo controlam
    *dy = 0;
}

// Helper genérico para verificar tecla
static bool input_tecla(const std::string& tecla) {
    int vk = input_parse_tecla(tecla);
    return input_tecla_pressionada(vk);
}

// Limpa inputs
static void input_cleanup() {
    for (auto& [id, input] : g_playerInputs) delete input;
    g_playerInputs.clear();
}

static bool input_player_tem_input(int playerId) {
    return g_playerInputs.find(playerId) != g_playerInputs.end();
}

#endif