// parser.hpp
// Parser principal - análise sintática e construção da AST da JPLang

#ifndef PARSER_HPP
#define PARSER_HPP

#include "lexer.hpp"
#include "ast.hpp"
#include "opcodes.hpp"
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

    // Método principal que gera o Bloco Global
    std::unique_ptr<BlockStmt> parse();

    // Helpers Públicos
    Token peek(int offset = 0) const {
        if (pos + offset >= tokens.size()) return {TokenType::END_OF_FILE, "", 0};
        return tokens[pos + offset];
    }

    Token consume(TokenType type, const std::string& err) {
        if (peek().type == type) {
            return tokens[pos++];
        }
        // Erro com número da linha
        int line = peek().line;
        std::string found = langErro("encontrado", {{"valor", peek().value}});
        throw std::runtime_error(langErro("linha", {{"num", std::to_string(line)}}) + ": " + err + ". " + found);
    }
    
    // Consome um nome de membro (aceita ID ou palavras reservadas como nomes de método/atributo)
    Token consumeMemberName(const std::string& err) {
        Token t = peek();
        // Aceita ID ou qualquer palavra reservada que possa ser nome de método
        if (t.type == TokenType::ID || 
            t.type == TokenType::REPETIR ||
            t.type == TokenType::PARA ||
            t.type == TokenType::SE ||
            t.type == TokenType::SENAO ||
            t.type == TokenType::ENQUANTO ||
            t.type == TokenType::LOOP ||
            t.type == TokenType::PARAR ||
            t.type == TokenType::CONTINUAR ||
            t.type == TokenType::FUNCAO ||
            t.type == TokenType::RETORNA ||
            t.type == TokenType::CLASSE ||
            t.type == TokenType::AUTO ||
            t.type == TokenType::IMPORTAR ||
            t.type == TokenType::NATIVO ||
            t.type == TokenType::DE ||
            t.type == TokenType::COMO ||
            t.type == TokenType::TRUE ||
            t.type == TokenType::FALSE ||
            t.type == TokenType::AND ||
            t.type == TokenType::OR ||
            t.type == TokenType::TYPE_INT ||
            t.type == TokenType::TYPE_FLOAT ||
            t.type == TokenType::TYPE_STR ||
            t.type == TokenType::TYPE_BOOL ||
            t.type == TokenType::EM ||
            t.type == TokenType::INTERVALO ||
            t.type == TokenType::OU_SE) {
            pos++;
            return t;
        }
        int line = peek().line;
        std::string found = langErro("encontrado", {{"valor", peek().value}});
        throw std::runtime_error(langErro("linha", {{"num", std::to_string(line)}}) + ": " + err + ". " + found);
    }
    
    // Helper para erro com linha
    void error(const std::string& msg) {
        int line = peek().line;
        throw std::runtime_error(langErro("linha", {{"num", std::to_string(line)}}) + ": " + msg);
    }

    // Métodos de Parse (Despachantes)
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseStatement();

private:
    const std::vector<Token>& tokens;
    size_t pos;
};

// ---------------------------------------------------------
// INCLUDES DOS MÓDULOS DE PARSE
// ---------------------------------------------------------
#include "parser_src/parser_expressoes.hpp"
#include "parser_src/parser_saida.hpp"
#include "parser_src/parser_variaveis.hpp"
#include "parser_src/parser_condicionais.hpp"
#include "parser_src/parser_repeticoes.hpp"
#include "parser_src/parser_funcoes.hpp"
#include "parser_src/parser_classes.hpp"
#include "parser_src/parser_nativos.hpp"
#include "parser_src/parser_importar.hpp"
#include "ast_src/ast_listas.hpp"

// ---------------------------------------------------------
// IMPLEMENTAÇÃO DA CLASSE PARSER
// ---------------------------------------------------------

inline std::unique_ptr<BlockStmt> Parser::parse() {
    auto block = std::make_unique<BlockStmt>();
    while (peek().type != TokenType::END_OF_FILE) {
        // Ignora DEDENTS extras no nível global
        if (peek().type == TokenType::DEDENT) {
            pos++;
            continue;
        }
        block->add(parseStatement());
    }
    return block;
}

inline std::unique_ptr<ASTNode> Parser::parseExpression() {
    return ParserExpr::parse(*this);
}

inline std::unique_ptr<ASTNode> Parser::parseStatement() {
    Token t = peek();

    // 1. IMPORTAÇÃO: de modulo importar ...
    if (t.type == TokenType::DE) {
        return ParserImportar::parseFromImport(*this);
    }

    // 2. IMPORTAÇÃO: importar modulo / importar "arquivo.jp" / importar modulo como alias
    if (t.type == TokenType::IMPORTAR) {
        return ParserImportar::parseImport(*this);
    }

    // 3. IMPORTAÇÃO NATIVA: nativo "path" importar ...
    if (t.type == TokenType::NATIVO) {
        return ParserNativo::parseNativeImport(*this);
    }

    // 4. DECLARAÇÃO DE CLASSE
    if (t.type == TokenType::CLASSE) {
        return ParserClass::parseClassDecl(*this);
    }

    // 5. ATRIBUIÇÃO auto.attr = valor
    if (t.type == TokenType::AUTO && peek(1).type == TokenType::DOT && peek(3).type == TokenType::EQUALS) {
        return ParserClass::parseAutoAssign(*this);
    }

    // 6. DECLARAÇÃO DE FUNÇÃO
    if (t.type == TokenType::FUNCAO) {
        return ParserFunc::parseFuncDecl(*this);
    }

    // 7. RETORNO
    if (t.type == TokenType::RETORNA) {
        return ParserFunc::parseReturn(*this);
    }

    // 8. CONDICIONAIS (SE)
    if (t.type == TokenType::SE) {
        return ParserCond::parseIf(*this);
    }

    // 9. REPETIÇÕES
    if (t.type == TokenType::LOOP) {
        return ParserRep::parseLoop(*this);
    }
    if (t.type == TokenType::ENQUANTO) {
        return ParserRep::parseWhile(*this);
    }
    if (t.type == TokenType::REPETIR) {
        return ParserRep::parseRepeat(*this);
    }
    if (t.type == TokenType::PARA) {
        return ParserRep::parseFor(*this);
    }
    if (t.type == TokenType::PARAR) {
        return ParserRep::parseBreak(*this);
    }
    if (t.type == TokenType::CONTINUAR) {
        return ParserRep::parseContinue(*this);
    }

    // 10. COMANDO DE SAIDA
    if (t.type == TokenType::ID && t.value.rfind(langSaidaPrefixo, 0) == 0) {
        return ParserSaida::parse(*this);
    }

    // 11. TIPO OPCIONAL + ATRIBUIÇÃO (int x = ... / texto x = ... / dec x = ...)
    if ((t.type == TokenType::TYPE_INT || t.type == TokenType::TYPE_FLOAT || 
         t.type == TokenType::TYPE_STR || t.type == TokenType::TYPE_BOOL ||
         t.value == "texto" || t.value == "inteiro" || t.value == "dec" || t.value == "decimal") && 
        peek(1).type == TokenType::ID && peek(2).type == TokenType::EQUALS) {
        consume(t.type, ""); // Consome o tipo (ignorado por enquanto)
        return ParserVar::parse(*this);
    }

    // 12. ACESSO A ELEMENTO DE LISTA (ID[idx].metodo() ou ID[idx] = valor)
    if (t.type == TokenType::ID && peek(1).type == TokenType::LBRACKET) {
        // Avança até encontrar o ] correspondente para verificar o que vem depois
        size_t tempPos = pos + 2;
        int bracketDepth = 1;
        while (tempPos < tokens.size() && bracketDepth > 0) {
            if (tokens[tempPos].type == TokenType::LBRACKET) bracketDepth++;
            if (tokens[tempPos].type == TokenType::RBRACKET) bracketDepth--;
            tempPos++;
        }
        
        // Verifica o token após o ]
        if (tempPos < tokens.size() && tokens[tempPos].type == TokenType::EQUALS) {
            // É atribuição: pessoas[index] = valor
            std::string listName = t.value;
            consume(TokenType::ID, "");
            consume(TokenType::LBRACKET, "");
            auto index = parseExpression();
            consume(TokenType::RBRACKET, langErro("esperado", {{"valor", "]"}}));
            consume(TokenType::EQUALS, langErro("esperado", {{"valor", "="}}));
            auto value = parseExpression();
            return std::make_unique<ListAssignStmt>(listName, std::move(index), std::move(value));
        } else {
            // É chamada de método ou acesso: pessoas[index].metodo() ou pessoas[index]
            auto expr = parseExpression();
            return std::make_unique<ExpressionStmt>(std::move(expr));
        }
    }

    // 13. ATRIBUIÇÃO DE VARIÁVEL (ID = ...)
    if (t.type == TokenType::ID && peek(1).type == TokenType::EQUALS) {
        return ParserVar::parse(*this);
    }

    // 13. CHAMADA DE MÉTODO (ID.metodo(...) ou obj.metodo(...))
    if (t.type == TokenType::ID && peek(1).type == TokenType::DOT) {
        auto expr = parseExpression();
        return std::make_unique<ExpressionStmt>(std::move(expr));
    }

    // 14. BUILTIN COMO STATEMENT (entrada, inteiro, decimal, texto, booleano, tipo, etc.)
    if (t.type == TokenType::ID && peek(1).type == TokenType::LPAREN &&
        langBuiltins.find(t.value) != langBuiltins.end()) {
        auto expr = parseExpression();
        return std::make_unique<ExpressionStmt>(std::move(expr));
    }

    // 14b. TOKEN DE TIPO COMO BUILTIN STATEMENT: int("42"), float(x), str(42), bool(1)
    if ((t.type == TokenType::TYPE_INT || t.type == TokenType::TYPE_FLOAT ||
         t.type == TokenType::TYPE_STR || t.type == TokenType::TYPE_BOOL) &&
        peek(1).type == TokenType::LPAREN) {
        auto expr = parseExpression();
        return std::make_unique<ExpressionStmt>(std::move(expr));
    }

    // 15. CHAMADA DE FUNÇÃO (ID(...)) - pode ser nativa ou normal
    if (t.type == TokenType::ID && peek(1).type == TokenType::LPAREN) {
        // Verifica se é função nativa registrada
        if (ParserNativo::isNativeFunc(t.value)) {
            return ParserNativo::parseNativeCall(*this);
        }
        
        // Verifica se é função do usuário conhecida
        if (functionTable.find(t.value) != functionTable.end()) {
            return ParserFunc::parseFuncCall(*this);
        }
        
        // Função desconhecida: pode ser nativa de DLL importada diretamente
        // Verifica se há alguma biblioteca nativa direta carregada
        bool hasNativeDirectLib = false;
        for (const auto& [key, info] : moduleTable) {
            if (info.isNativeDirect) {
                hasNativeDirectLib = true;
                break;
            }
        }
        
        if (hasNativeDirectLib) {
            // Trata como chamada nativa lazy (será validada na compilação)
            return ParserNativo::parseNativeCall(*this);
        }
        
        // Fallback: trata como chamada de função normal
        return ParserFunc::parseFuncCall(*this);
    }

    // Se chegou aqui, é um token que não sabemos lidar no início de linha
    error(langErro("comando_desconhecido", {{"valor", t.value}}));
    return nullptr; // Nunca alcançado
}

#endif