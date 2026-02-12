// parser_repeticoes.hpp
// Módulo do parser para análise de estruturas de repetição (loop, enquanto, repetir, para, parar, continuar)

#ifndef PARSER_SRC_PARSER_REPETICOES_HPP
#define PARSER_SRC_PARSER_REPETICOES_HPP

#include "../parser.hpp"

namespace ParserRep {

    // loop:
    //     corpo
    inline std::unique_ptr<LoopStmt> parseLoop(Parser& p) {
        p.consume(TokenType::LOOP, "Esperado 'loop'");
        p.consume(TokenType::COLON, "Esperado ':' apos loop");
        
        // Bloco indentado
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        auto body = std::make_unique<BlockStmt>();
        
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            body->add(p.parseStatement());
        }
        p.consume(TokenType::DEDENT, "Esperado fim de bloco (DEDENT)");
        
        return std::make_unique<LoopStmt>(std::move(body));
    }

    // enquanto condição:
    //     corpo
    inline std::unique_ptr<WhileStmt> parseWhile(Parser& p) {
        p.consume(TokenType::ENQUANTO, "Esperado 'enquanto'");
        
        // Condição
        auto condition = p.parseExpression();
        p.consume(TokenType::COLON, "Esperado ':' apos condicao");
        
        // Bloco indentado
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        auto body = std::make_unique<BlockStmt>();
        
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            body->add(p.parseStatement());
        }
        p.consume(TokenType::DEDENT, "Esperado fim de bloco (DEDENT)");
        
        return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
    }

    // repetir(n):
    //     corpo
    inline std::unique_ptr<RepeatStmt> parseRepeat(Parser& p) {
        p.consume(TokenType::REPETIR, "Esperado 'repetir'");
        p.consume(TokenType::LPAREN, "Esperado '(' apos repetir");
        
        // Quantidade de repetições
        auto count = p.parseExpression();
        
        p.consume(TokenType::RPAREN, "Esperado ')' apos quantidade");
        p.consume(TokenType::COLON, "Esperado ':' apos repetir(n)");
        
        // Bloco indentado
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        auto body = std::make_unique<BlockStmt>();
        
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            body->add(p.parseStatement());
        }
        p.consume(TokenType::DEDENT, "Esperado fim de bloco (DEDENT)");
        
        return std::make_unique<RepeatStmt>(std::move(count), std::move(body));
    }

    // para i em intervalo(inicio, fim, passo):
    //     corpo
    // ou
    // para i em intervalo(inicio, fim):
    //     corpo (passo = 1)
    inline std::unique_ptr<ForStmt> parseFor(Parser& p) {
        p.consume(TokenType::PARA, "Esperado 'para'");
        
        // Variável do iterador
        Token varToken = p.consume(TokenType::ID, "Esperado nome da variavel");
        std::string varName = varToken.value;
        
        p.consume(TokenType::EM, "Esperado 'em' apos variavel");
        p.consume(TokenType::INTERVALO, "Esperado 'intervalo'");
        p.consume(TokenType::LPAREN, "Esperado '(' apos intervalo");
        
        // Início
        auto start = p.parseExpression();
        p.consume(TokenType::COMMA, "Esperado ',' apos inicio");
        
        // Fim
        auto end = p.parseExpression();
        
        // Passo (opcional, padrão = 1)
        std::unique_ptr<ASTNode> step;
        if (p.peek().type == TokenType::COMMA) {
            p.consume(TokenType::COMMA, "");
            step = p.parseExpression();
        } else {
            step = std::make_unique<LiteralExpr>((long)1);
        }
        
        p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
        p.consume(TokenType::COLON, "Esperado ':' apos intervalo");
        
        // Bloco indentado
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        auto body = std::make_unique<BlockStmt>();
        
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            body->add(p.parseStatement());
        }
        p.consume(TokenType::DEDENT, "Esperado fim de bloco (DEDENT)");
        
        return std::make_unique<ForStmt>(varName, std::move(start), std::move(end), std::move(step), std::move(body));
    }

    // parar
    inline std::unique_ptr<BreakStmt> parseBreak(Parser& p) {
        p.consume(TokenType::PARAR, "Esperado 'parar'");
        return std::make_unique<BreakStmt>();
    }

    // continuar
    inline std::unique_ptr<ContinueStmt> parseContinue(Parser& p) {
        p.consume(TokenType::CONTINUAR, "Esperado 'continuar'");
        return std::make_unique<ContinueStmt>();
    }
}

#endif
