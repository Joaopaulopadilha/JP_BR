// ast_nativos.hpp
// Nós de módulos nativos do AST (importação e chamada de funções nativas)

#ifndef AST_SRC_AST_NATIVOS_HPP
#define AST_SRC_AST_NATIVOS_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declaration
struct ASTNode;

// Informação de função nativa importada
struct NativeFuncInfo {
    std::string dllPath;      // Caminho da DLL
    std::string funcName;     // Nome da função (sem prefixo jp_)
    std::string fullName;     // Nome completo na DLL (jp_prefixo_nome)
    int numArgs;              // Número de argumentos
};

// Tabela global de funções nativas
inline std::unordered_map<std::string, NativeFuncInfo> nativeFuncTable;

// Declaração de importação nativa: nativo "path" importar func1(n), func2(n), ...
struct NativeImportStmt : public ASTNode {
    std::string dllPath;
    std::vector<std::pair<std::string, int>> functions; // nome, numArgs
    
    NativeImportStmt(std::string path) : dllPath(std::move(path)) {}
    
    void addFunction(std::string name, int numArgs) {
        functions.push_back({name, numArgs});
        
        // Registra a função imediatamente durante o parsing
        NativeFuncInfo info;
        info.dllPath = dllPath;
        info.funcName = name;
        info.fullName = name; // Nome na DLL (tentará com e sem prefixo jp_)
        info.numArgs = numArgs;
        
        nativeFuncTable[name] = info;
    }
    
    void compile(std::vector<Instruction>& bytecode) override {
        // Nada a fazer - as funções já foram registradas durante o parsing
    }
};

// Chamada de função nativa: nome(args)
struct NativeCallExpr : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> args;
    
    NativeCallExpr(std::string n, std::vector<std::unique_ptr<ASTNode>> a)
        : name(std::move(n)), args(std::move(a)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        // Empilha os argumentos
        for (auto& arg : args) {
            arg->compile(bytecode);
        }
        
        // Chama a função nativa (nome e quantidade de args)
        bytecode.push_back({OpCode::NATIVE_CALL, name + ":" + std::to_string(args.size())});
    }
};

#endif