// lexer_classes.hpp
// Módulo do lexer para reconhecimento de palavras-chave de classes (classe, auto)

#ifndef LEXER_SRC_LEXER_CLASSES_HPP
#define LEXER_SRC_LEXER_CLASSES_HPP

#include <string>
#include "../opcodes.hpp"

namespace LexerClass {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "classe") return TokenType::CLASSE;
        if (val == "auto") return TokenType::AUTO;
        
        // Tipos primitivos (opcionais nas declarações)
        if (val == "int") return TokenType::TYPE_INT;
        if (val == "float") return TokenType::TYPE_FLOAT;
        if (val == "str") return TokenType::TYPE_STR;
        if (val == "bool") return TokenType::TYPE_BOOL;
        
        // Se não for palavra de classe, retorna ID
        return TokenType::ID;
    }
}

#endif
