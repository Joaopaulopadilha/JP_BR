// animation.hpp
// Sistema de animação skeletal (bones, skinning, playback) para engine sml3d
// Usa ufbx para ler skeleton e animações de arquivos FBX do Mixamo.
// Suporta múltiplas animações por modelo, blending, loop e controle de velocidade.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>

#include "glad/glad.h"
#include "mesh.hpp"
#include "ufbx.h"

// =============================================================================
// LIMITES
// =============================================================================

#define SML3D_MAX_BONES          128   // Máximo de bones por modelo
#define SML3D_MAX_BONE_INFLUENCE 4     // Pesos por vértice (padrão GPU skinning)

// =============================================================================
// SHADER DE SKINNING — substitui o shader padrão quando a mesh tem animação
// =============================================================================

static const char* SHADER_VERTEX_SKINNED = R"glsl(
#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in ivec4 aBoneIds;
layout(location = 4) in vec4 aBoneWeights;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_bones[128];
uniform int u_animated;

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vTexCoord;

void main() {
    vec4 pos = vec4(aPos, 1.0);
    vec4 norm = vec4(aNormal, 0.0);

    if (u_animated == 1) {
        mat4 skinMatrix = mat4(0.0);
        for (int i = 0; i < 4; i++) {
            if (aBoneIds[i] >= 0 && aBoneWeights[i] > 0.0) {
                skinMatrix += u_bones[aBoneIds[i]] * aBoneWeights[i];
            }
        }
        // Se nenhum bone afetou, usar identidade
        float totalWeight = aBoneWeights.x + aBoneWeights.y + aBoneWeights.z + aBoneWeights.w;
        if (totalWeight < 0.001) {
            skinMatrix = mat4(1.0);
        }
        pos = skinMatrix * pos;
        norm = skinMatrix * norm;
    }

    vec4 worldPos = u_model * pos;
    vFragPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(u_model))) * norm.xyz;
    vTexCoord = aTexCoord;
    gl_Position = u_projection * u_view * worldPos;
}
)glsl";

// Fragment shader é o mesmo do padrão (iluminação + textura)
static const char* SHADER_FRAGMENT_SKINNED = R"glsl(
#version 460 core

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vTexCoord;

uniform vec3 u_color;
uniform vec3 u_lightDir;
uniform vec3 u_lightColor;
uniform float u_ambient;
uniform int u_useTexture;
uniform sampler2D u_texture;
uniform vec2 u_texTile;

out vec4 FragColor;

void main() {
    vec3 baseColor;
    if (u_useTexture == 1) {
        vec2 tiledUV = vTexCoord * u_texTile;
        vec4 texColor = texture(u_texture, tiledUV);
        baseColor = texColor.rgb * u_color;
    } else {
        baseColor = u_color;
    }

    vec3 ambient = u_ambient * u_lightColor;
    vec3 norm = normalize(vNormal);
    vec3 lightD = normalize(-u_lightDir);
    float diff = max(dot(norm, lightD), 0.0);
    vec3 diffuse = diff * u_lightColor;

    vec3 result = (ambient + diffuse) * baseColor;
    FragColor = vec4(result, 1.0);
}
)glsl";

// =============================================================================
// ESTRUTURA DO SHADER SKINNED (locations extras)
// =============================================================================

struct SML3DShaderSkinned {
    GLuint program = 0;
    bool valido = false;

    // Locations herdados do shader padrão
    GLint loc_model = -1;
    GLint loc_view = -1;
    GLint loc_projection = -1;
    GLint loc_color = -1;
    GLint loc_lightDir = -1;
    GLint loc_lightColor = -1;
    GLint loc_ambient = -1;
    GLint loc_useTexture = -1;
    GLint loc_textureSampler = -1;
    GLint loc_texTile = -1;

    // Locations de skinning
    GLint loc_bones[SML3D_MAX_BONES];
    GLint loc_animated = -1;
};

static SML3DShaderSkinned g_shader_skinned;
static bool g_shader_skinned_criado = false;

// =============================================================================
// COMPILAR SHADER SKINNED
// =============================================================================

static bool criarShaderSkinned() {
    if (g_shader_skinned_criado) return g_shader_skinned.valido;

    g_shader_skinned_criado = true;

    // Compilar vertex
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &SHADER_VERTEX_SKINNED, nullptr);
    glCompileShader(vert);
    GLint sucesso;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &sucesso);
    if (!sucesso) {
        char log[1024];
        glGetShaderInfoLog(vert, sizeof(log), nullptr, log);
        fprintf(stderr, "[sml3d] Erro compilação VERTEX skinned:\n%s\n", log);
        glDeleteShader(vert);
        return false;
    }

    // Compilar fragment
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &SHADER_FRAGMENT_SKINNED, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &sucesso);
    if (!sucesso) {
        char log[1024];
        glGetShaderInfoLog(frag, sizeof(log), nullptr, log);
        fprintf(stderr, "[sml3d] Erro compilação FRAGMENT skinned:\n%s\n", log);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return false;
    }

    // Linkar
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGetProgramiv(program, GL_LINK_STATUS, &sucesso);
    if (!sucesso) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        fprintf(stderr, "[sml3d] Erro linkagem shader skinned:\n%s\n", log);
        glDeleteProgram(program);
        return false;
    }

    g_shader_skinned.program = program;
    g_shader_skinned.valido = true;

    // Cache locations padrão
    g_shader_skinned.loc_model      = glGetUniformLocation(program, "u_model");
    g_shader_skinned.loc_view       = glGetUniformLocation(program, "u_view");
    g_shader_skinned.loc_projection = glGetUniformLocation(program, "u_projection");
    g_shader_skinned.loc_color      = glGetUniformLocation(program, "u_color");
    g_shader_skinned.loc_lightDir   = glGetUniformLocation(program, "u_lightDir");
    g_shader_skinned.loc_lightColor = glGetUniformLocation(program, "u_lightColor");
    g_shader_skinned.loc_ambient    = glGetUniformLocation(program, "u_ambient");
    g_shader_skinned.loc_useTexture = glGetUniformLocation(program, "u_useTexture");
    g_shader_skinned.loc_textureSampler = glGetUniformLocation(program, "u_texture");
    g_shader_skinned.loc_texTile    = glGetUniformLocation(program, "u_texTile");
    g_shader_skinned.loc_animated   = glGetUniformLocation(program, "u_animated");

    // Cache locations dos bones
    for (int i = 0; i < SML3D_MAX_BONES; i++) {
        char nome[32];
        snprintf(nome, sizeof(nome), "u_bones[%d]", i);
        g_shader_skinned.loc_bones[i] = glGetUniformLocation(program, nome);
    }

    fprintf(stdout, "[sml3d] Shader skinned criado (program=%u)\n", program);
    return true;
}

// =============================================================================
// ESTRUTURA DE ANIMAÇÃO POR MESH
// =============================================================================

struct SML3DBoneInfo {
    int parent_index;                      // -1 = raiz
    ufbx_matrix bind_pose_inverse;         // Inversa da bind pose (local → bone space)
};

struct SML3DAnimClip {
    std::string nome;
    float duracao;           // Em segundos
    double time_begin;       // Tempo inicial no anim stack (pra offset)
    ufbx_anim* anim;        // Ponteiro pra animação no ufbx
    ufbx_scene* cena;       // Cena dona desta animação (pra evaluate)
};

struct SML3DAnimState {
    int clip_atual = 0;          // Índice do clip ativo (A)
    float tempo = 0.0f;         // Posição atual na animação (segundos)
    float velocidade = 1.0f;    // Multiplicador de velocidade
    bool loop = true;            // Repetir?
    bool tocando = true;         // Está rodando?

    // Blending: mistura clip A com clip B
    int clip_blend = -1;         // Clip B (-1 = sem blend)
    float tempo_blend = 0.0f;   // Tempo do clip B
    float blend_fator = 0.0f;   // 0.0 = 100% A, 1.0 = 100% B
    bool loop_blend = true;      // Loop do clip B
    float velocidade_blend = 1.0f;

    // Matrizes finais dos bones (pra enviar pro shader)
    std::vector<float> bone_matrices;  // num_bones * 16 floats (mat4 column-major)
};

struct SML3DAnimatedMesh {
    int mesh_id;                                 // ID da mesh na engine

    // Cenas ufbx (mantidas vivas pra avaliar animações)
    // A primeira é a cena principal (com mesh + skeleton)
    // As demais são cenas extras carregadas com sm_anim_load
    std::vector<ufbx_scene*> cenas;

    int num_bones = 0;
    std::vector<SML3DBoneInfo> bones;            // Info de cada bone
    std::vector<ufbx_node*> bone_nodes;          // Nodes dos bones no ufbx

    int root_bone_index = -1;                    // Índice do bone raiz (pra root motion)
    bool remove_root_motion = true;              // Zerar deslocamento XZ do root bone

    std::vector<SML3DAnimClip> clips;            // Animações disponíveis
    SML3DAnimState state;                        // Estado da animação

    // VAO/VBO separados (com bone ids/weights)
    GLuint VAO = 0, VBO = 0, EBO = 0;
    int numIndices = 0;
};

// =============================================================================
// ESTADO GLOBAL DE ANIMAÇÕES
// =============================================================================

static std::unordered_map<int, SML3DAnimatedMesh> g_animated_meshes;  // mesh_id → animação

// =============================================================================
// HELPER: CONVERTER UFBX MATRIX → FLOAT[16] COLUMN-MAJOR (OpenGL)
// ufbx_matrix é 4x3 (row-major, sem projeção). Precisamos expandir pra 4x4.
// =============================================================================

static void ufbxMatToGL(const ufbx_matrix& src, float* dst) {
    // ufbx_matrix: m00..m02, m10..m12, m20..m22, m03, m13, m23
    // São 3 rows de 3 colunas + 1 coluna de translação
    // Layout: rows = [X_axis, Y_axis, Z_axis], translação = [m03, m13, m23]

    // OpenGL column-major 4x4:
    // col0: X_axis.x, X_axis.y, X_axis.z, 0
    // col1: Y_axis.x, Y_axis.y, Y_axis.z, 0
    // col2: Z_axis.x, Z_axis.y, Z_axis.z, 0
    // col3: trans.x,  trans.y,  trans.z,   1

    // ufbx: rows[0] = (m00, m01, m02) = primeira LINHA
    // Mas o ufbx_matrix usa cols[0..2] como colunas e cols[3] como translação
    // ufbx_matrix.cols[0] = primeira coluna (eixo X)
    // ufbx_matrix.cols[1] = segunda coluna (eixo Y)
    // ufbx_matrix.cols[2] = terceira coluna (eixo Z)
    // ufbx_matrix.cols[3] = translação

    dst[0]  = (float)src.cols[0].x;
    dst[1]  = (float)src.cols[0].y;
    dst[2]  = (float)src.cols[0].z;
    dst[3]  = 0.0f;

    dst[4]  = (float)src.cols[1].x;
    dst[5]  = (float)src.cols[1].y;
    dst[6]  = (float)src.cols[1].z;
    dst[7]  = 0.0f;

    dst[8]  = (float)src.cols[2].x;
    dst[9]  = (float)src.cols[2].y;
    dst[10] = (float)src.cols[2].z;
    dst[11] = 0.0f;

    dst[12] = (float)src.cols[3].x;
    dst[13] = (float)src.cols[3].y;
    dst[14] = (float)src.cols[3].z;
    dst[15] = 1.0f;
}

// =============================================================================
// HELPER: MULTIPLICAR DUAS MATRIZES 4x4 (column-major)
// =============================================================================

static void mulMat4x4(const float* a, const float* b, float* out) {
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            out[c * 4 + r] =
                a[0 * 4 + r] * b[c * 4 + 0] +
                a[1 * 4 + r] * b[c * 4 + 1] +
                a[2 * 4 + r] * b[c * 4 + 2] +
                a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

// =============================================================================
// UPLOAD DE GEOMETRIA SKINNED (com bone IDs e weights)
// Formato por vértice: pos:3 + normal:3 + uv:2 + boneIds:4(int) + weights:4(float)
// =============================================================================

static void uploadMeshSkinned(SML3DAnimatedMesh& anim,
                              const std::vector<float>& vertices,
                              const std::vector<int>& bone_ids,
                              const std::vector<float>& bone_weights,
                              const std::vector<unsigned int>& indices) {
    int num_verts = (int)(vertices.size() / 8);

    // Montar buffer intercalado:
    // pos:3 + normal:3 + uv:2 + boneIds:4(como float) + weights:4
    // Total: 16 floats por vértice
    // Nota: bone IDs vão como int no shader (ivec4), mas podemos mandar como float
    // e converter, OU usar dois VBOs separados. Vamos usar buffer separado pra ints.

    glGenVertexArrays(1, &anim.VAO);
    glGenBuffers(1, &anim.VBO);
    glGenBuffers(1, &anim.EBO);

    glBindVertexArray(anim.VAO);

    // Buffer principal: pos + normal + uv (8 floats por vértice)
    glBindBuffer(GL_ARRAY_BUFFER, anim.VBO);

    // Montar buffer completo: 8 floats + 4 ints + 4 floats = stride variável
    // Mais simples: usar offsets no mesmo buffer com cast
    // Pra manter simples, vamos usar dois VBOs: um pra geometria, outro pra bones

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    int stride_geom = 8 * sizeof(float);

    // layout(location=0) posição
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride_geom, (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1) normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride_geom, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location=2) UV
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride_geom, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // VBO pra bone IDs (integers)
    GLuint boneIdVBO;
    glGenBuffers(1, &boneIdVBO);
    glBindBuffer(GL_ARRAY_BUFFER, boneIdVBO);
    glBufferData(GL_ARRAY_BUFFER, bone_ids.size() * sizeof(int), bone_ids.data(), GL_STATIC_DRAW);

    // layout(location=3) bone IDs (ivec4)
    glVertexAttribIPointer(3, 4, GL_INT, 4 * sizeof(int), (void*)0);
    glEnableVertexAttribArray(3);

    // VBO pra bone weights (floats)
    GLuint boneWeightVBO;
    glGenBuffers(1, &boneWeightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, boneWeightVBO);
    glBufferData(GL_ARRAY_BUFFER, bone_weights.size() * sizeof(float), bone_weights.data(), GL_STATIC_DRAW);

    // layout(location=4) bone weights (vec4)
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(4);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, anim.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    anim.numIndices = (int)indices.size();
}

// =============================================================================
// CARREGAR FBX ANIMADO
// Extrai geometria + skeleton + animações. Retorna mesh_id ou 0.
// =============================================================================

static int carregarFBXAnimado(int janelaId, const char* caminho) {
    if (!caminho || caminho[0] == '\0') return 0;

    // Carregar com ufbx — manter a cena viva pra avaliar animações
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error erro;
    ufbx_scene* cena = ufbx_load_file(caminho, &opts, &erro);
    if (!cena) {
        fprintf(stderr, "[sml3d] Falha ao carregar FBX animado: %s\n", caminho);
        fprintf(stderr, "[sml3d] Erro ufbx: %s\n", erro.description.data);
        return 0;
    }

    // =========================================================================
    // 1. ENCONTRAR SKIN DEFORMERS E MAPEAR BONES
    // =========================================================================

    // Coletar todos os skin clusters de todas as meshes
    // Cada cluster tem um bone (node) e a lista de vértices afetados

    // Mapa: bone node → índice no nosso array de bones
    std::unordered_map<ufbx_node*, int> bone_map;
    std::vector<ufbx_node*> bone_nodes;
    std::vector<ufbx_matrix> bind_pose_inverses;

    // Iterar meshes da cena e seus skin deformers
    for (size_t mi = 0; mi < cena->meshes.count; mi++) {
        ufbx_mesh* fbx_mesh = cena->meshes.data[mi];

        for (size_t si = 0; si < fbx_mesh->skin_deformers.count; si++) {
            ufbx_skin_deformer* skin = fbx_mesh->skin_deformers.data[si];

            for (size_t ci = 0; ci < skin->clusters.count; ci++) {
                ufbx_skin_cluster* cluster = skin->clusters.data[ci];
                ufbx_node* bone_node = cluster->bone_node;

                if (!bone_node) continue;
                if (bone_map.find(bone_node) != bone_map.end()) continue;

                int idx = (int)bone_nodes.size();
                if (idx >= SML3D_MAX_BONES) {
                    fprintf(stderr, "[sml3d] Aviso: modelo excede %d bones, extras ignorados\n", SML3D_MAX_BONES);
                    break;
                }

                bone_map[bone_node] = idx;
                bone_nodes.push_back(bone_node);
                bind_pose_inverses.push_back(cluster->geometry_to_bone);
            }
        }
    }

    int num_bones = (int)bone_nodes.size();

    if (num_bones == 0) {
        fprintf(stderr, "[sml3d] FBX sem skeleton/bones: %s — carregando como estático\n", caminho);
        ufbx_free_scene(cena);
        // Fallback: carregar como modelo estático normal
        return 0;  // Sinaliza pra usar carregarFBX normal
    }

    fprintf(stdout, "[sml3d] Skeleton: %d bones encontrados\n", num_bones);

    // Montar info dos bones (parent index)
    std::vector<SML3DBoneInfo> bones_info(num_bones);
    int root_bone_idx = -1;
    for (int i = 0; i < num_bones; i++) {
        bones_info[i].bind_pose_inverse = bind_pose_inverses[i];

        // Encontrar parent
        ufbx_node* parent = bone_nodes[i]->parent;
        auto itParent = bone_map.find(parent);
        bones_info[i].parent_index = (itParent != bone_map.end()) ? itParent->second : -1;

        // Root bone = primeiro bone sem parent no skeleton
        if (bones_info[i].parent_index == -1 && root_bone_idx == -1) {
            root_bone_idx = i;
        }
    }

    if (root_bone_idx >= 0) {
        fprintf(stdout, "[sml3d] Root bone: '%s' (idx=%d)\n",
                bone_nodes[root_bone_idx]->name.data, root_bone_idx);
    }

    // =========================================================================
    // 2. EXTRAIR GEOMETRIA COM BONE IDS E WEIGHTS
    // =========================================================================

    std::vector<float> vertices;          // pos:3 + normal:3 + uv:2
    std::vector<int> bone_ids_buf;        // 4 ints por vértice
    std::vector<float> bone_weights_buf;  // 4 floats por vértice
    std::vector<unsigned int> indices;

    for (size_t mi = 0; mi < cena->meshes.count; mi++) {
        ufbx_mesh* fbx_mesh = cena->meshes.data[mi];

        // Skin deformer desta mesh (pode não ter)
        ufbx_skin_deformer* skin = nullptr;
        if (fbx_mesh->skin_deformers.count > 0) {
            skin = fbx_mesh->skin_deformers.data[0];
        }

        for (size_t fi = 0; fi < fbx_mesh->faces.count; fi++) {
            ufbx_face face = fbx_mesh->faces.data[fi];

            for (uint32_t ti = 1; ti + 1 < face.num_indices; ti++) {
                uint32_t idx0 = face.index_begin;
                uint32_t idx1 = face.index_begin + ti;
                uint32_t idx2 = face.index_begin + ti + 1;

                uint32_t tri_indices[3] = { idx0, idx1, idx2 };

                for (int vi = 0; vi < 3; vi++) {
                    uint32_t idx = tri_indices[vi];

                    // Posição
                    ufbx_vec3 pos = ufbx_get_vertex_vec3(&fbx_mesh->vertex_position, idx);

                    // Normal
                    ufbx_vec3 norm = {0, 1, 0};
                    if (fbx_mesh->vertex_normal.exists) {
                        norm = ufbx_get_vertex_vec3(&fbx_mesh->vertex_normal, idx);
                    }

                    // UV
                    float tu = 0.0f, tv = 0.0f;
                    if (fbx_mesh->vertex_uv.exists) {
                        ufbx_vec2 uv = ufbx_get_vertex_vec2(&fbx_mesh->vertex_uv, idx);
                        tu = (float)uv.x;
                        tv = (float)uv.y;
                    }

                    vertices.push_back((float)pos.x);
                    vertices.push_back((float)pos.y);
                    vertices.push_back((float)pos.z);
                    vertices.push_back((float)norm.x);
                    vertices.push_back((float)norm.y);
                    vertices.push_back((float)norm.z);
                    vertices.push_back(tu);
                    vertices.push_back(tv);

                    // Bone IDs e Weights pra este vértice
                    int b_ids[SML3D_MAX_BONE_INFLUENCE] = {0, 0, 0, 0};
                    float b_weights[SML3D_MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};

                    if (skin) {
                        // O índice do vértice no mesh original
                        uint32_t vert_index = fbx_mesh->vertex_indices.data[idx];

                        // ufbx_get_skin_vertex_matrix não dá os pesos individuais.
                        // Precisamos iterar os clusters manualmente.
                        int count = 0;

                        for (size_t ci = 0; ci < skin->clusters.count && count < SML3D_MAX_BONE_INFLUENCE; ci++) {
                            ufbx_skin_cluster* cluster = skin->clusters.data[ci];

                            // Buscar o peso deste vértice neste cluster
                            // Os vértices do cluster são indexados por vert_index
                            for (size_t wi = 0; wi < cluster->num_weights; wi++) {
                                if (cluster->vertices.data[wi] == vert_index) {
                                    ufbx_node* bone_node = cluster->bone_node;
                                    auto itBone = bone_map.find(bone_node);
                                    if (itBone != bone_map.end()) {
                                        b_ids[count] = itBone->second;
                                        b_weights[count] = (float)cluster->weights.data[wi];
                                        count++;
                                    }
                                    break;
                                }
                            }
                        }

                        // Normalizar pesos
                        float total = b_weights[0] + b_weights[1] + b_weights[2] + b_weights[3];
                        if (total > 0.0001f) {
                            for (int k = 0; k < SML3D_MAX_BONE_INFLUENCE; k++) {
                                b_weights[k] /= total;
                            }
                        }
                    }

                    for (int k = 0; k < SML3D_MAX_BONE_INFLUENCE; k++) {
                        bone_ids_buf.push_back(b_ids[k]);
                        bone_weights_buf.push_back(b_weights[k]);
                    }

                    indices.push_back((unsigned int)(vertices.size() / 8) - 1);
                }
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        fprintf(stderr, "[sml3d] FBX animado vazio: %s\n", caminho);
        ufbx_free_scene(cena);
        return 0;
    }

    // =========================================================================
    // 3. CALCULAR BOUNDS
    // =========================================================================

    float bmin_x = vertices[0], bmin_y = vertices[1], bmin_z = vertices[2];
    float bmax_x = bmin_x, bmax_y = bmin_y, bmax_z = bmin_z;

    for (size_t i = 0; i < vertices.size(); i += 8) {
        float vx = vertices[i + 0];
        float vy = vertices[i + 1];
        float vz = vertices[i + 2];
        if (vx < bmin_x) bmin_x = vx;
        if (vy < bmin_y) bmin_y = vy;
        if (vz < bmin_z) bmin_z = vz;
        if (vx > bmax_x) bmax_x = vx;
        if (vy > bmax_y) bmax_y = vy;
        if (vz > bmax_z) bmax_z = vz;
    }

    // =========================================================================
    // 4. CRIAR MESH NA ENGINE
    // =========================================================================

    int mesh_id = g_prox_mesh_id++;
    SML3DMesh& mesh = g_meshes[mesh_id];
    mesh.tipo = SML3D_CUBO;  // Genérico
    mesh.janelaId = janelaId;
    mesh.bounds_min = {bmin_x, bmin_y, bmin_z};
    mesh.bounds_max = {bmax_x, bmax_y, bmax_z};

    // NÃO fazer uploadMesh normal — vamos usar o VAO skinned
    // Mas precisamos de um VAO válido no mesh pra renderização.
    // Vamos setar o VAO/VBO/EBO do animated mesh no mesh da engine.

    // =========================================================================
    // 5. CRIAR ANIMATED MESH E FAZER UPLOAD
    // =========================================================================

    // Garantir que o shader skinned existe
    criarShaderSkinned();

    SML3DAnimatedMesh& anim = g_animated_meshes[mesh_id];
    anim.mesh_id = mesh_id;
    anim.cenas.push_back(cena);  // Cena principal (índice 0)
    anim.num_bones = num_bones;
    anim.bones = bones_info;
    anim.bone_nodes = bone_nodes;
    anim.root_bone_index = root_bone_idx;
    anim.remove_root_motion = true;  // Por padrão, zera deslocamento XZ

    // Registrar como animada (pra render padrão pular esta mesh)
    g_meshes_animadas.insert(mesh_id);

    uploadMeshSkinned(anim, vertices, bone_ids_buf, bone_weights_buf, indices);

    // Setar o VAO/numIndices no mesh da engine (pra rendering)
    mesh.VAO = anim.VAO;
    mesh.VBO = anim.VBO;
    mesh.EBO = anim.EBO;
    mesh.numIndices = anim.numIndices;

    // Inicializar matrizes de bones como identidade
    anim.state.bone_matrices.resize(num_bones * 16, 0.0f);
    for (int i = 0; i < num_bones; i++) {
        anim.state.bone_matrices[i * 16 + 0] = 1.0f;
        anim.state.bone_matrices[i * 16 + 5] = 1.0f;
        anim.state.bone_matrices[i * 16 + 10] = 1.0f;
        anim.state.bone_matrices[i * 16 + 15] = 1.0f;
    }

    // =========================================================================
    // 6. EXTRAIR ANIMAÇÕES
    // =========================================================================

    for (size_t ai = 0; ai < cena->anim_stacks.count; ai++) {
        ufbx_anim_stack* stack = cena->anim_stacks.data[ai];

        SML3DAnimClip clip;
        clip.nome = std::string(stack->name.data, stack->name.length);
        clip.time_begin = stack->time_begin;
        clip.duracao = (float)(stack->time_end - stack->time_begin);
        clip.anim = stack->anim;
        clip.cena = cena;  // Clip pertence à cena principal

        if (clip.duracao < 0.001f) clip.duracao = 1.0f;

        anim.clips.push_back(clip);

        fprintf(stdout, "[sml3d] Animação [%d] '%s': %.2fs\n",
                (int)anim.clips.size() - 1, clip.nome.c_str(), clip.duracao);
    }

    if (anim.clips.empty()) {
        fprintf(stderr, "[sml3d] Aviso: FBX sem animações: %s\n", caminho);
    }

    int numTris = (int)indices.size() / 3;
    int numVerts = (int)(vertices.size() / 8);
    fprintf(stdout, "[sml3d] FBX animado carregado: %s (%d verts, %d tris, %d bones, %d anims)\n",
            caminho, numVerts, numTris, num_bones, (int)anim.clips.size());

    // Extrair material (cor + textura) do FBX e aplicar na mesh
    aplicarMaterialFBX(cena, mesh_id, caminho);

    return mesh_id;
}

// =============================================================================
// AVALIAR UM CLIP — retorna matrizes num buffer
// =============================================================================

static bool avaliarClip(SML3DAnimatedMesh& anim, SML3DAnimClip& clip, double tempo,
                        std::vector<float>& out_matrices) {
    if (!clip.anim) return false;

    double t = clip.time_begin + tempo;

    ufbx_evaluate_opts eval_opts = {};
    ufbx_scene* cena_avaliada = ufbx_evaluate_scene(clip.cena, clip.anim, t, &eval_opts, nullptr);
    if (!cena_avaliada) return false;

    bool cena_externa = (clip.cena != anim.cenas[0]);

    // Root motion removal
    double root_offset_x = 0.0;
    double root_offset_z = 0.0;

    if (anim.remove_root_motion && anim.root_bone_index >= 0) {
        ufbx_node* root_orig = anim.bone_nodes[anim.root_bone_index];

        ufbx_node* root_eval = nullptr;
        if (cena_externa) {
            root_eval = ufbx_find_node(cena_avaliada, root_orig->name.data);
        } else {
            ufbx_element* root_elem = cena_avaliada->elements.data[root_orig->element.element_id];
            root_eval = (ufbx_node*)root_elem;
        }

        if (root_eval) {
            root_offset_x = root_eval->node_to_world.cols[3].x - root_orig->node_to_world.cols[3].x;
            root_offset_z = root_eval->node_to_world.cols[3].z - root_orig->node_to_world.cols[3].z;
        }
    }

    out_matrices.resize(anim.num_bones * 16);

    for (int i = 0; i < anim.num_bones; i++) {
        ufbx_node* original_node = anim.bone_nodes[i];

        ufbx_node* eval_node = nullptr;
        if (cena_externa) {
            eval_node = ufbx_find_node(cena_avaliada, original_node->name.data);
        } else {
            ufbx_element* elem = cena_avaliada->elements.data[original_node->element.element_id];
            eval_node = (ufbx_node*)elem;
        }

        if (!eval_node) {
            float* dst = &out_matrices[i * 16];
            memset(dst, 0, 16 * sizeof(float));
            dst[0] = dst[5] = dst[10] = dst[15] = 1.0f;
            continue;
        }

        ufbx_matrix world = eval_node->node_to_world;

        if (anim.remove_root_motion && anim.root_bone_index >= 0) {
            world.cols[3].x -= root_offset_x;
            world.cols[3].z -= root_offset_z;
        }

        ufbx_matrix final_mat = ufbx_matrix_mul(&world, &anim.bones[i].bind_pose_inverse);
        ufbxMatToGL(final_mat, &out_matrices[i * 16]);
    }

    ufbx_free_scene(cena_avaliada);
    return true;
}

// =============================================================================
// LERP DE MATRIZES 4x4 (component-wise, suficiente pra blend suave)
// =============================================================================

static void lerpMat4(const float* a, const float* b, float t, float* out) {
    for (int i = 0; i < 16; i++) {
        out[i] = a[i] + (b[i] - a[i]) * t;
    }
}

// =============================================================================
// AVALIAR ANIMAÇÃO — com suporte a blending entre dois clips
// =============================================================================

static void avaliarAnimacao(SML3DAnimatedMesh& anim) {
    if (anim.clips.empty()) return;
    if (!anim.state.tocando) return;

    int clipA = anim.state.clip_atual;
    if (clipA < 0 || clipA >= (int)anim.clips.size()) return;

    // Avaliar clip A
    std::vector<float> matrices_a;
    if (!avaliarClip(anim, anim.clips[clipA], (double)anim.state.tempo, matrices_a)) return;

    int clipB = anim.state.clip_blend;
    float fator = anim.state.blend_fator;

    // Se tem blend ativo, avaliar clip B e misturar
    if (clipB >= 0 && clipB < (int)anim.clips.size() && fator > 0.001f) {
        std::vector<float> matrices_b;
        if (avaliarClip(anim, anim.clips[clipB], (double)anim.state.tempo_blend, matrices_b)) {
            // Lerp entre A e B
            for (int i = 0; i < anim.num_bones; i++) {
                lerpMat4(&matrices_a[i * 16], &matrices_b[i * 16], fator,
                         &anim.state.bone_matrices[i * 16]);
            }
            return;
        }
    }

    // Sem blend — usar A direto
    memcpy(anim.state.bone_matrices.data(), matrices_a.data(),
           anim.num_bones * 16 * sizeof(float));
}

// =============================================================================
// ATUALIZAR ANIMAÇÕES — chamar todo frame (antes do render)
// =============================================================================

static void atualizarAnimacoes(float deltaTime) {
    for (auto& [id, anim] : g_animated_meshes) {
        if (anim.clips.empty()) continue;

        // Se está tocando, avançar tempo
        if (anim.state.tocando) {
            // Avançar tempo do clip A
            anim.state.tempo += deltaTime * anim.state.velocidade;

            SML3DAnimClip& clip = anim.clips[anim.state.clip_atual];

            float duracao_loop = clip.duracao;
            if (anim.state.loop && duracao_loop > 0.04f) {
                duracao_loop -= (1.0f / 30.0f);
            }

            if (anim.state.tempo >= duracao_loop) {
                if (anim.state.loop) {
                    anim.state.tempo = fmodf(anim.state.tempo, duracao_loop);
                } else {
                    // Animação acabou — travar no último frame
                    anim.state.tempo = clip.duracao - (1.0f / 30.0f);
                    anim.state.tocando = false;
                }
            }

            // Avançar tempo do clip B (blend)
            int clipB = anim.state.clip_blend;
            if (clipB >= 0 && clipB < (int)anim.clips.size() && anim.state.blend_fator > 0.001f) {
                anim.state.tempo_blend += deltaTime * anim.state.velocidade_blend;

                SML3DAnimClip& clipb = anim.clips[clipB];
                float duracao_b = clipb.duracao;
                if (anim.state.loop_blend && duracao_b > 0.04f) {
                    duracao_b -= (1.0f / 30.0f);
                }

                if (anim.state.tempo_blend >= duracao_b) {
                    if (anim.state.loop_blend) {
                        anim.state.tempo_blend = fmodf(anim.state.tempo_blend, duracao_b);
                    } else {
                        anim.state.tempo_blend = clipb.duracao;
                    }
                }
            }
        }

        // Sempre avaliar bones (mesmo se parado — mantém último frame)
        avaliarAnimacao(anim);
    }
}

// =============================================================================
// RENDERIZAR MESHES ANIMADAS — substitui o render normal pra meshes com bones
// Chamar no lugar de (ou junto com) renderizarMeshes
// =============================================================================

static void renderizarMeshesAnimadas(int janelaId, const Mat4& view, const Mat4& projection) {
    if (g_animated_meshes.empty()) return;
    if (!g_shader_skinned.valido) return;

    glUseProgram(g_shader_skinned.program);

    // View e Projection
    glUniformMatrix4fv(g_shader_skinned.loc_view, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(g_shader_skinned.loc_projection, 1, GL_FALSE, projection.m);

    // Luz (mesma do shader padrão)
    glUniform3f(g_shader_skinned.loc_lightDir, -0.3f, -1.0f, -0.5f);
    glUniform3f(g_shader_skinned.loc_lightColor, 1.0f, 1.0f, 1.0f);
    glUniform1f(g_shader_skinned.loc_ambient, 0.25f);

    for (auto& [id, anim] : g_animated_meshes) {
        auto itMesh = g_meshes.find(id);
        if (itMesh == g_meshes.end()) continue;

        SML3DMesh& mesh = itMesh->second;
        if (mesh.janelaId != janelaId || !mesh.visivel) continue;

        // Model matrix
        Mat4 model = mesh.calcularModel();
        glUniformMatrix4fv(g_shader_skinned.loc_model, 1, GL_FALSE, model.m);

        // Cor
        glUniform3f(g_shader_skinned.loc_color, mesh.cor_r, mesh.cor_g, mesh.cor_b);

        // Textura
        if (mesh.texturaGL != 0) {
            glUniform1i(g_shader_skinned.loc_useTexture, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.texturaGL);
            glUniform1i(g_shader_skinned.loc_textureSampler, 0);
            glUniform2f(g_shader_skinned.loc_texTile, mesh.tex_tile_x, mesh.tex_tile_y);
        } else {
            glUniform1i(g_shader_skinned.loc_useTexture, 0);
        }

        // Enviar matrizes dos bones
        glUniform1i(g_shader_skinned.loc_animated, 1);
        for (int bi = 0; bi < anim.num_bones && bi < SML3D_MAX_BONES; bi++) {
            glUniformMatrix4fv(g_shader_skinned.loc_bones[bi], 1, GL_FALSE,
                               &anim.state.bone_matrices[bi * 16]);
        }

        // Renderizar com o VAO skinned
        glBindVertexArray(anim.VAO);
        glDrawElements(GL_TRIANGLES, anim.numIndices, GL_UNSIGNED_INT, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// =============================================================================
// API: CARREGAR ANIMAÇÃO DE OUTRO FBX E ADICIONAR COMO CLIP
// O FBX deve ter o mesmo skeleton (mesmo personagem do Mixamo).
// Só extrai a animação, ignora o mesh.
// Retorna o índice do clip adicionado, ou -1 se falhar.
// =============================================================================

static int animCarregarClip(int meshId, const char* caminho) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) {
        fprintf(stderr, "[sml3d] sm_anim_load: mesh %d não é animada\n", meshId);
        return -1;
    }

    SML3DAnimatedMesh& anim = it->second;

    // Carregar o FBX externo
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error erro;
    ufbx_scene* cena_extra = ufbx_load_file(caminho, &opts, &erro);
    if (!cena_extra) {
        fprintf(stderr, "[sml3d] sm_anim_load: falha ao carregar %s\n", caminho);
        fprintf(stderr, "[sml3d] Erro ufbx: %s\n", erro.description.data);
        return -1;
    }

    // Extrair animações da cena extra
    int primeiro_clip = (int)anim.clips.size();
    int adicionados = 0;

    for (size_t ai = 0; ai < cena_extra->anim_stacks.count; ai++) {
        ufbx_anim_stack* stack = cena_extra->anim_stacks.data[ai];

        SML3DAnimClip clip;
        clip.nome = std::string(stack->name.data, stack->name.length);
        clip.time_begin = stack->time_begin;
        clip.duracao = (float)(stack->time_end - stack->time_begin);
        clip.anim = stack->anim;
        clip.cena = cena_extra;  // Pertence à cena extra

        if (clip.duracao < 0.001f) clip.duracao = 1.0f;

        anim.clips.push_back(clip);
        adicionados++;

        fprintf(stdout, "[sml3d] Animação carregada [%d] '%s': %.2fs (%s)\n",
                (int)anim.clips.size() - 1, clip.nome.c_str(), clip.duracao, caminho);
    }

    if (adicionados == 0) {
        fprintf(stderr, "[sml3d] sm_anim_load: nenhuma animação em %s\n", caminho);
        ufbx_free_scene(cena_extra);
        return -1;
    }

    // Manter a cena viva (os clips referenciam ela)
    anim.cenas.push_back(cena_extra);

    return primeiro_clip;
}

// =============================================================================
// API: VERIFICAR SE MESH É ANIMADA
// =============================================================================

static bool meshEhAnimada(int meshId) {
    return g_animated_meshes.find(meshId) != g_animated_meshes.end();
}

// =============================================================================
// API: CONTROLE DE ANIMAÇÃO
// =============================================================================

// Setar clip de animação por índice (0, 1, 2...)
static bool animSetClip(int meshId, int clipIndex) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;

    SML3DAnimatedMesh& anim = it->second;
    if (clipIndex < 0 || clipIndex >= (int)anim.clips.size()) return false;

    anim.state.clip_atual = clipIndex;
    anim.state.tempo = 0.0f;
    anim.state.tocando = true;
    return true;
}

// Play/pause
static bool animPlay(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;
    it->second.state.tocando = true;
    return true;
}

static bool animPause(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;
    it->second.state.tocando = false;
    return true;
}

// Velocidade da animação (1.0 = normal, 2.0 = dobro, 0.5 = metade)
static bool animVelocidade(int meshId, float vel) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;
    it->second.state.velocidade = vel;
    return true;
}

// Loop
static bool animLoop(int meshId, bool loop) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;
    it->second.state.loop = loop;
    return true;
}

// Resetar animação (volta pro início)
static bool animReset(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;
    it->second.state.tempo = 0.0f;
    it->second.state.tocando = true;
    return true;
}

// Blend: misturar clip atual (A) com outro clip (B)
// clipB: índice do clip pra misturar (-1 pra desligar blend)
// fator: 0.0 = 100% A, 1.0 = 100% B
static bool animBlend(int meshId, int clipB, float fator) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;

    SML3DAnimatedMesh& anim = it->second;

    if (clipB < 0) {
        // Desligar blend
        anim.state.clip_blend = -1;
        anim.state.blend_fator = 0.0f;
        return true;
    }

    if (clipB >= (int)anim.clips.size()) return false;

    // Se mudou o clip B, resetar o tempo dele
    if (anim.state.clip_blend != clipB) {
        anim.state.tempo_blend = 0.0f;
    }

    anim.state.clip_blend = clipB;
    anim.state.blend_fator = fator;
    if (anim.state.blend_fator < 0.0f) anim.state.blend_fator = 0.0f;
    if (anim.state.blend_fator > 1.0f) anim.state.blend_fator = 1.0f;
    anim.state.loop_blend = true;
    anim.state.velocidade_blend = anim.state.velocidade;
    return true;
}

// Obter número de clips
static int animNumClips(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return 0;
    return (int)it->second.clips.size();
}

// Obter duração do clip atual
static float animDuracao(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return 0.0f;
    SML3DAnimatedMesh& anim = it->second;
    if (anim.clips.empty()) return 0.0f;
    return anim.clips[anim.state.clip_atual].duracao;
}

// Obter tempo atual
static float animTempo(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return 0.0f;
    return it->second.state.tempo;
}

// Ativar/desativar remoção de root motion (por padrão ativo)
// Quando ativo, o personagem anda no lugar — movimento controlado pelo jogo
// Quando desativado, a animação move o personagem diretamente
static bool animRootMotion(int meshId, bool remover) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return false;
    it->second.remove_root_motion = remover;
    return true;
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirAnimacao(int meshId) {
    auto it = g_animated_meshes.find(meshId);
    if (it == g_animated_meshes.end()) return;

    SML3DAnimatedMesh& anim = it->second;

    // Liberar todas as cenas ufbx (principal + extras)
    for (ufbx_scene* c : anim.cenas) {
        if (c) ufbx_free_scene(c);
    }
    anim.cenas.clear();

    // Remover do set de meshes animadas
    g_meshes_animadas.erase(meshId);

    // VAO/VBO/EBO são os mesmos do mesh (limpos pelo destruirMesh)
    // Não deletar aqui pra não double-free

    g_animated_meshes.erase(it);
}

static void destruirAnimacoesJanela(int janelaId) {
    auto it = g_animated_meshes.begin();
    while (it != g_animated_meshes.end()) {
        auto meshIt = g_meshes.find(it->first);
        if (meshIt != g_meshes.end() && meshIt->second.janelaId == janelaId) {
            for (ufbx_scene* c : it->second.cenas) {
                if (c) ufbx_free_scene(c);
            }
            it->second.cenas.clear();
            g_meshes_animadas.erase(it->first);
            it = g_animated_meshes.erase(it);
        } else {
            ++it;
        }
    }
}

static void destruirShaderSkinned() {
    if (g_shader_skinned.program) {
        glDeleteProgram(g_shader_skinned.program);
        g_shader_skinned.program = 0;
    }
    g_shader_skinned.valido = false;
    g_shader_skinned_criado = false;
}