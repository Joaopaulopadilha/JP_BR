// lexer.hpp
// Lexer principal - tokenização do código fonte JPLang

#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>
#include <string_view>
#include <cctype>
#include <stdexcept>
#include <iostream>

// Inclui as definições globais (TokenType, Token)
#include "opcodes.hpp"

// Módulos
#include "lexer_src/lexer_expressoes.hpp"
#include "lexer_src/lexer_variaveis.hpp"
#include "lexer_src/lexer_condicionais.hpp"
#include "lexer_src/lexer_repeticoes.hpp"
#include "lexer_src/lexer_classes.hpp"
#include "lexer_src/lexer_nativos.hpp"
#include "lexer_src/lexer_importar.hpp"

class Lexer {
public:
    explicit Lexer(std::string_view source) : src(source), pos(0), currentLine(1), contextDepth(0) {
        indentStack.push_back(0); // Nível base
    }

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        
        while (pos < src.length()) {
            char current = src[pos];

            // 1. Indentação (SOMENTE se não estiver dentro de [], () ou {})
            if (atLineStart && contextDepth == 0) {
                int spaces = 0;
                size_t tempPos = pos;
                
                // Conta espaços no início da linha
                while (tempPos < src.length() && src[tempPos] == ' ') {
                    spaces++;
                    tempPos++;
                }
                
                // Verifica se a linha não está vazia e não é comentário
                if (tempPos < src.length() && src[tempPos] != '\n' && src[tempPos] != '\r' && src[tempPos] != '#') {
                    pos = tempPos; 
                    current = src[pos];

                    if (spaces > indentStack.back()) {
                        indentStack.push_back(spaces);
                        tokens.push_back({TokenType::INDENT, "INDENT", currentLine});
                    } else if (spaces < indentStack.back()) {
                        while (spaces < indentStack.back()) {
                            indentStack.pop_back();
                            tokens.push_back({TokenType::DEDENT, "DEDENT", currentLine});
                        }
                        if (spaces != indentStack.back()) {
                            throw std::runtime_error(langErroLinha(currentLine, "indentacao_invalida"));
                        }
                    }
                }
                atLineStart = false;
            }
            
            // Se estiver dentro de contexto, apenas marca que não está mais no início da linha
            if (atLineStart && contextDepth > 0) {
                atLineStart = false;
            }

            // 2. Ignorar espaços
            if (current == ' ' || current == '\t') {
                pos++; continue;
            }
            
            // Ignorar \r (Windows)
            if (current == '\r') {
                pos++; continue;
            }

            // 3. Nova Linha
            if (current == '\n') {
                // Só marca como início de linha se não estiver dentro de contexto
                if (contextDepth == 0) {
                    atLineStart = true;
                }
                currentLine++;
                pos++; continue;
            }

            // 4. Comentários
            if (current == '#') {
                while (pos < src.length() && src[pos] != '\n') pos++;
                continue;
            }

            // 5. Símbolos
            if (current == ':') { tokens.push_back({TokenType::COLON, ":", currentLine}); pos++; continue; }
            if (current == ',') { tokens.push_back({TokenType::COMMA, ",", currentLine}); pos++; continue; }
            if (current == '.') { tokens.push_back({TokenType::DOT, ".", currentLine}); pos++; continue; }
            if (current == '+') { tokens.push_back({TokenType::PLUS, "+", currentLine}); pos++; continue; }
            if (current == '-') { tokens.push_back({TokenType::MINUS, "-", currentLine}); pos++; continue; }
            if (current == '*') { tokens.push_back({TokenType::STAR, "*", currentLine}); pos++; continue; }
            if (current == '/') { tokens.push_back({TokenType::SLASH, "/", currentLine}); pos++; continue; }
            if (current == '%') { tokens.push_back({TokenType::PERCENT, "%", currentLine}); pos++; continue; }
            
            // Parênteses, colchetes - incrementa/decrementa profundidade
            if (current == '(') { 
                tokens.push_back({TokenType::LPAREN, "(", currentLine}); 
                contextDepth++; 
                pos++; 
                continue; 
            }
            if (current == ')') { 
                tokens.push_back({TokenType::RPAREN, ")", currentLine}); 
                if (contextDepth > 0) contextDepth--; 
                pos++; 
                continue; 
            }
            if (current == '[') { 
                tokens.push_back({TokenType::LBRACKET, "[", currentLine}); 
                contextDepth++; 
                pos++; 
                continue; 
            }
            if (current == ']') { 
                tokens.push_back({TokenType::RBRACKET, "]", currentLine}); 
                if (contextDepth > 0) contextDepth--; 
                pos++; 
                continue; 
            }
            
            // Operadores de comparação (verificar dois caracteres primeiro)
            if (current == '>' && pos + 1 < src.length() && src[pos + 1] == '=') {
                tokens.push_back({TokenType::GTE, ">=", currentLine}); pos += 2; continue;
            }
            if (current == '<' && pos + 1 < src.length() && src[pos + 1] == '=') {
                tokens.push_back({TokenType::LTE, "<=", currentLine}); pos += 2; continue;
            }
            if (current == '!' && pos + 1 < src.length() && src[pos + 1] == '=') {
                tokens.push_back({TokenType::NEQ, "!=", currentLine}); pos += 2; continue;
            }
            if (current == '>') { tokens.push_back({TokenType::GT, ">", currentLine}); pos++; continue; }
            if (current == '<') { tokens.push_back({TokenType::LT, "<", currentLine}); pos++; continue; }
            
            if (current == '=') { 
                if (pos + 1 < src.length() && src[pos + 1] == '=') {
                    tokens.push_back({TokenType::EQ_OP, "==", currentLine}); pos += 2;
                } else {
                    tokens.push_back({TokenType::EQUALS, "=", currentLine}); pos++;
                }
                continue; 
            }

            // 6. Módulos
            if (current == '"' || current == '\'') {
                Token tok = LexerExpr::readString(src, pos);
                tok.line = currentLine;
                tokens.push_back(tok);
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(current))) {
                Token tok = LexerExpr::readNumber(src, pos);
                tok.line = currentLine;
                tokens.push_back(tok);
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(current))) {
                Token tok = LexerVar::readIdentifier(src, pos);
                tok.line = currentLine;
                tokens.push_back(tok);
                continue;
            }

            throw std::runtime_error(langErroLinha(currentLine, "caractere_inesperado", {{"valor", std::string(1, current)}}));
        }

        while (indentStack.size() > 1) {
            indentStack.pop_back();
            tokens.push_back({TokenType::DEDENT, "DEDENT", currentLine});
        }
        tokens.push_back({TokenType::END_OF_FILE, "", currentLine});
        
        return tokens;
    }

private:
    std::string_view src;
    size_t pos;
    int currentLine;
    std::vector<int> indentStack;
    bool atLineStart = true;
    int contextDepth;  // Contador de profundidade de [], () e {}
};

#endif