// collision.hpp
// Sistema de colisão (AABB, distância, raycast) para engine sml3d

#pragma once

#include <cmath>
#include "mesh.hpp"
#include "camera.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// AABB — Axis-Aligned Bounding Box
// =============================================================================

struct SML3DAABB {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
};

// =============================================================================
// CALCULAR AABB DE UMA MESH
// Baseado na posição e escala da mesh.
// Primitivas têm tamanho base de -0.5 a 0.5 (cubo/plano) ou raio 0.5 (esfera)
// =============================================================================

static SML3DAABB calcularAABB(const SML3DMesh& mesh) {
    SML3DAABB box;

    // Usar bounds locais da geometria × escala
    // Primitivas: bounds_min/max = -0.5/0.5 (padrão)
    // Modelos OBJ: bounds calculados dos vértices reais
    float min_x = mesh.bounds_min.x * mesh.escala.x;
    float min_y = mesh.bounds_min.y * mesh.escala.y;
    float min_z = mesh.bounds_min.z * mesh.escala.z;
    float max_x = mesh.bounds_max.x * mesh.escala.x;
    float max_y = mesh.bounds_max.y * mesh.escala.y;
    float max_z = mesh.bounds_max.z * mesh.escala.z;

    // Plano tem altura zero, dar uma espessura mínima pra colisão funcionar
    if (mesh.tipo == SML3D_PLANO) {
        min_y = -0.01f * mesh.escala.y;
        max_y =  0.01f * mesh.escala.y;
    }

    box.min_x = mesh.posicao.x + min_x;
    box.min_y = mesh.posicao.y + min_y;
    box.min_z = mesh.posicao.z + min_z;
    box.max_x = mesh.posicao.x + max_x;
    box.max_y = mesh.posicao.y + max_y;
    box.max_z = mesh.posicao.z + max_z;

    return box;
}

// =============================================================================
// COLISÃO AABB vs AABB
// =============================================================================

static bool testarColisaoAABB(const SML3DAABB& a, const SML3DAABB& b) {
    // Separação em qualquer eixo = sem colisão
    if (a.max_x < b.min_x || a.min_x > b.max_x) return false;
    if (a.max_y < b.min_y || a.min_y > b.max_y) return false;
    if (a.max_z < b.min_z || a.min_z > b.max_z) return false;
    return true;
}

// =============================================================================
// API: COLISÃO ENTRE DUAS MESHES
// =============================================================================

static bool colisaoMeshMesh(int id1, int id2) {
    auto it1 = g_meshes.find(id1);
    auto it2 = g_meshes.find(id2);
    if (it1 == g_meshes.end() || it2 == g_meshes.end()) return false;

    SML3DAABB a = calcularAABB(it1->second);
    SML3DAABB b = calcularAABB(it2->second);

    return testarColisaoAABB(a, b);
}

// =============================================================================
// API: DISTÂNCIA ENTRE DUAS MESHES (centro a centro)
// =============================================================================

static float distanciaMeshMesh(int id1, int id2) {
    auto it1 = g_meshes.find(id1);
    auto it2 = g_meshes.find(id2);
    if (it1 == g_meshes.end() || it2 == g_meshes.end()) return -1.0f;

    float dx = it1->second.posicao.x - it2->second.posicao.x;
    float dy = it1->second.posicao.y - it2->second.posicao.y;
    float dz = it1->second.posicao.z - it2->second.posicao.z;

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// =============================================================================
// API: BOUNDING BOX DE UMA MESH
// Retorna os 6 valores (min_x, min_y, min_z, max_x, max_y, max_z)
// via ponteiros — o wrapper no sml3d.cpp expõe valores individuais
// =============================================================================

static bool obterBounds(int id, SML3DAABB& out) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;

    out = calcularAABB(it->second);
    return true;
}

// =============================================================================
// RAYCAST: RAY vs AABB (slab method)
// Testa se um raio (origem + direção) intersecta uma AABB
// =============================================================================

static bool rayIntersectAABB(Vec3 origin, Vec3 dir, const SML3DAABB& box,
                             float tmin_limit = 0.0f, float tmax_limit = 10000.0f) {
    float tmin = tmin_limit;
    float tmax = tmax_limit;

    // Eixo X
    if (fabsf(dir.x) > 0.00001f) {
        float invD = 1.0f / dir.x;
        float t0 = (box.min_x - origin.x) * invD;
        float t1 = (box.max_x - origin.x) * invD;
        if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    } else {
        if (origin.x < box.min_x || origin.x > box.max_x) return false;
    }

    // Eixo Y
    if (fabsf(dir.y) > 0.00001f) {
        float invD = 1.0f / dir.y;
        float t0 = (box.min_y - origin.y) * invD;
        float t1 = (box.max_y - origin.y) * invD;
        if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    } else {
        if (origin.y < box.min_y || origin.y > box.max_y) return false;
    }

    // Eixo Z
    if (fabsf(dir.z) > 0.00001f) {
        float invD = 1.0f / dir.z;
        float t0 = (box.min_z - origin.z) * invD;
        float t1 = (box.max_z - origin.z) * invD;
        if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    } else {
        if (origin.z < box.min_z || origin.z > box.max_z) return false;
    }

    return true;
}

// =============================================================================
// RAYCAST: RAY vs AABB com retorno do T de entrada
// Retorna true se intersecta, e out_t recebe a distância ao longo do raio
// =============================================================================

static bool rayIntersectAABB_t(Vec3 origin, Vec3 dir, const SML3DAABB& box,
                               float& out_t,
                               float tmin_limit = 0.0f, float tmax_limit = 10000.0f) {
    float tmin = tmin_limit;
    float tmax = tmax_limit;

    // Eixo X
    if (fabsf(dir.x) > 0.00001f) {
        float invD = 1.0f / dir.x;
        float t0 = (box.min_x - origin.x) * invD;
        float t1 = (box.max_x - origin.x) * invD;
        if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    } else {
        if (origin.x < box.min_x || origin.x > box.max_x) return false;
    }

    // Eixo Y
    if (fabsf(dir.y) > 0.00001f) {
        float invD = 1.0f / dir.y;
        float t0 = (box.min_y - origin.y) * invD;
        float t1 = (box.max_y - origin.y) * invD;
        if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    } else {
        if (origin.y < box.min_y || origin.y > box.max_y) return false;
    }

    // Eixo Z
    if (fabsf(dir.z) > 0.00001f) {
        float invD = 1.0f / dir.z;
        float t0 = (box.min_z - origin.z) * invD;
        float t1 = (box.max_z - origin.z) * invD;
        if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    } else {
        if (origin.z < box.min_z || origin.z > box.max_z) return false;
    }

    out_t = tmin;
    return true;
}

// =============================================================================
// COLISÃO DE CÂMERA — atualizar posição efetiva da câmera orbital
// Faz raycast do alvo até a posição desejada contra todas as meshes visíveis
// da janela (excluindo a mesh seguida pela câmera).
// Chamar todo frame ANTES do render.
// =============================================================================

static void atualizarCameraCollision(int camId) {
    auto itCam = g_cameras.find(camId);
    if (itCam == g_cameras.end()) return;

    SML3DCamera& cam = itCam->second;

    // Só funciona no modo orbital
    if (cam.modo != SML3D_CAMERA_ORBITAL) {
        cam.pos_orbital_calculada = false;
        return;
    }

    // Calcular posição desejada
    Vec3 posDesejada = cam.calcularPosOrbital();

    // Se colisão está desativada, usar posição desejada direto
    if (!cam.camera_collision) {
        cam.pos_orbital_efetiva = posDesejada;
        cam.pos_orbital_calculada = true;
        return;
    }

    // Direção: do alvo até a posição desejada
    Vec3 dir;
    dir.x = posDesejada.x - cam.alvo.x;
    dir.y = posDesejada.y - cam.alvo.y;
    dir.z = posDesejada.z - cam.alvo.z;

    float distTotal = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (distTotal < 0.001f) {
        cam.pos_orbital_efetiva = posDesejada;
        cam.pos_orbital_calculada = true;
        return;
    }

    // Normalizar direção
    Vec3 dirNorm;
    dirNorm.x = dir.x / distTotal;
    dirNorm.y = dir.y / distTotal;
    dirNorm.z = dir.z / distTotal;

    // Descobrir qual mesh a câmera está seguindo (pra ignorar no raycast)
    int meshSeguida = -1;
    auto itFollow = g_camera_follow.find(camId);
    if (itFollow != g_camera_follow.end()) {
        meshSeguida = itFollow->second;
    }

    // Raycast contra todas as meshes visíveis da janela
    float menorT = distTotal;  // Começa com a distância total (sem colisão)
    bool colidiu = false;

    for (auto& [meshId, mesh] : g_meshes) {
        // Ignorar meshes de outra janela, invisíveis, e a mesh seguida
        if (mesh.janelaId != cam.janelaId) continue;
        if (!mesh.visivel) continue;
        if (meshId == meshSeguida) continue;

        SML3DAABB box = calcularAABB(mesh);

        // Ignorar meshes cujo AABB contém o ponto de origem (alvo da câmera)
        // Se a origem está dentro da AABB, o raio começa dentro da mesh
        // (ex: chão gigante embaixo do player) — não deve bloquear a câmera
        if (cam.alvo.x >= box.min_x && cam.alvo.x <= box.max_x &&
            cam.alvo.y >= box.min_y && cam.alvo.y <= box.max_y &&
            cam.alvo.z >= box.min_z && cam.alvo.z <= box.max_z) {
            continue;
        }

        float t;
        if (rayIntersectAABB_t(cam.alvo, dirNorm, box, t, 0.0f, distTotal)) {
            if (t < menorT) {
                menorT = t;
                colidiu = true;
            }
        }
    }

    if (colidiu) {
        // Aproximar a câmera: posicionar no ponto de colisão com offset
        float distFinal = menorT - cam.collision_offset;
        if (distFinal < 0.1f) distFinal = 0.1f;  // Mínimo pra não ficar dentro do alvo

        cam.pos_orbital_efetiva.x = cam.alvo.x + dirNorm.x * distFinal;
        cam.pos_orbital_efetiva.y = cam.alvo.y + dirNorm.y * distFinal;
        cam.pos_orbital_efetiva.z = cam.alvo.z + dirNorm.z * distFinal;
    } else {
        cam.pos_orbital_efetiva = posDesejada;
    }

    cam.pos_orbital_calculada = true;
}
// Calcula a direção que a câmera está olhando e testa se o raio
// intersecta a bounding box da mesh
// =============================================================================

static bool raycastCameraMesh(int camId, int meshId) {
    auto itCam = g_cameras.find(camId);
    auto itMesh = g_meshes.find(meshId);
    if (itCam == g_cameras.end() || itMesh == g_meshes.end()) return false;

    SML3DCamera& cam = itCam->second;
    SML3DAABB box = calcularAABB(itMesh->second);

    Vec3 origin, dir;

    if (cam.modo == SML3D_CAMERA_ORBITAL) {
        // Orbital: posição calculada a partir de alvo + ângulos + distância
        float radYaw   = cam.orbital_yaw   * (float)M_PI / 180.0f;
        float radPitch = cam.orbital_pitch * (float)M_PI / 180.0f;
        float p = radPitch;
        if (p > 1.55f) p = 1.55f;
        if (p < -1.55f) p = -1.55f;

        float cosP = cosf(p);
        origin.x = cam.alvo.x + cam.orbital_dist * cosP * sinf(radYaw);
        origin.y = cam.alvo.y + cam.orbital_dist * sinf(p);
        origin.z = cam.alvo.z + cam.orbital_dist * cosP * cosf(radYaw);

        // Direção: da posição calculada pro alvo
        dir.x = cam.alvo.x - origin.x;
        dir.y = cam.alvo.y - origin.y;
        dir.z = cam.alvo.z - origin.z;
    } else {
        // Modo livre: posição direta, direção via yaw/pitch
        origin = cam.posicao;

        float radYaw   = cam.yaw   * (float)M_PI / 180.0f;
        float radPitch = cam.pitch * (float)M_PI / 180.0f;
        float p = radPitch;
        if (p > 1.55f) p = 1.55f;
        if (p < -1.55f) p = -1.55f;

        dir.x = cosf(radYaw) * cosf(p);
        dir.y = sinf(p);
        dir.z = sinf(radYaw) * cosf(p);
    }

    // Normalizar direção
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.00001f) {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    }

    return rayIntersectAABB(origin, dir, box);
}

// =============================================================================
// PROPRIEDADES DE COLISÃO POR MESH
// =============================================================================

struct SML3DCollisionProps {
    bool pushable = false;     // Pode ser empurrado por outros
    float bounce = 0.0f;       // Fator de quique (0.0 a 1.0)
};

static std::unordered_map<int, SML3DCollisionProps> g_collision_props;

static bool definirPushable(int meshId, bool pushable) {
    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return false;
    g_collision_props[meshId].pushable = pushable;
    return true;
}

static bool definirBounce(int meshId, float bounce) {
    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return false;
    if (bounce < 0.0f) bounce = 0.0f;
    if (bounce > 1.0f) bounce = 1.0f;
    g_collision_props[meshId].bounce = bounce;
    return true;
}

// =============================================================================
// CALCULAR PENETRAÇÃO MÍNIMA ENTRE DOIS AABB (MTV — Minimum Translation Vector)
// Retorna o eixo e a distância mínima pra separar os dois objetos
// =============================================================================

struct SML3DMTV {
    float dx = 0, dy = 0, dz = 0;  // Direção e magnitude da separação
    bool valido = false;
};

static SML3DMTV calcularMTV(const SML3DAABB& a, const SML3DAABB& b) {
    SML3DMTV mtv;

    // Overlap em cada eixo
    float overlapX1 = a.max_x - b.min_x;  // A empurrado pra -X
    float overlapX2 = b.max_x - a.min_x;  // A empurrado pra +X
    float overlapY1 = a.max_y - b.min_y;  // A empurrado pra -Y
    float overlapY2 = b.max_y - a.min_y;  // A empurrado pra +Y
    float overlapZ1 = a.max_z - b.min_z;  // A empurrado pra -Z
    float overlapZ2 = b.max_z - a.min_z;  // A empurrado pra +Z

    // Se qualquer overlap é negativo, não há colisão
    if (overlapX1 <= 0 || overlapX2 <= 0 ||
        overlapY1 <= 0 || overlapY2 <= 0 ||
        overlapZ1 <= 0 || overlapZ2 <= 0) {
        return mtv;
    }

    // Menor overlap por eixo (com sinal)
    float minX = (overlapX1 < overlapX2) ? -overlapX1 : overlapX2;
    float minY = (overlapY1 < overlapY2) ? -overlapY1 : overlapY2;
    float minZ = (overlapZ1 < overlapZ2) ? -overlapZ1 : overlapZ2;

    float absX = fabsf(minX);
    float absY = fabsf(minY);
    float absZ = fabsf(minZ);

    // Escolher o eixo com menor penetração
    mtv.valido = true;
    if (absX <= absY && absX <= absZ) {
        mtv.dx = minX;
    } else if (absY <= absX && absY <= absZ) {
        mtv.dy = minY;
    } else {
        mtv.dz = minZ;
    }

    return mtv;
}

// =============================================================================
// RESOLVER COLISÃO ENTRE DUAS MESHES
// moverA: a mesh que está se movendo (jogador)
// staticB: a mesh estática ou pushable
// =============================================================================

static void resolverColisao(int idA, int idB, SML3DMesh& meshA, SML3DMesh& meshB) {
    SML3DAABB boxA = calcularAABB(meshA);
    SML3DAABB boxB = calcularAABB(meshB);

    if (!testarColisaoAABB(boxA, boxB)) return;

    SML3DMTV mtv = calcularMTV(boxA, boxB);
    if (!mtv.valido) return;

    // Buscar propriedades
    auto itPropsB = g_collision_props.find(idB);
    bool bPushable = (itPropsB != g_collision_props.end()) ? itPropsB->second.pushable : false;

    auto itPropsA = g_collision_props.find(idA);
    float bounceA = (itPropsA != g_collision_props.end()) ? itPropsA->second.bounce : 0.0f;

    if (bPushable) {
        // B é empurrável: A move metade, B move metade
        meshA.posicao.x += mtv.dx * 0.5f;
        meshA.posicao.y += mtv.dy * 0.5f;
        meshA.posicao.z += mtv.dz * 0.5f;

        meshB.posicao.x -= mtv.dx * 0.5f;
        meshB.posicao.y -= mtv.dy * 0.5f;
        meshB.posicao.z -= mtv.dz * 0.5f;
    } else {
        // B é sólido: só A é empurrado
        meshA.posicao.x += mtv.dx;
        meshA.posicao.y += mtv.dy;
        meshA.posicao.z += mtv.dz;
    }

    // Bounce e transferência de velocidade são tratados no gravity.hpp
    // onde g_physics é acessível. Aqui guardamos o MTV pra uso posterior.
    (void)bounceA;
}

// =============================================================================
// LIMPEZA DE PROPRIEDADES DE COLISÃO
// =============================================================================

static void destruirCollisionPropsMesh(int id) {
    g_collision_props.erase(id);
}

static void destruirCollisionPropsJanela(int janelaId) {
    auto it = g_collision_props.begin();
    while (it != g_collision_props.end()) {
        auto meshIt = g_meshes.find(it->first);
        if (meshIt != g_meshes.end() && meshIt->second.janelaId == janelaId) {
            it = g_collision_props.erase(it);
        } else {
            ++it;
        }
    }
}