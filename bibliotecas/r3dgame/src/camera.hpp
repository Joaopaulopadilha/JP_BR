// src/camera.hpp
// Sistema de Câmera (Fixa, Orbital, Player) para r3dgame

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <windows.h>
#include <cmath>
#include <map>
#include "matematica.hpp"
#include "player.hpp"

// ==========================================
// ENUMS E ESTRUTURAS
// ==========================================

enum CameraModo {
    CAM_FIXA = 0,
    CAM_ORBITAL = 1,
    CAM_PLAYER = 2
};

struct Camera {
    int jogoId;
    CameraModo modo;
    
    // Posição e Alvo (Usado na Fixa)
    float x, y, z;
    float targetX, targetY, targetZ;
    
    // Controle Orbital / Player
    float distancia;    // Distância do alvo
    float yaw;          // Rotação horizontal (radianos)
    float pitch;        // Rotação vertical (radianos)
    float sensibilidade;
    
    // Player seguido
    int targetPlayerId;
    float offsetY;      // Altura da câmera em relação ao pé do player
    
    // Controle do Mouse
    POINT lastMousePos;
    bool mouseAtivo;
};

// ==========================================
// GLOBAIS
// ==========================================

static std::map<int, Camera*> g_cameras;

// ==========================================
// FUNÇÕES INTERNAS (MOUSE)
// ==========================================

static void camera_mouse_delta(Camera* cam, float* dx, float* dy) {
    POINT p;
    GetCursorPos(&p);
    
    // Se é a primeira vez ou mouse estava inativo
    if (!cam->mouseAtivo) {
        cam->lastMousePos = p;
        cam->mouseAtivo = true;
        *dx = 0; *dy = 0;
        return;
    }
    
    // Calcula diferença
    *dx = (float)(p.x - cam->lastMousePos.x);
    *dy = (float)(p.y - cam->lastMousePos.y);
    
    // Centraliza o mouse na tela para não bater na borda (Travar mouse)
    // Para simplificar neste momento, apenas atualizamos a posição
    // Num jogo real FPS, usaríamos SetCursorPos aqui.
    cam->lastMousePos = p; 
}

// ==========================================
// FUNÇÕES PÚBLICAS
// ==========================================

// Inicializa/Reseta câmera para um jogo
static Camera* camera_obter(int jogoId) {
    auto it = g_cameras.find(jogoId);
    if (it != g_cameras.end()) return it->second;
    
    Camera* cam = new Camera();
    cam->jogoId = jogoId;
    cam->modo = CAM_FIXA;
    cam->x = 0; cam->y = 5; cam->z = -10;
    cam->targetX = 0; cam->targetY = 0; cam->targetZ = 0;
    cam->yaw = 0; cam->pitch = 0.5f; // Leve inclinação para baixo
    cam->distancia = 10.0f;
    cam->sensibilidade = 0.005f; // Sensibilidade padrão
    cam->targetPlayerId = -1;
    cam->offsetY = 2.0f;
    cam->mouseAtivo = false;
    
    g_cameras[jogoId] = cam;
    return cam;
}

static void camera_fixa(int jogoId, float x, float y, float z, float tx, float ty, float tz) {
    Camera* cam = camera_obter(jogoId);
    cam->modo = CAM_FIXA;
    cam->x = x; cam->y = y; cam->z = z;
    cam->targetX = tx; cam->targetY = ty; cam->targetZ = tz;
}

static void camera_orbital(int jogoId, float tx, float ty, float tz, float dist, float sensibilidade) {
    Camera* cam = camera_obter(jogoId);
    cam->modo = CAM_ORBITAL;
    cam->targetX = tx; cam->targetY = ty; cam->targetZ = tz;
    cam->distancia = dist;
    cam->sensibilidade = sensibilidade;
    // Reseta ângulos iniciais
    cam->yaw = 0; 
    cam->pitch = 0.5f; 
}

static void camera_player(int jogoId, int playerId, float dist, float altura, float sensibilidade) {
    Camera* cam = camera_obter(jogoId);
    cam->modo = CAM_PLAYER;
    cam->targetPlayerId = playerId;
    cam->distancia = dist;
    cam->offsetY = altura;
    cam->sensibilidade = sensibilidade;
    cam->yaw = 0;
    cam->pitch = 0.2f;
}

// Esta função deve ser chamada dentro de janela_renderizar para pegar a Matriz View atualizada
static Mat4x4 camera_atualizar_matriz(int jogoId) {
    Camera* cam = camera_obter(jogoId);
    
    // Variáveis finais da câmera
    Vec3 eye, target, up = {0, 1, 0};
    
    if (cam->modo == CAM_FIXA) {
        eye = {cam->x, cam->y, cam->z};
        target = {cam->targetX, cam->targetY, cam->targetZ};
    }
    else {
        // === LÓGICA DE MOUSE ===
        float mDx = 0, mDy = 0;
        
        // Verifica se botão direito (Orbital) ou Esquerdo/Nenhum (Player) está pressionado?
        // Por simplicidade, vamos assumir que se estiver nesses modos, o mouse controla sempre.
        // O ideal seria verificar if(GetAsyncKeyState(VK_RBUTTON)) para orbital.
        
        bool processarMouse = true;
        
        // Se for orbital, exigimos botão direito segurado para girar (padrão de editores)
        if (cam->modo == CAM_ORBITAL) {
            processarMouse = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            if (!processarMouse) cam->mouseAtivo = false; // Reseta delta quando solta
        }
        
        if (processarMouse) {
            camera_mouse_delta(cam, &mDx, &mDy);
            cam->yaw   += mDx * cam->sensibilidade;
            cam->pitch += mDy * cam->sensibilidade;
            
            // Limitar Pitch (não dar cambalhota completa)
            if (cam->pitch > 1.5f) cam->pitch = 1.5f;   // ~85 graus cima
            if (cam->pitch < -1.5f) cam->pitch = -1.5f; // ~85 graus baixo
        }

        // === POSICIONAMENTO ===
        
        float centroX, centroY, centroZ;
        
        if (cam->modo == CAM_PLAYER) {
            // Pega posição do Player
            Player* p = player_get(cam->targetPlayerId);
            if (p) {
                centroX = p->x;
                centroY = p->y + cam->offsetY; // Olha para a cabeça/meio
                centroZ = p->z;
                
                // *** ROTACIONAR O PLAYER JUNTO COM A CAMERA ***
                // Player gira na mesma direção que a câmera
                // +PI para o player ficar de costas para a câmera (TPS clássico)
                p->rotY = cam->yaw + PI;
                
            } else {
                // Fallback se player não existe
                centroX = 0; centroY = 0; centroZ = 0;
            }
        } else {
            // Orbital
            centroX = cam->targetX;
            centroY = cam->targetY;
            centroZ = cam->targetZ;
        }
        
        // Matemática Esférica para Posição da Câmera
        // x = dist * cos(pitch) * sin(yaw)
        // z = dist * cos(pitch) * cos(yaw)
        // y = dist * sin(pitch)
        
        eye.x = centroX + cam->distancia * cos(cam->pitch) * sin(cam->yaw);
        eye.z = centroZ + cam->distancia * cos(cam->pitch) * cos(cam->yaw);
        eye.y = centroY + cam->distancia * sin(cam->pitch);
        
        target = {centroX, centroY, centroZ};
    }
    
    return mat_lookat(eye, target, up);
}

static void camera_cleanup() {
    for (auto& [id, cam] : g_cameras) {
        delete cam;
    }
    g_cameras.clear();
}

#endif