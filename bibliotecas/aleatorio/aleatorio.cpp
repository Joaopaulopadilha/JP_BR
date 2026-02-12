// aleatorio.cpp
// Biblioteca de geração de números e caracteres aleatórios para JPLang
// Compatível com Windows e Linux (Universal)
//
// COMPILAÇÃO:
//   Windows: g++ -shared -o bibliotecas/aleatorio/aleatorio.jpd bibliotecas/aleatorio/aleatorio.cpp -O3 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/aleatorio/libaleatorio.jpd bibliotecas/aleatorio/aleatorio.cpp -O3 -static-libgcc -static-libstdc++

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <algorithm> // para std::swap

// =============================================================================
// MACROS DE PORTABILIDADE (WINDOWS / LINUX)
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// =============================================================================
// TIPOS OBRIGATÓRIOS JPLANG
// =============================================================================
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

// =============================================================================
// HELPERS INTERNOS (Conversores e Utilitários)
// =============================================================================

// Cria um JPValor Inteiro
static inline JPValor jp_int(int64_t i) {
    JPValor v; 
    v.tipo = JP_TIPO_INT; 
    v.valor.inteiro = i; 
    return v;
}

// Cria um JPValor Double
static inline JPValor jp_double(double d) {
    JPValor v; 
    v.tipo = JP_TIPO_DOUBLE; 
    v.valor.decimal = d; 
    return v;
}

// Cria um JPValor Booleano
static inline JPValor jp_bool(bool b) {
    JPValor v; 
    v.tipo = JP_TIPO_BOOL; 
    v.valor.booleano = b ? 1 : 0; 
    return v;
}

// Cria um JPValor String (Aloca memória compatível com C)
static inline JPValor jp_string(const std::string& s) {
    JPValor v;
    v.tipo = JP_TIPO_STRING;
    v.valor.texto = (char*)malloc(s.size() + 1);
    if (v.valor.texto) {
        memcpy(v.valor.texto, s.c_str(), s.size() + 1);
    }
    return v;
}

// Extrai inteiro de um argumento (seguro)
static inline int64_t get_int(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return 0;
    JPValor v = args[idx];
    if (v.tipo == JP_TIPO_INT) return v.valor.inteiro;
    if (v.tipo == JP_TIPO_DOUBLE) return (int64_t)v.valor.decimal;
    if (v.tipo == JP_TIPO_BOOL) return v.valor.booleano;
    return 0;
}

// Extrai double de um argumento (seguro)
static inline double get_double(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return 0.0;
    JPValor v = args[idx];
    if (v.tipo == JP_TIPO_DOUBLE) return v.valor.decimal;
    if (v.tipo == JP_TIPO_INT) return (double)v.valor.inteiro;
    return 0.0;
}

// Extrai string de um argumento (seguro)
static inline std::string get_string(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return "";
    JPValor v = args[idx];
    if (v.tipo == JP_TIPO_STRING && v.valor.texto) return std::string(v.valor.texto);
    return "";
}

// =============================================================================
// LÓGICA DO GERADOR (Thread-Safe)
// =============================================================================
static std::mt19937& obterGerador() {
    static thread_local std::mt19937 gerador(
        (unsigned int)std::chrono::steady_clock::now().time_since_epoch().count()
    );
    return gerador;
}

// =============================================================================
// FUNÇÕES EXPORTADAS (Interface Nativa Universal)
// =============================================================================

// Gera número inteiro entre min e max
// Uso: aleatorio.numero(1, 10)
JP_EXPORT JPValor jp_al_numero(JPValor* args, int numArgs) {
    int minimo = (int)get_int(args, 0, numArgs);
    int maximo = (int)get_int(args, 1, numArgs);
    
    if (minimo > maximo) std::swap(minimo, maximo);
    
    std::uniform_int_distribution<int> dist(minimo, maximo);
    return jp_int(dist(obterGerador()));
}

// Gera número decimal entre min e max
// Uso: aleatorio.decimal(0.0, 1.0)
JP_EXPORT JPValor jp_al_decimal(JPValor* args, int numArgs) {
    double minimo = get_double(args, 0, numArgs);
    double maximo = get_double(args, 1, numArgs);
    
    // Se recebeu apenas inteiros e não foi passado nada, assume padrão 0.0 a 1.0
    if (numArgs == 0) { maximo = 1.0; }
    
    if (minimo > maximo) std::swap(minimo, maximo);
    
    std::uniform_real_distribution<double> dist(minimo, maximo);
    return jp_double(dist(obterGerador()));
}

// Gera uma letra aleatória entre dois caracteres
// Uso: aleatorio.letra("a", "z")
JP_EXPORT JPValor jp_al_letra(JPValor* args, int numArgs) {
    std::string strMin = get_string(args, 0, numArgs);
    std::string strMax = get_string(args, 1, numArgs);
    
    char minChar = strMin.empty() ? 'a' : strMin[0];
    char maxChar = strMax.empty() ? 'z' : strMax[0];
    
    if (minChar > maxChar) std::swap(minChar, maxChar);
    
    std::uniform_int_distribution<int> dist((int)minChar, (int)maxChar);
    std::string res(1, (char)dist(obterGerador()));
    return jp_string(res);
}

// Retorna verdadeiro ou falso
// Uso: aleatorio.booleano()
JP_EXPORT JPValor jp_al_booleano(JPValor* args, int numArgs) {
    std::uniform_int_distribution<int> dist(0, 1);
    return jp_bool(dist(obterGerador()) == 1);
}

// Retorna um índice válido para um array de tamanho N (0 a N-1)
// Uso: aleatorio.indice(lista.tamanho())
JP_EXPORT JPValor jp_al_indice(JPValor* args, int numArgs) {
    int tamanho = (int)get_int(args, 0, numArgs);
    if (tamanho <= 0) return jp_int(0);
    
    std::uniform_int_distribution<int> dist(0, tamanho - 1);
    return jp_int(dist(obterGerador()));
}

// Gera texto aleatório de tamanho N (apenas letras minúsculas)
// Uso: aleatorio.texto(10)
JP_EXPORT JPValor jp_al_texto(JPValor* args, int numArgs) {
    int tamanho = (int)get_int(args, 0, numArgs);
    if (tamanho <= 0) return jp_string("");
    
    std::uniform_int_distribution<int> dist('a', 'z');
    std::string resultado;
    resultado.reserve(tamanho);
    
    auto& gen = obterGerador();
    for (int i = 0; i < tamanho; i++) {
        resultado += (char)dist(gen);
    }
    
    return jp_string(resultado);
}

// Gera string alfanumérica de tamanho N
// Uso: aleatorio.alfanumerico(8)
JP_EXPORT JPValor jp_al_alfanumerico(JPValor* args, int numArgs) {
    int tamanho = (int)get_int(args, 0, numArgs);
    if (tamanho <= 0) return jp_string("");
    
    static const char caracteres[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    // sizeof(caracteres) - 2 porque sizeof inclui \0 e queremos indice 0..61
    std::uniform_int_distribution<int> dist(0, sizeof(caracteres) - 2);
    
    std::string resultado;
    resultado.reserve(tamanho);
    
    auto& gen = obterGerador();
    for (int i = 0; i < tamanho; i++) {
        resultado += caracteres[dist(gen)];
    }
    
    return jp_string(resultado);
}

// Define a semente do gerador
// Uso: aleatorio.semente(12345)
JP_EXPORT JPValor jp_al_semente(JPValor* args, int numArgs) {
    int seed = (int)get_int(args, 0, numArgs);
    obterGerador().seed(seed);
    return jp_int(1);
}

// Retorna um inteiro aleatório completo (útil para hash/embaralhar)
JP_EXPORT JPValor jp_al_embaralhar(JPValor* args, int numArgs) {
    std::uniform_int_distribution<int> dist(0, 2147483647);
    return jp_int(dist(obterGerador()));
}

// Retorna true baseado em uma porcentagem (0 a 100)
// Uso: aleatorio.chance(50)
JP_EXPORT JPValor jp_al_chance(JPValor* args, int numArgs) {
    int porcentagem = (int)get_int(args, 0, numArgs);
    
    if (porcentagem <= 0) return jp_bool(false);
    if (porcentagem >= 100) return jp_bool(true);
    
    std::uniform_int_distribution<int> dist(1, 100);
    return jp_bool(dist(obterGerador()) <= porcentagem);
}

// Simula um dado de N lados (padrão 6)
// Uso: aleatorio.dado(20)
JP_EXPORT JPValor jp_al_dado(JPValor* args, int numArgs) {
    int lados = (int)get_int(args, 0, numArgs);
    if (lados <= 0) lados = 6;
    
    std::uniform_int_distribution<int> dist(1, lados);
    return jp_int(dist(obterGerador()));
}