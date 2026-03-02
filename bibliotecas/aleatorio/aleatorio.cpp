// aleatorio.cpp
// Biblioteca de geração de números e caracteres aleatórios para JPLang
// Linkagem estática via .obj/.o — extern "C" puro
//
// Compilação:
//   Windows: g++ -std=c++17 -c -O2 -o aleatorio.obj aleatorio.cpp
//   Linux:   g++ -std=c++17 -c -O2 -fPIC -o bibliotecas/aleatorio/aleatorio.o bibliotecas/aleatorio/aleatorio.cpp

#include <string>
#include <random>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <algorithm>

// Declaração direta — evita wrapper __mingw_sprintf (só Windows)
#ifdef _WIN32
extern "C" int sprintf(char*, const char*, ...);
#else
#include <cstdio>
#endif

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================

static const int NUM_BUFFERS = 8;
static const int BUF_SIZE = 4096;
static char str_buffers[NUM_BUFFERS][BUF_SIZE];
static int buf_index = 0;

static const char* retorna_str(const std::string& s) {
    char* buf = str_buffers[buf_index];
    buf_index = (buf_index + 1) % NUM_BUFFERS;
    size_t len = s.size();
    if (len >= BUF_SIZE) len = BUF_SIZE - 1;
    memcpy(buf, s.c_str(), len);
    buf[len] = '\0';
    return buf;
}

// =============================================================================
// GERADOR (Thread-Safe)
// =============================================================================

static std::mt19937& obter_gerador() {
    static thread_local std::mt19937 gerador(
        (unsigned int)std::chrono::steady_clock::now().time_since_epoch().count()
    );
    return gerador;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

extern "C" int64_t al_numero(int64_t minimo, int64_t maximo) {
    int mn = (int)minimo;
    int mx = (int)maximo;
    if (mn > mx) std::swap(mn, mx);
    std::uniform_int_distribution<int> dist(mn, mx);
    return (int64_t)dist(obter_gerador());
}

extern "C" const char* al_decimal(int64_t minimo, int64_t maximo) {
    double mn = (double)minimo;
    double mx = (double)maximo;
    if (mn > mx) std::swap(mn, mx);
    std::uniform_real_distribution<double> dist(mn, mx);
    double val = dist(obter_gerador());
    char buf[64];
    sprintf(buf, "%.4f", val);
    return retorna_str(std::string(buf));
}

extern "C" const char* al_letra(const char* txt_min, const char* txt_max) {
    char min_c = (txt_min && txt_min[0]) ? txt_min[0] : 'a';
    char max_c = (txt_max && txt_max[0]) ? txt_max[0] : 'z';
    if (min_c > max_c) std::swap(min_c, max_c);
    std::uniform_int_distribution<int> dist((int)min_c, (int)max_c);
    char res[2] = { (char)dist(obter_gerador()), '\0' };
    return retorna_str(std::string(res));
}

extern "C" int64_t al_booleano() {
    std::uniform_int_distribution<int> dist(0, 1);
    return (int64_t)dist(obter_gerador());
}

extern "C" int64_t al_indice(int64_t tamanho) {
    int n = (int)tamanho;
    if (n <= 0) return 0;
    std::uniform_int_distribution<int> dist(0, n - 1);
    return (int64_t)dist(obter_gerador());
}

extern "C" const char* al_texto(int64_t tamanho) {
    int n = (int)tamanho;
    if (n <= 0) return retorna_str("");
    std::uniform_int_distribution<int> dist('a', 'z');
    std::string resultado;
    resultado.reserve(n);
    auto& gen = obter_gerador();
    for (int i = 0; i < n; i++) {
        resultado += (char)dist(gen);
    }
    return retorna_str(resultado);
}

extern "C" const char* al_alfanumerico(int64_t tamanho) {
    int n = (int)tamanho;
    if (n <= 0) return retorna_str("");
    static const char caracteres[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<int> dist(0, sizeof(caracteres) - 2);
    std::string resultado;
    resultado.reserve(n);
    auto& gen = obter_gerador();
    for (int i = 0; i < n; i++) {
        resultado += caracteres[dist(gen)];
    }
    return retorna_str(resultado);
}

extern "C" int64_t al_semente(int64_t semente) {
    obter_gerador().seed((unsigned int)semente);
    return 1;
}

extern "C" int64_t al_embaralhar() {
    std::uniform_int_distribution<int> dist(0, 2147483647);
    return (int64_t)dist(obter_gerador());
}

extern "C" int64_t al_chance(int64_t porcentagem) {
    int p = (int)porcentagem;
    if (p <= 0) return 0;
    if (p >= 100) return 1;
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(obter_gerador()) <= p ? 1 : 0;
}

extern "C" int64_t al_dado(int64_t lados) {
    int n = (int)lados;
    if (n <= 0) n = 6;
    std::uniform_int_distribution<int> dist(1, n);
    return (int64_t)dist(obter_gerador());
}