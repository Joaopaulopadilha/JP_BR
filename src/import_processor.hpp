// import_processor.hpp
// Processador de importações - carrega e processa arquivos .jp

#ifndef IMPORT_PROCESSOR_HPP
#define IMPORT_PROCESSOR_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <memory>

#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"

namespace fs = std::filesystem;

class ImportProcessor {
public:
    // Diretório base (onde está o executável)
    static inline std::string baseDir = ".";
    
    // Define o diretório base e inicializa o callback de parsing eager
    static void setBaseDir(const std::string& dir) {
        baseDir = dir;
        importBaseDir = dir;
        
        // Define o callback para parsing eager
        // Quando um módulo é registrado, ele será parseado imediatamente
        onModuleRegistered = [](const std::string& path) {
            parseModuleEager(path);
        };
    }
    
    // Parseia um módulo imediatamente (eager loading)
    // Registra classes na classTable antes de continuar o parsing
    static void parseModuleEager(const std::string& path) {
        std::string fullPath = resolvePath(path);
        
        // Evita importação circular
        if (processedFiles.find(fullPath) != processedFiles.end()) {
            return;
        }
        processedFiles.insert(fullPath);
        
        // Lê o arquivo
        std::string source = readFile(fullPath);
        if (source.empty()) {
            return;
        }
        
        // Tokeniza
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        
        // Parseia - isso registra classes, funções nativas, etc.
        Parser parser(tokens);
        auto ast = parser.parse();
    }
    
    // Processa todas as importações pendentes (compila os módulos)
    static void processImports(std::vector<Instruction>& bytecode) {
        // Processa módulos na tabela
        for (auto& [key, info] : moduleTable) {
            if (!info.isLoaded) {
                loadModule(info, bytecode);
            }
        }
    }
    
    // Carrega e compila um módulo
    static void loadModule(ModuleInfo& info, std::vector<Instruction>& bytecode) {
        // Evita compilação duplicada
        if (info.isLoaded) {
            return;
        }
        
        // Marca como carregado ANTES de parsear para evitar loop infinito
        info.isLoaded = true;
        
        std::string fullPath = resolvePath(info.path);
        
        // Lê o arquivo
        std::string source = readFile(fullPath);
        if (source.empty()) {
            throw std::runtime_error("Arquivo nao encontrado: " + fullPath);
        }
        
        // Tokeniza
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        
        // Parseia
        Parser parser(tokens);
        auto ast = parser.parse();
        
        // Processa importações recursivamente do módulo carregado
        processImports(bytecode);
        
        // Compila o módulo
        ast->compile(bytecode);
    }
    
    // Resolve o caminho do arquivo
    static std::string resolvePath(const std::string& path) {
        // Se é caminho absoluto, usa direto
        if (fs::path(path).is_absolute()) {
            return path;
        }
        
        // Tenta relativo ao diretório base
        fs::path fullPath = fs::path(baseDir) / path;
        if (fs::exists(fullPath)) {
            return fullPath.string();
        }
        
        // Tenta no diretório atual
        if (fs::exists(path)) {
            return path;
        }
        
        return path;
    }
    
    // Lê o conteúdo de um arquivo
    static std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    // Limpa estado para nova compilação
    static void reset() {
        moduleTable.clear();
        processedFiles.clear();
        aliasToModule.clear();
    }
};

#endif