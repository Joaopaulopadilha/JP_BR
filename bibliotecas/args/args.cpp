// args.cpp
// Biblioteca de argumentos de linha de comando para JPLang

#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

// === TIPOS JPLANG ===
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
    } valor;
} JPValor;

// === EXPORTAÇÃO ===
#ifdef _WIN32
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// === ARMAZENAMENTO GLOBAL DOS ARGUMENTOS ===
static std::vector<std::string> g_argumentos;
static bool g_inicializado = false;

// Função para inicializar os argumentos (chamada pelo main.cpp)
JP_EXPORT void jp_args_init(int argc, char** argv) {
    g_argumentos.clear();
    for (int i = 0; i < argc; i++) {
        g_argumentos.push_back(argv[i]);
    }
    g_inicializado = true;
}

// Função alternativa para inicializar com vetor de strings
JP_EXPORT void jp_args_init_vec(const std::vector<std::string>& args) {
    g_argumentos = args;
    g_inicializado = true;
}

// === FUNÇÕES NATIVAS JPLANG ===

// args_total() - Retorna quantidade de argumentos
JP_EXPORT JPValor jp_args_total(JPValor* args, int numArgs) {
    JPValor resultado;
    resultado.tipo = JP_TIPO_INT;
    resultado.valor.inteiro = (int64_t)g_argumentos.size();
    return resultado;
}

// args_obter(indice) - Retorna argumento no índice especificado
JP_EXPORT JPValor jp_args_obter(JPValor* args, int numArgs) {
    JPValor resultado;
    
    if (numArgs < 1 || args[0].tipo != JP_TIPO_INT) {
        resultado.tipo = JP_TIPO_STRING;
        resultado.valor.texto = (char*)malloc(1);
        resultado.valor.texto[0] = '\0';
        return resultado;
    }
    
    int indice = (int)args[0].valor.inteiro;
    
    if (indice < 0 || indice >= (int)g_argumentos.size()) {
        resultado.tipo = JP_TIPO_STRING;
        resultado.valor.texto = (char*)malloc(1);
        resultado.valor.texto[0] = '\0';
        return resultado;
    }
    
    const std::string& arg = g_argumentos[indice];
    resultado.tipo = JP_TIPO_STRING;
    resultado.valor.texto = (char*)malloc(arg.size() + 1);
    memcpy(resultado.valor.texto, arg.c_str(), arg.size() + 1);
    
    return resultado;
}
