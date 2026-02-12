// tempo.cpp
// Biblioteca de tempo para JPLang - Versão Unificada Windows/Linux
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/tempo/tempo.jpd bibliotecas/tempo/tempo.cpp -O3
//   Linux:   g++ -shared -fPIC -o bibliotecas/tempo/libtempo.jpd bibliotecas/tempo/tempo.cpp -O3

#include <chrono>
#include <ctime>
#include <thread>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// =============================================================================
// DETECÇÃO DE PLATAFORMA E MACROS DE EXPORT
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// =============================================================================
// TIPOS C PUROS (interface com JPLang)
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
    } valor;
} JPValor;

// =============================================================================
// HELPERS PARA CRIAR RETORNOS
// =============================================================================
static JPValor jp_int(int v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static JPValor jp_string(const std::string& s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    r.valor.texto = (char*)malloc(s.size() + 1);
    if (r.valor.texto) {
        memcpy(r.valor.texto, s.c_str(), s.size() + 1);
    }
    return r;
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

// =============================================================================
// ESTADO GLOBAL
// =============================================================================
static std::chrono::steady_clock::time_point cronometro_inicio_global;

// =============================================================================
// HELPER - LOCALTIME THREAD-SAFE
// =============================================================================
static std::tm pegar_tempo() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm time_info;
    #if JP_WINDOWS
        localtime_s(&time_info, &now_c);
    #else
        localtime_r(&now_c, &time_info);
    #endif
    return time_info;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

JP_EXPORT JPValor jp_tm_data_str(JPValor* args, int numArgs) {
    auto t = pegar_tempo();
    std::ostringstream oss;
    oss << std::put_time(&t, "%d/%m/%Y");
    return jp_string(oss.str());
}

JP_EXPORT JPValor jp_tm_data_num(JPValor* args, int numArgs) {
    auto t = pegar_tempo();
    int val = (t.tm_mday * 1000000) + ((t.tm_mon + 1) * 10000) + (t.tm_year + 1900);
    return jp_int(val);
}

JP_EXPORT JPValor jp_tm_dia(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_mday);
}

JP_EXPORT JPValor jp_tm_mes(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_mon + 1);
}

JP_EXPORT JPValor jp_tm_ano(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_year + 1900);
}

JP_EXPORT JPValor jp_tm_hora_str(JPValor* args, int numArgs) {
    auto t = pegar_tempo();
    std::ostringstream oss;
    oss << std::put_time(&t, "%H:%M:%S");
    return jp_string(oss.str());
}

JP_EXPORT JPValor jp_tm_hora_num(JPValor* args, int numArgs) {
    auto t = pegar_tempo();
    return jp_int((t.tm_hour * 10000) + (t.tm_min * 100) + t.tm_sec);
}

JP_EXPORT JPValor jp_tm_h(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_hour);
}

JP_EXPORT JPValor jp_tm_m(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_min);
}

JP_EXPORT JPValor jp_tm_s(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_sec);
}

JP_EXPORT JPValor jp_tm_full(JPValor* args, int numArgs) {
    auto t = pegar_tempo();
    std::ostringstream oss;
    oss << std::put_time(&t, "%d/%m/%Y %H:%M:%S");
    return jp_string(oss.str());
}

JP_EXPORT JPValor jp_tm_wday(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_wday);
}

JP_EXPORT JPValor jp_tm_yday(JPValor* args, int numArgs) {
    return jp_int(pegar_tempo().tm_yday);
}

JP_EXPORT JPValor jp_tm_timestamp(JPValor* args, int numArgs) {
    auto now = std::chrono::system_clock::now();
    return jp_int((int)std::chrono::system_clock::to_time_t(now));
}

JP_EXPORT JPValor jp_tm_sleep(JPValor* args, int numArgs) {
    int ms = jp_get_int(args, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return jp_int(0);
}

JP_EXPORT JPValor jp_tm_start(JPValor* args, int numArgs) {
    cronometro_inicio_global = std::chrono::steady_clock::now();
    return jp_int(0);
}

JP_EXPORT JPValor jp_tm_end(JPValor* args, int numArgs) {
    auto agora = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(agora - cronometro_inicio_global).count();
    return jp_int((int)diff);
}