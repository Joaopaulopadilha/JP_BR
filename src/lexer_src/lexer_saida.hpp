// lexer_saida.hpp
// Módulo do lexer para verificação de palavras-chave gerais (verdadeiro, falso) e cadeia de verificação

#ifndef LEXER_SRC_LEXER_SAIDA_HPP
#define LEXER_SRC_LEXER_SAIDA_HPP

#include <string>
#include "../opcodes.hpp"
#include "lexer_condicionais.hpp"
#include "lexer_repeticoes.hpp"
#include "lexer_funcoes.hpp"
#include "lexer_classes.hpp"
#include "lexer_nativos.hpp"
#include "lexer_importar.hpp"

namespace LexerSaida {
    inline TokenType checkKeyword(const std::string& val) {
        if (val == "verdadeiro") return TokenType::TRUE;
        if (val == "falso") return TokenType::FALSE;
        
        // Verifica se é uma palavra reservada de controle (condicionais)
        TokenType condType = LexerCond::checkKeyword(val);
        if (condType != TokenType::ID) return condType;
        
        // Verifica se é uma palavra reservada de repetição
        TokenType repType = LexerRep::checkKeyword(val);
        if (repType != TokenType::ID) return repType;
        
        // Verifica se é uma palavra reservada de função
        TokenType funcType = LexerFunc::checkKeyword(val);
        if (funcType != TokenType::ID) return funcType;
        
        // Verifica se é uma palavra reservada de classe
        TokenType classType = LexerClass::checkKeyword(val);
        if (classType != TokenType::ID) return classType;
        
        // Verifica se é uma palavra reservada de módulo nativo
        TokenType nativoType = LexerNativo::checkKeyword(val);
        if (nativoType != TokenType::ID) return nativoType;
        
        // Verifica se é uma palavra reservada de importação
        TokenType importType = LexerImportar::checkKeyword(val);
        if (importType != TokenType::ID) return importType;

        return TokenType::ID;
    }
}

#endif