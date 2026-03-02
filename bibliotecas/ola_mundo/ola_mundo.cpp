// Tarefa: faca uma biblioteca .obj com uma funcao ola mundo
// Gerado em: 25/02/2026 21:07:05

#include <cstdio>
#include <cstdint>

extern "C" {

    __declspec(dllexport) int64_t ola_mundo() {
        printf("Olá Mundo\n");
        return 0;
    }

}