// ast_statements.hpp
// Nós de comandos básicos do AST (atribuição, saída, bloco, expressão como statement)

#ifndef AST_SRC_AST_STATEMENTS_HPP
#define AST_SRC_AST_STATEMENTS_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>

// Forward declaration
struct ASTNode;

// Enum para cores de saída
enum class OutputColor { DEFAULT, RED, GREEN, BLUE, YELLOW };

// Atribuição: nome = expressão
struct AssignStmt : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> expression;
    
    AssignStmt(std::string n, std::unique_ptr<ASTNode> expr) 
        : name(std::move(n)), expression(std::move(expr)) {}
        
    void compile(std::vector<Instruction>& bytecode) override {
        expression->compile(bytecode);
        bytecode.push_back({OpCode::STORE_VAR, name});
    }
};

// Expressão como statement (executa e descarta o resultado)
struct ExpressionStmt : public ASTNode {
    std::unique_ptr<ASTNode> expression;
    
    ExpressionStmt(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        expression->compile(bytecode);
        bytecode.push_back({OpCode::POP, std::nullopt});
    }
};

// Saída: saida, saida_vermelho, etc.
struct SaidaStmt : public ASTNode {
    std::unique_ptr<ASTNode> expression;
    OutputColor color;
    bool newLine;

    SaidaStmt(std::unique_ptr<ASTNode> expr, OutputColor c, bool nl) 
        : expression(std::move(expr)), color(c), newLine(nl) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        expression->compile(bytecode);
        
        OpCode op = OpCode::PRINT_NL;
        if (newLine) {
            switch (color) {
                case OutputColor::RED:    op = OpCode::PRINT_RED_NL; break;
                case OutputColor::GREEN:  op = OpCode::PRINT_GREEN_NL; break;
                case OutputColor::BLUE:   op = OpCode::PRINT_BLUE_NL; break;
                case OutputColor::YELLOW: op = OpCode::PRINT_YELLOW_NL; break;
                default:                  op = OpCode::PRINT_NL; break;
            }
        } else {
            switch (color) {
                case OutputColor::RED:    op = OpCode::PRINT_RED_NO_NL; break;
                case OutputColor::GREEN:  op = OpCode::PRINT_GREEN_NO_NL; break;
                case OutputColor::BLUE:   op = OpCode::PRINT_BLUE_NO_NL; break;
                case OutputColor::YELLOW: op = OpCode::PRINT_YELLOW_NO_NL; break;
                default:                  op = OpCode::PRINT_NO_NL; break;
            }
        }
        bytecode.push_back({op, std::nullopt});
    }
};

// Bloco: conjunto de statements
struct BlockStmt : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
    
    void add(std::unique_ptr<ASTNode> stmt) {
        statements.push_back(std::move(stmt));
    }
    
    void compile(std::vector<Instruction>& bytecode) override {
        for (const auto& stmt : statements) {
            stmt->compile(bytecode);
        }
    }
};

#endif
