// model.hpp
// Carregamento de modelos 3D (formato OBJ e FBX) para engine sml3d

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>

#include "mesh.hpp"
#include "ufbx.h"

// =============================================================================
// HELPER: EXTRAIR DIRETÓRIO DE UM CAMINHO DE ARQUIVO
// Retorna tudo até a última '/' ou '\', ou "" se não tiver separador.
// =============================================================================

static std::string extrairDiretorio(const char* caminho) {
    std::string path(caminho);
    size_t pos_slash = path.rfind('/');
    size_t pos_bslash = path.rfind('\\');

    size_t pos = std::string::npos;
    if (pos_slash != std::string::npos && pos_bslash != std::string::npos) {
        pos = (pos_slash > pos_bslash) ? pos_slash : pos_bslash;
    } else if (pos_slash != std::string::npos) {
        pos = pos_slash;
    } else if (pos_bslash != std::string::npos) {
        pos = pos_bslash;
    }

    if (pos == std::string::npos) return "";
    return path.substr(0, pos + 1);  // Inclui a barra
}

// =============================================================================
// HELPER: EXTRAIR MATERIAL DO FBX E APLICAR NA MESH
// Pega o primeiro material da primeira mesh do FBX.
// Extrai: diffuse color (base_color) e textura diffuse (se houver).
// caminhoFBX: caminho do arquivo FBX (pra resolver caminhos relativos de textura)
// =============================================================================

static void aplicarMaterialFBX(ufbx_scene* cena, int meshId, const char* caminhoFBX) {
    if (!cena) return;

    auto itMesh = g_meshes.find(meshId);
    if (itMesh == g_meshes.end()) return;

    SML3DMesh& mesh = itMesh->second;

    // Procurar o primeiro material nas meshes da cena
    ufbx_material* material = nullptr;

    for (size_t mi = 0; mi < cena->meshes.count && !material; mi++) {
        ufbx_mesh* fbx_mesh = cena->meshes.data[mi];
        if (fbx_mesh->materials.count > 0) {
            material = fbx_mesh->materials.data[0];
        }
    }

    // Fallback: pegar de materials globais da cena
    if (!material && cena->materials.count > 0) {
        material = cena->materials.data[0];
    }

    if (!material) return;

    // =========================================================================
    // 1. EXTRAIR COR DIFFUSE (base_color do PBR, ou diffuse_color do legacy)
    // =========================================================================

    bool cor_encontrada = false;

    // Tentar PBR base_color primeiro
    if (material->pbr.base_color.has_value) {
        ufbx_vec4 cor = material->pbr.base_color.value_vec4;
        mesh.cor_r = (float)cor.x;
        mesh.cor_g = (float)cor.y;
        mesh.cor_b = (float)cor.z;
        cor_encontrada = true;
    }
    // Fallback: diffuse_color do FBX legacy
    else if (material->fbx.diffuse_color.has_value) {
        ufbx_vec4 cor = material->fbx.diffuse_color.value_vec4;
        mesh.cor_r = (float)cor.x;
        mesh.cor_g = (float)cor.y;
        mesh.cor_b = (float)cor.z;
        cor_encontrada = true;
    }

    if (cor_encontrada) {
        fprintf(stdout, "[sml3d] Material cor: (%.2f, %.2f, %.2f)\n",
                mesh.cor_r, mesh.cor_g, mesh.cor_b);
    }

    // =========================================================================
    // 2. EXTRAIR TEXTURA DIFFUSE
    // =========================================================================

    ufbx_texture* textura = nullptr;

    // Tentar PBR base_color texture
    if (material->pbr.base_color.texture) {
        textura = material->pbr.base_color.texture;
    }
    // Fallback: diffuse_color texture do FBX legacy
    else if (material->fbx.diffuse_color.texture) {
        textura = material->fbx.diffuse_color.texture;
    }

    if (textura && textura->filename.length > 0) {
        // Resolver caminho da textura relativo ao FBX
        std::string dir_fbx = extrairDiretorio(caminhoFBX);

        // O ufbx pode dar caminho absoluto ou relativo
        std::string tex_filename(textura->filename.data, textura->filename.length);

        // Tentar relative_filename primeiro (mais confiável)
        if (textura->relative_filename.length > 0) {
            tex_filename = std::string(textura->relative_filename.data,
                                       textura->relative_filename.length);
        }

        // Substituir backslashes por forward slashes
        for (char& c : tex_filename) {
            if (c == '\\') c = '/';
        }

        // Se é caminho relativo, resolver em relação ao diretório do FBX
        std::string caminho_final;
        if (tex_filename.size() > 1 && (tex_filename[0] == '/' ||
            (tex_filename.size() > 2 && tex_filename[1] == ':'))) {
            // Caminho absoluto — usar direto
            caminho_final = tex_filename;
        } else {
            // Caminho relativo — concatenar com diretório do FBX
            caminho_final = dir_fbx + tex_filename;
        }

        // Tentar carregar a textura
        int texId = carregarTextura(caminho_final.c_str());
        if (texId > 0) {
            aplicarTexturaMesh(meshId, texId);
            fprintf(stdout, "[sml3d] Material textura: %s\n", caminho_final.c_str());
        } else {
            // Tentar só o nome do arquivo (sem path) no diretório do FBX
            size_t last_sep = tex_filename.rfind('/');
            if (last_sep != std::string::npos) {
                std::string nome_arquivo = tex_filename.substr(last_sep + 1);
                std::string tentativa = dir_fbx + nome_arquivo;
                texId = carregarTextura(tentativa.c_str());
                if (texId > 0) {
                    aplicarTexturaMesh(meshId, texId);
                    fprintf(stdout, "[sml3d] Material textura (fallback): %s\n", tentativa.c_str());
                } else {
                    fprintf(stderr, "[sml3d] Textura não encontrada: %s\n", caminho_final.c_str());
                }
            } else {
                fprintf(stderr, "[sml3d] Textura não encontrada: %s\n", caminho_final.c_str());
            }
        }
    }
}

// =============================================================================
// PARSER OBJ
// Suporta:
//   - Vértices (v), normais (vn), UVs (vt)
//   - Faces: f v/vt/vn, f v//vn, f v/vt, f v
//   - Triângulos e quads (quads são triangulados automaticamente)
//   - Modelos sem UVs ou sem normais
// =============================================================================

// Estrutura temporária pra um índice de face OBJ
struct OBJVertIdx {
    int v  = -1;   // Índice do vértice (obrigatório)
    int vt = -1;   // Índice da UV (opcional)
    int vn = -1;   // Índice da normal (opcional)
};

// Parsear um token de face: "v", "v/vt", "v//vn", "v/vt/vn"
static OBJVertIdx parsearIndiceOBJ(const char* token) {
    OBJVertIdx idx;

    // Formato: v
    // Formato: v/vt
    // Formato: v/vt/vn
    // Formato: v//vn

    char buf[64];
    strncpy(buf, token, 63);
    buf[63] = '\0';

    // Primeiro número: v
    char* p = buf;
    idx.v = atoi(p) - 1;  // OBJ é 1-indexed

    // Procurar primeira '/'
    char* slash1 = strchr(p, '/');
    if (!slash1) return idx;  // Só tem v

    slash1++;  // Pular a '/'

    // Procurar segunda '/'
    char* slash2 = strchr(slash1, '/');

    if (slash2) {
        // Tem duas barras: v/vt/vn ou v//vn
        if (slash2 == slash1) {
            // v//vn — sem vt
            idx.vn = atoi(slash2 + 1) - 1;
        } else {
            // v/vt/vn
            idx.vt = atoi(slash1) - 1;
            idx.vn = atoi(slash2 + 1) - 1;
        }
    } else {
        // Uma barra só: v/vt
        idx.vt = atoi(slash1) - 1;
    }

    return idx;
}

// =============================================================================
// CARREGAR OBJ
// Lê o arquivo, monta os buffers no formato da engine (pos:3 + normal:3 + uv:2)
// e cria a mesh via uploadMesh.
// =============================================================================

static int carregarOBJ(int janelaId, const char* caminho) {
    if (!caminho || caminho[0] == '\0') return 0;

    FILE* arquivo = fopen(caminho, "r");
    if (!arquivo) {
        fprintf(stderr, "[sml3d] Falha ao abrir modelo: %s\n", caminho);
        return 0;
    }

    // Dados brutos do OBJ
    std::vector<float> obj_v;    // Posições (x,y,z)
    std::vector<float> obj_vn;   // Normais (x,y,z)
    std::vector<float> obj_vt;   // UVs (u,v)

    // Dados finais pra GPU
    std::vector<float> vertices;         // pos:3 + normal:3 + uv:2 por vértice
    std::vector<unsigned int> indices;

    // Cache pra evitar vértices duplicados
    // Chave: "v/vt/vn" → índice no buffer final
    std::unordered_map<std::string, unsigned int> cache_vertices;

    char linha[512];
    while (fgets(linha, sizeof(linha), arquivo)) {
        // Remover \r\n
        char* fim = linha + strlen(linha) - 1;
        while (fim >= linha && (*fim == '\r' || *fim == '\n')) *fim-- = '\0';

        // Vértice: v x y z
        if (linha[0] == 'v' && linha[1] == ' ') {
            float x, y, z;
            if (sscanf(linha + 2, "%f %f %f", &x, &y, &z) == 3) {
                obj_v.push_back(x);
                obj_v.push_back(y);
                obj_v.push_back(z);
            }
        }
        // Normal: vn x y z
        else if (linha[0] == 'v' && linha[1] == 'n' && linha[2] == ' ') {
            float x, y, z;
            if (sscanf(linha + 3, "%f %f %f", &x, &y, &z) == 3) {
                obj_vn.push_back(x);
                obj_vn.push_back(y);
                obj_vn.push_back(z);
            }
        }
        // UV: vt u v
        else if (linha[0] == 'v' && linha[1] == 't' && linha[2] == ' ') {
            float u, v;
            if (sscanf(linha + 3, "%f %f", &u, &v) >= 2) {
                obj_vt.push_back(u);
                obj_vt.push_back(v);
            }
        }
        // Face: f ...
        else if (linha[0] == 'f' && linha[1] == ' ') {
            // Tokenizar os índices da face
            std::vector<OBJVertIdx> face_idx;
            char* token = strtok(linha + 2, " \t");
            while (token) {
                if (token[0] >= '0' && token[0] <= '9') {
                    face_idx.push_back(parsearIndiceOBJ(token));
                }
                token = strtok(nullptr, " \t");
            }

            if (face_idx.size() < 3) continue;

            // Converter cada vértice da face pro formato final
            std::vector<unsigned int> face_final_idx;
            for (auto& fi : face_idx) {
                // Criar chave pra cache
                char chave[64];
                snprintf(chave, sizeof(chave), "%d/%d/%d", fi.v, fi.vt, fi.vn);
                std::string key(chave);

                auto itCache = cache_vertices.find(key);
                if (itCache != cache_vertices.end()) {
                    face_final_idx.push_back(itCache->second);
                } else {
                    // Montar vértice novo
                    float px = 0, py = 0, pz = 0;
                    float nx = 0, ny = 1, nz = 0;
                    float tu = 0, tv = 0;

                    // Posição
                    if (fi.v >= 0 && fi.v * 3 + 2 < (int)obj_v.size()) {
                        px = obj_v[fi.v * 3 + 0];
                        py = obj_v[fi.v * 3 + 1];
                        pz = obj_v[fi.v * 3 + 2];
                    }

                    // Normal
                    if (fi.vn >= 0 && fi.vn * 3 + 2 < (int)obj_vn.size()) {
                        nx = obj_vn[fi.vn * 3 + 0];
                        ny = obj_vn[fi.vn * 3 + 1];
                        nz = obj_vn[fi.vn * 3 + 2];
                    }

                    // UV
                    if (fi.vt >= 0 && fi.vt * 2 + 1 < (int)obj_vt.size()) {
                        tu = obj_vt[fi.vt * 2 + 0];
                        tv = obj_vt[fi.vt * 2 + 1];
                    }

                    unsigned int novo_idx = (unsigned int)(vertices.size() / 8);
                    vertices.push_back(px);
                    vertices.push_back(py);
                    vertices.push_back(pz);
                    vertices.push_back(nx);
                    vertices.push_back(ny);
                    vertices.push_back(nz);
                    vertices.push_back(tu);
                    vertices.push_back(tv);

                    cache_vertices[key] = novo_idx;
                    face_final_idx.push_back(novo_idx);
                }
            }

            // Triangular: fan a partir do primeiro vértice
            // Funciona pra triângulos (3), quads (4) e polígonos convexos
            for (size_t i = 1; i + 1 < face_final_idx.size(); i++) {
                indices.push_back(face_final_idx[0]);
                indices.push_back(face_final_idx[i]);
                indices.push_back(face_final_idx[i + 1]);
            }
        }
    }

    fclose(arquivo);

    if (vertices.empty() || indices.empty()) {
        fprintf(stderr, "[sml3d] Modelo vazio ou inválido: %s\n", caminho);
        return 0;
    }

    // Calcular bounds reais a partir dos vértices (stride 8: pos:3 + normal:3 + uv:2)
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

    // Criar mesh na engine
    int id = g_prox_mesh_id++;
    SML3DMesh& mesh = g_meshes[id];
    mesh.tipo = SML3D_CUBO;  // Tipo genérico (não afeta renderização)
    mesh.janelaId = janelaId;

    // Setar bounds reais do modelo
    mesh.bounds_min = {bmin_x, bmin_y, bmin_z};
    mesh.bounds_max = {bmax_x, bmax_y, bmax_z};

    uploadMesh(mesh, vertices, indices);

    int numTris = (int)indices.size() / 3;
    int numVerts = (int)(vertices.size() / 8);
    fprintf(stdout, "[sml3d] Modelo carregado: %s (%d verts, %d tris, bounds [%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f])\n",
            caminho, numVerts, numTris, bmin_x, bmin_y, bmin_z, bmax_x, bmax_y, bmax_z);

    return id;
}

// =============================================================================
// CARREGAR FBX VIA UFBX
// Carrega a primeira mesh encontrada no arquivo FBX.
// Extrai posições, normais e UVs, triangula automaticamente.
// =============================================================================

static int carregarFBX(int janelaId, const char* caminho) {
    if (!caminho || caminho[0] == '\0') return 0;

    // Carregar o arquivo FBX
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error erro;
    ufbx_scene* cena = ufbx_load_file(caminho, &opts, &erro);
    if (!cena) {
        fprintf(stderr, "[sml3d] Falha ao carregar FBX: %s\n", caminho);
        fprintf(stderr, "[sml3d] Erro ufbx: %s\n", erro.description.data);
        return 0;
    }

    // Dados finais pra GPU
    std::vector<float> vertices;         // pos:3 + normal:3 + uv:2
    std::vector<unsigned int> indices;

    // Iterar todas as meshes da cena
    for (size_t mi = 0; mi < cena->meshes.count; mi++) {
        ufbx_mesh* fbx_mesh = cena->meshes.data[mi];

        // Offset dos vértices já adicionados (pra múltiplas meshes)
        unsigned int base_vertex = (unsigned int)(vertices.size() / 8);

        // Iterar cada face
        for (size_t fi = 0; fi < fbx_mesh->faces.count; fi++) {
            ufbx_face face = fbx_mesh->faces.data[fi];

            // Triangular a face (fan triangulation)
            for (uint32_t ti = 1; ti + 1 < face.num_indices; ti++) {
                // Três índices do triângulo
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

                    indices.push_back((unsigned int)(vertices.size() / 8) - 1);
                }
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        fprintf(stderr, "[sml3d] FBX vazio ou sem meshes: %s\n", caminho);
        ufbx_free_scene(cena);
        return 0;
    }

    // Calcular bounds reais
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

    // Criar mesh na engine
    int id = g_prox_mesh_id++;
    SML3DMesh& mesh = g_meshes[id];
    mesh.tipo = SML3D_CUBO;
    mesh.janelaId = janelaId;
    mesh.bounds_min = {bmin_x, bmin_y, bmin_z};
    mesh.bounds_max = {bmax_x, bmax_y, bmax_z};

    uploadMesh(mesh, vertices, indices);

    int numTris = (int)indices.size() / 3;
    int numVerts = (int)(vertices.size() / 8);
    fprintf(stdout, "[sml3d] FBX carregado: %s (%d verts, %d tris, bounds [%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f])\n",
            caminho, numVerts, numTris, bmin_x, bmin_y, bmin_z, bmax_x, bmax_y, bmax_z);

    // Extrair material (cor + textura) do FBX e aplicar na mesh
    aplicarMaterialFBX(cena, id, caminho);

    ufbx_free_scene(cena);

    return id;
}

// =============================================================================
// CARREGAR MODELO — detecta formato pela extensão (.obj ou .fbx)
// =============================================================================

// Forward declaration — implementada em animation.hpp
static int carregarFBXAnimado(int janelaId, const char* caminho);

static int carregarModelo(int janelaId, const char* caminho) {
    if (!caminho || caminho[0] == '\0') return 0;

    // Detectar extensão
    const char* ponto = strrchr(caminho, '.');
    if (!ponto) {
        fprintf(stderr, "[sml3d] Extensão não reconhecida: %s\n", caminho);
        return 0;
    }

    // Comparar extensão (case-insensitive)
    char ext[8] = {};
    for (int i = 0; i < 7 && ponto[i + 1]; i++) {
        char c = ponto[i + 1];
        ext[i] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
    }

    if (strcmp(ext, "fbx") == 0) {
        // Tentar carregar como animado primeiro (se tiver skeleton)
        int animId = carregarFBXAnimado(janelaId, caminho);
        if (animId > 0) return animId;
        // Fallback: carregar como estático
        return carregarFBX(janelaId, caminho);
    } else if (strcmp(ext, "obj") == 0) {
        return carregarOBJ(janelaId, caminho);
    }

    fprintf(stderr, "[sml3d] Formato não suportado: .%s (%s)\n", ext, caminho);
    return 0;
}