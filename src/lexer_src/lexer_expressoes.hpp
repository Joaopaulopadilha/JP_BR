// lexer_expressoes.hpp
// Módulo do lexer para leitura de strings e números

#ifndef LEXER_SRC_LEXER_EXPRESSOES_HPP
#define LEXER_SRC_LEXER_EXPRESSOES_HPP

#include <string>
#include <string_view>
#include <stdexcept>
#include "../opcodes.hpp"

namespace LexerExpr {

    inline Token readString(std::string_view src, size_t& pos) {
        char quote = src[pos]; // Pode ser " ou '
        
        // Verifica se é aspas triplas
        bool isTriple = false;
        if (pos + 2 < src.length() && src[pos + 1] == quote && src[pos + 2] == quote) {
            isTriple = true;
            pos += 3; // Pula as três aspas de abertura
        } else {
            pos++; // Pula a aspas de abertura simples
        }
        
        std::string value;
        
        if (isTriple) {
            // String com aspas triplas - termina com três aspas iguais
            // Não processa escapes - é literal puro
            while (pos < src.length()) {
                // Verifica se encontrou as três aspas de fechamento
                if (pos + 2 < src.length() && 
                    src[pos] == quote && src[pos + 1] == quote && src[pos + 2] == quote) {
                    pos += 3; // Pula as três aspas de fechamento
                    return {TokenType::STRING_RAW, value, 0}; // STRING_RAW não interpola
                }
                value += src[pos++];
            }
            throw std::runtime_error("String com aspas triplas nao terminada");
        } else {
            // String com aspas simples - termina com uma aspas igual
            while (pos < src.length() && src[pos] != quote) {
                // Suporte a escape básico
                if (src[pos] == '\\' && pos + 1 < src.length()) {
                    char next = src[pos + 1];
                    // Só interpreta escapes conhecidos
                    if (next == 'n') { value += '\n'; pos += 2; }
                    else if (next == 't') { value += '\t'; pos += 2; }
                    else if (next == 'r') { value += '\r'; pos += 2; }
                    else if (next == '\\') { value += '\\'; pos += 2; }
                    else if (next == '"') { value += '"'; pos += 2; }
                    else if (next == '\'') { value += '\''; pos += 2; }
                    else {
                        // Não é escape conhecido, preserva a barra
                        value += src[pos++];
                    }
                } else {
                    value += src[pos++];
                }
            }
            if (pos >= src.length()) throw std::runtime_error("String nao terminada");
            pos++; // Pula aspas de fechamento
            return {TokenType::STRING, value, 0};
        }
    }

    inline Token readNumber(std::string_view src, size_t& pos) {
        size_t start = pos;
        bool isFloat = false;
        while (pos < src.length() && (std::isdigit(src[pos]) || src[pos] == '.')) {
            if (src[pos] == '.') {
                if (isFloat) throw std::runtime_error("Numero com multiplos pontos");
                isFloat = true;
            }
            pos++;
        }
        std::string val(src.substr(start, pos - start));
        return {isFloat ? TokenType::NUMBER_FLOAT : TokenType::NUMBER_INT, val, 0}; // Linha será preenchida pelo Lexer
    }
}

#endif