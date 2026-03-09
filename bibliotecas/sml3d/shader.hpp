// shader.hpp
// Compilação e gerenciamento de shaders OpenGL 4.6 para engine sml3d

#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "glad/glad.h"

// =============================================================================
// SHADER PADRÃO EMBUTIDO
// =============================================================================

// Vertex shader: recebe posição + normal, aplica model/view/projection
static const char* SHADER_VERTEX_PADRAO = R"glsl(
#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = u_model * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(u_model))) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = u_projection * u_view * worldPos;
}
)glsl";

// Fragment shader: cor sólida com iluminação direcional simples
static const char* SHADER_FRAGMENT_PADRAO = R"glsl(
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
    // Cor base: textura ou cor sólida
    vec3 baseColor;
    if (u_useTexture == 1) {
        vec2 tiledUV = vTexCoord * u_texTile;
        vec4 texColor = texture(u_texture, tiledUV);
        baseColor = texColor.rgb * u_color;
    } else {
        baseColor = u_color;
    }

    // Ambient
    vec3 ambient = u_ambient * u_lightColor;

    // Diffuse
    vec3 norm = normalize(vNormal);
    vec3 lightD = normalize(-u_lightDir);
    float diff = max(dot(norm, lightD), 0.0);
    vec3 diffuse = diff * u_lightColor;

    vec3 result = (ambient + diffuse) * baseColor;
    FragColor = vec4(result, 1.0);
}
)glsl";

// =============================================================================
// ESTRUTURA DO SHADER
// =============================================================================

struct SML3DShader {
    GLuint program = 0;
    bool valido = false;

    // Locations cacheados das uniforms do shader padrão
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
};

// =============================================================================
// ESTADO GLOBAL DE SHADERS
// =============================================================================

static std::unordered_map<int, SML3DShader> g_shaders;
static int g_prox_shader_id = 1;
static int g_shader_padrao_id = 0; // ID do shader padrão, criado automaticamente

// =============================================================================
// COMPILAÇÃO
// =============================================================================

static GLuint compilarShaderGL(GLenum tipo, const char* fonte) {
    GLuint shader = glCreateShader(tipo);
    glShaderSource(shader, 1, &fonte, nullptr);
    glCompileShader(shader);

    GLint sucesso;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &sucesso);
    if (!sucesso) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        const char* nome_tipo = (tipo == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        fprintf(stderr, "[sml3d] Erro compilação %s:\n%s\n", nome_tipo, log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static int criarShader(const char* vertex_src, const char* fragment_src) {
    GLuint vert = compilarShaderGL(GL_VERTEX_SHADER, vertex_src);
    if (!vert) return 0;

    GLuint frag = compilarShaderGL(GL_FRAGMENT_SHADER, fragment_src);
    if (!frag) {
        glDeleteShader(vert);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    // Shaders compilados podem ser deletados após linkagem
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint sucesso;
    glGetProgramiv(program, GL_LINK_STATUS, &sucesso);
    if (!sucesso) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        fprintf(stderr, "[sml3d] Erro linkagem shader:\n%s\n", log);
        glDeleteProgram(program);
        return 0;
    }

    int id = g_prox_shader_id++;
    SML3DShader& s = g_shaders[id];
    s.program = program;
    s.valido = true;

    // Cache das uniform locations
    s.loc_model      = glGetUniformLocation(program, "u_model");
    s.loc_view       = glGetUniformLocation(program, "u_view");
    s.loc_projection = glGetUniformLocation(program, "u_projection");
    s.loc_color      = glGetUniformLocation(program, "u_color");
    s.loc_lightDir   = glGetUniformLocation(program, "u_lightDir");
    s.loc_lightColor = glGetUniformLocation(program, "u_lightColor");
    s.loc_ambient    = glGetUniformLocation(program, "u_ambient");
    s.loc_useTexture = glGetUniformLocation(program, "u_useTexture");
    s.loc_textureSampler = glGetUniformLocation(program, "u_texture");
    s.loc_texTile = glGetUniformLocation(program, "u_texTile");

    return id;
}

// =============================================================================
// SHADER PADRÃO — criado automaticamente na primeira chamada
// =============================================================================

static int obterShaderPadrao() {
    if (g_shader_padrao_id != 0) return g_shader_padrao_id;

    g_shader_padrao_id = criarShader(SHADER_VERTEX_PADRAO, SHADER_FRAGMENT_PADRAO);
    if (g_shader_padrao_id == 0) {
        fprintf(stderr, "[sml3d] Falha ao criar shader padrão!\n");
    } else {
        fprintf(stdout, "[sml3d] Shader padrão criado (id=%d)\n", g_shader_padrao_id);
    }

    return g_shader_padrao_id;
}

// =============================================================================
// USO
// =============================================================================

static bool usarShader(int id) {
    auto it = g_shaders.find(id);
    if (it == g_shaders.end() || !it->second.valido) return false;

    glUseProgram(it->second.program);
    return true;
}

static SML3DShader* obterShader(int id) {
    auto it = g_shaders.find(id);
    if (it == g_shaders.end()) return nullptr;
    return &it->second;
}

// =============================================================================
// HELPERS PARA SETAR UNIFORMS
// =============================================================================

static void shaderSetMat4(SML3DShader* s, GLint loc, const float* mat) {
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
}

static void shaderSetVec3(SML3DShader* s, GLint loc, float x, float y, float z) {
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

static void shaderSetFloat(SML3DShader* s, GLint loc, float v) {
    if (loc >= 0) glUniform1f(loc, v);
}

static void shaderSetInt(SML3DShader* s, GLint loc, int v) {
    if (loc >= 0) glUniform1i(loc, v);
}

static void shaderSetVec2(SML3DShader* s, GLint loc, float x, float y) {
    if (loc >= 0) glUniform2f(loc, x, y);
}

// =============================================================================
// LIMPEZA
// =============================================================================

static void destruirShader(int id) {
    auto it = g_shaders.find(id);
    if (it == g_shaders.end()) return;

    if (it->second.program) {
        glDeleteProgram(it->second.program);
    }
    g_shaders.erase(it);

    if (id == g_shader_padrao_id) {
        g_shader_padrao_id = 0;
    }
}

static void destruirTodosShaders() {
    for (auto& [id, s] : g_shaders) {
        if (s.program) glDeleteProgram(s.program);
    }
    g_shaders.clear();
    g_shader_padrao_id = 0;
}