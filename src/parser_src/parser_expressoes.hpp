// parser_expressoes.hpp
// Módulo do parser para análise de expressões (literais, variáveis, operações, chamadas de função)
// ATUALIZADO: Suporte a funções built-in (entrada, inteiro, decimal, texto)

#ifndef PARSER_SRC_PARSER_EXPRESSOES_HPP
#define PARSER_SRC_PARSER_EXPRESSOES_HPP

#include "../parser.hpp"
#include "../ast_src/ast_listas.hpp"
#include "../ast_src/ast_builtins.hpp"
#include <string>

namespace ParserExpr {

    // Forward declaration do ponto de entrada
    inline std::unique_ptr<ASTNode> parse(Parser& p);

    // --- 1. INTERPOLAÇÃO DE STRING ---
    // Forward declaration
    inline std::unique_ptr<ASTNode> parseInterpolatedExpr(const std::string& exprStr);
    
    // Helper para parsear um operando (pode ser auto.attr, módulo.metodo(), número, variável, string)
    inline std::unique_ptr<ASTNode> parseInterpolatedOperand(const std::string& tok) {
        std::string t = tok;
        
        // Remove espaços do início e fim
        while (!t.empty() && t[0] == ' ') t = t.substr(1);
        while (!t.empty() && t[t.length()-1] == ' ') t = t.substr(0, t.length()-1);
        
        // String literal com aspas simples ou duplas
        if (t.length() >= 2 && ((t[0] == '\'' && t.back() == '\'') || (t[0] == '"' && t.back() == '"'))) {
            return std::make_unique<LiteralExpr>(t.substr(1, t.length() - 2));
        }
        
        // Se começa e termina com parênteses externos, remove e parseia recursivamente
        if (t.length() >= 2 && t[0] == '(' && t[t.length()-1] == ')') {
            int depth = 0;
            bool valid = true;
            for (size_t i = 0; i < t.length() - 1; i++) {
                if (t[i] == '(') depth++;
                else if (t[i] == ')') depth--;
                if (depth == 0 && i > 0) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                return parseInterpolatedExpr(t.substr(1, t.length() - 2));
            }
        }
        
        // auto.attr ou auto.metodo()
        if (t.rfind("auto.", 0) == 0) {
            std::string rest = t.substr(5);
            size_t parenPos = rest.find('(');
            if (parenPos != std::string::npos && rest.back() == ')') {
                std::string methodName = rest.substr(0, parenPos);
                std::vector<std::unique_ptr<ASTNode>> args;
                return std::make_unique<MethodCallExpr>(
                    std::make_unique<AutoExpr>(),
                    methodName,
                    std::move(args)
                );
            }
            return std::make_unique<MemberAccessExpr>(
                std::make_unique<AutoExpr>(),
                rest
            );
        }
        
        // Verifica se é número
        bool isNum = true;
        bool hasDot = false;
        bool hasOnlyDigitsAndDot = true;
        for (size_t i = 0; i < t.length(); i++) {
            char c = t[i];
            if (c == '.' && !hasDot) hasDot = true;
            else if (!std::isdigit(c) && !(i == 0 && c == '-')) {
                isNum = false;
                hasOnlyDigitsAndDot = false;
            }
        }
        if (isNum && hasOnlyDigitsAndDot && !t.empty() && t[0] != '.') {
            if (hasDot) {
                return std::make_unique<LiteralExpr>(std::stod(t));
            } else {
                return std::make_unique<LiteralExpr>((long)std::stol(t));
            }
        }
        
        // lista[index] ou lista[index].attr
        size_t bracketPos = t.find('[');
        if (bracketPos != std::string::npos) {
            std::string listName = t.substr(0, bracketPos);
            
            // Encontra o ] correspondente
            size_t closeBracketPos = t.find(']', bracketPos);
            if (closeBracketPos == std::string::npos) {
                return std::make_unique<VarExpr>(t); // Erro: [ sem ]
            }
            
            std::string indexStr = t.substr(bracketPos + 1, closeBracketPos - bracketPos - 1);
            auto indexExpr = parseInterpolatedOperand(indexStr);
            
            // Cria ListAccessExpr
            std::unique_ptr<ASTNode> expr = std::make_unique<ListAccessExpr>(
                std::make_unique<VarExpr>(listName), 
                std::move(indexExpr)
            );
            
            // Verifica se há .attr ou .metodo() depois
            if (closeBracketPos + 1 < t.length() && t[closeBracketPos + 1] == '.') {
                std::string rest = t.substr(closeBracketPos + 2);
                
                size_t parenPos = rest.find('(');
                if (parenPos != std::string::npos && rest.back() == ')') {
                    // É método: lista[0].metodo()
                    std::string methodName = rest.substr(0, parenPos);
                    std::vector<std::unique_ptr<ASTNode>> args;
                    return std::make_unique<MethodCallExpr>(std::move(expr), methodName, std::move(args));
                } else {
                    // É atributo: lista[0].attr
                    return std::make_unique<MemberAccessExpr>(std::move(expr), rest);
                }
            }
            
            return expr;
        }
        
        // obj.metodo() ou Modulo.metodo() ou obj.attr
        size_t dotPos = t.find('.');
        if (dotPos != std::string::npos) {
            std::string objName = t.substr(0, dotPos);
            std::string rest = t.substr(dotPos + 1);
            
            size_t parenPos = rest.find('(');
            if (parenPos != std::string::npos && rest.back() == ')') {
                std::string methodName = rest.substr(0, parenPos);
                std::string argsStr = rest.substr(parenPos + 1, rest.length() - parenPos - 2);
                
                std::vector<std::unique_ptr<ASTNode>> args;
                if (!argsStr.empty()) {
                    std::string currentArg;
                    int depth = 0;
                    bool inString = false;
                    char stringChar = 0;
                    
                    for (size_t i = 0; i < argsStr.length(); i++) {
                        char c = argsStr[i];
                        
                        if (!inString && (c == '"' || c == '\'')) {
                            inString = true;
                            stringChar = c;
                            currentArg += c;
                        } else if (inString && c == stringChar) {
                            inString = false;
                            currentArg += c;
                        } else if (!inString && c == '(') {
                            depth++;
                            currentArg += c;
                        } else if (!inString && c == ')') {
                            depth--;
                            currentArg += c;
                        } else if (!inString && c == ',' && depth == 0) {
                            if (!currentArg.empty()) {
                                args.push_back(parseInterpolatedOperand(currentArg));
                                currentArg.clear();
                            }
                        } else {
                            currentArg += c;
                        }
                    }
                    if (!currentArg.empty()) {
                        args.push_back(parseInterpolatedOperand(currentArg));
                    }
                }
                
                bool isStatic = (objName[0] >= 'A' && objName[0] <= 'Z') ||
                               (moduleTable.find(objName) != moduleTable.end()) ||
                               (classTable.find(objName) != classTable.end());
                
                if (isStatic) {
                    return std::make_unique<MethodCallExpr>(objName, methodName, std::move(args));
                } else {
                    return std::make_unique<MethodCallExpr>(
                        std::make_unique<VarExpr>(objName),
                        methodName,
                        std::move(args)
                    );
                }
            }
            
            return std::make_unique<MemberAccessExpr>(
                std::make_unique<VarExpr>(objName),
                rest
            );
        }
        
        return std::make_unique<VarExpr>(t);
    }
    
    inline std::unique_ptr<ASTNode> parseInterpolatedExpr(const std::string& exprStr) {
        std::vector<std::string> tokens;
        std::string current;
        int parenDepth = 0;
        
        for (size_t i = 0; i < exprStr.length(); i++) {
            char c = exprStr[i];
            
            if (c == '(') {
                parenDepth++;
                current += c;
            } else if (c == ')') {
                parenDepth--;
                current += c;
            } else if (c == ' ' && parenDepth == 0) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        
        if (tokens.size() == 1) {
            return parseInterpolatedOperand(tokens[0]);
        }
        
        if (tokens.empty()) {
            return std::make_unique<LiteralExpr>(std::string(""));
        }
        
        auto result = parseInterpolatedOperand(tokens[0]);
        
        for (size_t i = 1; i + 1 < tokens.size(); i += 2) {
            std::string op = tokens[i];
            auto right = parseInterpolatedOperand(tokens[i + 1]);
            
            OpCode opCode = OpCode::ADD;
            if (op == "+") opCode = OpCode::ADD;
            else if (op == "*") opCode = OpCode::MUL;
            else if (op == ">") opCode = OpCode::GT;
            else if (op == "<") opCode = OpCode::LT;
            else if (op == "==") opCode = OpCode::EQ;
            
            result = std::make_unique<BinaryExpr>(std::move(result), std::move(right), opCode);
        }
        
        return result;
    }
    
    inline std::unique_ptr<ASTNode> parseInterpolatedString(const std::string& raw) {
        std::unique_ptr<ASTNode> currentExpr = nullptr;
        std::string buffer;
        bool inVar = false;
        int braceDepth = 0;
        std::string varBuffer;

        for (size_t i = 0; i < raw.length(); i++) {
            char c = raw[i];
            
            if (c == '{' && !inVar) {
                inVar = true;
                braceDepth = 1;
                varBuffer.clear();
            } else if (c == '{' && inVar) {
                braceDepth++;
                varBuffer += c;
            } else if (c == '}' && inVar) {
                braceDepth--;
                if (braceDepth == 0) {
                    bool shouldInterpolate = false;
                    std::string trimmed = varBuffer;
                    while (!trimmed.empty() && trimmed[0] == ' ') trimmed = trimmed.substr(1);
                    
                    if (!trimmed.empty()) {
                        char first = trimmed[0];
                        if ((first >= 'a' && first <= 'z') || first == '(') {
                            shouldInterpolate = true;
                        }
                        if (trimmed.find('+') != std::string::npos ||
                            trimmed.find('-') != std::string::npos ||
                            trimmed.find('*') != std::string::npos ||
                            trimmed.find('/') != std::string::npos ||
                            trimmed.find('.') != std::string::npos) {
                            shouldInterpolate = true;
                        }
                    }
                    
                    if (shouldInterpolate) {
                        if (!buffer.empty()) {
                            auto lit = std::make_unique<LiteralExpr>(buffer);
                            if (currentExpr) 
                                currentExpr = std::make_unique<BinaryExpr>(std::move(currentExpr), std::move(lit), OpCode::ADD);
                            else 
                                currentExpr = std::move(lit);
                            buffer.clear();
                        }
                        
                        auto varExpr = parseInterpolatedExpr(varBuffer);
                        
                        if (currentExpr) 
                            currentExpr = std::make_unique<BinaryExpr>(std::move(currentExpr), std::move(varExpr), OpCode::ADD);
                        else 
                            currentExpr = std::move(varExpr);
                    } else {
                        buffer += '{';
                        buffer += varBuffer;
                        buffer += '}';
                    }
                    
                    varBuffer.clear();
                    inVar = false;
                } else {
                    varBuffer += c;
                }
            } else if (inVar) {
                varBuffer += c;
            } else {
                buffer += c;
            }
        }

        if (inVar) {
            buffer += '{';
            buffer += varBuffer;
        }

        if (!buffer.empty()) {
            auto lit = std::make_unique<LiteralExpr>(buffer);
            if (currentExpr) 
                currentExpr = std::make_unique<BinaryExpr>(std::move(currentExpr), std::move(lit), OpCode::ADD);
            else 
                currentExpr = std::move(lit);
        }

        if (!currentExpr) return std::make_unique<LiteralExpr>("");
        return currentExpr;
    }

    // --- 2. FATORES PRIMÁRIOS ---
    inline std::unique_ptr<ASTNode> parsePrimary(Parser& p) {
        Token token = p.peek();
        
        if (token.type == TokenType::STRING_RAW) {
            p.consume(TokenType::STRING_RAW, "");
            return std::make_unique<LiteralExpr>(token.value);
        }
        
        if (token.type == TokenType::STRING) {
            p.consume(TokenType::STRING, "");
            if (token.value.find('{') != std::string::npos) {
                return parseInterpolatedString(token.value);
            }
            return std::make_unique<LiteralExpr>(token.value);
        }
        
        if (token.type == TokenType::AUTO) {
            p.consume(TokenType::AUTO, "");
            std::unique_ptr<ASTNode> expr = std::make_unique<AutoExpr>();
            
            while (p.peek().type == TokenType::DOT) {
                p.consume(TokenType::DOT, "");
                Token memberToken = p.consume(TokenType::ID, "Esperado nome do atributo");
                expr = std::make_unique<MemberAccessExpr>(std::move(expr), memberToken.value);
            }
            
            return expr;
        }
        
        if (token.type == TokenType::ID) {
            std::string name = token.value;
            p.consume(TokenType::ID, "");
            
            // Chamada de função: nome(args)
            if (p.peek().type == TokenType::LPAREN) {
                p.consume(TokenType::LPAREN, "");
                
                std::vector<std::unique_ptr<ASTNode>> args;
                
                if (p.peek().type != TokenType::RPAREN) {
                    args.push_back(parse(p));
                    
                    while (p.peek().type == TokenType::COMMA) {
                        p.consume(TokenType::COMMA, "");
                        args.push_back(parse(p));
                    }
                }
                
                p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
                
                // === FUNÇÕES BUILT-IN ===
                
                // entrada(prompt) - lê entrada do usuário
                if (name == "entrada") {
                    if (args.size() != 1) {
                        p.error("entrada() espera 1 argumento (prompt)");
                    }
                    return std::make_unique<InputExpr>(std::move(args[0]));
                }
                
                // inteiro(valor) / int(valor) - converte para inteiro
                if (name == "inteiro" || name == "int") {
                    if (args.size() != 1) {
                        p.error(name + "() espera 1 argumento");
                    }
                    return std::make_unique<ToIntExpr>(std::move(args[0]));
                }
                
                // decimal(valor) / dec(valor) - converte para decimal/float
                if (name == "decimal" || name == "dec") {
                    if (args.size() != 1) {
                        p.error(name + "() espera 1 argumento");
                    }
                    return std::make_unique<ToFloatExpr>(std::move(args[0]));
                }
                
                // texto(valor) - converte para string
                if (name == "texto") {
                    if (args.size() != 1) {
                        p.error("texto() espera 1 argumento");
                    }
                    return std::make_unique<ToStringExpr>(std::move(args[0]));
                }
                
                // booleano(valor) / bool(valor) - converte para booleano
                if (name == "booleano" || name == "bool") {
                    if (args.size() != 1) {
                        p.error(name + "() espera 1 argumento");
                    }
                    return std::make_unique<ToBoolExpr>(std::move(args[0]));
                }
                
                // tipo(valor) - retorna o tipo do valor como string
                if (name == "tipo") {
                    if (args.size() != 1) {
                        p.error("tipo() espera 1 argumento");
                    }
                    return std::make_unique<TypeOfExpr>(std::move(args[0]));
                }
                
                // Verifica se é função nativa
                if (nativeFuncTable.find(name) != nativeFuncTable.end()) {
                    return std::make_unique<NativeCallExpr>(name, std::move(args));
                }
                
                return std::make_unique<FuncCallExpr>(name, std::move(args));
            }
            
            // Acesso a índice de lista: nome[indice]
            if (p.peek().type == TokenType::LBRACKET) {
                p.consume(TokenType::LBRACKET, "");
                auto index = parse(p);
                p.consume(TokenType::RBRACKET, "Esperado ']' apos indice");
                
                // Cria a expressão de acesso à lista
                std::unique_ptr<ASTNode> expr = std::make_unique<ListAccessExpr>(
                    std::make_unique<VarExpr>(name), std::move(index));
                
                // Verifica se há acesso a membro/método após o ]
                while (p.peek().type == TokenType::DOT) {
                    p.consume(TokenType::DOT, "");
                    Token memberToken = p.consumeMemberName("Esperado nome do membro");
                    
                    if (p.peek().type == TokenType::LPAREN) {
                        // É uma chamada de método: lista[0].metodo()
                        p.consume(TokenType::LPAREN, "");
                        std::vector<std::unique_ptr<ASTNode>> args;
                        
                        if (p.peek().type != TokenType::RPAREN) {
                            args.push_back(parse(p));
                            while (p.peek().type == TokenType::COMMA) {
                                p.consume(TokenType::COMMA, "");
                                args.push_back(parse(p));
                            }
                        }
                        
                        p.consume(TokenType::RPAREN, "Esperado ')'");
                        expr = std::make_unique<MethodCallExpr>(std::move(expr), memberToken.value, std::move(args));
                    } else {
                        // É um acesso a atributo: lista[0].atributo
                        expr = std::make_unique<MemberAccessExpr>(std::move(expr), memberToken.value);
                    }
                }
                
                return expr;
            }
            
            // Acesso a membro ou método: nome.algo
            if (p.peek().type == TokenType::DOT) {
                p.consume(TokenType::DOT, "");
                Token memberToken = p.consumeMemberName("Esperado nome do membro");
                std::string memberName = memberToken.value;
                
                bool isModuleOrClass = (name[0] >= 'A' && name[0] <= 'Z') ||
                                       (moduleTable.find(name) != moduleTable.end()) ||
                                       (classTable.find(name) != classTable.end());
                
                if (isModuleOrClass && p.peek().type == TokenType::LPAREN) {
                    p.consume(TokenType::LPAREN, "");
                    
                    std::vector<std::unique_ptr<ASTNode>> args;
                    
                    if (p.peek().type != TokenType::RPAREN) {
                        args.push_back(parse(p));
                        
                        while (p.peek().type == TokenType::COMMA) {
                            p.consume(TokenType::COMMA, "");
                            args.push_back(parse(p));
                        }
                    }
                    
                    p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
                    return std::make_unique<MethodCallExpr>(name, memberName, std::move(args));
                }
                
                if (!isModuleOrClass) {
                    bool isListMethod = (memberName == "adicionar" || memberName == "add" || memberName == "append" ||
                                        memberName == "remover" || memberName == "remove" ||
                                        memberName == "tamanho" || memberName == "size" || memberName == "len" ||
                                        memberName == "exibir" || memberName == "display" || memberName == "mostrar");
                    
                    if (isListMethod && p.peek().type == TokenType::LPAREN) {
                        p.consume(TokenType::LPAREN, "");
                        
                        auto listMethod = std::make_unique<ListMethodExpr>(
                            std::make_unique<VarExpr>(name), memberName);
                        
                        if (p.peek().type != TokenType::RPAREN) {
                            listMethod->addArg(parse(p));
                            
                            while (p.peek().type == TokenType::COMMA) {
                                p.consume(TokenType::COMMA, "");
                                listMethod->addArg(parse(p));
                            }
                        }
                        
                        p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
                        return listMethod;
                    }
                }
                
                if (p.peek().type == TokenType::LPAREN) {
                    p.consume(TokenType::LPAREN, "");
                    
                    std::vector<std::unique_ptr<ASTNode>> args;
                    
                    if (p.peek().type != TokenType::RPAREN) {
                        args.push_back(parse(p));
                        
                        while (p.peek().type == TokenType::COMMA) {
                            p.consume(TokenType::COMMA, "");
                            args.push_back(parse(p));
                        }
                    }
                    
                    p.consume(TokenType::RPAREN, "Esperado ')' apos argumentos");
                    auto objExpr = std::make_unique<VarExpr>(name);
                    return std::make_unique<MethodCallExpr>(std::move(objExpr), memberName, std::move(args));
                }
                
                auto objExpr = std::make_unique<VarExpr>(name);
                std::unique_ptr<ASTNode> expr = std::make_unique<MemberAccessExpr>(std::move(objExpr), memberName);
                
                while (p.peek().type == TokenType::DOT) {
                    p.consume(TokenType::DOT, "");
                    Token nextMember = p.consumeMemberName("Esperado nome do membro");
                    
                    if (p.peek().type == TokenType::LPAREN) {
                        p.consume(TokenType::LPAREN, "");
                        std::vector<std::unique_ptr<ASTNode>> args;
                        
                        if (p.peek().type != TokenType::RPAREN) {
                            args.push_back(parse(p));
                            while (p.peek().type == TokenType::COMMA) {
                                p.consume(TokenType::COMMA, "");
                                args.push_back(parse(p));
                            }
                        }
                        
                        p.consume(TokenType::RPAREN, "Esperado ')'");
                        expr = std::make_unique<MethodCallExpr>(std::move(expr), nextMember.value, std::move(args));
                    } else {
                        expr = std::make_unique<MemberAccessExpr>(std::move(expr), nextMember.value);
                    }
                }
                
                return expr;
            }
            
            return std::make_unique<VarExpr>(name);
        }

        if (token.type == TokenType::NUMBER_INT) {
            p.consume(TokenType::NUMBER_INT, "");
            return std::make_unique<LiteralExpr>((long)std::stol(token.value));
        }
        if (token.type == TokenType::NUMBER_FLOAT) {
            p.consume(TokenType::NUMBER_FLOAT, "");
            return std::make_unique<LiteralExpr>(std::stod(token.value));
        }
        
        if (token.type == TokenType::MINUS) {
            p.consume(TokenType::MINUS, "");
            auto operand = parsePrimary(p);
            auto zero = std::make_unique<LiteralExpr>((long)0);
            return std::make_unique<BinaryExpr>(std::move(zero), std::move(operand), OpCode::SUB);
        }
        
        if (token.type == TokenType::TRUE) {
            p.consume(TokenType::TRUE, "");
            return std::make_unique<LiteralExpr>(true);
        }
        if (token.type == TokenType::FALSE) {
            p.consume(TokenType::FALSE, "");
            return std::make_unique<LiteralExpr>(false);
        }
        
        if (token.type == TokenType::LPAREN) {
            p.consume(TokenType::LPAREN, "");
            auto expr = parse(p);
            p.consume(TokenType::RPAREN, "Esperado ')'");
            return expr;
        }
        
        if (token.type == TokenType::LBRACKET) {
            p.consume(TokenType::LBRACKET, "");
            
            auto list = std::make_unique<ListCreateExpr>();
            
            if (p.peek().type == TokenType::RBRACKET) {
                p.consume(TokenType::RBRACKET, "");
                return list;
            }
            
            list->addElement(parse(p));
            
            while (p.peek().type == TokenType::COMMA) {
                p.consume(TokenType::COMMA, "");
                list->addElement(parse(p));
            }
            
            p.consume(TokenType::RBRACKET, "Esperado ']' apos elementos da lista");
            return list;
        }

        p.error("Expressao invalida: " + token.value);
        return nullptr;
    }

    // --- 3. MULTIPLICAÇÃO / DIVISÃO / MÓDULO ---
    inline std::unique_ptr<ASTNode> parseMultiplication(Parser& p) {
        auto left = parsePrimary(p);
        while (p.peek().type == TokenType::STAR || 
               p.peek().type == TokenType::SLASH || 
               p.peek().type == TokenType::PERCENT) {
            TokenType opType = p.peek().type;
            p.consume(opType, "");
            auto right = parsePrimary(p);
            
            OpCode op = OpCode::MUL;
            if (opType == TokenType::SLASH) op = OpCode::DIV;
            else if (opType == TokenType::PERCENT) op = OpCode::MOD;
            
            left = std::make_unique<BinaryExpr>(std::move(left), std::move(right), op);
        }
        return left;
    }

    // --- 4. SOMA / SUBTRAÇÃO ---
    inline std::unique_ptr<ASTNode> parseAddition(Parser& p) {
        auto left = parseMultiplication(p);
        while (p.peek().type == TokenType::PLUS || p.peek().type == TokenType::MINUS) {
            TokenType opType = p.peek().type;
            p.consume(opType, "");
            auto right = parseMultiplication(p);
            
            OpCode op = (opType == TokenType::PLUS) ? OpCode::ADD : OpCode::SUB;
            left = std::make_unique<BinaryExpr>(std::move(left), std::move(right), op);
        }
        return left;
    }

    // --- 5. COMPARAÇÃO ---
    inline std::unique_ptr<ASTNode> parseComparison(Parser& p) {
        auto left = parseAddition(p);

        TokenType t = p.peek().type;
        while (t == TokenType::GT || t == TokenType::LT || t == TokenType::EQ_OP ||
               t == TokenType::GTE || t == TokenType::LTE || t == TokenType::NEQ) {
            OpCode op = OpCode::EQ; 
            if (t == TokenType::GT) op = OpCode::GT;
            else if (t == TokenType::LT) op = OpCode::LT;
            else if (t == TokenType::EQ_OP) op = OpCode::EQ;
            else if (t == TokenType::GTE) op = OpCode::GTE;
            else if (t == TokenType::LTE) op = OpCode::LTE;
            else if (t == TokenType::NEQ) op = OpCode::NEQ;

            p.consume(t, ""); 
            auto right = parseAddition(p);
            left = std::make_unique<BinaryExpr>(std::move(left), std::move(right), op);
            t = p.peek().type;
        }
        return left;
    }

    // --- 6. LÓGICA - PONTO DE ENTRADA ---
    inline std::unique_ptr<ASTNode> parse(Parser& p) {
        auto left = parseComparison(p);

        TokenType t = p.peek().type;
        while (t == TokenType::AND || t == TokenType::OR) {
            OpCode op = (t == TokenType::AND) ? OpCode::AND : OpCode::OR;
            
            p.consume(t, ""); 
            auto right = parseComparison(p);
            left = std::make_unique<BinaryExpr>(std::move(left), std::move(right), op);
            t = p.peek().type;
        }
        return left;
    }
}

#endif