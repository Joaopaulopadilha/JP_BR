// texto.cpp
// Biblioteca de manipulação de texto para JPLang
// Linkagem estática via .obj/.o — extern "C" puro
//
// Compilação:
//   Windows: g++ -std=c++17 -c -O2 -o bibliotecas/texto/texto.obj bibliotecas/texto/texto.cpp -static
//   Linux:   g++ -std=c++17 -c -O2 -fPIC -o bibliotecas/texto/texto.o bibliotecas/texto/texto.cpp

#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// ---------------------------------------------------------------------------
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// ---------------------------------------------------------------------------

static const int NUM_BUFFERS = 8;
static const int BUF_SIZE    = 4096;
static char txt_str_buffers[NUM_BUFFERS][BUF_SIZE];
static int  txt_buf_index   = 0;

static const char* retorna_str(const std::string& s)
{
    char* buf = txt_str_buffers[txt_buf_index];
    txt_buf_index = (txt_buf_index + 1) % NUM_BUFFERS;

    size_t len = s.size();
    if (len >= static_cast<size_t>(BUF_SIZE))
        len = BUF_SIZE - 1;

    memcpy(buf, s.c_str(), len);
    buf[len] = '\0';

    return buf;
}

// ---------------------------------------------------------------------------
// FUNÇÕES EXPORTADAS
// ---------------------------------------------------------------------------

extern "C" const char* txt_upper(const char* texto)
{
    if (!texto) return retorna_str("");
    std::string str(texto);
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return retorna_str(str);
}

extern "C" const char* txt_lower(const char* texto)
{
    if (!texto) return retorna_str("");
    std::string str(texto);
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return retorna_str(str);
}

extern "C" int64_t txt_tamanho(const char* texto)
{
    if (!texto) return 0;
    return static_cast<int64_t>(strlen(texto));
}

extern "C" int64_t txt_contem(const char* texto, const char* busca)
{
    if (!texto || !busca) return 0;
    return strstr(texto, busca) != nullptr ? 1 : 0;
}

extern "C" const char* txt_trim(const char* texto)
{
    if (!texto) return retorna_str("");
    std::string str(texto);
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return retorna_str("");
    size_t end   = str.find_last_not_of(" \t\r\n");
    return retorna_str(str.substr(start, end - start + 1));
}

extern "C" const char* txt_substituir(const char* texto,
                                      const char* antigo,
                                      const char* novo_txt)
{
    if (!texto || !antigo || !novo_txt) return retorna_str("");
    std::string str(texto);
    std::string old_str(antigo);
    std::string new_str(novo_txt);
    if (old_str.empty()) return retorna_str(str);

    size_t pos = 0;
    while ((pos = str.find(old_str, pos)) != std::string::npos) {
        str.replace(pos, old_str.length(), new_str);
        pos += new_str.length();
    }
    return retorna_str(str);
}

extern "C" const char* txt_repetir(const char* texto, int64_t vezes)
{
    int n = static_cast<int>(vezes);
    if (!texto || n <= 0) return retorna_str("");
    std::string str(texto);
    std::string res;
    res.reserve(str.length() * n);
    for (int i = 0; i < n; ++i) res += str;
    return retorna_str(res);
}

extern "C" const char* txt_inverter(const char* texto)
{
    if (!texto) return retorna_str("");
    std::string str(texto);
    std::reverse(str.begin(), str.end());
    return retorna_str(str);
}

extern "C" const char* txt_substr(const char* texto,
                                 int64_t inicio,
                                 int64_t tamanho)
{
    if (!texto) return retorna_str("");
    size_t slen = strlen(texto);

    size_t start = (inicio < 0) ? 0 : static_cast<size_t>(inicio);
    if (start >= slen) return retorna_str("");

    size_t len = static_cast<size_t>(tamanho);
    if (len > slen - start) len = slen - start;

    return retorna_str(std::string(texto).substr(start, len));
}

extern "C" int64_t txt_comeca_com(const char* texto, const char* prefixo)
{
    if (!texto || !prefixo) return 0;
    size_t plen = strlen(prefixo);
    return strncmp(texto, prefixo, plen) == 0 ? 1 : 0;
}

extern "C" int64_t txt_termina_com(const char* texto, const char* sufixo)
{
    if (!texto || !sufixo) return 0;
    size_t slen   = strlen(texto);
    size_t suflen = strlen(sufixo);
    if (suflen > slen) return 0;
    return strcmp(texto + slen - suflen, sufixo) == 0 ? 1 : 0;
}

extern "C" int64_t txt_posicao(const char* texto, const char* busca)
{
    if (!texto || !busca) return -1;
    const char* found = strstr(texto, busca);
    if (!found) return -1;
    return static_cast<int64_t>(found - texto);
}

extern "C" const char* txt_char(const char* texto, int64_t posicao)
{
    if (!texto) return retorna_str("");
    int idx = static_cast<int>(posicao);
    int slen = static_cast<int>(strlen(texto));
    if (idx < 0 || idx >= slen) return retorna_str("");
    char buf[2] = { texto[idx], '\0' };
    return retorna_str(std::string(buf));
}

extern "C" int64_t txt_contar(const char* texto, const char* busca)
{
    if (!texto || !busca) return 0;
    size_t sublen = strlen(busca);
    if (sublen == 0) return 0;
    int count = 0;
    const char* p = texto;
    while ((p = strstr(p, busca)) != nullptr) {
        ++count;
        p += sublen;
    }
    return static_cast<int64_t>(count);
}

extern "C" const char* txt_dividir(const char* texto,
                                   const char* delimitador,
                                   int64_t indice)
{
    if (!texto || !delimitador) return retorna_str("");
    std::string str(texto);
    std::string del(delimitador);
    if (del.empty()) return retorna_str(str);

    size_t pos = 0;
    int current = 0;
    size_t findPos;
    while ((findPos = str.find(del, pos)) != std::string::npos) {
        if (current == indice) {
            return retorna_str(str.substr(pos, findPos - pos));
        }
        pos = findPos + del.length();
        ++current;
    }
    if (current == indice) return retorna_str(str.substr(pos));
    return retorna_str("");
}

extern "C" int64_t txt_dividir_contar(const char* texto,
                                      const char* delimitador)
{
    if (!texto || !delimitador) return 0;
    std::string str(texto);
    std::string del(delimitador);
    if (del.empty()) return 1;
    if (str.empty()) return 0;
    int count = 1;
    size_t pos = 0;
    while ((pos = str.find(del, pos)) != std::string::npos) {
        ++count;
        pos += del.length();
    }
    return static_cast<int64_t>(count);
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

// ---------------------------------------------
// NOVA FUNÇÃO: TXT_INICIAL_NOME
// Formata nome em "Title Case" (Primeira letra de cada palavra Maiúscula)
// Exemplo: txt_inicial_nome("joao paulo") -> "Joao Paulo"
// ---------------------------------------------
extern "C" const char* txt_inicial_nome(const char* texto)
{
    if (!texto || *texto == '\0') return retorna_str("");

    std::string str(texto);
    
    // Lógica para capitalizar a primeira letra de cada nova palavra (nome ou frase)
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        if (isalpha(c)) {
            // Capitaliza se for o início da string OU se o anterior foi espaço/pontuação
            bool e_inicio_palavra = (i == 0);
            
            // Verifica se o caractere ANTERIOR era um delimitador de palavra (espaço, tab, etc)
            if (!e_inicio_palavra) {
                // Atenção: str[i-1] é seguro porque i >= 1 no bloco !e_inicio_palavra
                unsigned char anterior = static_cast<unsigned char>(str[i - 1]);
                e_inicio_palavra = (isspace(anterior) || ispunct(anterior));
            }

            if (e_inicio_palavra) {
                // Primeiro letra maiúscula
                str[i] = toupper(c);
            } else {
                // Letras do meio da palavra ficam minúsculas (padrão de formatação de nomes)
                str[i] = tolower(c);
            }
        }
        // Se não for letra (espaço/pontuação), mantém como está no string original
    }

    return retorna_str(str);
}


// ---------------------------------------------------------------------------
// FIM DAS FUNÇÕES EXPORTADAS
// ---------------------------------------------------------------------------
