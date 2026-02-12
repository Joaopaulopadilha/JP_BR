// ast_importar.hpp
// Nós de importação do AST (importar módulos e arquivos .jp)
// CORRIGIDO: Carrega módulos durante o parsing para registrar classes antes do uso

#ifndef AST_SRC_AST_IMPORTAR_HPP
#define AST_SRC_AST_IMPORTAR_HPP

#include "../opcodes.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>

// Forward declarations
struct ASTNode;
struct BlockStmt;

// Tipo de importação
enum class ImportType {
    MODULE,         // importar tempo (de bibliotecas/)
    FILE,           // importar "arquivo.jp"
    SELECTIVE,      // de tempo importar func1, func2
    ALIAS           // importar tempo como tp
};

// Informação de módulo importado
struct ModuleInfo {
    std::string name;           // Nome do módulo (tempo, teclado, etc)
    std::string path;           // Caminho do arquivo .jp
    std::string alias;          // Alias (se houver)
    std::vector<std::string> selectedFuncs; // Funções selecionadas (se importação seletiva)
    bool isLoaded;              // Se já foi compilado
    bool isParsed;              // Se já foi parseado (classes registradas)
};

// Tabela global de módulos importados
inline std::unordered_map<std::string, ModuleInfo> moduleTable;

// Conjunto de arquivos já processados (evita importação circular)
inline std::unordered_set<std::string> processedFiles;

// Mapeamento de alias para nome real do módulo
inline std::unordered_map<std::string, std::string> aliasToModule;

// Diretório base para resolução de caminhos
inline std::string importBaseDir = ".";

// Callback para parsing eager - será definido pelo import_processor.hpp
// Isso evita dependência circular com Parser
inline std::function<void(const std::string&)> onModuleRegistered = nullptr;

// Declaração de importação
struct ImportStmt : public ASTNode {
    ImportType type;
    std::string moduleName;     // Nome do módulo ou caminho do arquivo
    std::string alias;          // Alias (para "importar X como Y")
    std::vector<std::string> selectedItems; // Itens selecionados (para "de X importar a, b")
    bool registered = false;
    
    // Construtor para importação simples: importar tempo
    ImportStmt(const std::string& name) 
        : type(ImportType::MODULE), moduleName(name), alias("") {
        // Registra o módulo imediatamente durante o parsing
        registerModule();
    }
    
    // Construtor para importação de arquivo: importar "teste.jp"
    static std::unique_ptr<ImportStmt> fromFile(const std::string& path) {
        auto stmt = std::make_unique<ImportStmt>(path);
        stmt->type = ImportType::FILE;
        stmt->updateRegistration();
        return stmt;
    }
    
    // Construtor para importação com alias: importar tempo como tp
    static std::unique_ptr<ImportStmt> withAlias(const std::string& name, const std::string& als) {
        auto stmt = std::make_unique<ImportStmt>(name);
        stmt->type = ImportType::ALIAS;
        stmt->alias = als;
        stmt->updateRegistration();
        return stmt;
    }
    
    // Construtor para importação seletiva: de tempo importar func1, func2
    static std::unique_ptr<ImportStmt> selective(const std::string& name, std::vector<std::string> items) {
        auto stmt = std::make_unique<ImportStmt>(name);
        stmt->type = ImportType::SELECTIVE;
        stmt->selectedItems = std::move(items);
        stmt->updateRegistration();
        return stmt;
    }
    
    void registerModule() {
        if (registered) return;
        registered = true;
        
        ModuleInfo info;
        info.name = moduleName;
        info.alias = alias;
        info.selectedFuncs = selectedItems;
        info.isLoaded = false;
        info.isParsed = false;
        
        // Determina o caminho baseado no tipo
        if (type == ImportType::FILE) {
            info.path = moduleName;
        } else {
            // Módulo de biblioteca: bibliotecas/nome/nome.jp
            info.path = "bibliotecas/" + moduleName + "/" + moduleName + ".jp";
        }
        
        // Usa alias ou nome como chave
        std::string key = alias.empty() ? moduleName : alias;
        
        // Verifica se já foi registrado e parseado (evita reprocessamento)
        if (moduleTable.find(key) != moduleTable.end() && moduleTable[key].isParsed) {
            return;
        }
        
        moduleTable[key] = info;
        
        // Registra alias se houver
        if (!alias.empty()) {
            aliasToModule[alias] = moduleName;
        }
        
        // NOVO: Chama callback para parsing eager (se definido)
        // O callback é definido pelo import_processor.hpp após incluir Parser
        if (onModuleRegistered) {
            onModuleRegistered(info.path);
            moduleTable[key].isParsed = true;
        }
    }
    
    void updateRegistration() {
        // Remove registro antigo e re-registra com novos dados
        std::string oldKey = moduleName;
        if (moduleTable.find(oldKey) != moduleTable.end()) {
            moduleTable.erase(oldKey);
        }
        registered = false;
        registerModule();
    }
    
    void compile(std::vector<Instruction>& bytecode) override {
        // Nada a fazer aqui - o registro já foi feito no construtor
        // A compilação do módulo é feita pelo ImportProcessor
    }
};

#endif