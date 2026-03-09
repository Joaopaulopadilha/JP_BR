// gravity.hpp
// Sistema de gravidade e física básica para engine sml3d

#pragma once

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

#include "mesh.hpp"
#include "collision.hpp"

// =============================================================================
// ESTADO FÍSICO POR MESH
// =============================================================================

struct SML3DPhysics {
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    bool gravidade_ativa = false;
    bool no_chao = false;

    // Chão manual (fallback): Y mínimo definido pelo usuário
    bool tem_ground_manual = false;
    float ground_y = 0.0f;
};

// =============================================================================
// ESTADO GLOBAL DE FÍSICA
// =============================================================================

static std::unordered_map<int, SML3DPhysics> g_physics;
static float g_gravidade = -9.8f;  // Força da gravidade (negativa = pra baixo)

// Delta time
static std::chrono::high_resolution_clock::time_point g_ultimo_frame;
static bool g_primeiro_frame = true;
static float g_delta_time = 0.016f;  // ~60fps como fallback inicial

// Meshes que servem como chão (colidem com objetos que têm gravidade)
static std::unordered_set<int> g_meshes_chao;

// =============================================================================
// DELTA TIME — calcular a cada frame
// =============================================================================

static void atualizarDeltaTime() {
    auto agora = std::chrono::high_resolution_clock::now();

    if (g_primeiro_frame) {
        g_ultimo_frame = agora;
        g_primeiro_frame = false;
        g_delta_time = 0.016f;
        return;
    }

    std::chrono::duration<float> diff = agora - g_ultimo_frame;
    g_delta_time = diff.count();
    g_ultimo_frame = agora;

    // Clamp pra evitar picos absurdos (ex: debugger pausado)
    if (g_delta_time > 0.1f) g_delta_time = 0.1f;
    if (g_delta_time < 0.0001f) g_delta_time = 0.0001f;
}

// =============================================================================
// API: DELTA TIME
// =============================================================================

static float obterDeltaTime() {
    return g_delta_time;
}

// =============================================================================
// API: FORÇA DA GRAVIDADE GLOBAL
// =============================================================================

static void definirGravidade(float forca) {
    g_gravidade = forca;
}

// =============================================================================
// API: ATIVAR/DESATIVAR GRAVIDADE NUMA MESH
// =============================================================================

static bool ativarGravidade(int meshId, bool ativar) {
    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return false;

    g_physics[meshId].gravidade_ativa = ativar;
    return true;
}

// =============================================================================
// API: DEFINIR VELOCIDADE DE UMA MESH
// =============================================================================

static bool definirVelocidade(int meshId, float vx, float vy, float vz) {
    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return false;

    SML3DPhysics& phys = g_physics[meshId];
    phys.vel_x = vx;
    phys.vel_y = vy;
    phys.vel_z = vz;
    return true;
}

// =============================================================================
// API: DEFINIR CHÃO MANUAL (Y mínimo)
// =============================================================================

static bool definirGround(int meshId, float y) {
    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return false;

    SML3DPhysics& phys = g_physics[meshId];
    phys.tem_ground_manual = true;
    phys.ground_y = y;
    return true;
}

// =============================================================================
// API: MARCAR MESH COMO CHÃO (outros objetos param nela)
// =============================================================================

static bool definirMeshChao(int meshId, bool ehChao) {
    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return false;

    if (ehChao) {
        g_meshes_chao.insert(meshId);
    } else {
        g_meshes_chao.erase(meshId);
    }
    return true;
}

// =============================================================================
// API: CONSULTAR SE ESTÁ NO CHÃO
// =============================================================================

static bool estaNoChao(int meshId) {
    auto it = g_physics.find(meshId);
    if (it == g_physics.end()) return false;
    return it->second.no_chao;
}

// =============================================================================
// PHYSICS UPDATE — atualizar gravidade e resolver colisões
// =============================================================================

static void atualizarFisica(int janelaId) {
    atualizarDeltaTime();
    float dt = g_delta_time;

    for (auto& [id, phys] : g_physics) {
        if (!phys.gravidade_ativa) continue;

        auto it = g_meshes.find(id);
        if (it == g_meshes.end()) continue;
        if (it->second.janelaId != janelaId) continue;

        SML3DMesh& mesh = it->second;
        phys.no_chao = false;

        // Aplicar gravidade à velocidade Y
        phys.vel_y += g_gravidade * dt;

        // Calcular nova posição
        float nova_x = mesh.posicao.x + phys.vel_x * dt;
        float nova_y = mesh.posicao.y + phys.vel_y * dt;
        float nova_z = mesh.posicao.z + phys.vel_z * dt;

        // Salvar posição temporária pra testar colisão
        float old_x = mesh.posicao.x;
        float old_y = mesh.posicao.y;
        float old_z = mesh.posicao.z;

        mesh.posicao.x = nova_x;
        mesh.posicao.y = nova_y;
        mesh.posicao.z = nova_z;

        // Testar colisão com meshes marcadas como chão
        SML3DAABB meshBox = calcularAABB(mesh);

        for (int chaoId : g_meshes_chao) {
            if (chaoId == id) continue;  // Não colidir consigo mesmo

            auto itChao = g_meshes.find(chaoId);
            if (itChao == g_meshes.end()) continue;

            SML3DAABB chaoBox = calcularAABB(itChao->second);

            if (testarColisaoAABB(meshBox, chaoBox)) {
                float mesh_bottom = meshBox.min_y;
                float chao_top = chaoBox.max_y;

                // Só resolver como "chão" se:
                // 1. Estava caindo (vel_y <= 0 ou old_y >= nova_y)
                // 2. O CENTRO do jogador está ACIMA do topo do chão
                //    (indica que veio de cima, não de lado)
                float mesh_center_y = mesh.posicao.y;

                if (old_y >= nova_y && mesh_bottom < chao_top && mesh_center_y > chaoBox.min_y) {
                    // Verificar se realmente veio de cima:
                    // O centro do jogador no frame anterior estava acima do topo do chão
                    float old_center_y = old_y;
                    float old_bottom = old_center_y - (mesh.posicao.y - meshBox.min_y);

                    if (old_bottom >= chaoBox.max_y - 0.1f) {
                        // Veio de cima: colocar em cima do chão
                        // Usar bounds reais: posicao.y + bounds_min.y * escala.y = chao_top
                        float bottom_offset = mesh.bounds_min.y * mesh.escala.y;
                        mesh.posicao.y = chao_top - bottom_offset;
                        phys.vel_y = 0.0f;
                        phys.no_chao = true;
                    }
                    // Se não veio de cima, a fase 2 resolve horizontalmente
                }
            }
        }

        // Fallback: chão manual
        if (!phys.no_chao && phys.tem_ground_manual) {
            float bottom_offset = mesh.bounds_min.y * mesh.escala.y;
            float min_y = phys.ground_y - bottom_offset;

            if (mesh.posicao.y < min_y) {
                mesh.posicao.y = min_y;
                phys.vel_y = 0.0f;
                phys.no_chao = true;
            }
        }

        // Fricção horizontal simples (desacelera aos poucos)
        phys.vel_x *= 0.98f;
        phys.vel_z *= 0.98f;

        // Parar velocidades muito pequenas
        if (fabsf(phys.vel_x) < 0.001f) phys.vel_x = 0.0f;
        if (fabsf(phys.vel_z) < 0.001f) phys.vel_z = 0.0f;
    }

    // =========================================================================
    // Fase 2: Resolver colisões entre objetos com física e sólidos/pushables
    // =========================================================================

    // Coletar IDs de meshes com física nesta janela
    std::vector<int> phys_ids;
    for (auto& [pid, phys] : g_physics) {
        auto itM = g_meshes.find(pid);
        if (itM != g_meshes.end() && itM->second.janelaId == janelaId) {
            phys_ids.push_back(pid);
        }
    }

    // Testar cada mesh com física contra sólidos e pushables
    for (int pid : phys_ids) {
        auto itA = g_meshes.find(pid);
        if (itA == g_meshes.end()) continue;

        for (auto& [otherId, otherMesh] : g_meshes) {
            if (otherId == pid) continue;
            if (otherMesh.janelaId != janelaId) continue;

            // Só resolver contra sólidos ou pushables
            bool isSolid = (g_meshes_chao.find(otherId) != g_meshes_chao.end());
            auto itProps = g_collision_props.find(otherId);
            bool isPushable = (itProps != g_collision_props.end()) ? itProps->second.pushable : false;

            if (!isSolid && !isPushable) continue;

            // Calcular MTV antes da resolução pra aplicar bounce depois
            SML3DAABB boxA = calcularAABB(itA->second);
            SML3DAABB boxB = calcularAABB(otherMesh);
            if (!testarColisaoAABB(boxA, boxB)) continue;

            SML3DMTV mtv = calcularMTV(boxA, boxB);
            if (!mtv.valido) continue;

            // Resolver posição
            resolverColisao(pid, otherId, itA->second, otherMesh);

            // Se a resolução foi em Y positivo (empurrado pra cima),
            // significa que está em cima deste objeto = no chão
            if (mtv.dy > 0.001f) {
                auto itPhysP = g_physics.find(pid);
                if (itPhysP != g_physics.end()) {
                    itPhysP->second.vel_y = 0.0f;
                    itPhysP->second.no_chao = true;
                }
            }

            // Bounce: inverter velocidade no eixo da colisão
            auto itPropsA = g_collision_props.find(pid);
            float bounceA = (itPropsA != g_collision_props.end()) ? itPropsA->second.bounce : 0.0f;

            auto itPhysA = g_physics.find(pid);
            if (itPhysA != g_physics.end() && bounceA > 0.0f) {
                if (fabsf(mtv.dx) > 0.0001f) itPhysA->second.vel_x = -itPhysA->second.vel_x * bounceA;
                if (fabsf(mtv.dy) > 0.0001f) itPhysA->second.vel_y = -itPhysA->second.vel_y * bounceA;
                if (fabsf(mtv.dz) > 0.0001f) itPhysA->second.vel_z = -itPhysA->second.vel_z * bounceA;
            }

            // Se B é pushable e tem física, transferir velocidade
            if (isPushable) {
                auto itPhysB = g_physics.find(otherId);
                if (itPhysB != g_physics.end() && itPhysA != g_physics.end()) {
                    if (fabsf(mtv.dx) > 0.0001f) itPhysB->second.vel_x += itPhysA->second.vel_x * 0.3f;
                    if (fabsf(mtv.dz) > 0.0001f) itPhysB->second.vel_z += itPhysA->second.vel_z * 0.3f;
                }
            }
        }
    }
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirPhysicsMesh(int id) {
    g_physics.erase(id);
    g_meshes_chao.erase(id);
}

static void destruirPhysicsJanela(int janelaId) {
    auto it = g_physics.begin();
    while (it != g_physics.end()) {
        auto meshIt = g_meshes.find(it->first);
        if (meshIt != g_meshes.end() && meshIt->second.janelaId == janelaId) {
            g_meshes_chao.erase(it->first);
            it = g_physics.erase(it);
        } else {
            ++it;
        }
    }
}