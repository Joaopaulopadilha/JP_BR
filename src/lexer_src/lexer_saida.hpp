// lexer_saida.hpp
// Módulo do lexer para verificação de palavras-chave usando o mapa de idioma carregado pelo lang_loader

#ifndef LEXER_SRC_LEXER_SAIDA_HPP
#define LEXER_SRC_LEXER_SAIDA_HPP

#include <string>
#include "../opcodes.hpp"
#include "../lang_loader.hpp"

namespace LexerSaida {
    inline TokenType checkKeyword(const std::string& val) {
        // Consulta o mapa de keywords do idioma carregado
        auto it = langKeywords.find(val);
        if (it != langKeywords.end()) {
            return it->second;
        }
        return TokenType::ID;
    }
}

#endif