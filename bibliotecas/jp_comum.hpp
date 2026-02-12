// jp_comum.hpp
// Header comum para todas as bibliotecas JPLang
// Contém tipos e helpers compartilhados

#ifndef JP_COMUM_HPP
#define JP_COMUM_HPP

#include <vector>
#include <variant>
#include <string>
#include <memory>
#include <map>

// --- TIPOS JPLANG ---
#ifndef JPLANG_TYPES_DEFINED
#define JPLANG_TYPES_DEFINED 1

struct Instancia;
using ValorVariant = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;
using Var = ValorVariant;

struct Instancia {
    std::string nomeClasse;
    std::map<std::string, Var> propriedades;
};

#endif

// --- HELPERS COMPARTILHADOS (inline para evitar multiple definition) ---

inline std::string get_str(const ValorVariant& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "verdadeiro" : "falso";
    return "";
}

inline int get_int(const ValorVariant& v) {
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    if (std::holds_alternative<double>(v)) return (int)std::get<double>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
    if (std::holds_alternative<std::string>(v)) {
        try { return std::stoi(std::get<std::string>(v)); } catch(...) {}
    }
    return 0;
}

inline double get_double(const ValorVariant& v) {
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<int>(v)) return (double)std::get<int>(v);
    if (std::holds_alternative<std::string>(v)) {
        try { return std::stod(std::get<std::string>(v)); } catch(...) {}
    }
    return 0.0;
}

inline bool get_bool(const ValorVariant& v) {
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
    if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0;
    if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
    return false;
}

#endif