// ast.hpp
// Nós da árvore sintática abstrata (AST), enums e tipos base para JPLang v1.0

#ifndef JPLANG_AST_HPP
#define JPLANG_AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace jplang {

// ============================================================================
// OPERADORES
// ============================================================================

enum class BinOp {
    Add,    // +
    Sub,    // -
    Mul,    // *
    Div,    // /
    Mod     // %
};

enum class CmpOp {
    Eq,     // ==
    Ne,     // !=
    Gt,     // >
    Lt,     // <
    Ge,     // >=
    Le      // <=
};

enum class LogicOp {
    And,    // e
    Or      // ou
};

// ============================================================================
// TIPOS DE VARIÁVEIS
// ============================================================================

enum class VarType {
    Int,
    Float,
    String,
    Bool,
    Object,
    Nativo
};

// ============================================================================
// CORES (para saída)
// ============================================================================

enum class Cor : int {
    Nenhuma  = -1,
    Vermelho = 0,
    Verde    = 1,
    Azul     = 2,
    Amarelo  = 3
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using StmtList = std::vector<StmtPtr>;

// ============================================================================
// EXPRESSÕES
// ============================================================================

struct NumberLit {
    int value;
    int line;
};

struct FloatLit {
    double value;
    int line;
};

struct StringLit {
    std::string value;
    int line;
};

// Parte de interpolação: texto literal, nome de variável, ou expressão
struct InterpPart {
    bool is_var;            // true = variável/expressão, false = texto literal
    std::string value;      // nome da variável (simples) ou texto literal
    ExprPtr expr;           // expressão completa (se não-null, usa em vez de value)
};

struct StringInterp {
    std::vector<InterpPart> parts;
    int line;
};

struct BoolLit {
    bool value;
    int line;
};

struct NullLit {
    int line;
};

struct VarExpr {
    std::string name;
    int line;
};

struct BinOpExpr {
    BinOp op;
    ExprPtr left;
    ExprPtr right;
    int line;
};

struct CmpOpExpr {
    CmpOp op;
    ExprPtr left;
    ExprPtr right;
    int line;
};

struct LogicOpExpr {
    LogicOp op;
    ExprPtr left;
    ExprPtr right;
    int line;
};

struct ConcatExpr {
    ExprPtr left;
    ExprPtr right;
    int line;
};

struct ChamadaExpr {
    std::string name;
    std::vector<ExprPtr> args;
    int line;
};

struct AttrGetExpr {
    ExprPtr object;
    std::string attr;
    int line;
};

struct MetodoChamadaExpr {
    ExprPtr object;
    std::string method;
    std::vector<ExprPtr> args;
    int line;
};

struct AutoExpr {
    int line;
};

struct ListLitExpr {
    std::vector<ExprPtr> elements;
    int line;
};

struct IndexGetExpr {
    ExprPtr object;
    ExprPtr index;
    int line;
};

// ============================================================================
// NÓ EXPRESSÃO (variant)
// ============================================================================

struct Expr {
    std::variant<
        NumberLit,
        FloatLit,
        StringLit,
        StringInterp,
        BoolLit,
        NullLit,
        VarExpr,
        BinOpExpr,
        CmpOpExpr,
        LogicOpExpr,
        ConcatExpr,
        ChamadaExpr,
        AttrGetExpr,
        MetodoChamadaExpr,
        AutoExpr,
        ListLitExpr,
        IndexGetExpr
    > node;

    template <typename T>
    Expr(T&& val) : node(std::forward<T>(val)) {}
};

// ============================================================================
// STATEMENTS
// ============================================================================

struct AssignStmt {
    std::string name;
    ExprPtr value;
    int line;
};

struct AttrSetStmt {
    ExprPtr object;
    std::string attr;
    ExprPtr value;
    int line;
};

struct SaidaStmt {
    ExprPtr value;
    bool newline;           // saida = true, saidal = false
    Cor cor;
    int line;
};

// Ramo condicional: usado em se / ou_se / senao
struct CondBranch {
    ExprPtr condition;      // nullptr para 'senao'
    StmtList body;
};

struct IfStmt {
    std::vector<CondBranch> branches;   // se, ou_se..., senao
    int line;
};

struct RepetirStmt {
    ExprPtr count;
    StmtList body;
    int line;
};

struct EnquantoStmt {
    ExprPtr condition;
    StmtList body;
    int line;
};

struct ParaStmt {
    std::string var;
    ExprPtr start;
    ExprPtr end;
    ExprPtr step;           // pode ser nullptr (default = 1)
    StmtList body;
    int line;
};

struct PararStmt {
    int line;
};

struct ContinuarStmt {
    int line;
};

struct RetornaStmt {
    ExprPtr value;          // pode ser nullptr
    int line;
};

struct FuncaoStmt {
    std::string name;
    std::vector<std::string> params;
    StmtList body;
    int line;
};

struct ClasseStmt {
    std::string name;
    StmtList body;          // métodos (FuncaoStmt)
    int line;
};

struct NativoStmt {
    std::string lib_path;
    std::vector<std::string> functions;
    bool auto_mode;         // true = carrega do JSON, false = lista explícita
    int line;
};

struct ExprStmt {
    ExprPtr expr;
    int line;
};

struct IndexSetStmt {
    std::string name;
    ExprPtr index;
    ExprPtr value;
    int line;
};

// ============================================================================
// NÓ STATEMENT (variant)
// ============================================================================

struct Stmt {
    std::variant<
        AssignStmt,
        AttrSetStmt,
        SaidaStmt,
        IfStmt,
        RepetirStmt,
        EnquantoStmt,
        ParaStmt,
        PararStmt,
        ContinuarStmt,
        RetornaStmt,
        FuncaoStmt,
        ClasseStmt,
        NativoStmt,
        ExprStmt,
        IndexSetStmt
    > node;

    template <typename T>
    Stmt(T&& val) : node(std::forward<T>(val)) {}
};

// ============================================================================
// PROGRAMA (nó raiz)
// ============================================================================

struct Program {
    StmtList statements;
};

// ============================================================================
// HELPERS PARA CRIAÇÃO DE NÓS
// ============================================================================

template <typename T, typename... Args>
ExprPtr make_expr(Args&&... args) {
    return std::make_unique<Expr>(T{std::forward<Args>(args)...});
}

template <typename T, typename... Args>
StmtPtr make_stmt(Args&&... args) {
    return std::make_unique<Stmt>(T{std::forward<Args>(args)...});
}

} // namespace jplang

#endif // JPLANG_AST_HPP