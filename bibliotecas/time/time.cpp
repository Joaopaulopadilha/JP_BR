// time.cpp
// Biblioteca de tempo para JPLang - Cronômetro e Sleep (Windows/Linux)
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/time/time.jpd bibliotecas/time/time.cpp -O3
//   Linux:   g++ -shared -fPIC -o bibliotecas/time/libtime.jpd bibliotecas/time/time.cpp -O3

#include <chrono>
#include <thread>
#include <cstdint>

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

static JPValor jp_int(int64_t v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static int64_t get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int64_t)args[idx].valor.decimal;
    return 0;
}

static std::chrono::steady_clock::time_point cronometro;

JP_EXPORT JPValor sleep(JPValor* args, int numArgs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(get_int(args, 0)));
    return jp_int(0);
}

JP_EXPORT JPValor start(JPValor* args, int numArgs) {
    cronometro = std::chrono::steady_clock::now();
    return jp_int(0);
}

JP_EXPORT JPValor end(JPValor* args, int numArgs) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cronometro).count();
    return jp_int((int64_t)ms);
}