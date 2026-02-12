// lexer_condicionais.hpp
// Módulo do lexer para reconhecimento de palavras-chave de condicionais (se, senao, ou_se, e, ou)

#ifndef LEXER_SRC_LEXER_CONDICIONAIS_HPP
#define LEXER_SRC_LEXER_CONDICIONAIS_HPP

#include <string>
#include "../opcodes.hpp"

namespace LexerCond {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "se") return TokenType::SE;
        if (val == "senao") return TokenType::SENAO;
        if (val == "ou_se") return TokenType::OU_SE;
        if (val == "e") return TokenType::AND;
        if (val == "ou") return TokenType::OR;
        
        // Se não for condicional, retorna ID para ser processado normalmente
        return TokenType::ID; 
    }
}

#endif