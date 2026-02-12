// arquivo.cpp
// Biblioteca de manipulação de arquivos para JPLang
// Compatível com Windows e Linux
//
// COMPILAÇÃO:
//   Windows: g++ -shared -o bibliotecas/arquivo/arquivo.jpd bibliotecas/arquivo/arquivo.cpp -O3 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/arquivo/libarquivo.jpd bibliotecas/arquivo/arquivo.cpp -O3 -static-libgcc -static-libstdc++

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint> // <--- ADICIONADO: Essencial para int64_t
#include <cstdio>  // <--- ADICIONADO: Para remove, rename
#include <sys/stat.h>

// =============================================================================
// MACROS DE PORTABILIDADE
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <direct.h> // Para _mkdir, _rmdir
    #include <io.h>     // Para _access
    
    #define MKDIR(path) _mkdir(path)
    #define RMDIR(path) _rmdir(path)
    #define ACCESS(path) _access(path, 0)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <unistd.h> // Para access, rmdir
    #include <sys/types.h>
    
    #define MKDIR(path) mkdir(path, 0777)
    #define RMDIR(path) rmdir(path)
    #define ACCESS(path) access(path, F_OK)
#endif

// =============================================================================
// TIPOS OBRIGATÓRIOS JPLANG
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
// HELPERS INTERNOS
// =============================================================================

static inline JPValor jp_bool(bool b) {
    JPValor v; v.tipo = JP_TIPO_BOOL; v.valor.booleano = b ? 1 : 0; return v;
}

static inline JPValor jp_int(int64_t i) {
    JPValor v; v.tipo = JP_TIPO_INT; v.valor.inteiro = i; return v;
}

static inline JPValor jp_string(const std::string& s) {
    JPValor v;
    v.tipo = JP_TIPO_STRING;
    v.valor.texto = (char*)malloc(s.size() + 1);
    if (v.valor.texto) {
        memcpy(v.valor.texto, s.c_str(), s.size() + 1);
    }
    return v;
}

static inline std::string get_string(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return "";
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) 
        return std::string(args[idx].valor.texto);
    return "";
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// Verifica se um arquivo ou diretório existe
JP_EXPORT JPValor jp_arquivo_existe(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    if (caminho.empty()) return jp_bool(false);
    
    // Retorna 0 se existe, -1 se não existe
    return jp_bool(ACCESS(caminho.c_str()) == 0);
}

// Cria um arquivo com o conteúdo especificado (sobrescreve se existir)
JP_EXPORT JPValor jp_arquivo_criar(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    std::string conteudo = get_string(args, 1, numArgs);
    
    std::ofstream arquivo(caminho);
    if (arquivo.is_open()) {
        arquivo << conteudo;
        arquivo.close();
        return jp_bool(true);
    }
    return jp_bool(false);
}

// Lê todo o conteúdo de um arquivo
JP_EXPORT JPValor jp_arquivo_ler(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    
    std::ifstream arquivo(caminho);
    if (arquivo.is_open()) {
        std::stringstream buffer;
        buffer << arquivo.rdbuf();
        return jp_string(buffer.str());
    }
    return jp_string("");
}

// Escreve conteúdo (alias para criar/sobrescrever)
JP_EXPORT JPValor jp_arquivo_escrever(JPValor* args, int numArgs) {
    return jp_arquivo_criar(args, numArgs);
}

// Anexa conteúdo ao final de um arquivo
JP_EXPORT JPValor jp_arquivo_anexar(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    std::string conteudo = get_string(args, 1, numArgs);
    
    // std::ios::app move o cursor para o fim antes de escrever
    std::ofstream arquivo(caminho, std::ios::app);
    if (arquivo.is_open()) {
        arquivo << conteudo;
        arquivo.close();
        return jp_bool(true);
    }
    return jp_bool(false);
}

// Deleta um arquivo
JP_EXPORT JPValor jp_arquivo_deletar(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    if (caminho.empty()) return jp_bool(false);
    
    if (remove(caminho.c_str()) == 0) {
        return jp_bool(true);
    }
    return jp_bool(false);
}

// Renomeia ou move um arquivo
JP_EXPORT JPValor jp_arquivo_renomear(JPValor* args, int numArgs) {
    std::string antigo = get_string(args, 0, numArgs);
    std::string novo = get_string(args, 1, numArgs);
    
    if (antigo.empty() || novo.empty()) return jp_bool(false);
    
    if (rename(antigo.c_str(), novo.c_str()) == 0) {
        return jp_bool(true);
    }
    return jp_bool(false);
}

// Retorna o tamanho do arquivo em bytes
JP_EXPORT JPValor jp_arquivo_tamanho(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    
    struct stat stat_buf;
    int rc = stat(caminho.c_str(), &stat_buf);
    
    if (rc == 0) {
        return jp_int((int64_t)stat_buf.st_size);
    }
    return jp_int(-1);
}

// Cria uma pasta (diretório)
JP_EXPORT JPValor jp_arquivo_criar_pasta(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    if (caminho.empty()) return jp_bool(false);
    
    if (MKDIR(caminho.c_str()) == 0) {
        return jp_bool(true);
    }
    return jp_bool(false);
}

// Deleta uma pasta (diretório deve estar vazio)
JP_EXPORT JPValor jp_arquivo_deletar_pasta(JPValor* args, int numArgs) {
    std::string caminho = get_string(args, 0, numArgs);
    if (caminho.empty()) return jp_bool(false);
    
    if (RMDIR(caminho.c_str()) == 0) {
        return jp_bool(true);
    }
    return jp_bool(false);
}