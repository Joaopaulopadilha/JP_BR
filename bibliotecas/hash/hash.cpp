// hash.cpp
// Biblioteca nativa de Hash para JPLang - SHA-256 com suporte a salt
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/hash/hash.jpd bibliotecas/hash/hash.cpp -O3
//   Linux:   g++ -shared -fPIC -o bibliotecas/hash/libhash.jpd bibliotecas/hash/hash.cpp -O3

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

// =============================================================================
// DETECÇÃO DE PLATAFORMA
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// =============================================================================
// TIPOS C PUROS (INTERFACE JPLang)
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
// HELPERS
// =============================================================================
static JPValor jp_string(const std::string& s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    r.valor.texto = (char*)malloc(s.size() + 1);
    if (r.valor.texto) {
        memcpy(r.valor.texto, s.c_str(), s.size() + 1);
    }
    return r;
}

static JPValor jp_int(int64_t v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static JPValor jp_bool(bool v) {
    JPValor r;
    r.tipo = JP_TIPO_BOOL;
    r.valor.booleano = v ? 1 : 0;
    return r;
}

static std::string jp_get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) {
        return std::string(args[idx].valor.texto);
    }
    return "";
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

// =============================================================================
// IMPLEMENTAÇÃO SHA-256 (RFC 6234)
// =============================================================================
namespace sha256 {

// Constantes K para SHA-256
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Funções auxiliares
inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t sig0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t sig1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t gam0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t gam1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

// Processa um bloco de 64 bytes
void processBlock(const uint8_t* block, uint32_t* H) {
    uint32_t W[64];
    
    // Prepara o schedule de mensagens
    for (int i = 0; i < 16; i++) {
        W[i] = (block[i*4] << 24) | (block[i*4+1] << 16) | 
               (block[i*4+2] << 8) | block[i*4+3];
    }
    
    for (int i = 16; i < 64; i++) {
        W[i] = gam1(W[i-2]) + W[i-7] + gam0(W[i-15]) + W[i-16];
    }
    
    // Inicializa variáveis de trabalho
    uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
    uint32_t e = H[4], f = H[5], g = H[6], h = H[7];
    
    // Loop principal
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + W[i];
        uint32_t t2 = sig0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // Atualiza hash
    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
}

// Calcula SHA-256 de uma string
std::string hash(const std::string& input) {
    // Valores iniciais do hash
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    // Prepara a mensagem com padding
    size_t msgLen = input.length();
    size_t bitLen = msgLen * 8;
    
    // Calcula tamanho com padding: mensagem + 1 byte (0x80) + zeros + 8 bytes (tamanho)
    size_t paddedLen = ((msgLen + 9 + 63) / 64) * 64;
    
    std::vector<uint8_t> padded(paddedLen, 0);
    memcpy(padded.data(), input.c_str(), msgLen);
    
    // Adiciona bit 1
    padded[msgLen] = 0x80;
    
    // Adiciona tamanho em bits (big-endian, 64 bits)
    for (int i = 0; i < 8; i++) {
        padded[paddedLen - 1 - i] = (bitLen >> (i * 8)) & 0xFF;
    }
    
    // Processa cada bloco de 64 bytes
    for (size_t i = 0; i < paddedLen; i += 64) {
        processBlock(padded.data() + i, H);
    }
    
    // Converte para string hexadecimal
    std::ostringstream result;
    for (int i = 0; i < 8; i++) {
        result << std::hex << std::setfill('0') << std::setw(8) << H[i];
    }
    
    return result.str();
}

} // namespace sha256

// =============================================================================
// GERADOR DE SALT
// =============================================================================
static bool rng_inicializado = false;

static std::string gerar_salt(int tamanho = 16) {
    if (!rng_inicializado) {
        srand((unsigned int)time(nullptr) ^ (unsigned int)clock());
        rng_inicializado = true;
    }
    
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::string salt;
    salt.reserve(tamanho);
    
    for (int i = 0; i < tamanho; i++) {
        salt += charset[rand() % (sizeof(charset) - 1)];
    }
    
    return salt;
}

// =============================================================================
// FUNÇÕES EXPORTADAS PARA JPLang
// =============================================================================

// Calcula hash SHA-256 de uma string
// Args: texto
// Retorna: hash em hexadecimal (64 caracteres)
JP_EXPORT JPValor jp_hash_sha256(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    std::string texto = jp_get_string(args, 0);
    return jp_string(sha256::hash(texto));
}

// Gera um salt aleatório
// Args: tamanho (opcional, padrão 16)
// Retorna: string aleatória
JP_EXPORT JPValor jp_hash_salt(JPValor* args, int numArgs) {
    int tamanho = 16;
    if (numArgs >= 1) {
        tamanho = jp_get_int(args, 0);
        if (tamanho < 8) tamanho = 8;
        if (tamanho > 64) tamanho = 64;
    }
    return jp_string(gerar_salt(tamanho));
}

// Cria hash de senha com salt (formato: salt$hash)
// Args: senha
// Retorna: string no formato "salt$hash"
JP_EXPORT JPValor jp_hash_senha(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    
    std::string senha = jp_get_string(args, 0);
    std::string salt = gerar_salt(16);
    std::string hash = sha256::hash(salt + senha);
    
    return jp_string(salt + "$" + hash);
}

// Verifica se uma senha corresponde ao hash armazenado
// Args: senha, hash_armazenado (formato "salt$hash")
// Retorna: verdadeiro ou falso
JP_EXPORT JPValor jp_hash_verificar(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_bool(false);
    
    std::string senha = jp_get_string(args, 0);
    std::string armazenado = jp_get_string(args, 1);
    
    // Encontra o separador $
    size_t pos = armazenado.find('$');
    if (pos == std::string::npos) {
        return jp_bool(false);
    }
    
    std::string salt = armazenado.substr(0, pos);
    std::string hash_esperado = armazenado.substr(pos + 1);
    
    // Calcula hash da senha com o mesmo salt
    std::string hash_calculado = sha256::hash(salt + senha);
    
    // Comparação em tempo constante (evita timing attacks)
    if (hash_calculado.length() != hash_esperado.length()) {
        return jp_bool(false);
    }
    
    int resultado = 0;
    for (size_t i = 0; i < hash_calculado.length(); i++) {
        resultado |= hash_calculado[i] ^ hash_esperado[i];
    }
    
    return jp_bool(resultado == 0);
}

// Cria hash com salt personalizado
// Args: senha, salt
// Retorna: string no formato "salt$hash"
JP_EXPORT JPValor jp_hash_senha_com_salt(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_string("");
    
    std::string senha = jp_get_string(args, 0);
    std::string salt = jp_get_string(args, 1);
    std::string hash = sha256::hash(salt + senha);
    
    return jp_string(salt + "$" + hash);
}