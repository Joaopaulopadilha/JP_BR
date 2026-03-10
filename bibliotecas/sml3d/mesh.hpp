// mesh.hpp
// Gerenciamento de meshes e primitivas 3D (cubo, esfera, plano) para engine sml3d

#pragma once

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "glad/glad.h"
#include "shader.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// MATH HELPERS — matrizes 4x4 column-major (OpenGL)
// =============================================================================

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Mat4 {
    float m[16];

    static Mat4 identidade() {
        Mat4 r = {};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 translacao(float x, float y, float z) {
        Mat4 r = identidade();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 escala(float x, float y, float z) {
        Mat4 r = identidade();
        r.m[0] = x; r.m[5] = y; r.m[10] = z;
        return r;
    }

    static Mat4 rotacaoY(float rad) {
        Mat4 r = identidade();
        float c = cosf(rad), s = sinf(rad);
        r.m[0] = c;  r.m[2] = s;
        r.m[8] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotacaoX(float rad) {
        Mat4 r = identidade();
        float c = cosf(rad), s = sinf(rad);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotacaoZ(float rad) {
        Mat4 r = identidade();
        float c = cosf(rad), s = sinf(rad);
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    static Mat4 perspectiva(float fovRad, float aspect, float near, float far) {
        Mat4 r = {};
        float tanHalf = tanf(fovRad / 2.0f);
        r.m[0]  = 1.0f / (aspect * tanHalf);
        r.m[5]  = 1.0f / tanHalf;
        r.m[10] = -(far + near) / (far - near);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * far * near) / (far - near);
        return r;
    }

    static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
        float flen = sqrtf(f.x*f.x + f.y*f.y + f.z*f.z);
        f.x /= flen; f.y /= flen; f.z /= flen;

        // s = f x up
        Vec3 s = { f.y*up.z - f.z*up.y, f.z*up.x - f.x*up.z, f.x*up.y - f.y*up.x };
        float slen = sqrtf(s.x*s.x + s.y*s.y + s.z*s.z);
        s.x /= slen; s.y /= slen; s.z /= slen;

        // u = s x f
        Vec3 u = { s.y*f.z - s.z*f.y, s.z*f.x - s.x*f.z, s.x*f.y - s.y*f.x };

        Mat4 r = identidade();
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -(s.x*eye.x + s.y*eye.y + s.z*eye.z);
        r.m[13] = -(u.x*eye.x + u.y*eye.y + u.z*eye.z);
        r.m[14] =  (f.x*eye.x + f.y*eye.y + f.z*eye.z);
        return r;
    }

    static Mat4 multiplicar(const Mat4& a, const Mat4& b) {
        Mat4 r = {};
        for (int c = 0; c < 4; c++)
            for (int l = 0; l < 4; l++)
                r.m[c*4+l] = a.m[0*4+l]*b.m[c*4+0] + a.m[1*4+l]*b.m[c*4+1]
                            + a.m[2*4+l]*b.m[c*4+2] + a.m[3*4+l]*b.m[c*4+3];
        return r;
    }
};

// =============================================================================
// ESTRUTURA DA MESH
// =============================================================================

enum SML3DTipoMesh {
    SML3D_CUBO,
    SML3D_ESFERA,
    SML3D_PLANO
};

struct SML3DMesh {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    int numIndices = 0;
    int janelaId = 0;
    SML3DTipoMesh tipo;

    // Transformações
    Vec3 posicao = {0, 0, 0};
    Vec3 rotacao = {0, 0, 0};   // radianos
    Vec3 escala  = {1, 1, 1};

    // Bounds locais da geometria (antes de escala/posição)
    // Primitivas: -0.5 a 0.5. Modelos OBJ: calculado dos vértices.
    Vec3 bounds_min = {-0.5f, -0.5f, -0.5f};
    Vec3 bounds_max = { 0.5f,  0.5f,  0.5f};

    // Cor (0-1)
    float cor_r = 1.0f, cor_g = 1.0f, cor_b = 1.0f;

    // Textura (0 = sem textura, usa cor sólida)
    GLuint texturaGL = 0;
    float tex_tile_x = 1.0f;   // Repetição horizontal da textura
    float tex_tile_y = 1.0f;   // Repetição vertical da textura

    bool visivel = true;

    // Parenting: se parentId > 0, posição e rotação são locais (relativas ao pai)
    // Escala é sempre independente (não herda do pai)
    int parentId = 0;  // 0 = sem pai

    // Offsets: ajuste base do modelo (aplicado antes da transformação normal)
    // Útil pra corrigir orientação de modelos importados sem mexer na rotação de gameplay
    Vec3 offset_pos = {0, 0, 0};     // Offset de posição local (antes de tudo)
    Vec3 offset_rot = {0, 0, 0};     // Offset de rotação local em radianos (antes da rotação normal)

    Mat4 calcularModel() const {
        Mat4 t = Mat4::translacao(posicao.x, posicao.y, posicao.z);
        Mat4 rx = Mat4::rotacaoX(rotacao.x);
        Mat4 ry = Mat4::rotacaoY(rotacao.y);
        Mat4 rz = Mat4::rotacaoZ(rotacao.z);
        Mat4 s = Mat4::escala(escala.x, escala.y, escala.z);

        // Offset de rotação (ajuste base do modelo)
        Mat4 orx = Mat4::rotacaoX(offset_rot.x);
        Mat4 ory = Mat4::rotacaoY(offset_rot.y);
        Mat4 orz = Mat4::rotacaoZ(offset_rot.z);
        Mat4 offset_r = Mat4::multiplicar(ory, Mat4::multiplicar(orx, orz));

        // Offset de posição
        Mat4 offset_t = Mat4::translacao(offset_pos.x, offset_pos.y, offset_pos.z);

        // model = T * RY * RX * RZ * S * OffsetRot * OffsetPos
        // Ordem: primeiro aplica offset (ajuste do modelo), depois escala,
        // depois rotação de gameplay, depois posição
        Mat4 rot = Mat4::multiplicar(ry, Mat4::multiplicar(rx, rz));
        Mat4 offset = Mat4::multiplicar(offset_r, offset_t);
        return Mat4::multiplicar(t, Mat4::multiplicar(rot, Mat4::multiplicar(s, offset)));
    }
};

// =============================================================================
// ESTADO GLOBAL DE MESHES
// =============================================================================

static std::unordered_map<int, SML3DMesh> g_meshes;
static int g_prox_mesh_id = 1;

// IDs de meshes animadas (renderizadas pelo shader skinned, não pelo padrão)
static std::unordered_set<int> g_meshes_animadas;

// =============================================================================
// PARENTING — MODEL MATRIX COM HIERARQUIA
// Calcula a model matrix final considerando a cadeia de pais.
// Posição e rotação do filho são locais (relativas ao pai).
// Escala é sempre independente (não herda do pai).
// =============================================================================

static Mat4 calcularModelComHierarquia(int meshId, int profundidade = 0) {
    // Limite de profundidade pra evitar loops infinitos
    if (profundidade > 16) return Mat4::identidade();

    auto it = g_meshes.find(meshId);
    if (it == g_meshes.end()) return Mat4::identidade();

    SML3DMesh& mesh = it->second;

    // Transformação local deste mesh
    Mat4 local = mesh.calcularModel();

    // Se não tem pai, retorna a transformação local
    if (mesh.parentId <= 0) return local;

    // Se tem pai, calcular: model_pai (sem escala) × local
    auto itPai = g_meshes.find(mesh.parentId);
    if (itPai == g_meshes.end()) return local;

    SML3DMesh& pai = itPai->second;

    // Model do pai SEM escala (só posição + rotação)
    // Porque a escala do pai não deve afetar o filho
    Mat4 t_pai = Mat4::translacao(pai.posicao.x, pai.posicao.y, pai.posicao.z);
    Mat4 rx_pai = Mat4::rotacaoX(pai.rotacao.x);
    Mat4 ry_pai = Mat4::rotacaoY(pai.rotacao.y);
    Mat4 rz_pai = Mat4::rotacaoZ(pai.rotacao.z);
    Mat4 rot_pai = Mat4::multiplicar(ry_pai, Mat4::multiplicar(rx_pai, rz_pai));
    Mat4 model_pai_sem_escala = Mat4::multiplicar(t_pai, rot_pai);

    // Se o pai também tem pai, resolver recursivamente
    if (pai.parentId > 0) {
        // Pegar o model completo do pai (com hierarquia), mas sem escala do pai
        // Pra isso, calcular a hierarquia do avô e aplicar a transformação do pai sem escala
        Mat4 hierarquia_avo = calcularModelComHierarquia(pai.parentId, profundidade + 1);

        // Recompor: avô × (pos+rot do pai) — sem escala do pai
        model_pai_sem_escala = Mat4::multiplicar(hierarquia_avo, model_pai_sem_escala);
    }

    return Mat4::multiplicar(model_pai_sem_escala, local);
}

// =============================================================================
// VINCULAR/DESVINCULAR PARENT
// =============================================================================

static bool meshParent(int filhoId, int paiId) {
    auto itFilho = g_meshes.find(filhoId);
    if (itFilho == g_meshes.end()) return false;

    if (paiId > 0) {
        // Verificar se o pai existe
        auto itPai = g_meshes.find(paiId);
        if (itPai == g_meshes.end()) return false;

        // Evitar parentar a si mesmo
        if (filhoId == paiId) return false;
    }

    itFilho->second.parentId = paiId;
    return true;
}

// =============================================================================
// UPLOAD DE GEOMETRIA PARA GPU
// =============================================================================

static void uploadMesh(SML3DMesh& mesh,
                       const std::vector<float>& vertices,
                       const std::vector<unsigned int>& indices) {
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Stride: 8 floats por vértice (pos:3 + normal:3 + uv:2)
    int stride = 8 * sizeof(float);

    // layout(location=0) posição: 3 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1) normal: 3 floats
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location=2) UV: 2 floats
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    mesh.numIndices = (int)indices.size();
}

// =============================================================================
// GERAÇÃO DE PRIMITIVAS
// =============================================================================

static int criarCubo(int janelaId) {
    // Cubo unitário (-0.5 a 0.5) com normais por face e UVs
    // Formato: x, y, z, nx, ny, nz, u, v
    std::vector<float> v = {
        // Frente (Z+)
        -0.5f, -0.5f,  0.5f,  0, 0, 1,  0, 0,
         0.5f, -0.5f,  0.5f,  0, 0, 1,  1, 0,
         0.5f,  0.5f,  0.5f,  0, 0, 1,  1, 1,
        -0.5f,  0.5f,  0.5f,  0, 0, 1,  0, 1,
        // Trás (Z-)
        -0.5f, -0.5f, -0.5f,  0, 0,-1,  1, 0,
        -0.5f,  0.5f, -0.5f,  0, 0,-1,  1, 1,
         0.5f,  0.5f, -0.5f,  0, 0,-1,  0, 1,
         0.5f, -0.5f, -0.5f,  0, 0,-1,  0, 0,
        // Topo (Y+)
        -0.5f,  0.5f, -0.5f,  0, 1, 0,  0, 0,
        -0.5f,  0.5f,  0.5f,  0, 1, 0,  0, 1,
         0.5f,  0.5f,  0.5f,  0, 1, 0,  1, 1,
         0.5f,  0.5f, -0.5f,  0, 1, 0,  1, 0,
        // Base (Y-)
        -0.5f, -0.5f, -0.5f,  0,-1, 0,  0, 1,
         0.5f, -0.5f, -0.5f,  0,-1, 0,  1, 1,
         0.5f, -0.5f,  0.5f,  0,-1, 0,  1, 0,
        -0.5f, -0.5f,  0.5f,  0,-1, 0,  0, 0,
        // Direita (X+)
         0.5f, -0.5f, -0.5f,  1, 0, 0,  0, 0,
         0.5f,  0.5f, -0.5f,  1, 0, 0,  0, 1,
         0.5f,  0.5f,  0.5f,  1, 0, 0,  1, 1,
         0.5f, -0.5f,  0.5f,  1, 0, 0,  1, 0,
        // Esquerda (X-)
        -0.5f, -0.5f, -0.5f, -1, 0, 0,  1, 0,
        -0.5f, -0.5f,  0.5f, -1, 0, 0,  0, 0,
        -0.5f,  0.5f,  0.5f, -1, 0, 0,  0, 1,
        -0.5f,  0.5f, -0.5f, -1, 0, 0,  1, 1,
    };

    std::vector<unsigned int> idx = {
         0, 1, 2,  2, 3, 0,   // frente
         4, 5, 6,  6, 7, 4,   // trás
         8, 9,10, 10,11, 8,   // topo
        12,13,14, 14,15,12,   // base
        16,17,18, 18,19,16,   // direita
        20,21,22, 22,23,20,   // esquerda
    };

    int id = g_prox_mesh_id++;
    SML3DMesh& mesh = g_meshes[id];
    mesh.tipo = SML3D_CUBO;
    mesh.janelaId = janelaId;
    uploadMesh(mesh, v, idx);

    return id;
}

static int criarPlano(int janelaId) {
    // Plano unitário 1x1 no eixo XZ, centrado na origem
    // Formato: x, y, z, nx, ny, nz, u, v
    std::vector<float> v = {
        -0.5f, 0.0f, -0.5f,  0, 1, 0,  0, 0,
         0.5f, 0.0f, -0.5f,  0, 1, 0,  1, 0,
         0.5f, 0.0f,  0.5f,  0, 1, 0,  1, 1,
        -0.5f, 0.0f,  0.5f,  0, 1, 0,  0, 1,
    };

    std::vector<unsigned int> idx = {
        0, 1, 2,  2, 3, 0
    };

    int id = g_prox_mesh_id++;
    SML3DMesh& mesh = g_meshes[id];
    mesh.tipo = SML3D_PLANO;
    mesh.janelaId = janelaId;
    uploadMesh(mesh, v, idx);

    return id;
}

static int criarEsfera(int janelaId, int setores = 32, int pilhas = 16) {
    std::vector<float> v;
    std::vector<unsigned int> idx;

    for (int i = 0; i <= pilhas; i++) {
        float phi = (float)M_PI * (float)i / (float)pilhas;  // 0 a PI
        float y  = cosf(phi);
        float r  = sinf(phi);

        for (int j = 0; j <= setores; j++) {
            float theta = 2.0f * (float)M_PI * (float)j / (float)setores;
            float x = r * cosf(theta);
            float z = r * sinf(theta);

            // Posição (raio 0.5)
            v.push_back(x * 0.5f);
            v.push_back(y * 0.5f);
            v.push_back(z * 0.5f);
            // Normal (unitária)
            v.push_back(x);
            v.push_back(y);
            v.push_back(z);
            // UV (coordenadas esféricas)
            v.push_back((float)j / (float)setores);
            v.push_back((float)i / (float)pilhas);
        }
    }

    for (int i = 0; i < pilhas; i++) {
        for (int j = 0; j < setores; j++) {
            int a = i * (setores + 1) + j;
            int b = a + setores + 1;

            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(a + 1);

            idx.push_back(a + 1);
            idx.push_back(b);
            idx.push_back(b + 1);
        }
    }

    int id = g_prox_mesh_id++;
    SML3DMesh& mesh = g_meshes[id];
    mesh.tipo = SML3D_ESFERA;
    mesh.janelaId = janelaId;
    uploadMesh(mesh, v, idx);

    return id;
}

// =============================================================================
// TRANSFORMAÇÕES
// =============================================================================

static bool meshPosicao(int id, float x, float y, float z) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.posicao = {x, y, z};
    return true;
}

static bool meshRotacao(int id, float x, float y, float z) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.rotacao = {x, y, z};
    return true;
}

static bool meshEscala(int id, float x, float y, float z) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.escala = {x, y, z};
    return true;
}

static bool meshOffsetPosicao(int id, float x, float y, float z) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.offset_pos = {x, y, z};
    return true;
}

static bool meshOffsetRotacao(int id, float x, float y, float z) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.offset_rot = {x, y, z};
    return true;
}

// =============================================================================
// MOVIMENTO NA DIREÇÃO PRÓPRIA DA MESH
// Move a mesh pra frente/trás baseado na sua rotação Y atual.
// dist > 0 = pra frente, dist < 0 = pra trás.
// =============================================================================

static bool meshMoverFrente(int id, float dist) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;

    SML3DMesh& mesh = it->second;

    // Direção frente baseada na rotação Y (eixo Z negativo rotacionado)
    float dx = -sinf(mesh.rotacao.y) * dist;
    float dz =  cosf(mesh.rotacao.y) * dist;

    mesh.posicao.x += dx;
    mesh.posicao.z += dz;
    return true;
}

// Rotacionar apenas Y (somar ângulo em radianos)
static bool meshRotacaoYDelta(int id, float delta_rad) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.rotacao.y += delta_rad;
    return true;
}

static bool meshCor(int id, int r, int g, int b) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.cor_r = r / 255.0f;
    it->second.cor_g = g / 255.0f;
    it->second.cor_b = b / 255.0f;
    return true;
}

static bool meshVisivel(int id, bool vis) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.visivel = vis;
    return true;
}

static bool meshTexTile(int id, float tx, float ty) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return false;
    it->second.tex_tile_x = tx;
    it->second.tex_tile_y = ty;
    return true;
}

// =============================================================================
// CONSULTAS DE POSIÇÃO
// =============================================================================

static float meshObterPosX(int id) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return 0.0f;
    return it->second.posicao.x;
}

static float meshObterPosY(int id) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return 0.0f;
    return it->second.posicao.y;
}

static float meshObterPosZ(int id) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return 0.0f;
    return it->second.posicao.z;
}

static float meshObterRotY(int id) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return 0.0f;
    return it->second.rotacao.y;
}

// =============================================================================
// RENDER DE TODAS AS MESHES DE UMA JANELA
// =============================================================================

static void renderizarMeshes(int janelaId, const Mat4& view, const Mat4& projection) {
    int shaderId = obterShaderPadrao();
    if (shaderId == 0) return;

    SML3DShader* s = obterShader(shaderId);
    if (!s) return;

    glUseProgram(s->program);

    // View e Projection (mesmas pra todas as meshes)
    shaderSetMat4(s, s->loc_view, view.m);
    shaderSetMat4(s, s->loc_projection, projection.m);

    // Luz padrão (direcional vindo de cima-direita-frente)
    shaderSetVec3(s, s->loc_lightDir, -0.3f, -1.0f, -0.5f);
    shaderSetVec3(s, s->loc_lightColor, 1.0f, 1.0f, 1.0f);
    shaderSetFloat(s, s->loc_ambient, 0.25f);

    for (auto& [id, mesh] : g_meshes) {
        if (mesh.janelaId != janelaId || !mesh.visivel) continue;

        // Meshes animadas são renderizadas pelo shader skinned
        if (g_meshes_animadas.count(id)) continue;

        Mat4 model = (mesh.parentId > 0) ? calcularModelComHierarquia(id) : mesh.calcularModel();
        shaderSetMat4(s, s->loc_model, model.m);
        shaderSetVec3(s, s->loc_color, mesh.cor_r, mesh.cor_g, mesh.cor_b);

        // Textura: bind se a mesh tem uma, senão desativa
        if (mesh.texturaGL != 0) {
            shaderSetInt(s, s->loc_useTexture, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.texturaGL);
            shaderSetInt(s, s->loc_textureSampler, 0);
            shaderSetVec2(s, s->loc_texTile, mesh.tex_tile_x, mesh.tex_tile_y);
        } else {
            shaderSetInt(s, s->loc_useTexture, 0);
        }

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirMesh(int id) {
    auto it = g_meshes.find(id);
    if (it == g_meshes.end()) return;

    SML3DMesh& mesh = it->second;
    if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
    if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
    if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    g_meshes.erase(it);
}

static void destruirMeshesJanela(int janelaId) {
    auto it = g_meshes.begin();
    while (it != g_meshes.end()) {
        if (it->second.janelaId == janelaId) {
            SML3DMesh& mesh = it->second;
            if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
            if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
            if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
            it = g_meshes.erase(it);
        } else {
            ++it;
        }
    }
}