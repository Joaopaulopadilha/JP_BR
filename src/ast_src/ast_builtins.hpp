// ast_builtins.hpp
// Nós de funções built-in do AST (entrada, inteiro, decimal, texto)

#ifndef AST_SRC_AST_BUILTINS_HPP
#define AST_SRC_AST_BUILTINS_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>

// Forward declaration
struct ASTNode;

// Expressão entrada(prompt) - lê entrada do usuário
struct InputExpr : public ASTNode {
    std::unique_ptr<ASTNode> prompt;
    
    InputExpr(std::unique_ptr<ASTNode> p) : prompt(std::move(p)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        // Compila o prompt (vai para a pilha)
        prompt->compile(bytecode);
        // Chama INPUT - pop prompt, push resultado
        bytecode.push_back({OpCode::INPUT, std::nullopt});
    }
};

// Expressão inteiro(valor) - converte para inteiro
struct ToIntExpr : public ASTNode {
    std::unique_ptr<ASTNode> value;
    
    ToIntExpr(std::unique_ptr<ASTNode> v) : value(std::move(v)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        value->compile(bytecode);
        bytecode.push_back({OpCode::TO_INT, std::nullopt});
    }
};

// Expressão decimal(valor) - converte para decimal/float
struct ToFloatExpr : public ASTNode {
    std::unique_ptr<ASTNode> value;
    
    ToFloatExpr(std::unique_ptr<ASTNode> v) : value(std::move(v)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        value->compile(bytecode);
        bytecode.push_back({OpCode::TO_FLOAT, std::nullopt});
    }
};

// Expressão texto(valor) - converte para string
struct ToStringExpr : public ASTNode {
    std::unique_ptr<ASTNode> value;
    
    ToStringExpr(std::unique_ptr<ASTNode> v) : value(std::move(v)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        value->compile(bytecode);
        bytecode.push_back({OpCode::TO_STRING, std::nullopt});
    }
};

// Expressão booleano(valor) - converte para booleano
struct ToBoolExpr : public ASTNode {
    std::unique_ptr<ASTNode> value;
    
    ToBoolExpr(std::unique_ptr<ASTNode> v) : value(std::move(v)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        value->compile(bytecode);
        bytecode.push_back({OpCode::TO_BOOL, std::nullopt});
    }
};

// Expressão tipo(valor) - retorna o tipo do valor como string
struct TypeOfExpr : public ASTNode {
    std::unique_ptr<ASTNode> value;
    
    TypeOfExpr(std::unique_ptr<ASTNode> v) : value(std::move(v)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        value->compile(bytecode);
        bytecode.push_back({OpCode::TYPE_OF, std::nullopt});
    }
};

#endif