// tempo.cpp
// Biblioteca de tempo para JPLang — multiplataforma (Windows/Linux), linkagem estática via .obj/.o, extern "C" puro
//
// Compilação:
//   Windows: g++ -std=c++17 -c -o bibliotecas/tempo/tempo.obj bibliotecas/tempo/tempo.cpp
//   Linux:   g++ -std=c++17 -c -o bibliotecas/tempo/tempo.o   bibliotecas/tempo/tempo.cpp

#include <cstdint>
#include <cstring>
#include <ctime>

// =============================================================================
// DECLARAÇÕES POR PLATAFORMA
// =============================================================================

#ifdef _WIN32

extern "C" {
    typedef long long time64_t;
    time64_t _time64(time64_t*);
    struct tm* _localtime64(const time64_t*);
    void __stdcall Sleep(unsigned long dwMilliseconds);
    int __stdcall QueryPerformanceCounter(int64_t* lpPerformanceCount);
    int __stdcall QueryPerformanceFrequency(int64_t* lpFrequency);
}

#else

#include <unistd.h>

#endif

// =============================================================================
// HELPERS INTERNOS
// =============================================================================

static char str_buffer[256];

struct TmLocal {
    int sec;
    int min;
    int hour;
    int mday;
    int mon;
    int year;
    int wday;
    int yday;
};

static TmLocal pegar_tempo() {
    TmLocal r;

#ifdef _WIN32
    time64_t now;
    _time64(&now);
    struct tm* t = _localtime64(&now);
#else
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
#endif

    r.sec  = t->tm_sec;
    r.min  = t->tm_min;
    r.hour = t->tm_hour;
    r.mday = t->tm_mday;
    r.mon  = t->tm_mon;
    r.year = t->tm_year;
    r.wday = t->tm_wday;
    r.yday = t->tm_yday;
    return r;
}

static const char* formatar_tempo(const char* fmt) {
#ifdef _WIN32
    time64_t now;
    _time64(&now);
    struct tm* t = _localtime64(&now);
#else
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
#endif

    strftime(str_buffer, sizeof(str_buffer), fmt, t);
    return str_buffer;
}

// Cronômetro
static int64_t crono_inicio = 0;

#ifdef _WIN32
static int64_t crono_freq = 0;
#endif

// =============================================================================
// FUNÇÕES EXPORTADAS - RETORNO INT
// =============================================================================

extern "C" int64_t tm_dia() {
    return pegar_tempo().mday;
}

extern "C" int64_t tm_mes() {
    return pegar_tempo().mon + 1;
}

extern "C" int64_t tm_ano() {
    return pegar_tempo().year + 1900;
}

extern "C" int64_t tm_hora() {
    return pegar_tempo().hour;
}

extern "C" int64_t tm_minuto() {
    return pegar_tempo().min;
}

extern "C" int64_t tm_segundo() {
    return pegar_tempo().sec;
}

extern "C" int64_t tm_dia_semana() {
    return pegar_tempo().wday;
}

extern "C" int64_t tm_dia_ano() {
    return pegar_tempo().yday;
}

extern "C" int64_t tm_timestamp() {
#ifdef _WIN32
    time64_t now;
    _time64(&now);
    return (int64_t)now;
#else
    return (int64_t)time(nullptr);
#endif
}

extern "C" int64_t tm_data_num() {
    auto t = pegar_tempo();
    return (t.mday * 1000000) + ((t.mon + 1) * 10000) + (t.year + 1900);
}

extern "C" int64_t tm_hora_num() {
    auto t = pegar_tempo();
    return (t.hour * 10000) + (t.min * 100) + t.sec;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - RETORNO STRING
// =============================================================================

extern "C" const char* tm_data_str() {
    return formatar_tempo("%d/%m/%Y");
}

extern "C" const char* tm_hora_str() {
    return formatar_tempo("%H:%M:%S");
}

extern "C" const char* tm_completo() {
    return formatar_tempo("%d/%m/%Y %H:%M:%S");
}

// =============================================================================
// FUNÇÕES EXPORTADAS - UTILITÁRIOS
// =============================================================================

extern "C" int64_t tm_sleep(int64_t ms) {
#ifdef _WIN32
    Sleep((unsigned long)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
    return 0;
}

extern "C" int64_t tm_cronometro_iniciar() {
#ifdef _WIN32
    if (crono_freq == 0) {
        QueryPerformanceFrequency(&crono_freq);
    }
    QueryPerformanceCounter(&crono_inicio);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    crono_inicio = ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
    return 0;
}

extern "C" int64_t tm_cronometro_parar() {
#ifdef _WIN32
    int64_t agora;
    QueryPerformanceCounter(&agora);
    if (crono_freq == 0) return 0;
    return ((agora - crono_inicio) * 1000) / crono_freq;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t agora = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return (agora - crono_inicio) / 1000000; // nanossegundos → milissegundos
#endif
}