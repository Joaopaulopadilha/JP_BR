// cumprimentar.cpp
// Biblioteca nativa de cumprimentos para JPLang
//
// Compilar:
//   Windows: g++ -c -o bibliotecas/cumprimentar/cumprimentar.obj bibliotecas/cumprimentar/cumprimentar.cpp -O3
//   Linux:   g++ -c -fPIC -o bibliotecas/cumprimentar/cumprimentar.o bibliotecas/cumprimentar/cumprimentar.cpp -O3

#include <cstdint>
#include <string>
#include <mutex>

#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Buffer rotativo para retorno de strings
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

// Função que retorna um cumprimento
JP_EXPORT int64_t cumpr_cumprimentar(int64_t nome_ptr) {
    std::string nome((const char*)nome_ptr);
    std::string mensagem = "Olá, " + nome + "!";
    return retorna_str(mensagem);
}

// Função que retorna a versão da biblioteca
JP_EXPORT int64_t cumpr_versao() {
    return retorna_str("cumprimentar 1.0 - JPLang Greeting Library");
}
