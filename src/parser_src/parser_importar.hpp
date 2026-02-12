// parser_importar.hpp
// Módulo do parser para análise de importações

#ifndef PARSER_SRC_PARSER_IMPORTAR_HPP
#define PARSER_SRC_PARSER_IMPORTAR_HPP

#include "../parser.hpp"

namespace ParserImportar {

    // importar tempo
    // importar "arquivo.jp"
    // importar tempo como tp
    inline std::unique_ptr<ImportStmt> parseImport(Parser& p) {
        p.consume(TokenType::IMPORTAR, "Esperado 'importar'");
        
        // Verifica se é string (arquivo) ou ID (módulo)
        if (p.peek().type == TokenType::STRING) {
            // importar "arquivo.jp"
            Token pathToken = p.consume(TokenType::STRING, "");
            return ImportStmt::fromFile(pathToken.value);
        }
        
        // importar modulo
        Token nameToken = p.consume(TokenType::ID, "Esperado nome do modulo");
        std::string moduleName = nameToken.value;
        
        // Verifica se tem "como" (alias)
        if (p.peek().type == TokenType::COMO) {
            p.consume(TokenType::COMO, "");
            Token aliasToken = p.consume(TokenType::ID, "Esperado alias apos 'como'");
            return ImportStmt::withAlias(moduleName, aliasToken.value);
        }
        
        // Importação simples
        return std::make_unique<ImportStmt>(moduleName);
    }

    // de tempo importar func1, func2, ...
    inline std::unique_ptr<ImportStmt> parseFromImport(Parser& p) {
        p.consume(TokenType::DE, "Esperado 'de'");
        
        Token nameToken = p.consume(TokenType::ID, "Esperado nome do modulo");
        std::string moduleName = nameToken.value;
        
        p.consume(TokenType::IMPORTAR, "Esperado 'importar' apos nome do modulo");
        
        // Lista de itens a importar
        std::vector<std::string> items;
        
        Token itemToken = p.consume(TokenType::ID, "Esperado nome do item a importar");
        items.push_back(itemToken.value);
        
        while (p.peek().type == TokenType::COMMA) {
            p.consume(TokenType::COMMA, "");
            Token nextItem = p.consume(TokenType::ID, "Esperado nome do item");
            items.push_back(nextItem.value);
        }
        
        return ImportStmt::selective(moduleName, std::move(items));
    }
}

#endif
