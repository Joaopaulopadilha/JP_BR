// main.cpp
// JPLang - Compilador (bytecode -> C -> TCC)
// Sem VM: toda execução passa por TCC

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

// INCLUDES OBRIGATÓRIOS
#include "lexer.hpp"
#include "parser.hpp"
#include "opcodes.hpp"
#include "import_processor.hpp"
#include "jpc_compilador.hpp"

namespace fs = std::filesystem;

// Configuração console UTF-8
#ifdef _WIN32
#include <windows.h>
void setupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setvbuf(stdout, nullptr, _IOFBF, 1000);
}
#else
#include <clocale>
void setupConsole() { std::setlocale(LC_ALL, ""); }
#endif

void saveDebugFile(const std::string& originalFilename, const std::vector<Instruction>& code) {
    if (!fs::exists("debug")) {
        fs::create_directory("debug");
    }

    fs::path p(originalFilename);
    std::string debugName = "debug/" + p.stem().string() + ".jpdbg";

    std::ofstream out(debugName);
    if (out.is_open()) {
        out << "--- BYTECODE JP ---\n";
        int idx = 0;
        for (const auto& inst : code) {
            out << idx << "\t" << opToString(inst.op);
            if (inst.operand.has_value()) {
                out << "\t" << valToString(inst.operand.value());
            }
            out << "\n";
            idx++;
        }
        out.close();
    }
}

void mostrarAjuda() {
    std::cout << "JPLang - Compilador\n\n";
    std::cout << "Uso:\n";
    std::cout << "  jp <arquivo.jp>              Executa o arquivo\n";
    std::cout << "  jp build <arquivo.jp>        Compila para executavel\n";
    std::cout << "  jp debug <arquivo.jp>        Executa e gera debug/opcodes\n";
    std::cout << "\nOpcoes:\n";
    std::cout << "  -w                           Modo janela (sem console)\n";
    std::cout << "\nExemplos:\n";
    std::cout << "  jp meu_programa.jp           Compila e executa\n";
    std::cout << "  jp build meu_programa.jp     Gera output/meu_programa/meu_programa.exe\n";
    std::cout << "  jp debug meu_programa.jp     Executa + gera debug/meu_programa.jpdbg\n";
}

int main(int argc, char* argv[]) {
    setupConsole();

    if (argc < 2) {
        mostrarAjuda();
        return 1;
    }

    std::string arg1 = argv[1];

    // Verifica modo
    bool modoBuild = (arg1 == "compilar" || arg1 == "build");
    bool modoDebug = (arg1 == "debug");
    bool modoJanela = false;
    std::string caminhoArquivo;

    if (modoBuild || modoDebug) {
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "-w") {
                modoJanela = true;
            } else if (caminhoArquivo.empty()) {
                caminhoArquivo = arg;
            }
        }
        if (caminhoArquivo.empty()) {
            std::cerr << "Erro: Nenhum arquivo especificado.\n";
            std::cerr << "Uso: jp build <arquivo.jp>\n";
            return 1;
        }
    } else {
        caminhoArquivo = arg1;
    }

    fs::path filePath = caminhoArquivo;
    if (!fs::exists(filePath)) {
        std::cerr << "Erro: Arquivo nao encontrado: " << filePath << std::endl;
        return 1;
    }

    // Define o diretório base como o diretório do executável
    fs::path exePath = fs::path(argv[0]).parent_path();
    if (exePath.empty()) {
        exePath = fs::current_path();
    }
    ImportProcessor::setBaseDir(exePath.string());

    // Lê o arquivo fonte
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try {
        // Limpa estado
        ImportProcessor::reset();
        functionTable.clear();
        classTable.clear();
        classMethodTable.clear();
        nativeFuncTable.clear();

        // Tokeniza
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        // Parseia (registra imports durante parsing)
        Parser parser(tokens);
        auto ast = parser.parse();

        // Compila módulos primeiro (bytecode)
        std::vector<Instruction> code;
        ImportProcessor::processImports(code);

        // Compila arquivo principal
        ast->compile(code);
        code.push_back({OpCode::HALT, std::nullopt});

        // Salva debug apenas no modo debug
        if (modoDebug) {
            saveDebugFile(caminhoArquivo, code);
        }

        // Compila e executa/gera
        CompiladorC compilador;
        std::string nomeBase = filePath.stem().string();

        if (modoBuild) {
            // === BUILD: gera executável em output/ ===
            std::cout << "[JP] Compilando: " << filePath << std::endl;
            if (modoJanela) {
                std::cout << "[JP] Modo: Janela (sem console)" << std::endl;
            }

            if (!compilador.compilar(code, nomeBase, modoJanela)) {
                return 1;
            }
        } else {
            // === RUN / DEBUG: compila em temp/ e executa ===
            if (modoDebug) {
                std::cout << "[JP] Debug: debug/" << nomeBase << ".jpdbg" << std::endl;
            }
            if (!compilador.executar(code, nomeBase)) {
                return 1;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}