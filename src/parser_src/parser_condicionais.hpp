#ifndef PARSER_SRC_PARSER_CONDICIONAIS_HPP
#define PARSER_SRC_PARSER_CONDICIONAIS_HPP

#include "../parser.hpp"

namespace ParserCond {
    
    // Declaração antecipada
    inline std::unique_ptr<IfStmt> parseIf(Parser& p);

    inline std::unique_ptr<IfStmt> parseIf(Parser& p) {
        // 1. Consome a palavra chave inicial ('se', 'ou_se')
        // Se viermos de um 'senao se', o token atual já é 'se', então consumimos.
        if (p.peek().type == TokenType::SE) {
            p.consume(TokenType::SE, "");
        } 
        else if (p.peek().type == TokenType::OU_SE) {
            p.consume(TokenType::OU_SE, "");
        }

        // 2. Condição (ex: x > y)
        auto cond = p.parseExpression();
        p.consume(TokenType::COLON, "Esperado ':' apos condicao");

        // 3. Bloco Identado (THEN)
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        auto thenBlock = std::make_unique<BlockStmt>();
        
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            thenBlock->add(p.parseStatement());
        }
        p.consume(TokenType::DEDENT, "Esperado fim de bloco (DEDENT)");

        // 4. Lógica de Encadeamento (ELSE / ELIF)
        std::unique_ptr<BlockStmt> elseBlock = nullptr;
        
        // Caso: 'ou_se'
        if (p.peek().type == TokenType::OU_SE) {
            elseBlock = std::make_unique<BlockStmt>();
            elseBlock->add(parseIf(p)); // Recursão
        } 
        // Caso: 'senao'
        else if (p.peek().type == TokenType::SENAO) {
            p.consume(TokenType::SENAO, "");
            
            // SUPORTE PARA 'senao se'
            // Se após 'senao' vier um 'se', tratamos como 'ou_se'
            if (p.peek().type == TokenType::SE) {
                elseBlock = std::make_unique<BlockStmt>();
                elseBlock->add(parseIf(p)); // Recursão consome o 'se'
            }
            // Caso padrão: 'senao:'
            else {
                p.consume(TokenType::COLON, "Esperado ':' apos senao");
                p.consume(TokenType::INDENT, "Esperado bloco indentado no senao");
                
                elseBlock = std::make_unique<BlockStmt>();
                while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
                    elseBlock->add(p.parseStatement());
                }
                p.consume(TokenType::DEDENT, "DEDENT no senao");
            }
        }

        return std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock), std::move(elseBlock));
    }
}
#endif