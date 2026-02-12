// lexer_funcoes.hpp
// Módulo do lexer para reconhecimento de palavras-chave de funções (funcao, retorna)

#ifndef LEXER_SRC_LEXER_FUNCOES_HPP
#define LEXER_SRC_LEXER_FUNCOES_HPP

#include <string>
#include "../opcodes.hpp"

namespace LexerFunc {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "funcao") return TokenType::FUNCAO;
        if (val == "retorna") return TokenType::RETORNA;
        
        // Se não for palavra de função, retorna ID
        return TokenType::ID;
    }
}

#endif
