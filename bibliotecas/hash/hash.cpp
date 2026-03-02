// hash.cpp
// Biblioteca nativa de Hash para JPLang — SHA-256 com suporte a salt
// Linkagem estática via .o — convenção int64_t / const char*
//
// Compilar:
//   Linux:   g++ -c -O2 -o bibliotecas/hash/hash.o bibliotecas/hash/hash.cpp
//   Windows: g++ -c -O2 -o bibliotecas/hash/hash.obj bibliotecas/hash/hash.cpp

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

// ---------------------------------------------------------------------------
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// ---------------------------------------------------------------------------

static const int NUM_BUFFERS = 8;
static const int BUF_SIZE    = 4096;
static char hash_str_buffers[NUM_BUFFERS][BUF_SIZE];
static int  hash_buf_index   = 0;

static const char* retorna_str(const std::string& s)
{
    char* buf = hash_str_buffers[hash_buf_index];
    hash_buf_index = (hash_buf_index + 1) % NUM_BUFFERS;

    size_t len = s.size();
    if (len >= static_cast<size_t>(BUF_SIZE))
        len = BUF_SIZE - 1;

    memcpy(buf, s.c_str(), len);
    buf[len] = '\0';

    return buf;
}

// =============================================================================
// IMPLEMENTAÇÃO SHA-256 (RFC 6234)
// =============================================================================
namespace sha256 {

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

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t gam0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t gam1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

void processBlock(const uint8_t* block, uint32_t* H) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = (block[i*4] << 24) | (block[i*4+1] << 16) |
               (block[i*4+2] << 8) | block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        W[i] = gam1(W[i-2]) + W[i-7] + gam0(W[i-15]) + W[i-16];
    }

    uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
    uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + W[i];
        uint32_t t2 = sig0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
}

std::string hash(const std::string& input) {
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    size_t msgLen = input.length();
    size_t bitLen = msgLen * 8;
    size_t paddedLen = ((msgLen + 9 + 63) / 64) * 64;

    std::vector<uint8_t> padded(paddedLen, 0);
    memcpy(padded.data(), input.c_str(), msgLen);
    padded[msgLen] = 0x80;

    for (int i = 0; i < 8; i++) {
        padded[paddedLen - 1 - i] = (bitLen >> (i * 8)) & 0xFF;
    }

    for (size_t i = 0; i < paddedLen; i += 64) {
        processBlock(padded.data() + i, H);
    }

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
// FUNÇÕES EXPORTADAS
// =============================================================================

// hash_sha256(texto) -> texto (hash hex 64 chars)
extern "C" const char* hash_sha256(const char* texto)
{
    if (!texto) return retorna_str("");
    return retorna_str(sha256::hash(std::string(texto)));
}

// hash_salt(tamanho) -> texto (string aleatória)
extern "C" const char* hash_salt(int64_t tamanho)
{
    int tam = static_cast<int>(tamanho);
    if (tam < 8) tam = 8;
    if (tam > 64) tam = 64;
    return retorna_str(gerar_salt(tam));
}

// hash_senha(senha) -> texto (formato "salt$hash")
extern "C" const char* hash_senha(const char* senha)
{
    if (!senha) return retorna_str("");
    std::string salt = gerar_salt(16);
    std::string h = sha256::hash(salt + std::string(senha));
    return retorna_str(salt + "$" + h);
}

// hash_verificar(senha, hash_armazenado) -> inteiro (1 = ok, 0 = falha)
extern "C" int64_t hash_verificar(const char* senha, const char* armazenado)
{
    if (!senha || !armazenado) return 0;

    std::string arm(armazenado);
    size_t pos = arm.find('$');
    if (pos == std::string::npos) return 0;

    std::string salt = arm.substr(0, pos);
    std::string hash_esperado = arm.substr(pos + 1);
    std::string hash_calculado = sha256::hash(salt + std::string(senha));

    if (hash_calculado.length() != hash_esperado.length()) return 0;

    // Comparação em tempo constante
    int resultado = 0;
    for (size_t i = 0; i < hash_calculado.length(); i++) {
        resultado |= hash_calculado[i] ^ hash_esperado[i];
    }
    return (resultado == 0) ? 1 : 0;
}

// hash_senha_com_salt(senha, salt) -> texto (formato "salt$hash")
extern "C" const char* hash_senha_com_salt(const char* senha, const char* salt)
{
    if (!senha || !salt) return retorna_str("");
    std::string h = sha256::hash(std::string(salt) + std::string(senha));
    return retorna_str(std::string(salt) + "$" + h);
}
