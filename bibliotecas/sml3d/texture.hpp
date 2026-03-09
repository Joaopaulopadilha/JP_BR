// texture.hpp
// Sistema de texturas (carregamento PNG/JPG via stb_image, cache, bind por mesh) para engine sml3d

#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "glad/glad.h"
#include "mesh.hpp"

// stb_image — implementação incluída no .cpp principal
// Aqui só precisamos das declarações
#ifndef STBI_INCLUDE_STB_IMAGE_H
extern unsigned char* stbi_load(const char*, int*, int*, int*, int);
extern void stbi_image_free(void*);
extern void stbi_set_flip_vertically_on_load(int);
#endif

// =============================================================================
// ESTRUTURA DA TEXTURA
// =============================================================================

struct SML3DTexture {
    GLuint gl_id = 0;       // ID OpenGL da textura
    int largura = 0;
    int altura = 0;
    int canais = 0;
    std::string caminho;    // Caminho do arquivo (pra cache)
};

// =============================================================================
// ESTADO GLOBAL DE TEXTURAS
// =============================================================================

static std::unordered_map<int, SML3DTexture> g_texturas;
static int g_prox_textura_id = 1;

// Cache: caminho do arquivo → ID da textura (evita recarregar a mesma imagem)
static std::unordered_map<std::string, int> g_textura_cache;

// =============================================================================
// CARREGAR TEXTURA DO DISCO
// Retorna o ID da textura, ou 0 se falhar.
// Se o mesmo caminho já foi carregado, retorna o ID do cache.
// =============================================================================

static int carregarTextura(const char* caminho) {
    if (!caminho || caminho[0] == '\0') return 0;

    std::string path(caminho);

    // Verificar cache
    auto itCache = g_textura_cache.find(path);
    if (itCache != g_textura_cache.end()) {
        // Verificar se a textura ainda existe (pode ter sido destruída)
        auto itTex = g_texturas.find(itCache->second);
        if (itTex != g_texturas.end()) {
            return itCache->second;
        }
        // Cache inválido, remover
        g_textura_cache.erase(itCache);
    }

    // Carregar imagem com stb_image
    stbi_set_flip_vertically_on_load(1);  // OpenGL espera Y invertido

    int largura, altura, canais;
    unsigned char* dados = stbi_load(caminho, &largura, &altura, &canais, 0);
    if (!dados) {
        fprintf(stderr, "[sml3d] Falha ao carregar textura: %s\n", caminho);
        return 0;
    }

    // Determinar formato OpenGL
    GLenum formato_interno, formato;
    if (canais == 4) {
        formato_interno = GL_RGBA8;
        formato = GL_RGBA;
    } else if (canais == 3) {
        formato_interno = GL_RGB8;
        formato = GL_RGB;
    } else if (canais == 1) {
        formato_interno = GL_R8;
        formato = GL_RED;
    } else {
        fprintf(stderr, "[sml3d] Formato de imagem não suportado (%d canais): %s\n", canais, caminho);
        stbi_image_free(dados);
        return 0;
    }

    // Criar textura OpenGL
    GLuint gl_id;
    glGenTextures(1, &gl_id);
    glBindTexture(GL_TEXTURE_2D, gl_id);

    // Parâmetros de filtragem e wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload dos pixels
    glTexImage2D(GL_TEXTURE_2D, 0, formato_interno, largura, altura, 0,
                 formato, GL_UNSIGNED_BYTE, dados);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Liberar dados da CPU
    stbi_image_free(dados);

    // Registrar
    int id = g_prox_textura_id++;
    SML3DTexture& tex = g_texturas[id];
    tex.gl_id = gl_id;
    tex.largura = largura;
    tex.altura = altura;
    tex.canais = canais;
    tex.caminho = path;

    // Adicionar ao cache
    g_textura_cache[path] = id;

    fprintf(stdout, "[sml3d] Textura carregada: %s (%dx%d, %dch, id=%d)\n",
            caminho, largura, altura, canais, id);

    return id;
}

// =============================================================================
// APLICAR TEXTURA A UMA MESH
// Seta o campo texturaGL da mesh com o GL ID da textura
// =============================================================================

static bool aplicarTexturaMesh(int meshId, int texturaId) {
    auto itMesh = g_meshes.find(meshId);
    if (itMesh == g_meshes.end()) return false;

    auto itTex = g_texturas.find(texturaId);
    if (itTex == g_texturas.end()) return false;

    itMesh->second.texturaGL = itTex->second.gl_id;
    return true;
}

// =============================================================================
// CARREGAR E APLICAR EM UM PASSO
// Conveniência: carrega a textura (ou pega do cache) e aplica na mesh
// =============================================================================

static bool texturizarMesh(int meshId, const char* caminho) {
    int texId = carregarTextura(caminho);
    if (texId == 0) return false;
    return aplicarTexturaMesh(meshId, texId);
}

// =============================================================================
// REMOVER TEXTURA DE UMA MESH (volta pra cor sólida)
// =============================================================================

static bool removerTexturaMesh(int meshId) {
    auto itMesh = g_meshes.find(meshId);
    if (itMesh == g_meshes.end()) return false;
    itMesh->second.texturaGL = 0;
    return true;
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirTextura(int id) {
    auto it = g_texturas.find(id);
    if (it == g_texturas.end()) return;

    GLuint gl_id = it->second.gl_id;

    // Remover do cache
    if (!it->second.caminho.empty()) {
        g_textura_cache.erase(it->second.caminho);
    }

    // Limpar referência em todas as meshes que usam esta textura
    for (auto& [mid, mesh] : g_meshes) {
        if (mesh.texturaGL == gl_id) {
            mesh.texturaGL = 0;
        }
    }

    // Deletar na GPU
    if (gl_id) {
        glDeleteTextures(1, &gl_id);
    }

    g_texturas.erase(it);
}

// Destruir todas as texturas (encerramento)
static void destruirTodasTexturas() {
    for (auto& [id, tex] : g_texturas) {
        if (tex.gl_id) {
            glDeleteTextures(1, &tex.gl_id);
        }
    }
    g_texturas.clear();
    g_textura_cache.clear();
    g_prox_textura_id = 1;

    // Limpar referências nas meshes
    for (auto& [mid, mesh] : g_meshes) {
        mesh.texturaGL = 0;
    }
}