// ast_expressoes.hpp
// Nós de expressões do AST (literais, variáveis, operações binárias)

#ifndef AST_SRC_AST_EXPRESSOES_HPP
#define AST_SRC_AST_EXPRESSOES_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>

// Forward declaration
struct ASTNode;

// Literal: número, string, bool
struct LiteralExpr : public ASTNode {
    Value val;
    LiteralExpr(Value v) : val(std::move(v)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        bytecode.push_back({OpCode::LOAD_CONST, val});
    }
};

// Variável: carrega valor de uma variável
struct VarExpr : public ASTNode {
    std::string name;
    VarExpr(std::string n) : name(std::move(n)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        bytecode.push_back({OpCode::LOAD_VAR, name});
    }
};

// Expressão Binária Genérica (Matemática, Lógica e Comparação)
struct BinaryExpr : public ASTNode {
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    OpCode op;
    
    BinaryExpr(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r, OpCode opcode)
        : left(std::move(l)), right(std::move(r)), op(opcode) {}

    void compile(std::vector<Instruction>& bytecode) override {
        left->compile(bytecode);
        right->compile(bytecode);
        bytecode.push_back({op, std::nullopt});
    }
};

#endif
