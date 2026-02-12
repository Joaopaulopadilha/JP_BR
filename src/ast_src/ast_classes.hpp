// ast_classes.hpp
// Nós de classes do AST (declaração, métodos, atributos, auto)
// CORRIGIDO: Registra classe durante construção para parsing eager funcionar

#ifndef AST_SRC_AST_CLASSES_HPP
#define AST_SRC_AST_CLASSES_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declarations
struct ASTNode;
struct BlockStmt;

// Informação de classe
struct ClassInfo {
    std::string name;
    std::unordered_map<std::string, int> methods;
    std::vector<std::string> methodParams;
};

inline std::unordered_map<std::string, ClassInfo> classTable;

// Informação de método
struct MethodInfo {
    std::string name;
    std::vector<std::string> params;
    int address;
};

inline std::unordered_map<std::string, std::unordered_map<std::string, MethodInfo>> classMethodTable;

// Expressão 'auto' (referência ao objeto atual)
struct AutoExpr : public ASTNode {
    void compile(std::vector<Instruction>& bytecode) override {
        bytecode.push_back({OpCode::LOAD_VAR, std::string("__auto__")});
    }
};

// Acesso a atributo: obj.atributo ou auto.atributo
struct MemberAccessExpr : public ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string member;
    
    MemberAccessExpr(std::unique_ptr<ASTNode> obj, std::string m)
        : object(std::move(obj)), member(std::move(m)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        object->compile(bytecode);
        bytecode.push_back({OpCode::GET_ATTR, member});
    }
};

// Atribuição a atributo: auto.atributo = valor
struct MemberAssignStmt : public ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string member;
    std::unique_ptr<ASTNode> value;
    
    MemberAssignStmt(std::unique_ptr<ASTNode> obj, std::string m, std::unique_ptr<ASTNode> val)
        : object(std::move(obj)), member(std::move(m)), value(std::move(val)) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        value->compile(bytecode);
        object->compile(bytecode);
        bytecode.push_back({OpCode::SET_ATTR, member});
    }
};

// Chamada de método: obj.metodo(args) ou Classe.metodo(args)
struct MethodCallExpr : public ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string className;
    std::string methodName;
    std::vector<std::unique_ptr<ASTNode>> args;
    bool isStatic;
    
    // Chamada de instância: obj.metodo(args)
    MethodCallExpr(std::unique_ptr<ASTNode> obj, std::string method, std::vector<std::unique_ptr<ASTNode>> a)
        : object(std::move(obj)), className(""), methodName(std::move(method)), 
          args(std::move(a)), isStatic(false) {}
    
    // Chamada estática: Classe.metodo(args)
    MethodCallExpr(std::string cls, std::string method, std::vector<std::unique_ptr<ASTNode>> a)
        : object(nullptr), className(std::move(cls)), methodName(std::move(method)),
          args(std::move(a)), isStatic(true) {}
    
    void compile(std::vector<Instruction>& bytecode) override {
        for (auto& arg : args) {
            arg->compile(bytecode);
        }
        
        if (isStatic) {
            bytecode.push_back({OpCode::METHOD_CALL, className + "." + methodName + ":" + std::to_string(args.size())});
        } else {
            object->compile(bytecode);
            bytecode.push_back({OpCode::METHOD_CALL, "." + methodName + ":" + std::to_string(args.size())});
        }
    }
};

// Declaração de classe: classe Nome:
struct ClassDeclStmt : public ASTNode {
    std::string name;
    std::vector<std::pair<std::string, std::vector<std::string>>> methods;
    std::vector<std::unique_ptr<BlockStmt>> methodBodies;
    
    ClassDeclStmt(std::string n) : name(std::move(n)) {
        // CORREÇÃO: Registra a classe imediatamente durante a construção
        // Isso permite que o parser reconheça a classe antes de compile()
        // Necessário para o parsing eager funcionar corretamente
        if (classTable.find(name) == classTable.end()) {
            classTable[name] = {name, {}, {}};
        }
    }
    
    void addMethod(std::string methodName, std::vector<std::string> params, std::unique_ptr<BlockStmt> body) {
        methods.push_back({methodName, params});
        methodBodies.push_back(std::move(body));
        
        // CORREÇÃO: Registra o método imediatamente (sem endereço ainda)
        // O endereço será atualizado durante compile()
        classMethodTable[name][methodName] = {methodName, params, -1};
    }
    
    void compile(std::vector<Instruction>& bytecode) override {
        int jumpOverClass = bytecode.size();
        bytecode.push_back({OpCode::JUMP, (long)-1});
        
        // A classe já foi registrada no construtor, mas atualizamos se necessário
        if (classTable.find(name) == classTable.end()) {
            classTable[name] = {name, {}, {}};
        }
        
        for (size_t i = 0; i < methods.size(); i++) {
            std::string methodName = methods[i].first;
            std::vector<std::string> params = methods[i].second;
            
            int methodAddress = bytecode.size();
            
            // Atualiza o endereço do método (agora conhecido)
            classMethodTable[name][methodName] = {methodName, params, methodAddress};
            classTable[name].methods[methodName] = methodAddress;
            
            // IMPORTANTE: Salva os parâmetros da pilha em variáveis
            // Os argumentos são empilhados na ordem, então desempilhamos na ordem reversa
            for (int p = params.size() - 1; p >= 0; p--) {
                bytecode.push_back({OpCode::STORE_VAR, params[p]});
            }
            
            // NOTA: Métodos de instância recebem o objeto via dispatch dinâmico em __auto__
            // Apenas construtores (que retornam 'auto') devem criar novo objeto
            // Por isso, a criação de objeto foi movida para dentro do método quando necessário
            
            // Para métodos construtores comuns, criamos objeto aqui
            // Identificamos construtores por retornarem 'auto'
            // Isso é uma heurística simples - poderia ser melhorada
            bool isConstructor = (methodName == "criar" || methodName == "construtor" || methodName == "init");
            
            if (isConstructor) {
                // Cria novo objeto e armazena em __auto__
                bytecode.push_back({OpCode::NEW_OBJECT, std::nullopt});
                bytecode.push_back({OpCode::STORE_VAR, std::string("__auto__")});
                
                // Define a classe: valor (nome classe), depois objeto (__auto__)
                bytecode.push_back({OpCode::LOAD_CONST, name});  // Nome da classe (valor)
                bytecode.push_back({OpCode::LOAD_VAR, std::string("__auto__")});  // Objeto
                bytecode.push_back({OpCode::SET_ATTR, std::string("__classe__")});
            }
            
            methodBodies[i]->compile(bytecode);
            
            bytecode.push_back({OpCode::LOAD_CONST, (long)0});
            bytecode.push_back({OpCode::RETURN, std::nullopt});
        }
        
        bytecode[jumpOverClass].operand = (long)bytecode.size();
    }
};

#endif