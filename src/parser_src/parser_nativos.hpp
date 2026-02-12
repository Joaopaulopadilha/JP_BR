// parser_nativos.hpp
// Módulo do parser para análise de importações nativas

#ifndef PARSER_SRC_PARSER_NATIVOS_HPP
#define PARSER_SRC_PARSER_NATIVOS_HPP

#include "../parser.hpp"

namespace ParserNativo {

    // nativo "caminho/arquivo.jpd" importar func1(n), func2(n), ...
    // ou: nativo "caminho/arquivo.jpd" importar func1, func2, ...  (sem aridade)
    inline std::unique_ptr<NativeImportStmt> parseNativeImport(Parser& p) {
        p.consume(TokenType::NATIVO, "Esperado 'nativo'");
        
        // Caminho da DLL
        Token pathToken = p.consume(TokenType::STRING, "Esperado caminho da biblioteca entre aspas");
        std::string dllPath = pathToken.value;
        
        p.consume(TokenType::IMPORTAR, "Esperado 'importar' apos caminho da biblioteca");
        
        auto importStmt = std::make_unique<NativeImportStmt>(dllPath);
        
        // Lista de funções: nome(numArgs) ou nome, nome2, ...
        do {
            // Se tem vírgula, consome
            if (importStmt->functions.size() > 0) {
                p.consume(TokenType::COMMA, "Esperado ',' entre funcoes");
            }
            
            // Nome da função
            Token funcToken = p.consume(TokenType::ID, "Esperado nome da funcao");
            std::string funcName = funcToken.value;
            
            // Número de argumentos (opcional)
            int numArgs = -1; // -1 = aridade resolvida na chamada
            if (p.peek().type == TokenType::LPAREN) {
                p.consume(TokenType::LPAREN, "");
                Token numArgsToken = p.consume(TokenType::NUMBER_INT, "Esperado numero de argumentos");
                numArgs = std::stoi(numArgsToken.value);
                p.consume(TokenType::RPAREN, "Esperado ')' apos numero de argumentos");
            }
            
            importStmt->addFunction(funcName, numArgs);
            
        } while (p.peek().type == TokenType::COMMA);
        
        return importStmt;
    }

    // Verifica se um nome é uma função nativa e retorna a chamada
    inline bool isNativeFunc(const std::string& name) {
        return nativeFuncTable.find(name) != nativeFuncTable.end();
    }

    // Parseia chamada de função nativa: nome(args)
    inline std::unique_ptr<NativeCallExpr> parseNativeCall(Parser& p) {
        Token nameToken = p.consume(TokenType::ID, "Esperado nome da funcao");
        std::string funcName = nameToken.value;
        
        p.consume(TokenType::LPAREN, "Esperado '(' apos nome da funcao");
        
        std::vector<std::unique_ptr<ASTNode>> args;
        
        if (p.peek().type != TokenType::RPAREN) {
            args.push_back(p.parseExpression());
            
            while (p.peek().type == TokenType::COMMA) {
                p.consume(TokenType::COMMA, "");
                args.push_back(p.parseExpression());
            }
        }
        
        p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
        
        return std::make_unique<NativeCallExpr>(funcName, std::move(args));
    }
}

#endif