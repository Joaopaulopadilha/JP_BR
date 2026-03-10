// camera.hpp
// Câmera 3D com modo livre (FPS) e orbital para engine sml3d

#pragma once

#include <cmath>
#include <cstdio>
#include <unordered_map>

#include "mesh.hpp" // Mat4, Vec3

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// MODOS DE CÂMERA
// =============================================================================

enum SML3DCameraModo {
    SML3D_CAMERA_LIVRE   = 0,  // FPS-style: posição + yaw/pitch
    SML3D_CAMERA_ORBITAL = 1   // Gira ao redor de um alvo
};

// =============================================================================
// ESTRUTURA DA CÂMERA
// =============================================================================

struct SML3DCamera {
    int janelaId = 0;

    // Posição e alvo
    Vec3 posicao = {0, 2, 5};
    Vec3 alvo    = {0, 0, 0};
    Vec3 up      = {0, 1, 0};

    // Modo livre (FPS): yaw/pitch em graus
    float yaw   = -90.0f;  // Olhando pra -Z por padrão
    float pitch = 0.0f;

    // Modo orbital: distância e ângulos em graus
    float orbital_dist  = 5.0f;
    float orbital_yaw   = 0.0f;
    float orbital_pitch = 20.0f;

    // Perspectiva
    float fov  = 45.0f;   // Graus
    float near = 0.1f;
    float far  = 1000.0f;

    // Modo atual
    SML3DCameraModo modo = SML3D_CAMERA_LIVRE;

    // Colisão de câmera: impede que a câmera atravesse meshes
    bool camera_collision = true;
    float collision_offset = 0.2f;  // Offset pra não grudar na superfície

    // Posição efetiva da câmera orbital (após colisão)
    // Atualizada por atualizarCameraCollision() antes do render
    Vec3 pos_orbital_efetiva = {0, 0, 0};
    bool pos_orbital_calculada = false;  // Flag: se true, usa pos_efetiva no calcularView

    bool ativa = true;

    // -----------------------------------------------------------------
    // Calcula posição orbital desejada (sem colisão)
    // -----------------------------------------------------------------
    Vec3 calcularPosOrbital() const {
        float radYaw   = orbital_yaw   * (float)M_PI / 180.0f;
        float radPitch = orbital_pitch * (float)M_PI / 180.0f;

        float p = radPitch;
        if (p > 1.55f) p = 1.55f;
        if (p < -1.55f) p = -1.55f;

        float cosP = cosf(p);
        Vec3 pos;
        pos.x = alvo.x + orbital_dist * cosP * sinf(radYaw);
        pos.y = alvo.y + orbital_dist * sinf(p);
        pos.z = alvo.z + orbital_dist * cosP * cosf(radYaw);
        return pos;
    }

    // -----------------------------------------------------------------
    // Calcula a view matrix baseada no modo
    // -----------------------------------------------------------------
    Mat4 calcularView() const {
        if (modo == SML3D_CAMERA_ORBITAL) {
            Vec3 pos;
            if (pos_orbital_calculada) {
                // Usar posição ajustada pela colisão
                pos = pos_orbital_efetiva;
            } else {
                pos = calcularPosOrbital();
            }

            return Mat4::lookAt(pos, alvo, up);
        }

        // Modo livre: calcula direção a partir de yaw/pitch
        float radYaw   = yaw   * (float)M_PI / 180.0f;
        float radPitch = pitch * (float)M_PI / 180.0f;

        // Clamp pitch
        float p = radPitch;
        if (p > 1.55f) p = 1.55f;
        if (p < -1.55f) p = -1.55f;

        Vec3 frente;
        frente.x = cosf(radYaw) * cosf(p);
        frente.y = sinf(p);
        frente.z = sinf(radYaw) * cosf(p);

        Vec3 alvoCalc = {
            posicao.x + frente.x,
            posicao.y + frente.y,
            posicao.z + frente.z
        };

        return Mat4::lookAt(posicao, alvoCalc, up);
    }

    // -----------------------------------------------------------------
    // Calcula a projection matrix
    // -----------------------------------------------------------------
    Mat4 calcularProjection(float aspect) const {
        return Mat4::perspectiva(fov * (float)M_PI / 180.0f, aspect, near, far);
    }

    // -----------------------------------------------------------------
    // Movimento modo livre — frente/trás/esquerda/direita
    // -----------------------------------------------------------------
    void moverFrente(float delta) {
        float radYaw = yaw * (float)M_PI / 180.0f;
        float radPitch = pitch * (float)M_PI / 180.0f;
        float p = radPitch;
        if (p > 1.55f) p = 1.55f;
        if (p < -1.55f) p = -1.55f;

        posicao.x += cosf(radYaw) * cosf(p) * delta;
        posicao.y += sinf(p) * delta;
        posicao.z += sinf(radYaw) * cosf(p) * delta;
    }

    void moverLado(float delta) {
        float radYaw = yaw * (float)M_PI / 180.0f;
        // Direita = cross(frente, up) simplificado no plano XZ
        posicao.x += cosf(radYaw + (float)M_PI / 2.0f) * delta;
        posicao.z += sinf(radYaw + (float)M_PI / 2.0f) * delta;
    }

    void moverCima(float delta) {
        posicao.y += delta;
    }
};

// =============================================================================
// ESTADO GLOBAL DE CÂMERAS
// =============================================================================

static std::unordered_map<int, SML3DCamera> g_cameras;
static int g_prox_camera_id = 1;

// Câmera ativa por janela (última câmera criada ou setada)
static std::unordered_map<int, int> g_camera_ativa_janela;

// =============================================================================
// CRIAÇÃO
// =============================================================================

static int criarCamera(int janelaId, float px, float py, float pz,
                       float ax, float ay, float az) {
    int id = g_prox_camera_id++;
    SML3DCamera& cam = g_cameras[id];
    cam.janelaId = janelaId;
    cam.posicao  = {px, py, pz};
    cam.alvo     = {ax, ay, az};

    // Calcula yaw/pitch iniciais a partir da direção posição->alvo
    float dx = ax - px;
    float dy = ay - py;
    float dz = az - pz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (dist > 0.001f) {
        cam.yaw   = atan2f(dz, dx) * 180.0f / (float)M_PI;
        cam.pitch = asinf(dy / dist) * 180.0f / (float)M_PI;

        // Orbital defaults
        cam.orbital_dist  = dist;
        cam.orbital_yaw   = atan2f(px - ax, pz - az) * 180.0f / (float)M_PI;
        cam.orbital_pitch = asinf((py - ay) / dist) * 180.0f / (float)M_PI;
    }

    // Define como câmera ativa da janela
    g_camera_ativa_janela[janelaId] = id;

    return id;
}

// =============================================================================
// OBTER CÂMERA ATIVA DE UMA JANELA
// =============================================================================

static SML3DCamera* obterCameraAtivaJanela(int janelaId) {
    auto it = g_camera_ativa_janela.find(janelaId);
    if (it == g_camera_ativa_janela.end()) return nullptr;

    auto itCam = g_cameras.find(it->second);
    if (itCam == g_cameras.end()) return nullptr;

    return &itCam->second;
}

// =============================================================================
// CONFIGURAÇÃO
// =============================================================================

static bool cameraPosicao(int id, float x, float y, float z) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.posicao = {x, y, z};
    return true;
}

static bool cameraAlvo(int id, float x, float y, float z) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.alvo = {x, y, z};
    return true;
}

static bool cameraModo(int id, int modo) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    if (modo == 0) it->second.modo = SML3D_CAMERA_LIVRE;
    else           it->second.modo = SML3D_CAMERA_ORBITAL;
    return true;
}

static bool cameraFov(int id, float fov) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.fov = fov;
    return true;
}

static bool cameraYawPitch(int id, float yaw, float pitch) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.yaw = yaw;
    it->second.pitch = pitch;
    return true;
}

// =============================================================================
// ORBITAL — configuração
// =============================================================================

static bool cameraOrbitalDist(int id, float dist) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.orbital_dist = dist;
    return true;
}

static bool cameraOrbitalAngulos(int id, float yaw, float pitch) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.orbital_yaw = yaw;
    it->second.orbital_pitch = pitch;
    return true;
}

// =============================================================================
// MOVIMENTO MODO LIVRE
// =============================================================================

static bool cameraMoverFrente(int id, float delta) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.moverFrente(delta);
    return true;
}

static bool cameraMoverLado(int id, float delta) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.moverLado(delta);
    return true;
}

static bool cameraMoverCima(int id, float delta) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.moverCima(delta);
    return true;
}

// =============================================================================
// ATIVAR CÂMERA
// =============================================================================

static bool cameraAtivar(int id) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    g_camera_ativa_janela[it->second.janelaId] = id;
    return true;
}

// =============================================================================
// FOLLOW — câmera segue uma mesh, movimento relativo à câmera
// =============================================================================

// Mapa: câmera ID → mesh ID que está seguindo
static std::unordered_map<int, int> g_camera_follow;

// Rotação atual do mesh seguido (Y, em radianos) — pra lerp suave
static std::unordered_map<int, float> g_mesh_rotacao_alvo;

static bool cameraFollow(int camId, int meshId) {
    auto itCam = g_cameras.find(camId);
    if (itCam == g_cameras.end()) return false;

    if (meshId <= 0) {
        // Desvincular
        g_camera_follow.erase(camId);
        return true;
    }

    g_camera_follow[camId] = meshId;
    return true;
}

// Atualizar follow: mover alvo da câmera orbital pra posição da mesh
static void atualizarFollow(int camId) {
    auto itFollow = g_camera_follow.find(camId);
    if (itFollow == g_camera_follow.end()) return;

    auto itCam = g_cameras.find(camId);
    if (itCam == g_cameras.end()) return;

    auto itMesh = g_meshes.find(itFollow->second);
    if (itMesh == g_meshes.end()) return;

    // Alvo da câmera orbital = posição da mesh
    itCam->second.alvo = itMesh->second.posicao;
}

// Verificar se a câmera está seguindo algo
static bool cameraEstaSeguindo(int camId) {
    return g_camera_follow.find(camId) != g_camera_follow.end();
}

// =============================================================================
// MOVIMENTO RELATIVO À CÂMERA
// Move a mesh na direção relativa ao yaw da câmera orbital
// frente: positivo = pra frente da câmera, negativo = pra trás
// lado: positivo = pra direita da câmera, negativo = pra esquerda
// Retorna: true e seta posX/posZ com a nova posição
// =============================================================================

static bool moverRelativoCamera(int camId, int meshId,
                                float frente, float lado,
                                float& posX, float& posZ) {
    auto itCam = g_cameras.find(camId);
    auto itMesh = g_meshes.find(meshId);
    if (itCam == g_cameras.end() || itMesh == g_meshes.end()) return false;

    SML3DCamera& cam = itCam->second;
    SML3DMesh& mesh = itMesh->second;

    // Calcular posição real da câmera orbital
    float camYaw = cam.orbital_yaw * (float)M_PI / 180.0f;
    float camPitch = cam.orbital_pitch * (float)M_PI / 180.0f;
    if (camPitch > 1.55f) camPitch = 1.55f;
    if (camPitch < -1.55f) camPitch = -1.55f;

    float cosP = cosf(camPitch);
    float camPosX = cam.alvo.x + cam.orbital_dist * cosP * sinf(camYaw);
    float camPosZ = cam.alvo.z + cam.orbital_dist * cosP * cosf(camYaw);

    // Vetor câmera → alvo projetado no plano XZ = direção "frente"
    float fwdX = cam.alvo.x - camPosX;
    float fwdZ = cam.alvo.z - camPosZ;

    // Normalizar no plano XZ
    float fwdLen = sqrtf(fwdX * fwdX + fwdZ * fwdZ);
    if (fwdLen > 0.0001f) {
        fwdX /= fwdLen;
        fwdZ /= fwdLen;
    }

    // Direção "lado" = perpendicular no plano XZ (rotação 90° horária)
    float rightX = -fwdZ;
    float rightZ = fwdX;

    // Calcular deslocamento
    float dx = fwdX * frente + rightX * lado;
    float dz = fwdZ * frente + rightZ * lado;

    posX = mesh.posicao.x + dx;
    posZ = mesh.posicao.z + dz;

    return true;
}

// =============================================================================
// ATUALIZAR ROTAÇÃO DO MESH PELA CÂMERA — chamar todo frame
// O mesh sempre fica virado pra onde a câmera está olhando
// =============================================================================

static void atualizarRotacaoFollow(int camId) {
    auto itFollow = g_camera_follow.find(camId);
    if (itFollow == g_camera_follow.end()) return;

    auto itCam = g_cameras.find(camId);
    if (itCam == g_cameras.end()) return;

    int meshId = itFollow->second;

    SML3DCamera& cam = itCam->second;

    // Calcular posição real da câmera orbital
    float camYaw = cam.orbital_yaw * (float)M_PI / 180.0f;
    float camPitch = cam.orbital_pitch * (float)M_PI / 180.0f;
    if (camPitch > 1.55f) camPitch = 1.55f;
    if (camPitch < -1.55f) camPitch = -1.55f;

    float cosP = cosf(camPitch);
    float camPosX = cam.alvo.x + cam.orbital_dist * cosP * sinf(camYaw);
    float camPosZ = cam.alvo.z + cam.orbital_dist * cosP * cosf(camYaw);

    // Vetor câmera → alvo no plano XZ
    float fwdX = cam.alvo.x - camPosX;
    float fwdZ = cam.alvo.z - camPosZ;

    float fwdLen = sqrtf(fwdX * fwdX + fwdZ * fwdZ);
    if (fwdLen > 0.0001f) {
        fwdX /= fwdLen;
        fwdZ /= fwdLen;
    }

    float alvo_rot = -atan2f(fwdX, fwdZ);
    g_mesh_rotacao_alvo[meshId] = alvo_rot;
}

// =============================================================================
// ATUALIZAR ROTAÇÃO SUAVE (LERP) — chamar por frame
// =============================================================================

static void atualizarRotacaoSuave(int meshId, float velocidade) {
    auto itMesh = g_meshes.find(meshId);
    if (itMesh == g_meshes.end()) return;

    auto itAlvo = g_mesh_rotacao_alvo.find(meshId);
    if (itAlvo == g_mesh_rotacao_alvo.end()) return;

    float atual = itMesh->second.rotacao.y;
    float alvo = itAlvo->second;

    // Normalizar diferença pra [-PI, PI] pra pegar o caminho mais curto
    float diff = alvo - atual;
    while (diff > (float)M_PI)  diff -= 2.0f * (float)M_PI;
    while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;

    // Lerp
    float nova = atual + diff * velocidade;

    itMesh->second.rotacao.y = nova;
}

// =============================================================================
// COLISÃO DE CÂMERA — toggle
// =============================================================================

static bool cameraCollision(int id, bool ativar) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return false;
    it->second.camera_collision = ativar;
    return true;
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirCamera(int id) {
    auto it = g_cameras.find(id);
    if (it == g_cameras.end()) return;

    int janId = it->second.janelaId;
    g_cameras.erase(it);

    // Remove da ativa se era essa
    auto itAtiva = g_camera_ativa_janela.find(janId);
    if (itAtiva != g_camera_ativa_janela.end() && itAtiva->second == id) {
        g_camera_ativa_janela.erase(itAtiva);
    }
}

static void destruirCamerasJanela(int janelaId) {
    auto it = g_cameras.begin();
    while (it != g_cameras.end()) {
        if (it->second.janelaId == janelaId) {
            it = g_cameras.erase(it);
        } else {
            ++it;
        }
    }
    g_camera_ativa_janela.erase(janelaId);
}