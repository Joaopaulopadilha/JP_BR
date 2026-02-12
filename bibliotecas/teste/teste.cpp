// teste.cpp
// Biblioteca de teste para validar aviso de colisão de funções
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/teste/teste.jpd bibliotecas/teste/teste.cpp -O3
//   Linux:   g++ -shared -fPIC -o bibliotecas/teste/libteste.jpd bibliotecas/teste/teste.cpp -O3

#include <cstdint>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

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

static JPValor jp_string(const char* s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    size_t len = strlen(s);
    r.valor.texto = (char*)malloc(len + 1);
    if (r.valor.texto) memcpy(r.valor.texto, s, len + 1);
    return r;
}

JP_EXPORT JPValor sleep(JPValor* args, int numArgs) {
    return jp_string("ola mundo");
}