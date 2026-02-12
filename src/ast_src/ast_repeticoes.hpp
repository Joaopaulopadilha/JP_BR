// ast_repeticoes.hpp
// Nós de repetição do AST (loop, enquanto, repetir, para, parar, continuar)

#ifndef AST_SRC_AST_REPETICOES_HPP
#define AST_SRC_AST_REPETICOES_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>

// Forward declarations
struct ASTNode;
struct BlockStmt;

// Contexto de loop para resolver break/continue
struct LoopContext {
    static std::vector<std::pair<int*, int*>> stack;
    
    static void push(int* breakTarget, int* continueTarget) {
        stack.push_back({breakTarget, continueTarget});
    }
    
    static void pop() {
        stack.pop_back();
    }
    
    static bool inLoop() {
        return !stack.empty();
    }
    
    static int* breakTarget() {
        return stack.back().first;
    }
    
    static int* continueTarget() {
        return stack.back().second;
    }
};

// Definição estática
inline std::vector<std::pair<int*, int*>> LoopContext::stack;

// Lista de patches pendentes para break/continue
struct LoopPatches {
    std::vector<int> breakPatches;
    std::vector<int> continuePatches;
};

// Helper para patch de break/continue
inline void patchLoopControl(std::vector<Instruction>& bytecode, int continueTarget, int breakTarget) {
    for (size_t i = 0; i < bytecode.size(); i++) {
        if (bytecode[i].op == OpCode::LOOP_BREAK && 
            bytecode[i].operand.has_value() &&
            std::get<long>(bytecode[i].operand.value()) == -1) {
            bytecode[i] = {OpCode::JUMP, (long)breakTarget};
        }
        if (bytecode[i].op == OpCode::LOOP_CONTINUE && 
            bytecode[i].operand.has_value() &&
            std::get<long>(bytecode[i].operand.value()) == -1) {
            bytecode[i] = {OpCode::JUMP, (long)continueTarget};
        }
    }
}

// Loop infinito: loop:
struct LoopStmt : public ASTNode {
    std::unique_ptr<BlockStmt> body;
    
    LoopStmt(std::unique_ptr<BlockStmt> b) : body(std::move(b)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        int loopStart = bytecode.size();
        body->compile(bytecode);
        bytecode.push_back({OpCode::JUMP, (long)loopStart});
        int loopEnd = bytecode.size();
        patchLoopControl(bytecode, loopStart, loopEnd);
    }
};

// Enquanto: enquanto condição:
struct WhileStmt : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<BlockStmt> body;
    
    WhileStmt(std::unique_ptr<ASTNode> cond, std::unique_ptr<BlockStmt> b)
        : condition(std::move(cond)), body(std::move(b)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        int loopStart = bytecode.size();
        condition->compile(bytecode);
        int jumpToEndIndex = bytecode.size();
        bytecode.push_back({OpCode::JUMP_IF_FALSE, (long)-1});
        body->compile(bytecode);
        bytecode.push_back({OpCode::JUMP, (long)loopStart});
        int loopEnd = bytecode.size();
        bytecode[jumpToEndIndex].operand = (long)loopEnd;
        patchLoopControl(bytecode, loopStart, loopEnd);
    }
};

// Repetir N vezes: repetir(n):
struct RepeatStmt : public ASTNode {
    std::unique_ptr<ASTNode> count;
    std::unique_ptr<BlockStmt> body;
    std::string counterVar;
    
    RepeatStmt(std::unique_ptr<ASTNode> c, std::unique_ptr<BlockStmt> b)
        : count(std::move(c)), body(std::move(b)), counterVar("__rep_counter__") {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        bytecode.push_back({OpCode::LOAD_CONST, (long)0});
        bytecode.push_back({OpCode::STORE_VAR, counterVar});
        
        int loopStart = bytecode.size();
        bytecode.push_back({OpCode::LOAD_VAR, counterVar});
        count->compile(bytecode);
        bytecode.push_back({OpCode::LT, std::nullopt});
        
        int jumpToEndIndex = bytecode.size();
        bytecode.push_back({OpCode::JUMP_IF_FALSE, (long)-1});
        
        body->compile(bytecode);
        
        bytecode.push_back({OpCode::LOAD_VAR, counterVar});
        bytecode.push_back({OpCode::LOAD_CONST, (long)1});
        bytecode.push_back({OpCode::ADD, std::nullopt});
        bytecode.push_back({OpCode::STORE_VAR, counterVar});
        
        bytecode.push_back({OpCode::JUMP, (long)loopStart});
        
        int loopEnd = bytecode.size();
        bytecode[jumpToEndIndex].operand = (long)loopEnd;
        patchLoopControl(bytecode, loopStart, loopEnd);
    }
};

// Para em intervalo: para i em intervalo(inicio, fim, passo):
struct ForStmt : public ASTNode {
    std::string varName;
    std::unique_ptr<ASTNode> start;
    std::unique_ptr<ASTNode> end;
    std::unique_ptr<ASTNode> step;
    std::unique_ptr<BlockStmt> body;
    
    ForStmt(std::string var, std::unique_ptr<ASTNode> s, std::unique_ptr<ASTNode> e, 
            std::unique_ptr<ASTNode> st, std::unique_ptr<BlockStmt> b)
        : varName(std::move(var)), start(std::move(s)), end(std::move(e)), 
          step(std::move(st)), body(std::move(b)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        start->compile(bytecode);
        bytecode.push_back({OpCode::STORE_VAR, varName});
        
        int loopStart = bytecode.size();
        bytecode.push_back({OpCode::LOAD_VAR, varName});
        end->compile(bytecode);
        bytecode.push_back({OpCode::LT, std::nullopt});
        
        int jumpToEndIndex = bytecode.size();
        bytecode.push_back({OpCode::JUMP_IF_FALSE, (long)-1});
        
        body->compile(bytecode);
        
        bytecode.push_back({OpCode::LOAD_VAR, varName});
        step->compile(bytecode);
        bytecode.push_back({OpCode::ADD, std::nullopt});
        bytecode.push_back({OpCode::STORE_VAR, varName});
        
        bytecode.push_back({OpCode::JUMP, (long)loopStart});
        
        int loopEnd = bytecode.size();
        bytecode[jumpToEndIndex].operand = (long)loopEnd;
        patchLoopControl(bytecode, loopStart, loopEnd);
    }
};

// Break: parar
struct BreakStmt : public ASTNode {
    void compile(std::vector<Instruction>& bytecode) override {
        bytecode.push_back({OpCode::LOOP_BREAK, (long)-1});
    }
};

// Continue: continuar
struct ContinueStmt : public ASTNode {
    void compile(std::vector<Instruction>& bytecode) override {
        bytecode.push_back({OpCode::LOOP_CONTINUE, (long)-1});
    }
};

#endif
