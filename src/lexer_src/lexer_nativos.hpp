// lexer_nativos.hpp
// Módulo do lexer para reconhecimento de palavras-chave de módulos nativos

#ifndef LEXER_SRC_LEXER_NATIVOS_HPP
#define LEXER_SRC_LEXER_NATIVOS_HPP

#include <string>
#include "../opcodes.hpp"

namespace LexerNativo {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "nativo") return TokenType::NATIVO;
        if (val == "importar") return TokenType::IMPORTAR;
        
        return TokenType::ID;
    }
}

#endif
