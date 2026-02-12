// ast_funcoes.hpp
// Nós de funções do AST (declaração, chamada, retorno)
// CORRIGIDO: Gera STORE_VAR para parâmetros das funções

#ifndef AST_SRC_AST_FUNCOES_HPP
#define AST_SRC_AST_FUNCOES_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declarations
struct ASTNode;
struct BlockStmt;

// Tabela global de funções (endereço no bytecode)
struct FunctionInfo {
    std::string name;
    std::vector<std::string> params;
    int address;
};

inline std::unordered_map<std::string, FunctionInfo> functionTable;

// Declaração de função: funcao nome(params):
struct FuncDeclStmt : public ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStmt> body;
    
    FuncDeclStmt(std::string n, std::vector<std::string> p, std::unique_ptr<BlockStmt> b)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        // Pula a função (será chamada depois)
        int jumpOverFunc = bytecode.size();
        bytecode.push_back({OpCode::JUMP, (long)-1});
        
        // Registra o endereço da função
        int funcAddress = bytecode.size();
        functionTable[name] = {name, params, funcAddress};
        
        // CORREÇÃO: Gera STORE_VAR para cada parâmetro (em ordem reversa)
        // Os argumentos estão na pilha na ordem: arg1, arg2, arg3 (do topo para baixo)
        // Então precisamos fazer POP na ordem reversa para atribuir corretamente
        for (int i = params.size() - 1; i >= 0; i--) {
            bytecode.push_back({OpCode::STORE_VAR, params[i]});
        }
        
        // Corpo da função
        body->compile(bytecode);
        
        // Retorno implícito
        bytecode.push_back({OpCode::LOAD_CONST, (long)0});
        bytecode.push_back({OpCode::RETURN, std::nullopt});
        
        // Patch do pulo
        bytecode[jumpOverFunc].operand = (long)bytecode.size();
    }
};

// Chamada de função: nome(args)
struct FuncCallExpr : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> args;
    
    FuncCallExpr(std::string n, std::vector<std::unique_ptr<ASTNode>> a)
        : name(std::move(n)), args(std::move(a)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        // Empilha os argumentos
        for (auto& arg : args) {
            arg->compile(bytecode);
        }
        
        // Chama a função (nome e quantidade de args)
        bytecode.push_back({OpCode::CALL, name + ":" + std::to_string(args.size())});
    }
};

// Retorno: retorna valor
struct ReturnStmt : public ASTNode {
    std::unique_ptr<ASTNode> expression;
    
    ReturnStmt(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        if (expression) {
            expression->compile(bytecode);
        } else {
            bytecode.push_back({OpCode::LOAD_CONST, (long)0});
        }
        bytecode.push_back({OpCode::RETURN, std::nullopt});
    }
};

#endif