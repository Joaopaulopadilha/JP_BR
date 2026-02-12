// ast_condicionais.hpp
// Nós de condicionais do AST (se, senao, ou_se)

#ifndef AST_SRC_AST_CONDICIONAIS_HPP
#define AST_SRC_AST_CONDICIONAIS_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>

// Forward declarations
struct ASTNode;
struct BlockStmt;

// Condicional: se condição: ... senao: ...
struct IfStmt : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock; // Opcional (pode ser null)

    IfStmt(std::unique_ptr<ASTNode> cond, 
           std::unique_ptr<BlockStmt> tBlock, 
           std::unique_ptr<BlockStmt> eBlock)
        : condition(std::move(cond)), thenBlock(std::move(tBlock)), elseBlock(std::move(eBlock)) {}

    void compile(std::vector<Instruction>& bytecode) override {
        // 1. Compila a expressão condicional
        condition->compile(bytecode);

        // 2. Emite o JUMP_IF_FALSE (placeholder)
        int jumpToElseIndex = bytecode.size();
        bytecode.push_back({OpCode::JUMP_IF_FALSE, (long)-1}); 

        // 3. Compila o bloco THEN
        thenBlock->compile(bytecode);

        // 4. Prepara o pulo para pular o bloco ELSE ao final do THEN
        int jumpOverElseIndex = -1;
        if (elseBlock) {
            jumpOverElseIndex = bytecode.size();
            bytecode.push_back({OpCode::JUMP, (long)-1});
        }

        // PATCH 1: Atualiza o pulo para o início do else
        bytecode[jumpToElseIndex].operand = (long)bytecode.size();

        // 5. Compila o bloco ELSE (se existir)
        if (elseBlock) {
            elseBlock->compile(bytecode);
            
            // PATCH 2: Atualiza o pulo para o fim total
            bytecode[jumpOverElseIndex].operand = (long)bytecode.size();
        }
    }
};

#endif
