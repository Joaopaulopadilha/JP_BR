// texto.cpp
// Biblioteca de manipulação de texto para JPLang - Versão Unificada Windows/Linux
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/texto/texto.jpd bibliotecas/texto/texto.cpp -O3 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/texto/libtexto.jpd bibliotecas/texto/texto.cpp -O3

#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// =============================================================================
// DETECÇÃO DE PLATAFORMA E MACROS DE EXPORT
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// =============================================================================
// TIPOS C PUROS (interface com JPLang)
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
// HELPERS PARA CRIAR RETORNOS
// =============================================================================
static JPValor jp_int(int v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static JPValor jp_string(const std::string& s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    r.valor.texto = (char*)malloc(s.size() + 1);
    if (r.valor.texto) {
        memcpy(r.valor.texto, s.c_str(), s.size() + 1);
    }
    return r;
}

static std::string jp_get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) {
        return std::string(args[idx].valor.texto);
    }
    if (args[idx].tipo == JP_TIPO_INT) {
        return std::to_string(args[idx].valor.inteiro);
    }
    if (args[idx].tipo == JP_TIPO_DOUBLE) {
        return std::to_string(args[idx].valor.decimal);
    }
    return "";
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// Maiúsculo
JP_EXPORT JPValor jp_txt_upper(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return jp_string(s);
}

// Minúsculo
JP_EXPORT JPValor jp_txt_lower(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return jp_string(s);
}

// Tamanho
JP_EXPORT JPValor jp_txt_len(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    return jp_int((int)s.length());
}

// Contém (retorna 1 ou 0)
JP_EXPORT JPValor jp_txt_contains(JPValor* args, int numArgs) {
    std::string haystack = jp_get_string(args, 0);
    std::string needle = jp_get_string(args, 1);
    return jp_int((haystack.find(needle) != std::string::npos) ? 1 : 0);
}

// Remover espaços (trim)
JP_EXPORT JPValor jp_txt_trim(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return jp_string("");
    size_t end = s.find_last_not_of(" \t\r\n");
    return jp_string(s.substr(start, end - start + 1));
}

// Substituir
JP_EXPORT JPValor jp_txt_replace(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string oldVal = jp_get_string(args, 1);
    std::string newVal = jp_get_string(args, 2);
    
    if (oldVal.empty()) return jp_string(s);
    size_t pos = 0;
    while ((pos = s.find(oldVal, pos)) != std::string::npos) {
        s.replace(pos, oldVal.length(), newVal);
        pos += newVal.length();
    }
    return jp_string(s);
}

// Repetir
JP_EXPORT JPValor jp_txt_repeat(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    int n = jp_get_int(args, 1);
    std::string res;
    res.reserve(s.length() * n);
    for (int i = 0; i < n; i++) res += s;
    return jp_string(res);
}

// Inverter
JP_EXPORT JPValor jp_txt_reverse(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::reverse(s.begin(), s.end());
    return jp_string(s);
}

// Substring (inicio, tamanho)
JP_EXPORT JPValor jp_txt_substr(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    int start = jp_get_int(args, 1);
    int len = jp_get_int(args, 2);
    if (start < 0) start = 0;
    if (start >= (int)s.length()) return jp_string("");
    return jp_string(s.substr(start, len));
}

// Começa com
JP_EXPORT JPValor jp_txt_starts(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string prefix = jp_get_string(args, 1);
    return jp_int((s.rfind(prefix, 0) == 0) ? 1 : 0);
}

// Termina com
JP_EXPORT JPValor jp_txt_ends(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string suffix = jp_get_string(args, 1);
    if (s.length() < suffix.length()) return jp_int(0);
    return jp_int((s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0) ? 1 : 0);
}

// Dividir e pegar elemento N
JP_EXPORT JPValor jp_txt_split_get(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string delimiter = jp_get_string(args, 1);
    int index = jp_get_int(args, 2);

    if (delimiter.empty()) return jp_string(s);
    
    size_t pos = 0;
    int current = 0;
    
    while ((pos = s.find(delimiter)) != std::string::npos) {
        if (current == index) return jp_string(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
        current++;
    }
    if (current == index) return jp_string(s);
    return jp_string("");
}

// Contar ocorrências
JP_EXPORT JPValor jp_txt_count(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string sub = jp_get_string(args, 1);
    if (sub.empty()) return jp_int(0);
    
    int count = 0;
    size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) {
        count++;
        pos += sub.length();
    }
    return jp_int(count);
}

// Posição da primeira ocorrência (-1 se não encontrar)
JP_EXPORT JPValor jp_txt_index(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string sub = jp_get_string(args, 1);
    size_t pos = s.find(sub);
    return jp_int((pos != std::string::npos) ? (int)pos : -1);
}

// Pegar caractere na posição
JP_EXPORT JPValor jp_txt_char_at(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    int idx = jp_get_int(args, 1);
    if (idx < 0 || idx >= (int)s.length()) return jp_string("");
    return jp_string(std::string(1, s[idx]));
}

// Contar partes após split (quantidade de elementos que dividir retornaria)
JP_EXPORT JPValor jp_txt_split_count(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string delimiter = jp_get_string(args, 1);
    
    if (delimiter.empty()) return jp_int(1);
    if (s.empty()) return jp_int(0);
    
    int count = 1;
    size_t pos = 0;
    while ((pos = s.find(delimiter, pos)) != std::string::npos) {
        count++;
        pos += delimiter.length();
    }
    return jp_int(count);
}

// Remover aspas duplas e simples
JP_EXPORT JPValor jp_txt_strip_quotes(JPValor* args, int numArgs) {
    std::string s = jp_get_string(args, 0);
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '"' && s[i] != '\'') {
            result += s[i];
        }
    }
    return jp_string(result);
}