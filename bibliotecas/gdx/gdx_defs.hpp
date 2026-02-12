// gdx_defs.hpp
// Definições base para GDX - Biblioteca Gráfica JPLang com Win32/X11 + OpenGL + ImGui

#ifndef GDX_DEFS_HPP
#define GDX_DEFS_HPP

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <algorithm>

// =============================================================================
// PLATAFORMA E MACROS
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #ifndef UNICODE
    #define UNICODE
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <GL/gl.h>
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/Xatom.h>
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <unistd.h>
#endif

// =============================================================================
// IMGUI
// =============================================================================
#include "src/imgui.h"

// =============================================================================
// TIPOS JPLANG
// =============================================================================
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
    } valor;
} JPValor;

// =============================================================================
// HELPERS GLOBAIS
// =============================================================================

inline JPValor jp_bool(bool b) {
    JPValor v; 
    v.tipo = JP_TIPO_BOOL; 
    v.valor.booleano = b ? 1 : 0; 
    return v;
}

inline JPValor jp_int(int64_t i) {
    JPValor v; 
    v.tipo = JP_TIPO_INT; 
    v.valor.inteiro = i; 
    return v;
}

inline std::string get_string(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return "";
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) 
        return std::string(args[idx].valor.texto);
    return "";
}

inline int64_t get_int(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return 0;
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int64_t)args[idx].valor.decimal;
    return 0;
}

// =============================================================================
// HELPERS DE COR IMGUI
// =============================================================================

inline ImVec4 rgb_para_imvec4(int r, int g, int b, int a = 255) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline ImU32 rgb_para_imu32(int r, int g, int b, int a = 255) {
    return IM_COL32(r, g, b, a);
}

// =============================================================================
// HELPER UTF-8 PARA WINDOWS
// =============================================================================
#if JP_WINDOWS
inline std::wstring utf8_to_utf16(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
#endif

#endif