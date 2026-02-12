// parser_classes.hpp
// Módulo do parser para análise de classes (declaração, métodos, atributos)

#ifndef PARSER_SRC_PARSER_CLASSES_HPP
#define PARSER_SRC_PARSER_CLASSES_HPP

#include "../parser.hpp"

namespace ParserClass {

    // classe Nome:
    //     funcao metodo(params):
    //         corpo
    inline std::unique_ptr<ClassDeclStmt> parseClassDecl(Parser& p) {
        p.consume(TokenType::CLASSE, "Esperado 'classe'");
        
        // Nome da classe
        Token nameToken = p.consume(TokenType::ID, "Esperado nome da classe");
        std::string className = nameToken.value;
        
        p.consume(TokenType::COLON, "Esperado ':' apos nome da classe");
        
        // Bloco indentado (corpo da classe)
        p.consume(TokenType::INDENT, "Esperado bloco indentado");
        
        auto classDecl = std::make_unique<ClassDeclStmt>(className);
        
        // Parse dos métodos
        while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
            if (p.peek().type == TokenType::FUNCAO) {
                p.consume(TokenType::FUNCAO, "");
                
                // Nome do método
                Token methodToken = p.consumeMemberName("Esperado nome do metodo");
                std::string methodName = methodToken.value;
                
                // Parâmetros
                p.consume(TokenType::LPAREN, "Esperado '(' apos nome do metodo");
                std::vector<std::string> params;
                
                if (p.peek().type != TokenType::RPAREN) {
                    // Verifica se tem tipo opcional
                    if (p.peek().type == TokenType::TYPE_INT || 
                        p.peek().type == TokenType::TYPE_FLOAT ||
                        p.peek().type == TokenType::TYPE_STR ||
                        p.peek().type == TokenType::TYPE_BOOL) {
                        p.consume(p.peek().type, ""); // Consome o tipo (ignorado por enquanto)
                    }
                    
                    Token param = p.consume(TokenType::ID, "Esperado nome do parametro");
                    params.push_back(param.value);
                    
                    while (p.peek().type == TokenType::COMMA) {
                        p.consume(TokenType::COMMA, "");
                        
                        // Tipo opcional
                        if (p.peek().type == TokenType::TYPE_INT || 
                            p.peek().type == TokenType::TYPE_FLOAT ||
                            p.peek().type == TokenType::TYPE_STR ||
                            p.peek().type == TokenType::TYPE_BOOL) {
                            p.consume(p.peek().type, "");
                        }
                        
                        Token nextParam = p.consume(TokenType::ID, "Esperado nome do parametro");
                        params.push_back(nextParam.value);
                    }
                }
                
                p.consume(TokenType::RPAREN, "Esperado ')' apos parametros");
                p.consume(TokenType::COLON, "Esperado ':' apos declaracao do metodo");
                
                // Corpo do método
                p.consume(TokenType::INDENT, "Esperado bloco indentado no metodo");
                auto body = std::make_unique<BlockStmt>();
                
                while (p.peek().type != TokenType::DEDENT && p.peek().type != TokenType::END_OF_FILE) {
                    body->add(p.parseStatement());
                }
                p.consume(TokenType::DEDENT, "Esperado fim de bloco do metodo");
                
                classDecl->addMethod(methodName, std::move(params), std::move(body));
            } else {
                throw std::runtime_error("Esperado 'funcao' dentro da classe");
            }
        }
        
        p.consume(TokenType::DEDENT, "Esperado fim de bloco da classe");
        
        return classDecl;
    }

    // auto.atributo = valor
    inline std::unique_ptr<MemberAssignStmt> parseAutoAssign(Parser& p) {
        p.consume(TokenType::AUTO, "Esperado 'auto'");
        p.consume(TokenType::DOT, "Esperado '.' apos auto");
        
        Token attrToken = p.consume(TokenType::ID, "Esperado nome do atributo");
        std::string attrName = attrToken.value;
        
        p.consume(TokenType::EQUALS, "Esperado '=' apos atributo");
        
        auto value = p.parseExpression();
        
        return std::make_unique<MemberAssignStmt>(
            std::make_unique<AutoExpr>(),
            attrName,
            std::move(value)
        );
    }
}

#endif