// jpc_compilador.hpp
// Compilador JPLang -> C -> Executável
// Gera código C e compila com TCC
// CORRIGIDO: Não trata arquivos .jp importados como bibliotecas nativas

#ifndef JPC_COMPILADOR_HPP
#define JPC_COMPILADOR_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <algorithm>

#include "opcodes.hpp"
#include "ast_src/ast_importar.hpp"  // Para acessar moduleTable e importBaseDir
#include "ast_src/ast_funcoes.hpp"   // Para acessar functionTable
#include "ast_src/ast_classes.hpp"   // Para acessar classMethodTable
#include "jpc_src/jpc_runtime.hpp"
#include "jpc_src/jpc_gerador.hpp"

namespace fs = std::filesystem;

class CompiladorC {
public:
    
    // Verifica se um módulo é uma biblioteca nativa (DLL) ou código JPLang
    // Retorna true se for biblioteca nativa, false se for arquivo .jp de código
    static bool isNativeLibrary(const std::string& name, const std::string& path) {
        // Se o nome contém barras, é um arquivo de código importado (rotas/caixa.jp)
        if (name.find('\\') != std::string::npos || name.find('/') != std::string::npos) {
            return false;
        }
        
        // Se o nome termina com .jp, é código fonte, não biblioteca
        if (name.length() > 3 && name.substr(name.length() - 3) == ".jp") {
            return false;
        }
        
        // Se o path contém barras E termina com .jp, verifica se é wrapper de biblioteca
        if (!path.empty()) {
            // Normaliza barras para verificação
            std::string normalizedPath = path;
            std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
            
            // Se termina com .jp
            if (normalizedPath.length() > 3 && normalizedPath.substr(normalizedPath.length() - 3) == ".jp") {
                // Verifica se é o padrão bibliotecas/nome/nome.jp
                // Isso indica wrapper de biblioteca nativa
                std::string expected1 = "bibliotecas/" + name + "/" + name + ".jp";
                std::string expected2 = "bibliotecas\\" + name + "\\" + name + ".jp";
                
                if (normalizedPath == expected1 || path == expected2 ||
                    normalizedPath.find("bibliotecas/" + name + "/" + name + ".jp") != std::string::npos) {
                    return true; // É wrapper de biblioteca nativa
                }
                
                // Qualquer outro arquivo .jp não é biblioteca nativa
                return false;
            }
        }
        
        // Módulos simples (sem .jp no nome e sem barras) são bibliotecas nativas
        // Ex: svs, tempo, sqlite -> são bibliotecas
        return true;
    }
    
    // Gera código C a partir do bytecode
    // workDir: diretório de trabalho original (modo executar usa cwd, modo build fica vazio)
    void gerarC(const std::vector<Instruction>& code, const std::string& saida, bool modoJanela = false, const std::string& workDir = "") {
        std::ofstream out(saida);
        if (!out.is_open()) throw std::runtime_error("Erro ao criar arquivo de saida: " + saida);

        // Coleta informações do código
        std::set<std::string> bibliotecasUsadas;
        std::map<std::string, int> nativasMap;
        std::map<std::string, int> variaveis;
        std::map<std::string, size_t> funcoesInternas;    // Métodos de módulo (METHOD_CALL)
        std::map<std::string, size_t> funcoesUsuario;     // Funções do usuário (CALL)
        int varIndex = 0;
        
        // CORREÇÃO: Usa APENAS módulos que são bibliotecas nativas (não arquivos .jp)
        for (const auto& [key, info] : moduleTable) {
            if (isNativeLibrary(info.name, info.path)) {
                bibliotecasUsadas.insert(info.name);
            }
        }
        
        // Analisa o bytecode para coletar variáveis, funções nativas e funções do usuário
        for (size_t i = 0; i < code.size(); i++) {
            const auto& instr = code[i];
            
            // Registra variáveis
            if ((instr.op == OpCode::STORE_VAR || instr.op == OpCode::LOAD_VAR) && instr.operand.has_value()) {
                std::string nome = std::get<std::string>(instr.operand.value());
                if (variaveis.find(nome) == variaveis.end()) {
                    variaveis[nome] = varIndex++;
                }
            }
            
            // Registra funções nativas (NATIVE_CALL)
            if (instr.op == OpCode::NATIVE_CALL && instr.operand.has_value()) {
                std::string callInfo = std::get<std::string>(instr.operand.value());
                size_t colonPos = callInfo.find(':');
                if (colonPos != std::string::npos) {
                    std::string nome = callInfo.substr(0, colonPos);
                    int aridade = std::stoi(callInfo.substr(colonPos + 1));
                    nativasMap[nome] = aridade;
                }
            }
            
            // Registra funções do usuário (CALL)
            if (instr.op == OpCode::CALL && instr.operand.has_value()) {
                std::string callInfo = std::get<std::string>(instr.operand.value());
                size_t colonPos = callInfo.find(':');
                if (colonPos != std::string::npos) {
                    std::string funcName = callInfo.substr(0, colonPos);
                    if (funcoesUsuario.find(funcName) == funcoesUsuario.end()) {
                        funcoesUsuario[funcName] = 0; // Endereço será preenchido depois
                    }
                }
            }
            
            // Registra funções internas de módulo (METHOD_CALL)
            if (instr.op == OpCode::METHOD_CALL && instr.operand.has_value()) {
                std::string callInfo = std::get<std::string>(instr.operand.value());
                size_t colonPos = callInfo.find(':');
                if (colonPos != std::string::npos) {
                    std::string fullName = callInfo.substr(0, colonPos);
                    
                    size_t dotPos = fullName.find('.');
                    if (dotPos != std::string::npos) {
                        std::string modulo = fullName.substr(0, dotPos);
                        std::string metodo = fullName.substr(dotPos + 1);
                        std::string funcName = modulo + "_" + metodo;
                        
                        if (funcoesInternas.find(funcName) == funcoesInternas.end()) {
                            funcoesInternas[funcName] = 0;
                        }
                    }
                }
            }
        }
        
        // Mapeia funções do usuário para endereços (da functionTable)
        for (auto& [funcName, addr] : funcoesUsuario) {
            if (functionTable.find(funcName) != functionTable.end()) {
                addr = (size_t)functionTable[funcName].address;
            }
        }
        
        // Mapeia funções internas de módulo para endereços (da classMethodTable)
        for (auto& [funcName, addr] : funcoesInternas) {
            size_t underPos = funcName.find('_');
            if (underPos != std::string::npos) {
                std::string moduleName = funcName.substr(0, underPos);
                std::string methodName = funcName.substr(underPos + 1);
                
                if (classMethodTable.find(moduleName) != classMethodTable.end()) {
                    auto& methods = classMethodTable[moduleName];
                    if (methods.find(methodName) != methods.end()) {
                        addr = (size_t)methods[methodName].address;
                    }
                }
            }
        }

        // === HEADER ===
        out << R"(// Codigo gerado pelo JP Compiler
#pragma execution_character_set("utf-8")
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "jpruntime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

)";

        // === MEMÓRIA ===
        out << "#define MEM_SIZE " << std::max((int)variaveis.size() + 64, 256) << "\n";
        out << "#define STACK_SIZE 2048\n";
        out << "#define CALL_STACK_SIZE 1024\n\n";
        out << "JPValor vars[MEM_SIZE];\n";
        out << "JPValor stack[STACK_SIZE];\n";
        out << "int sp = 0;\n";
        out << "int call_stack[CALL_STACK_SIZE];\n";
        out << "int csp = 0;\n\n";

        // === MACROS ===
        out << "#define PUSH(v) do { stack[sp++] = (v); } while(0)\n";
        out << "#define POP() (stack[--sp])\n";
        out << "#define TOP() (stack[sp-1])\n\n";

        // === BIBLIOTECAS NATIVAS ===
        if (!bibliotecasUsadas.empty()) {
            out << "// === BIBLIOTECAS NATIVAS ===\n";
            for (const auto& bib : bibliotecasUsadas) {
                out << "JPBiblioteca lib_" << bib << ";\n";
            }
            out << "\n";

            for (const auto& [nome, aridade] : nativasMap) {
                out << "JPFuncaoNativa fn_" << nome << " = NULL;\n";
            }
            out << "\n";

            out << "int carregar_bibliotecas() {\n";
            for (const auto& bib : bibliotecasUsadas) {
                out << "    lib_" << bib << " = jp_carregar_lib(\"" << bib << "\");\n";
                out << "    if (!lib_" << bib << ".handle) {\n";
                out << "        printf(\"[ERRO] Falha ao carregar biblioteca: " << bib << "\\n\");\n";
                out << "        return 0;\n";
                out << "    }\n";
            }
            out << "\n";

            for (const auto& [nome, aridade] : nativasMap) {
                for (const auto& bib : bibliotecasUsadas) {
                    out << "    if (!fn_" << nome << ") fn_" << nome << " = jp_obter_funcao(&lib_" << bib << ", \"jp_" << nome << "\");\n";
                }
            }
            out << "    return 1;\n";
            out << "}\n\n";

            out << "void descarregar_bibliotecas() {\n";
            for (const auto& bib : bibliotecasUsadas) {
                out << "    jp_descarregar_lib(&lib_" << bib << ");\n";
            }
            out << "}\n\n";
        }

        // === HELPERS E LISTAS ===
        out << JPCRuntime::HELPERS;
        out << JPCRuntime::LISTAS;
        out << JPCRuntime::OBJETOS;

        // === MAIN ===
        if (modoJanela) {
            out << "#ifdef _WIN32\n";
            out << "int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {\n";
            out << "#else\n";
            out << "int main() {\n";
            out << "#endif\n";
        } else {
            out << "int main() {\n";
        }
        
        // Configura console para UTF-8
        out << "#ifdef _WIN32\n";
        out << "    SetConsoleOutputCP(65001);\n";
        out << "    SetConsoleCP(65001);\n";
        out << "#endif\n\n";
        
        // Restaura diretório de trabalho original (modo executar roda de temp/)
        if (!workDir.empty()) {
            // Escapa barras invertidas para string C
            std::string escaped = workDir;
            std::string result;
            for (char c : escaped) {
                if (c == '\\') result += "\\\\";
                else result += c;
            }
            out << "    // Restaura diretorio de trabalho original\n";
            out << "#ifdef _WIN32\n";
            out << "    _chdir(\"" << result << "\");\n";
            out << "#else\n";
            out << "    chdir(\"" << result << "\");\n";
            out << "#endif\n\n";
        }
        
        // Inicializa variáveis
        out << "    // Inicializa variaveis\n";
        out << "    for (int i = 0; i < MEM_SIZE; i++) vars[i] = jp_nulo();\n\n";
        
        if (!bibliotecasUsadas.empty()) {
            out << "    if (!carregar_bibliotecas()) return 1;\n";
        }
        out << "\n";

        // Mapeamento de variáveis (para debug)
        out << "    // Mapeamento de variaveis:\n";
        for (const auto& [nome, idx] : variaveis) {
            out << "    // vars[" << idx << "] = " << nome << "\n";
        }
        out << "\n";

        // Gera o código
        out << "    goto L_0;\n\n";
        
        // Gera labels para funções do usuário
        for (const auto& [funcName, addr] : funcoesUsuario) {
            out << "L_FUNC_" << funcName << ":\n";
            out << "    goto L_" << addr << ";\n\n";
        }
        
        // Gera labels para métodos de classes (de classMethodTable)
        for (const auto& [className, methods] : classMethodTable) {
            for (const auto& [methodName, methodInfo] : methods) {
                if (methodInfo.address >= 0) {
                    out << "L_FUNC_" << className << "_" << methodName << ":\n";
                    out << "    goto L_" << methodInfo.address << ";\n\n";
                }
            }
        }
        
        // Gera labels para funções internas de módulo (legado - mantém compatibilidade)
        for (const auto& [funcName, addr] : funcoesInternas) {
            // Só gera se não for um método de classe (que já foi gerado acima)
            bool isClassMethod = false;
            for (const auto& [className, methods] : classMethodTable) {
                for (const auto& [methodName, methodInfo] : methods) {
                    if (funcName == className + "_" + methodName) {
                        isClassMethod = true;
                        break;
                    }
                }
                if (isClassMethod) break;
            }
            
            if (!isClassMethod) {
                out << "L_FUNC_" << funcName << ":\n";
                out << "    goto L_" << addr << ";\n\n";
            }
        }

        // Gera instruções
        for (size_t i = 0; i < code.size(); i++) {
            JPCGerador::gerarInstrucao(out, i, code[i], variaveis, nativasMap);
        }

        // === TRAMPOLINE DE RETORNO ===
        out << "\nTRAMPOLINE_RET:\n";
        out << "    if (csp == 0) goto FIM;\n";
        out << "    switch (call_stack[--csp]) {\n";
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == OpCode::CALL || code[i].op == OpCode::METHOD_CALL) {
                out << "        case " << (i + 1) << ": goto L_" << (i + 1) << ";\n";
            }
        }
        out << "    }\n";
        out << "    goto FIM;\n";

        // === FIM ===
        out << "\nFIM:\n";
        if (!bibliotecasUsadas.empty()) {
            out << "    descarregar_bibliotecas();\n";
        }
        out << "    return 0;\n";
        out << "}\n";
        
        out.close();
    }

    // Encontra o TCC - busca relativo ao executável E ao diretório atual
    std::string encontrarTcc() {
        fs::path baseDir = importBaseDir;
        
        std::vector<fs::path> caminhos = {
            #ifdef _WIN32
            // Relativos ao diretório do executável
            baseDir / "compilador" / "windows" / "tcc.exe",
            baseDir / "compilador" / "tcc.exe",
            baseDir / "tcc" / "tcc.exe",
            baseDir / "tcc.exe",
            // Relativos ao diretório atual (fallback)
            "compilador\\windows\\tcc.exe",
            "compilador\\tcc.exe",
            "tcc\\tcc.exe",
            "tcc.exe"
            #else
            // Relativos ao diretório do executável
            baseDir / "compilador" / "linux" / "tcc",
            baseDir / "compilador" / "tcc",
            baseDir / "tcc" / "tcc",
            baseDir / "tcc",
            // Relativos ao diretório atual (fallback)
            "./compilador/linux/tcc",
            "./compilador/tcc",
            "./tcc/tcc",
            "tcc"
            #endif
        };
        
        for (const auto& caminho : caminhos) {
            if (fs::exists(caminho)) return caminho.string();
        }
        
        // Tenta no PATH do sistema
        #ifdef _WIN32
        int ret = system("tcc -v >nul 2>&1");
        #else
        int ret = system("tcc -v >/dev/null 2>&1");
        #endif
        
        if (ret == 0) return "tcc";
        return "";
    }

    // Encontra o jpruntime.h - busca relativo ao executável E ao diretório atual
    fs::path encontrarRuntime() {
        fs::path baseDir = importBaseDir;
        
        std::vector<fs::path> caminhos = {
            // Relativos ao diretório do executável
            baseDir / "src" / "jpruntime.h",
            baseDir / "jpc" / "jpruntime.h",
            baseDir / "jpruntime.h",
            baseDir / "compilador" / "jpruntime.h",
            // Relativos ao diretório atual (fallback)
            "src/jpruntime.h",
            "jpc/jpruntime.h",
            "jpruntime.h",
            "compilador/jpruntime.h"
        };
        
        for (const auto& caminho : caminhos) {
            if (fs::exists(caminho)) {
                return fs::absolute(caminho).parent_path();
            }
        }
        return "";
    }

    // Copia bibliotecas - busca relativo ao executável E ao diretório atual
    void copiarBibliotecas(const std::set<std::string>& libs, const fs::path& dirRuntime) {
        fs::path baseDir = importBaseDir;
        
        for (const auto& lib : libs) {
            #ifdef _WIN32
            std::vector<fs::path> caminhos = {
                // Relativos ao diretório do executável
                baseDir / "bibliotecas" / lib / (lib + ".jpd"),
                baseDir / "bibliotecas" / (lib + ".jpd"),
                // Relativos ao diretório atual (fallback)
                fs::path("bibliotecas") / lib / (lib + ".jpd"),
                fs::path("bibliotecas") / (lib + ".jpd")
            };
            #else
            std::vector<fs::path> caminhos = {
                // Relativos ao diretório do executável
                baseDir / "bibliotecas" / lib / ("lib" + lib + ".jpd"),
                baseDir / "bibliotecas" / lib / (lib + ".jpd"),
                baseDir / "bibliotecas" / ("lib" + lib + ".jpd"),
                // Relativos ao diretório atual (fallback)
                fs::path("bibliotecas") / lib / ("lib" + lib + ".jpd"),
                fs::path("bibliotecas") / lib / (lib + ".jpd"),
                fs::path("bibliotecas") / ("lib" + lib + ".jpd")
            };
            #endif
            
            for (const auto& origem : caminhos) {
                if (fs::exists(origem)) {
                    fs::path destino = dirRuntime / origem.filename();
                    try {
                        fs::copy_file(origem, destino, fs::copy_options::overwrite_existing);
                        std::cout << "[JP] Biblioteca copiada: " << destino.filename().string() << std::endl;
                    } catch (...) {}
                    break;
                }
            }
        }
    }

    // Executa o programa: compila em temp/ e roda
    bool executar(const std::vector<Instruction>& code, const std::string& nomeBase, bool modoJanela = false) {
        // Limpa resíduos de execuções anteriores
        if (fs::exists("temp")) {
            try { fs::remove_all("temp"); } catch (...) {}
        }

        std::string tccPath = encontrarTcc();
        if (tccPath.empty()) {
            std::cerr << "[ERRO] TCC nao encontrado!\n";
            return false;
        }

        fs::path runtimeDir = encontrarRuntime();
        if (runtimeDir.empty()) {
            std::cerr << "[ERRO] jpruntime.h nao encontrado!\n";
            return false;
        }

        // Cria diretório temporário
        fs::path dirTemp = "temp";
        fs::path dirTempRuntime = dirTemp / "runtime";
        fs::create_directories(dirTemp);
        fs::create_directories(dirTempRuntime);

        fs::path cOutput = dirTemp / (nomeBase + ".c");
        #ifdef _WIN32
        fs::path exeOutput = dirTemp / (nomeBase + ".exe");
        #else
        fs::path exeOutput = dirTemp / nomeBase;
        #endif

        // Gera código C (passa diretório de trabalho original para chdir)
        std::string workDir = fs::current_path().string();
        gerarC(code, cOutput.string(), modoJanela, workDir);

        // Copia bibliotecas nativas para temp/runtime/
        std::set<std::string> bibliotecasUsadas;
        for (const auto& [key, info] : moduleTable) {
            if (isNativeLibrary(info.name, info.path)) {
                bibliotecasUsadas.insert(info.name);
            }
        }
        if (!bibliotecasUsadas.empty()) {
            copiarBibliotecas(bibliotecasUsadas, dirTempRuntime);
        }

        // Compila com TCC
        std::stringstream cmd;

        #ifdef _WIN32
        cmd << "\"";
        #endif

        cmd << "\"" << fs::path(tccPath).make_preferred().string() << "\" ";

        fs::path tccDir = fs::path(tccPath).parent_path();
        if (!tccDir.empty()) {
            cmd << "-B\"" << tccDir.make_preferred().string() << "\" ";
        }

        cmd << "-o \"" << exeOutput.make_preferred().string() << "\" ";
        cmd << "\"" << cOutput.make_preferred().string() << "\" ";
        cmd << "-I\"" << runtimeDir.make_preferred().string() << "\" ";

        #ifdef _WIN32
        if (modoJanela) cmd << "-mwindows ";
        cmd << "-lkernel32 ";
        cmd << "\"";
        #else
        cmd << "-ldl ";
        #endif

        int ret = system(cmd.str().c_str());

        if (ret != 0) {
            std::cerr << cOutput.string() << ": erro na compilacao\n";
            std::cerr << "[JP] Erro na compilacao.\n";
            return false;
        }

        // Executa
        std::string exeCmd;
        #ifdef _WIN32
        exeCmd = "\"" + exeOutput.make_preferred().string() + "\"";
        #else
        exeCmd = "./" + exeOutput.string();
        #endif

        ret = system(exeCmd.c_str());

        // Limpa temp/ após execução
        try { fs::remove_all("temp"); } catch (...) {}

        return ret == 0;
    }

    // Compila o programa (build para output/)
    bool compilar(const std::vector<Instruction>& code, const std::string& nomeBase, bool modoJanela = false) {
        std::string tccPath = encontrarTcc();
        if (tccPath.empty()) {
            std::cerr << "[ERRO] TCC nao encontrado!\n";
            return false;
        }
        
        fs::path runtimeDir = encontrarRuntime();
        if (runtimeDir.empty()) {
            std::cerr << "[ERRO] jpruntime.h nao encontrado!\n";
            return false;
        }
        
        // Cria diretórios
        fs::path dirProjeto = fs::absolute("output") / nomeBase;
        fs::path dirRuntime = dirProjeto / "runtime";
        fs::create_directories(dirProjeto);
        fs::create_directories(dirRuntime);
        
        fs::path cOutput = dirProjeto / (nomeBase + ".c");
        #ifdef _WIN32
        fs::path exeOutput = dirProjeto / (nomeBase + ".exe");
        #else
        fs::path exeOutput = dirProjeto / nomeBase;
        #endif
        
        // Gera código C
        gerarC(code, cOutput.string(), modoJanela);
        
        // Compila com TCC
        std::stringstream cmd;
        
        #ifdef _WIN32
        cmd << "\"";
        #endif
        
        cmd << "\"" << fs::path(tccPath).make_preferred().string() << "\" ";
        
        fs::path tccDir = fs::path(tccPath).parent_path();
        if (!tccDir.empty()) {
            cmd << "-B\"" << tccDir.make_preferred().string() << "\" ";
        }
        
        cmd << "-o \"" << exeOutput.make_preferred().string() << "\" ";
        cmd << "\"" << cOutput.make_preferred().string() << "\" ";
        cmd << "-I\"" << runtimeDir.make_preferred().string() << "\" ";
        
        #ifdef _WIN32
        if (modoJanela) cmd << "-mwindows ";
        cmd << "-lkernel32 ";
        cmd << "\"";
        #else
        cmd << "-ldl ";
        #endif
        
        int ret = system(cmd.str().c_str());
        
        if (ret != 0) {
            std::cerr << cOutput.string() << ": erro na compilacao\n";
            std::cerr << "[JP] Erro na compilacao.\n";
            return false;
        }
        
        // Copia bibliotecas usando moduleTable (apenas bibliotecas nativas)
        std::set<std::string> bibliotecasUsadas;
        for (const auto& [key, info] : moduleTable) {
            if (isNativeLibrary(info.name, info.path)) {
                bibliotecasUsadas.insert(info.name);
            }
        }
        
        if (!bibliotecasUsadas.empty()) {
            copiarBibliotecas(bibliotecasUsadas, dirRuntime);
        }
        
        std::cout << "[JP] Sucesso: " << exeOutput << std::endl;
        return true;
    }
};

#endif // JPC_COMPILADOR_HPP