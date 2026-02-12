// lexer_variaveis.hpp
// Módulo do lexer para leitura de identificadores e palavras reservadas

#ifndef LEXER_SRC_LEXER_VARIAVEIS_HPP
#define LEXER_SRC_LEXER_VARIAVEIS_HPP

#include <string>
#include <string_view>
#include <cctype>
#include "lexer_saida.hpp" // Importante: usa o verificador de keywords

namespace LexerVar {
    inline Token readIdentifier(std::string_view src, size_t& pos) {
        size_t start = pos;
        // Permite letras, números e underscore (mas deve começar com letra, verificado antes)
        while (pos < src.length() && (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_')) {
            pos++;
        }
        std::string val(src.substr(start, pos - start));
        
        // Verifica se é uma palavra reservada (ex: verdadeiro, falso)
        TokenType type = LexerSaida::checkKeyword(val);
        
        return {type, val, 0}; // Linha será preenchida pelo Lexer
    }
}

#endif