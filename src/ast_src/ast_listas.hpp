// ast_listas.hpp
// Nós AST para operações com listas

#ifndef AST_LISTAS_HPP
#define AST_LISTAS_HPP

#include "../ast.hpp"
#include <vector>
#include <memory>

// Criação de lista: [1, 2, 3] ou []
class ListCreateExpr : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> elements;
    
    ListCreateExpr() = default;
    
    void addElement(std::unique_ptr<ASTNode> elem) {
        elements.push_back(std::move(elem));
    }
    
    void compile(std::vector<Instruction>& code) override {
        // Cria lista vazia
        code.push_back({OpCode::LIST_CREATE, std::nullopt});
        
        // Adiciona cada elemento
        for (auto& elem : elements) {
            elem->compile(code);
            code.push_back({OpCode::LIST_ADD, std::nullopt});
        }
    }
};

// Acesso a elemento: lista[indice]
class ListAccessExpr : public ASTNode {
public:
    std::unique_ptr<ASTNode> list;
    std::unique_ptr<ASTNode> index;
    
    ListAccessExpr(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> i)
        : list(std::move(l)), index(std::move(i)) {}
    
    void compile(std::vector<Instruction>& code) override {
        list->compile(code);   // Empilha a lista
        index->compile(code);  // Empilha o índice
        code.push_back({OpCode::LIST_GET, std::nullopt});
    }
};

// Atribuição a elemento: lista[indice] = valor
class ListAssignStmt : public ASTNode {
public:
    std::string listName;
    std::unique_ptr<ASTNode> index;
    std::unique_ptr<ASTNode> value;
    
    ListAssignStmt(const std::string& name, std::unique_ptr<ASTNode> idx, std::unique_ptr<ASTNode> val)
        : listName(name), index(std::move(idx)), value(std::move(val)) {}
    
    void compile(std::vector<Instruction>& code) override {
        code.push_back({OpCode::LOAD_VAR, listName});  // Empilha a lista
        index->compile(code);   // Empilha o índice
        value->compile(code);   // Empilha o valor
        code.push_back({OpCode::LIST_SET, std::nullopt});
    }
};

// Método de lista: lista.adicionar(x), lista.remover(i), lista.tamanho(), lista.exibir()
class ListMethodExpr : public ASTNode {
public:
    std::unique_ptr<ASTNode> list;
    std::string methodName;
    std::vector<std::unique_ptr<ASTNode>> args;
    
    ListMethodExpr(std::unique_ptr<ASTNode> l, const std::string& method)
        : list(std::move(l)), methodName(method) {}
    
    void addArg(std::unique_ptr<ASTNode> arg) {
        args.push_back(std::move(arg));
    }
    
    void compile(std::vector<Instruction>& code) override {
        list->compile(code);  // Empilha a lista
        
        if (methodName == "adicionar" || methodName == "add" || methodName == "append") {
            if (!args.empty()) {
                args[0]->compile(code);
            }
            code.push_back({OpCode::LIST_ADD, std::nullopt});
            // Descarta a lista retornada (método void)
            code.push_back({OpCode::POP, std::nullopt});
        }
        else if (methodName == "remover" || methodName == "remove") {
            if (!args.empty()) {
                args[0]->compile(code);
            }
            code.push_back({OpCode::LIST_REMOVE, std::nullopt});
            // LIST_REMOVE já não retorna nada
        }
        else if (methodName == "tamanho" || methodName == "size" || methodName == "len") {
            code.push_back({OpCode::LIST_SIZE, std::nullopt});
            // Retorna o tamanho (não faz POP)
        }
        else if (methodName == "exibir" || methodName == "display" || methodName == "mostrar") {
            code.push_back({OpCode::LIST_DISPLAY, std::nullopt});
            // LIST_DISPLAY já não retorna nada
        }
    }
};

#endif // AST_LISTAS_HPP