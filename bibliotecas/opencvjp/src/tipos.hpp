// tipos.hpp
// Tipos base para integração JPLang <-> C++

#ifndef OPENCVJP_TIPOS_HPP
#define OPENCVJP_TIPOS_HPP

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// =============================================================================
// TIPOS JPLANG (C++)
// =============================================================================
struct Instancia;
using ValorVariant = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

struct Instancia {
    std::string nomeClasse;
    std::map<std::string, ValorVariant> propriedades;
};

// =============================================================================
// TIPOS C PUROS (para comunicação com TCC)
// =============================================================================
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4,
    JP_TIPO_OBJETO = 5,
    JP_TIPO_LISTA = 6,
    JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
        void* lista;
        void* ponteiro;
    } valor;
} JPValor;

// =============================================================================
// CONVERSORES C <-> C++
// =============================================================================
inline ValorVariant jp_para_variant(const JPValor& jp) {
    switch (jp.tipo) {
        case JP_TIPO_INT:
            return (int)jp.valor.inteiro;
        case JP_TIPO_DOUBLE:
            return jp.valor.decimal;
        case JP_TIPO_BOOL:
            return (bool)jp.valor.booleano;
        case JP_TIPO_STRING:
            return jp.valor.texto ? std::string(jp.valor.texto) : std::string("");
        default:
            return std::string("");
    }
}

inline JPValor variant_para_jp(const ValorVariant& var) {
    JPValor jp;
    memset(&jp, 0, sizeof(jp));
    
    if (std::holds_alternative<int>(var)) {
        jp.tipo = JP_TIPO_INT;
        jp.valor.inteiro = std::get<int>(var);
    }
    else if (std::holds_alternative<double>(var)) {
        jp.tipo = JP_TIPO_DOUBLE;
        jp.valor.decimal = std::get<double>(var);
    }
    else if (std::holds_alternative<bool>(var)) {
        jp.tipo = JP_TIPO_BOOL;
        jp.valor.booleano = std::get<bool>(var) ? 1 : 0;
    }
    else if (std::holds_alternative<std::string>(var)) {
        jp.tipo = JP_TIPO_STRING;
        const std::string& s = std::get<std::string>(var);
        jp.valor.texto = (char*)malloc(s.size() + 1);
        if (jp.valor.texto) {
            memcpy(jp.valor.texto, s.c_str(), s.size() + 1);
        }
    }
    else {
        jp.tipo = JP_TIPO_NULO;
    }
    
    return jp;
}

inline std::vector<ValorVariant> jp_array_para_vector(JPValor* args, int numArgs) {
    std::vector<ValorVariant> result;
    result.reserve(numArgs);
    for (int i = 0; i < numArgs; i++) {
        result.push_back(jp_para_variant(args[i]));
    }
    return result;
}

// =============================================================================
// HELPERS
// =============================================================================
inline int get_int(const ValorVariant& v) {
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    if (std::holds_alternative<double>(v)) return (int)std::get<double>(v);
    return 0;
}

inline std::string get_str(const ValorVariant& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    return "";
}

#endif // OPENCVJP_TIPOS_HPP
