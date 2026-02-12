// parser_funcoes.hpp
// Módulo do parser para análise de funções (declaração, chamada, retorno)

#ifndef PARSER_SRC_PARSER_FUNCOES_HPP
#define PARSER_SRC_PARSER_FUNCOES_HPP

#include "../parser.hpp"

namespace ParserFunc {

    // funcao nome(param1, param2, ...):
    //     corpo
    inline std::unique_ptr<FuncDeclStmt> parseFuncDecl(Parser& p) {
        p.consume(TokenType::FUNCAO, "Esperado 'funcao'");
        
        // Nome da função
        Token nameToken = p.consume(TokenType::ID, "Esperado nome da funcao");
        std::string funcName = nameToken.value;
        
        // Parâmetros
        p.consume(TokenType::LPAREN, "Esperado '(' apos nome da funcao");
        std::vector<std::string> params;
        
        if (p.peek().type != TokenType::RPAREN) {
            // Primeiro parâmetro
            Token param = p.consume(TokenType::ID, "Esperado nome do parametro");
            params.push_back(param.value);
            
            // Parâmetros adicionais
            while (p.peek().type == TokenType::COMMA) {
                p.consume(TokenType::COMMA, "");
                Token nextParam = p.consume(TokenType::ID, "Esperado nome do parametro");
                params.push_back(nextParam.value);
            }
        }
        
        p.consume(TokenType::RPAREN, "Esperado ')' apos parametros");
        p.consume(TokenType::COLON, "Esperado ':' apos declaracao de funcao");
        
        // Bloco indentado (corpo)
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        auto body = std::make_unique<BlockStmt>();
        
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            body->add(p.parseStatement());
        }
        p.consume(TokenType::DEDENT, "Esperado fim de bloco (DEDENT)");
        
        return std::make_unique<FuncDeclStmt>(funcName, std::move(params), std::move(body));
    }

    // nome(arg1, arg2, ...)
    inline std::unique_ptr<FuncCallExpr> parseFuncCall(Parser& p) {
        // Nome da função já foi verificado antes de chamar
        Token nameToken = p.consume(TokenType::ID, "Esperado nome da funcao");
        std::string funcName = nameToken.value;
        
        p.consume(TokenType::LPAREN, "Esperado '(' apos nome da funcao");
        std::vector<std::unique_ptr<ASTNode>> args;
        
        if (p.peek().type != TokenType::RPAREN) {
            // Primeiro argumento
            args.push_back(p.parseExpression());
            
            // Argumentos adicionais
            while (p.peek().type == TokenType::COMMA) {
                p.consume(TokenType::COMMA, "");
                args.push_back(p.parseExpression());
            }
        }
        
        p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
        
        return std::make_unique<FuncCallExpr>(funcName, std::move(args));
    }

    // retorna expressao
    inline std::unique_ptr<ReturnStmt> parseReturn(Parser& p) {
        p.consume(TokenType::RETORNA, "Esperado 'retorna'");
        
        // Expressão de retorno (opcional em alguns casos)
        std::unique_ptr<ASTNode> expr = nullptr;
        
        // Se não for fim de linha/bloco, tem expressão
        if (p.peek().type != TokenType::DEDENT && 
            p.peek().type != TokenType::END_OF_FILE &&
            p.peek().type != TokenType::INDENT) {
            expr = p.parseExpression();
        }
        
        return std::make_unique<ReturnStmt>(std::move(expr));
    }
}

#endif
