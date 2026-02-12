// ast.hpp
// Definições dos nós da Árvore Sintática Abstrata (AST) da JPLang

#ifndef AST_HPP
#define AST_HPP

#include "opcodes.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// Interface Base
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void compile(std::vector<Instruction>& bytecode) = 0;
};

// Forward declaration para BlockStmt (usado por vários módulos)
struct BlockStmt;

// Módulos do AST
#include "ast_src/ast_expressoes.hpp"
#include "ast_src/ast_statements.hpp"
#include "ast_src/ast_condicionais.hpp"
#include "ast_src/ast_repeticoes.hpp"
#include "ast_src/ast_funcoes.hpp"
#include "ast_src/ast_classes.hpp"
#include "ast_src/ast_nativos.hpp"
#include "ast_src/ast_importar.hpp"

#endif