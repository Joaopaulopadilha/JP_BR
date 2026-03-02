// teste.cpp
// Biblioteca nativa de teste para validar linkagem de dois .obj com JPLink

#include <cstdint>
#include <cstring>

// Declaração direta — evita wrappers do MinGW
extern "C" int sprintf(char*, const char*, ...);

// =============================================================================
// BUFFER PARA RETORNO DE STRINGS
// =============================================================================

static char str_buf[256];

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// Soma dois inteiros
extern "C" int64_t teste_somar(int64_t a, int64_t b) {
    return a + b;
}

// Multiplica dois inteiros
extern "C" int64_t teste_multiplicar(int64_t a, int64_t b) {
    return a * b;
}

// Retorna o maior entre dois inteiros
extern "C" int64_t teste_maior(int64_t a, int64_t b) {
    return (a > b) ? a : b;
}

// Retorna uma saudação (texto)
extern "C" const char* teste_saudacao(const char* nome) {
    sprintf(str_buf, "Ola, %s! Bem-vindo ao JPLink!", nome);
    return str_buf;
}

// Calcula fatorial (recursivo)
extern "C" int64_t teste_fatorial(int64_t n) {
    if (n <= 1) return 1;
    return n * teste_fatorial(n - 1);
}