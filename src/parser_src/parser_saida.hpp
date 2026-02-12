#ifndef PARSER_SRC_PARSER_SAIDA_HPP
#define PARSER_SRC_PARSER_SAIDA_HPP

#include "../parser.hpp"
#include "../lang_loader.hpp"
#include <string>

namespace ParserSaida {
    
    // Verifica se um identificador é um comando de saída no idioma atual
    inline bool isSaidaCmd(const std::string& val) {
        return val.rfind(langSaidaPrefixo, 0) == 0;
    }
    
    inline std::unique_ptr<ASTNode> parse(Parser& p) {
        Token cmdToken = p.consume(TokenType::ID, langErro("esperado_comando_saida"));
        std::string cmd = cmdToken.value;

        // Configuração Padrão
        bool newLine = true;
        OutputColor color = OutputColor::DEFAULT;

        // Tamanho do prefixo base (ex: "saida" = 5, "output" = 6)
        size_t prefixLen = langSaidaPrefixo.length();
        size_t suffixStart = prefixLen;
        
        // Detecção de sufixo sem quebra de linha (ex: "saidal", "outputl")
        if (cmd.length() > prefixLen && 
            cmd.substr(prefixLen, langSaidaSufixoSemQuebra.length()) == langSaidaSufixoSemQuebra) {
            newLine = false;
            suffixStart = prefixLen + langSaidaSufixoSemQuebra.length();
        }

        // Detecção de Cor (sufixo após o prefixo e eventual "l")
        if (cmd.length() > suffixStart) {
            std::string suffix = cmd.substr(suffixStart);
            
            auto it = langSaidaCores.find(suffix);
            if (it != langSaidaCores.end()) {
                std::string cor = it->second;
                if (cor == "YELLOW") color = OutputColor::YELLOW;
                else if (cor == "RED") color = OutputColor::RED;
                else if (cor == "BLUE") color = OutputColor::BLUE;
                else if (cor == "GREEN") color = OutputColor::GREEN;
            }
        }

        // Conteúdo
        p.consume(TokenType::LPAREN, langErro("esperado", {{"valor", "("}}));
        
        // Coleta todos os argumentos separados por vírgula
        std::vector<std::unique_ptr<ASTNode>> args;
        
        if (p.peek().type != TokenType::RPAREN) {
            args.push_back(p.parseExpression());
            
            while (p.peek().type == TokenType::COMMA) {
                p.consume(TokenType::COMMA, "");
                args.push_back(p.parseExpression());
            }
        }
        
        p.consume(TokenType::RPAREN, langErro("esperado", {{"valor", ")"}}));
        
        // Se tiver múltiplos argumentos, concatena com ADD
        std::unique_ptr<ASTNode> expr;
        if (args.empty()) {
            expr = std::make_unique<LiteralExpr>(std::string(""));
        } else if (args.size() == 1) {
            expr = std::move(args[0]);
        } else {
            expr = std::move(args[0]);
            for (size_t i = 1; i < args.size(); i++) {
                expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(args[i]), OpCode::ADD);
            }
        }

        return std::make_unique<SaidaStmt>(std::move(expr), color, newLine);
    }
}

#endif