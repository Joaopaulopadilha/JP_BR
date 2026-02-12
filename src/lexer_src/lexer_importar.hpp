// lexer_importar.hpp
// Módulo do lexer para reconhecimento de palavras-chave de importação

#ifndef LEXER_SRC_LEXER_IMPORTAR_HPP
#define LEXER_SRC_LEXER_IMPORTAR_HPP

#include <string>
#include "../opcodes.hpp"

namespace LexerImportar {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "de") return TokenType::DE;
        if (val == "como") return TokenType::COMO;
        
        // "importar" já está definido em lexer_nativos.hpp
        
        return TokenType::ID;
    }
}

#endif
