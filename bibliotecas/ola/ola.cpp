// ola.cpp
// DLL/SO de teste para JPLang — exporta função ola_mundo e ola_nome (Windows e Linux)

#include <cstdio>
#include <cstdint>

#ifdef _WIN32
    #define JP_EXPORT __declspec(dllexport)
#else
    #define JP_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

    JP_EXPORT int64_t ola_mundo() {
        printf("Ola, Mundo! (via DLL .jpd)\n");
        return 0;
    }

    JP_EXPORT int64_t ola_nome(const char* nome) {
        printf("Ola, %s! (via DLL .jpd)\n", nome);
        return 0;
    }

}