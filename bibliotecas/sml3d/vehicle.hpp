// vehicle.hpp
// Sistema de veículo com suspensão e inclinação para engine sml3d

#pragma once

#include <cmath>
#include <vector>
#include <unordered_map>

#include "mesh.hpp"
#include "collision.hpp"

// =============================================================================
// ESTRUTURAS
// =============================================================================

struct SML3DVehicleWheel {
    int meshId = 0;
    float off_x = 0, off_y = 0, off_z = 0;
    float altura_chao = -9999.0f;
    bool tocando_chao = false;
};

struct SML3DVehicle {
    int corpoId = 0;
    std::vector<SML3DVehicleWheel> rodas;

    float susp_forca = 30.0f;
    float susp_amortecimento = 15.0f;

    float vel_y = 0.0f;
    float pitch_atual = 0.0f;
    float roll_atual = 0.0f;
    float inclinacao_lerp = 8.0f;
    float max_inclinacao = 15.0f;
};

static std::unordered_map<int, SML3DVehicle> g_vehicles;

// =============================================================================
// API
// =============================================================================

static bool registrarVeiculo(int corpoId) {
    if (g_meshes.find(corpoId) == g_meshes.end()) return false;
    g_vehicles[corpoId].corpoId = corpoId;
    return true;
}

static bool registrarRodaVeiculo(int corpoId, int rodaId, float offX, float offY, float offZ) {
    auto itV = g_vehicles.find(corpoId);
    if (itV == g_vehicles.end()) return false;
    if (g_meshes.find(rodaId) == g_meshes.end()) return false;

    SML3DVehicleWheel roda;
    roda.meshId = rodaId;
    roda.off_x = offX;
    roda.off_y = offY;
    roda.off_z = offZ;
    itV->second.rodas.push_back(roda);
    return true;
}

static bool configurarSuspensao(int corpoId, float forca, float amortecimento) {
    auto itV = g_vehicles.find(corpoId);
    if (itV == g_vehicles.end()) return false;
    itV->second.susp_forca = forca;
    itV->second.susp_amortecimento = amortecimento;
    return true;
}

// =============================================================================
// HELPER: ENCONTRAR TOPO DO CHÃO ABAIXO DE UM PONTO
// Verifica quais meshes sólidas estão embaixo (XZ overlap) e retorna o topo mais alto
// =============================================================================

static float encontrarChao(Vec3 pos, const std::vector<int>& ignorar, int janelaId) {
    float melhor_y = -9999.0f;

    for (int chaoId : g_meshes_chao) {
        bool ignorado = false;
        for (int ig : ignorar) {
            if (chaoId == ig) { ignorado = true; break; }
        }
        if (ignorado) continue;

        auto itChao = g_meshes.find(chaoId);
        if (itChao == g_meshes.end()) continue;
        if (itChao->second.janelaId != janelaId) continue;

        SML3DAABB box = calcularAABB(itChao->second);

        // A roda está acima deste sólido em XZ?
        if (pos.x < box.min_x || pos.x > box.max_x) continue;
        if (pos.z < box.min_z || pos.z > box.max_z) continue;

        // O topo deste sólido está abaixo (ou no nível) da posição da roda?
        if (box.max_y <= pos.y + 0.5f) {
            if (box.max_y > melhor_y) {
                melhor_y = box.max_y;
            }
        }
    }

    return melhor_y;
}

// =============================================================================
// HELPER: PONTO LOCAL → MUNDIAL (rotação Y do corpo)
// =============================================================================

static Vec3 pontoLocalParaMundial(const SML3DMesh& corpo, float lx, float ly, float lz) {
    float cosR = cosf(corpo.rotacao.y);
    float sinR = sinf(corpo.rotacao.y);

    Vec3 r;
    r.x = corpo.posicao.x + lx * cosR + lz * sinR;
    r.y = corpo.posicao.y + ly;
    r.z = corpo.posicao.z - lx * sinR + lz * cosR;
    return r;
}

// =============================================================================
// ATUALIZAR VEÍCULO
// =============================================================================

static void atualizarVeiculo(int corpoId, float dt) {
    auto itV = g_vehicles.find(corpoId);
    if (itV == g_vehicles.end()) return;
    SML3DVehicle& v = itV->second;

    auto itCorpo = g_meshes.find(corpoId);
    if (itCorpo == g_meshes.end()) return;
    SML3DMesh& corpo = itCorpo->second;

    if (v.rodas.empty() || dt < 0.0001f) return;

    // Meshes a ignorar
    std::vector<int> ignorar;
    ignorar.push_back(corpoId);
    for (auto& roda : v.rodas) ignorar.push_back(roda.meshId);
    for (auto& [mid, mesh] : g_meshes) {
        if (mesh.parentId == corpoId) ignorar.push_back(mid);
    }

    // -----------------------------------------------------------------
    // 1. ENCONTRAR CHÃO EMBAIXO DE CADA RODA
    // -----------------------------------------------------------------

    int rodas_no_chao = 0;

    for (auto& roda : v.rodas) {
        Vec3 pos_roda = pontoLocalParaMundial(corpo, roda.off_x, roda.off_y, roda.off_z);
        float chao = encontrarChao(pos_roda, ignorar, corpo.janelaId);
        roda.altura_chao = chao;
        roda.tocando_chao = (chao > -9000.0f);
        if (roda.tocando_chao) rodas_no_chao++;
    }

    // -----------------------------------------------------------------
    // 2. AJUSTAR ALTURA DO CORPO
    //
    // off_y é negativo (ex: -0.6) → roda fica abaixo do corpo
    // Queremos: corpo.y + off_y = chão → corpo.y = chão - off_y
    // Pegamos o MAIOR alvo pra nenhuma roda enterrar
    // -----------------------------------------------------------------

    if (rodas_no_chao > 0) {
        float alvo = -9999.0f;

        for (auto& roda : v.rodas) {
            if (!roda.tocando_chao) continue;
            float a = roda.altura_chao - roda.off_y;
            if (a > alvo) alvo = a;
        }

        if (alvo > -9000.0f) {
            float diff = alvo - corpo.posicao.y;

            // Spring-damper
            float forca = diff * v.susp_forca - v.vel_y * v.susp_amortecimento;
            v.vel_y += forca * dt;

            // Limitar vel vertical
            if (v.vel_y > 3.0f) v.vel_y = 3.0f;
            if (v.vel_y < -3.0f) v.vel_y = -3.0f;

            corpo.posicao.y += v.vel_y * dt;

            // Hard limit
            if (corpo.posicao.y < alvo - 0.02f) {
                corpo.posicao.y = alvo - 0.02f;
                if (v.vel_y < 0.0f) v.vel_y = 0.0f;
            }
        }
    } else {
        // No ar
        v.vel_y += -9.8f * dt;
        corpo.posicao.y += v.vel_y * dt;
    }

    // -----------------------------------------------------------------
    // 3. INCLINAÇÃO
    // -----------------------------------------------------------------

    if (v.rodas.size() >= 4) {
        float mf = 0, mt = 0, me = 0, md = 0;
        int nf = 0, nt = 0, ne = 0, nd = 0;

        for (auto& roda : v.rodas) {
            if (!roda.tocando_chao) continue;
            float h = roda.altura_chao;

            if (roda.off_z > 0) { mf += h; nf++; } else { mt += h; nt++; }
            if (roda.off_x < 0) { me += h; ne++; } else { md += h; nd++; }
        }

        float pitch_alvo = 0.0f;
        if (nf > 0 && nt > 0) {
            mf /= (float)nf; mt /= (float)nt;
            float dz = 0;
            for (auto& r : v.rodas) { float a = fabsf(r.off_z); if (a > dz) dz = a; }
            if (dz > 0.01f) pitch_alvo = atanf((mf - mt) / (dz * 2.0f)) * 180.0f / (float)M_PI;
        }

        float roll_alvo = 0.0f;
        if (ne > 0 && nd > 0) {
            me /= (float)ne; md /= (float)nd;
            float dx = 0;
            for (auto& r : v.rodas) { float a = fabsf(r.off_x); if (a > dx) dx = a; }
            if (dx > 0.01f) roll_alvo = atanf((me - md) / (dx * 2.0f)) * 180.0f / (float)M_PI;
        }

        if (pitch_alvo > v.max_inclinacao) pitch_alvo = v.max_inclinacao;
        if (pitch_alvo < -v.max_inclinacao) pitch_alvo = -v.max_inclinacao;
        if (roll_alvo > v.max_inclinacao) roll_alvo = v.max_inclinacao;
        if (roll_alvo < -v.max_inclinacao) roll_alvo = -v.max_inclinacao;

        float lt = v.inclinacao_lerp * dt;
        if (lt > 1.0f) lt = 1.0f;

        v.pitch_atual += (pitch_alvo - v.pitch_atual) * lt;
        v.roll_atual += (roll_alvo - v.roll_atual) * lt;

        corpo.rotacao.x = v.pitch_atual * (float)M_PI / 180.0f;
        corpo.rotacao.z = v.roll_atual * (float)M_PI / 180.0f;
    }
}

// =============================================================================
// CONSULTA E LIMPEZA
// =============================================================================

static bool meshEhVeiculo(int meshId) {
    return g_vehicles.find(meshId) != g_vehicles.end();
}

static void destruirVeiculo(int corpoId) {
    g_vehicles.erase(corpoId);
}

static void destruirVeiculosJanela(int janelaId) {
    auto it = g_vehicles.begin();
    while (it != g_vehicles.end()) {
        auto meshIt = g_meshes.find(it->first);
        if (meshIt != g_meshes.end() && meshIt->second.janelaId == janelaId) {
            it = g_vehicles.erase(it);
        } else {
            ++it;
        }
    }
}