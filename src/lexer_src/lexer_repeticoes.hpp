// lexer_repeticoes.hpp
// Módulo do lexer para reconhecimento de palavras-chave de repetição (loop, enquanto, repetir, para, parar, continuar)

#ifndef LEXER_SRC_LEXER_REPETICOES_HPP
#define LEXER_SRC_LEXER_REPETICOES_HPP

#include <string>
#include "../opcodes.hpp"

namespace LexerRep {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "loop") return TokenType::LOOP;
        if (val == "enquanto") return TokenType::ENQUANTO;
        if (val == "repetir") return TokenType::REPETIR;
        if (val == "para") return TokenType::PARA;
        if (val == "em") return TokenType::EM;
        if (val == "intervalo") return TokenType::INTERVALO;
        if (val == "parar") return TokenType::PARAR;
        if (val == "continuar") return TokenType::CONTINUAR;
        
        // Se não for palavra de repetição, retorna ID
        return TokenType::ID;
    }
}

#endif
