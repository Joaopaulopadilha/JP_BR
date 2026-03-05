// txt.cpp
// Biblioteca nativa de manipulação de texto para JPLang
// Usa POOL ROTATIVO DE STRINGS igual ao texto.cpp original
// Compilar:
//   Windows: g++ -std=c++17 -c -O3 -o bibliotecas/txt/txt.obj bibliotecas/txt/txt.cpp
//   Linux:   g++ -std=c++17 -c -O3 -fPIC -o bibliotecas/txt/txt.o bibliotecas/txt/txt.cpp

#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <mutex> // Para evitar corrupção em multithreading

// ---------------------------------------------------------------------------
// POOL ROTATIVO DE STRINGS (Buffer Estático)
// ---------------------------------------------------------------------------

static const int NUM_BUFFERS = 8;      // Quantidade de slots no pool
static char text_buffers[NUM_BUFFERS][4096]; // Buffer estático de strings
static int text_buf_index = 0;         // Índice atual do buffer
static std::mutex text_mutex;          // Mutex para proteger acesso concorrente

// Função auxiliar interna: Retorna ponteiro para string no pool rotativo
static const char* retorna_str(const std::string& s) {
    std::lock_guard<std::mutex> lock(text_mutex);
    
    char* buf = text_buffers[text_buf_index];
    text_buf_index = (text_buf_index + 1) % NUM_BUFFERS;

    size_t len = s.size();
    if (len >= 4096) len = 4095; // Limita ao tamanho do buffer

    memcpy(buf, s.c_str(), len);
    buf[len] = '\0';

    return buf;
}

// ---------------------------------------------------------------------------
// FUNÇÕES EXPORTADAS - TEXTO
// ---------------------------------------------------------------------------

extern "C" const char* txt_maiusculo(const char* texto) {
    if (!texto) return retorna_str(""); // Caso nulo
    
    std::string original(texto);
    std::string resultado = "";

    for (char &c : original) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        resultado += c;
    }

    return retorna_str(resultado); // Copia para pool e retorna ponteiro
}

extern "C" const char* txt_inicial(const char* texto) {
    if (!texto || *texto == '\0') return retorna_str("");

    std::string original(texto);
    std::string resultado = "";

    if (original.length() > 0) {
        char c = original[0];
        // Converte para maiúsculo se for letra minúscula
        if (c >= 'a' && c <= 'z') {
            resultado += static_cast<char>(toupper(static_cast<unsigned char>(c)));
        } else {
            resultado += c; // Já é maiúsculo ou não alfabético
        }

        // Adiciona o resto da string original (mantém minúsculo)
        if (original.length() > 1) {
            resultado.append(original.substr(1));
        }
    }

    return retorna_str(resultado); // Copia para pool e retorna ponteiro
}

// ---------------------------------------------------------------------------
// FIM DA BIBLIOTEIRA DE TEXTO
// ---------------------------------------------------------------------------
