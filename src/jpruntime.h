// jpruntime.h
// Header C puro para runtime JPLang com TCC
// Define tipos e estruturas para comunicação entre executável (TCC) e DLLs (C++)

#ifndef JPRUNTIME_H
#define JPRUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #define JP_EXPORT __declspec(dllexport)
    #define JP_IMPORT __declspec(dllimport)
#else
    #define JP_EXPORT __attribute__((visibility("default")))
    #define JP_IMPORT
    #include <dlfcn.h>
    #include <dirent.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// === TIPOS DE VALOR ===
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4,
    JP_TIPO_OBJETO = 5,
    JP_TIPO_LISTA = 6,
    JP_TIPO_PONTEIRO = 7
} JPTipo;

// === VALOR VARIANT (C puro) ===
typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
        void* lista;
        void* ponteiro;
    } valor;
} JPValor;

// === ASSINATURA DE FUNÇÃO NATIVA ===
typedef JPValor (*JPFuncaoNativa)(JPValor* args, int numArgs);

// === HELPERS PARA CRIAR VALORES ===
static inline JPValor jp_nulo(void) {
    JPValor v;
    v.tipo = JP_TIPO_NULO;
    v.valor.inteiro = 0;
    return v;
}

static inline JPValor jp_int(int64_t i) {
    JPValor v;
    v.tipo = JP_TIPO_INT;
    v.valor.inteiro = i;
    return v;
}

static inline JPValor jp_double(double d) {
    JPValor v;
    v.tipo = JP_TIPO_DOUBLE;
    v.valor.decimal = d;
    return v;
}

static inline JPValor jp_bool(int b) {
    JPValor v;
    v.tipo = JP_TIPO_BOOL;
    v.valor.booleano = b ? 1 : 0;
    return v;
}

static inline JPValor jp_string(const char* s) {
    JPValor v;
    v.tipo = JP_TIPO_STRING;
    if (s) {
        size_t len = strlen(s);
        v.valor.texto = (char*)malloc(len + 1);
        if (v.valor.texto) {
            memcpy(v.valor.texto, s, len + 1);
        }
    } else {
        v.valor.texto = NULL;
    }
    return v;
}

static inline JPValor jp_string_nocopy(char* s) {
    JPValor v;
    v.tipo = JP_TIPO_STRING;
    v.valor.texto = s;
    return v;
}

static inline JPValor jp_ponteiro(void* p) {
    JPValor v;
    v.tipo = JP_TIPO_PONTEIRO;
    v.valor.ponteiro = p;
    return v;
}

static inline JPValor jp_objeto(void* obj) {
    JPValor v;
    v.tipo = JP_TIPO_OBJETO;
    v.valor.objeto = obj;
    return v;
}

// === HELPERS PARA EXTRAIR VALORES ===
static inline int64_t jp_get_int(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_INT: return v.valor.inteiro;
        case JP_TIPO_DOUBLE: return (int64_t)v.valor.decimal;
        case JP_TIPO_BOOL: return v.valor.booleano;
        case JP_TIPO_STRING: 
            if (v.valor.texto) return (int64_t)atoll(v.valor.texto);
            return 0;
        default: return 0;
    }
}

static inline double jp_get_double(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_DOUBLE: return v.valor.decimal;
        case JP_TIPO_INT: return (double)v.valor.inteiro;
        case JP_TIPO_BOOL: return (double)v.valor.booleano;
        case JP_TIPO_STRING:
            if (v.valor.texto) return atof(v.valor.texto);
            return 0.0;
        default: return 0.0;
    }
}

static inline const char* jp_get_string(JPValor v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto != NULL) {
        return v.valor.texto;
    }
    return "";
}

static inline int jp_get_bool(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_BOOL: return v.valor.booleano;
        case JP_TIPO_INT: return v.valor.inteiro != 0;
        case JP_TIPO_STRING: return v.valor.texto != NULL && v.valor.texto[0] != '\0';
        default: return 0;
    }
}

static inline int jp_is_true(JPValor v) {
    return jp_get_bool(v);
}

// === LIBERAÇÃO DE MEMÓRIA ===
static inline void jp_liberar(JPValor* v) {
    if (v->tipo == JP_TIPO_STRING && v->valor.texto != NULL) {
        free(v->valor.texto);
        v->valor.texto = NULL;
    }
    v->tipo = JP_TIPO_NULO;
}

// === CÓPIA DE VALOR ===
static inline JPValor jp_copiar(JPValor v) {
    if (v.tipo == JP_TIPO_STRING) {
        return jp_string(v.valor.texto);
    }
    return v;
}

// === CONVERSÃO PARA STRING ===
static inline char* jp_to_string(JPValor v) {
    char* buf;
    switch (v.tipo) {
        case JP_TIPO_INT:
            buf = (char*)malloc(32);
            if (buf) snprintf(buf, 32, "%lld", (long long)v.valor.inteiro);
            return buf;
        case JP_TIPO_DOUBLE:
            buf = (char*)malloc(32);
            if (buf) snprintf(buf, 32, "%.6g", v.valor.decimal);
            return buf;
        case JP_TIPO_BOOL:
            buf = (char*)malloc(16);
            if (buf) strcpy(buf, v.valor.booleano ? "verdadeiro" : "falso");
            return buf;
        case JP_TIPO_STRING:
            if (v.valor.texto) {
                size_t len = strlen(v.valor.texto);
                buf = (char*)malloc(len + 1);
                if (buf) memcpy(buf, v.valor.texto, len + 1);
                return buf;
            }
            buf = (char*)malloc(1);
            if (buf) buf[0] = '\0';
            return buf;
        case JP_TIPO_LISTA:
            buf = (char*)malloc(8);
            if (buf) strcpy(buf, "[Lista]");
            return buf;
        case JP_TIPO_OBJETO:
            buf = (char*)malloc(16);
            if (buf) strcpy(buf, "[Objeto]");
            return buf;
        case JP_TIPO_PONTEIRO:
            buf = (char*)malloc(24);
            if (buf) snprintf(buf, 24, "[Ptr:%p]", v.valor.ponteiro);
            return buf;
        default:
            buf = (char*)malloc(8);
            if (buf) strcpy(buf, "nulo");
            return buf;
    }
}

// === ESTRUTURA PARA CARREGAR DLLs ===
typedef struct {
    char nome[256];
    #ifdef _WIN32
        HMODULE handle;
    #else
        void* handle;
    #endif
} JPBiblioteca;

// === FUNÇÕES DE BIBLIOTECA ===

static inline JPBiblioteca jp_carregar_lib(const char* caminho) {
    JPBiblioteca lib;
    memset(&lib, 0, sizeof(lib));
    strncpy(lib.nome, caminho, 255);
    lib.nome[255] = '\0';
    
    #ifdef _WIN32
        typedef BOOL (WINAPI *SetDllDirectoryA_t)(LPCSTR);
        SetDllDirectoryA_t pSetDllDir = (SetDllDirectoryA_t)GetProcAddress(
            GetModuleHandleA("kernel32.dll"), "SetDllDirectoryA");
        if (pSetDllDir) {
            pSetDllDir("runtime");
        }
        
        char runtime[512];
        snprintf(runtime, 512, "runtime\\%s.jpd", caminho);
        lib.handle = LoadLibraryA(runtime);
        
        if (!lib.handle) {
            char local[512];
            snprintf(local, 512, "%s.jpd", caminho);
            lib.handle = LoadLibraryA(local);
        }
        
        if (!lib.handle) {
            lib.handle = LoadLibraryA(caminho);
        }
        
        if (!lib.handle) {
            char alt[512];
            snprintf(alt, 512, "bibliotecas\\%s\\%s.jpd", caminho, caminho);
            lib.handle = LoadLibraryA(alt);
        }
    #else
        // Pre-carrega .so da pasta runtime para resolver dependencias
        {
            const char* dirs[] = {"./runtime", "./bibliotecas"};
            char dirpath[512];
            for (int d = 0; d < 2; d++) {
                if (d == 1) snprintf(dirpath, 512, "./bibliotecas/%s", caminho);
                else strcpy(dirpath, dirs[d]);
                
                void* dirp = opendir(dirpath);
                if (dirp) {
                    struct dirent* ent;
                    while ((ent = readdir((DIR*)dirp)) != NULL) {
                        const char* name = ent->d_name;
                        size_t nlen = strlen(name);
                        // Carrega .so mas nao a .jpd
                        if (nlen > 3 && strcmp(name + nlen - 3, ".so") == 0) {
                            char fullpath[1024];
                            snprintf(fullpath, 1024, "%s/%s", dirpath, name);
                            dlopen(fullpath, RTLD_LAZY | RTLD_GLOBAL);
                        }
                    }
                    closedir((DIR*)dirp);
                }
            }
        }
        
        char runtime[512];
        snprintf(runtime, 512, "./runtime/lib%s.jpd", caminho);
        lib.handle = dlopen(runtime, RTLD_LAZY);
        
        if (!lib.handle) {
            snprintf(runtime, 512, "./runtime/%s.jpd", caminho);
            lib.handle = dlopen(runtime, RTLD_LAZY);
        }
        
        if (!lib.handle) {
            char local[512];
            snprintf(local, 512, "./lib%s.jpd", caminho);
            lib.handle = dlopen(local, RTLD_LAZY);
        }
        
        if (!lib.handle) {
            lib.handle = dlopen(caminho, RTLD_LAZY);
        }
        
        if (!lib.handle) {
            char alt[512];
            snprintf(alt, 512, "./bibliotecas/%s/lib%s.jpd", caminho, caminho);
            lib.handle = dlopen(alt, RTLD_LAZY);
        }
    #endif
    
    return lib;
}

static inline JPFuncaoNativa jp_obter_funcao(JPBiblioteca* lib, const char* nome) {
    if (!lib || !lib->handle) return NULL;
    
    #ifdef _WIN32
        return (JPFuncaoNativa)GetProcAddress(lib->handle, nome);
    #else
        return (JPFuncaoNativa)dlsym(lib->handle, nome);
    #endif
}

static inline void jp_descarregar_lib(JPBiblioteca* lib) {
    if (!lib || !lib->handle) return;
    
    #ifdef _WIN32
        FreeLibrary(lib->handle);
    #else
        dlclose(lib->handle);
    #endif
    
    lib->handle = NULL;
}

#ifdef __cplusplus
}
#endif

#endif // JPRUNTIME_H