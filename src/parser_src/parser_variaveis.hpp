#ifndef PARSER_SRC_PARSER_VARIAVEIS_HPP
#define PARSER_SRC_PARSER_VARIAVEIS_HPP

#include "../parser.hpp"

namespace ParserVar {
    inline std::unique_ptr<ASTNode> parse(Parser& p) {
        Token varName = p.consume(TokenType::ID, "Esperado ID");
        p.consume(TokenType::EQUALS, "Esperado '='");
        
        auto expr = p.parseExpression();
        
        return std::make_unique<AssignStmt>(varName.value, std::move(expr));
    }
}

#endif