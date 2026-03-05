#include <cstdint>
#include <cstdio>
#include <string>
#include <mutex>

#if defined(_WIN32) || defined(_WIN64)
#define JP_EXPORT extern "C" __declspec(dllexport)
#else
#define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

static const int NUM_BUFS = 8;
static std::string bufs[NUM_BUFS];
static int buf_idx = 0;
static std::mutex buf_mutex;

static int64_t retorna_str(const std::string& s) {
    std::lock_guard<std::mutex> lock(buf_mutex);
    int idx = buf_idx++ & (NUM_BUFS - 1);
    bufs[idx] = s;
    return (int64_t)bufs[idx].c_str();
}

JP_EXPORT int64_t mat_somar(int64_t a, int64_t b) {
    return a + b;
}

JP_EXPORT int64_t mat_subtrair(int64_t a, int64_t b) {
    return a - b;
}

JP_EXPORT int64_t mat_multiplicar(int64_t a, int64_t b) {
    return a * b;
}

JP_EXPORT int64_t mat_dividir(int64_t a, int64_t b) {
    if (b == 0) {
        return retorna_str("Erro: Divisao por zero");
    }
    return a / b;
}

JP_EXPORT int64_t mat_nome() {
    return retorna_str("matematica");
}