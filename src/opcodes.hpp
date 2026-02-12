// opcodes.hpp
// Definições centrais de tokens, valores e instruções da VM

#ifndef OPCODES_HPP
#define OPCODES_HPP

// Includes essenciais do sistema
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <iostream>

// --- PARTE 1: TIPOS DO LEXER ---

enum class TokenType {
    ID, STRING, STRING_RAW, NUMBER_INT, NUMBER_FLOAT, 
    TRUE, FALSE, 
    EQUALS, PLUS, MINUS, STAR, SLASH, PERCENT, LPAREN, RPAREN,
    LBRACKET, RBRACKET,  // [ ]
    
    // Tokens de Controle
    COLON,      // :
    COMMA,      // ,
    SE, SENAO, OU_SE,
    AND, OR,
    GT, LT, EQ_OP, // >, <, ==
    GTE, LTE, NEQ, // >=, <=, !=
    
    // Tokens de Repetição
    LOOP,       // loop
    ENQUANTO,   // enquanto
    REPETIR,    // repetir
    PARA,       // para
    EM,         // em
    INTERVALO,  // intervalo
    PARAR,      // parar (break)
    CONTINUAR,  // continuar (continue)
    
    // Tokens de Funções
    FUNCAO,     // funcao
    RETORNA,    // retorna
    
    // Tokens de Classes
    CLASSE,     // classe
    AUTO,       // auto (self/this)
    DOT,        // . (acesso a membro)
    TYPE_INT,   // int
    TYPE_FLOAT, // float
    TYPE_STR,   // str
    TYPE_BOOL,  // bool
    
    // Tokens de Módulos Nativos
    NATIVO,     // nativo
    IMPORTAR,   // importar
    
    // Tokens de Importação
    DE,         // de (de modulo importar func)
    COMO,       // como (importar modulo como alias)
    
    INDENT,     // Inicio de bloco
    DEDENT,     // Fim de bloco
    
    END_OF_FILE,
    // Tipos opcionais
    TYPE_DEF
};

struct Token {
    TokenType type;
    std::string value;
    int line; // Número da linha no código fonte
};

// --- PARTE 2: TIPOS DA VM ---

// Tipo de dado dinâmico (String, Inteiro, Decimal, Booleano)
using Value = std::variant<std::string, long, double, bool>;

enum class OpCode {
    LOAD_CONST, LOAD_VAR, STORE_VAR,
    
    // Impressão
    PRINT_NL, PRINT_NO_NL,    
    PRINT_RED_NL, PRINT_GREEN_NL, PRINT_BLUE_NL, PRINT_YELLOW_NL,
    PRINT_RED_NO_NL, PRINT_GREEN_NO_NL, PRINT_BLUE_NO_NL, PRINT_YELLOW_NO_NL,

    // Entrada e Conversões
    INPUT,          // Lê entrada do usuário (com prompt)
    TO_INT,         // Converte para inteiro
    TO_FLOAT,       // Converte para decimal
    TO_STRING,      // Converte para texto
    TO_BOOL,        // Converte para booleano
    TYPE_OF,        // Retorna o tipo do valor como string

    // Operações
    ADD, SUB, MUL, DIV, MOD, GT, LT, EQ, GTE, LTE, NEQ, AND, OR,

    // Fluxo
    JUMP,           // Salto incondicional
    JUMP_IF_FALSE,  // Salto se falso
    
    // Controle de Loop (usados internamente pela VM)
    LOOP_BREAK,     // Sair do loop atual
    LOOP_CONTINUE,  // Pular para próxima iteração
    
    // Funções
    CALL,           // Chamar função
    RETURN,         // Retornar de função
    NATIVE_CALL,    // Chamar função nativa (DLL)
    
    // Classes/Objetos
    NEW_OBJECT,     // Criar novo objeto
    GET_ATTR,       // Pegar atributo do objeto
    SET_ATTR,       // Setar atributo do objeto
    METHOD_CALL,    // Chamar método do objeto
    
    // Stack
    POP,            // Descartar valor do topo da pilha

    // Listas
    LIST_CREATE,    // Criar lista vazia
    LIST_ADD,       // Adicionar elemento à lista
    LIST_GET,       // Obter elemento por índice
    LIST_SET,       // Definir elemento por índice
    LIST_SIZE,      // Tamanho da lista
    LIST_REMOVE,    // Remover elemento por índice
    LIST_DISPLAY,   // Exibir lista

    HALT
};

struct Instruction {
    OpCode op;
    std::optional<Value> operand;
};

// --- HELPERS ---

inline std::string opToString(OpCode op) {
    switch(op) {
        case OpCode::LOAD_CONST:    return "LOAD_CONST";
        case OpCode::LOAD_VAR:      return "LOAD_VAR";
        case OpCode::STORE_VAR:     return "STORE_VAR";
        case OpCode::INPUT:         return "INPUT";
        case OpCode::TO_INT:        return "TO_INT";
        case OpCode::TO_FLOAT:      return "TO_FLOAT";
        case OpCode::TO_STRING:     return "TO_STRING";
        case OpCode::TO_BOOL:       return "TO_BOOL";
        case OpCode::TYPE_OF:       return "TYPE_OF";
        case OpCode::ADD:           return "ADD";
        case OpCode::SUB:           return "SUB";
        case OpCode::MUL:           return "MUL";
        case OpCode::DIV:           return "DIV";
        case OpCode::MOD:           return "MOD";
        case OpCode::GT:            return "GT";
        case OpCode::LT:            return "LT";
        case OpCode::EQ:            return "EQ";
        case OpCode::GTE:           return "GTE";
        case OpCode::LTE:           return "LTE";
        case OpCode::NEQ:           return "NEQ";
        case OpCode::JUMP:          return "JUMP";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::LOOP_BREAK:    return "LOOP_BREAK";
        case OpCode::LOOP_CONTINUE: return "LOOP_CONTINUE";
        case OpCode::CALL:          return "CALL";
        case OpCode::RETURN:        return "RETURN";
        case OpCode::NATIVE_CALL:   return "NATIVE_CALL";
        case OpCode::NEW_OBJECT:    return "NEW_OBJECT";
        case OpCode::GET_ATTR:      return "GET_ATTR";
        case OpCode::SET_ATTR:      return "SET_ATTR";
        case OpCode::METHOD_CALL:   return "METHOD_CALL";
        case OpCode::POP:           return "POP";
        case OpCode::LIST_CREATE:   return "LIST_CREATE";
        case OpCode::LIST_ADD:      return "LIST_ADD";
        case OpCode::LIST_GET:      return "LIST_GET";
        case OpCode::LIST_SET:      return "LIST_SET";
        case OpCode::LIST_SIZE:     return "LIST_SIZE";
        case OpCode::LIST_REMOVE:   return "LIST_REMOVE";
        case OpCode::LIST_DISPLAY:  return "LIST_DISPLAY";
        case OpCode::PRINT_NL:      return "PRINT_NL";
        case OpCode::PRINT_NO_NL:   return "PRINT_NO_NL";
        case OpCode::PRINT_RED_NL:      return "PRINT_RED_NL";
        case OpCode::PRINT_GREEN_NL:    return "PRINT_GREEN_NL";
        case OpCode::PRINT_BLUE_NL:     return "PRINT_BLUE_NL";
        case OpCode::PRINT_YELLOW_NL:   return "PRINT_YELLOW_NL";
        case OpCode::PRINT_RED_NO_NL:   return "PRINT_RED_NO_NL";
        case OpCode::PRINT_GREEN_NO_NL: return "PRINT_GREEN_NO_NL";
        case OpCode::PRINT_BLUE_NO_NL:  return "PRINT_BLUE_NO_NL";
        case OpCode::PRINT_YELLOW_NO_NL:return "PRINT_YELLOW_NO_NL";
        case OpCode::HALT:          return "HALT";
        default:                    return "CMD";
    }
}

struct ValueToString {
    std::string operator()(const std::string& s) { return "\"" + s + "\""; }
    std::string operator()(long n) { return std::to_string(n); }
    std::string operator()(double d) { return std::to_string(d); }
    std::string operator()(bool b) { return b ? "verdadeiro" : "falso"; }
};

inline std::string valToString(const Value& v) {
    return std::visit(ValueToString{}, v);
}

#endif