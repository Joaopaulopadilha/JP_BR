#ifndef PARSER_SRC_PARSER_SAIDA_HPP
#define PARSER_SRC_PARSER_SAIDA_HPP

#include "../parser.hpp"
#include <string>

namespace ParserSaida {
    
    inline std::unique_ptr<ASTNode> parse(Parser& p) {
        Token cmdToken = p.consume(TokenType::ID, "Esperado comando saida");
        std::string cmd = cmdToken.value;

        // Configuração Padrão
        bool newLine = true;
        OutputColor color = OutputColor::DEFAULT;

        // Detecção de 'saidal' (sem new line)
        size_t suffixStart = 5; // len("saida")
        if (cmd.length() > 5 && cmd[5] == 'l') {
            newLine = false;
            suffixStart = 6; // len("saidal")
        }

        // Detecção de Cor (Sufixo)
        std::string suffix = "";
        if (cmd.length() > suffixStart) {
            suffix = cmd.substr(suffixStart);
        }

        if (suffix == "_amarelo") color = OutputColor::YELLOW;
        else if (suffix == "_vermelho") color = OutputColor::RED;
        else if (suffix == "_azul") color = OutputColor::BLUE;
        else if (suffix == "_verde") color = OutputColor::GREEN;

        // Conteúdo
        p.consume(TokenType::LPAREN, "Esperado '('");
        
        // Coleta todos os argumentos separados por vírgula
        std::vector<std::unique_ptr<ASTNode>> args;
        
        if (p.peek().type != TokenType::RPAREN) {
            args.push_back(p.parseExpression());
            
            while (p.peek().type == TokenType::COMMA) {
                p.consume(TokenType::COMMA, "");
                args.push_back(p.parseExpression());
            }
        }
        
        p.consume(TokenType::RPAREN, "Esperado ')'");
        
        // Se tiver múltiplos argumentos, concatena com ADD
        std::unique_ptr<ASTNode> expr;
        if (args.empty()) {
            // Sem argumentos, imprime string vazia
            expr = std::make_unique<LiteralExpr>(std::string(""));
        } else if (args.size() == 1) {
            // Um argumento, usa direto
            expr = std::move(args[0]);
        } else {
            // Múltiplos argumentos, concatena todos
            expr = std::move(args[0]);
            for (size_t i = 1; i < args.size(); i++) {
                expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(args[i]), OpCode::ADD);
            }
        }

        return std::make_unique<SaidaStmt>(std::move(expr), color, newLine);
    }
}

#endif