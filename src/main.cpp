// main.cpp
// Entry point de TESTE do compilador JPLang — codegen unificado (só saida por enquanto)

#include "src/frontend/lexer.hpp"
#include "src/frontend/parser.hpp"
#include "src/codegen_comum/codegen.hpp"

// Linker ainda é por plataforma
#ifdef _WIN32
    #include "src/backend_windows/linker_windows.hpp"
    #define JP_OBJ_EXT ".obj"
    #define JP_EXE_EXT ".exe"
    #define JP_PLATFORM "COFF/PE x64 — Windows (codegen unificado)"
#else
    #include "src/backend_linux/linker_linux.hpp"
    #define JP_OBJ_EXT ".o"
    #define JP_EXE_EXT ""
    #define JP_PLATFORM "ELF x86-64 — Linux (codegen unificado)"
#endif

// Gerenciador de bibliotecas
#include "src/jp_install.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// DIRETÓRIO DO EXECUTÁVEL
// ============================================================================

static std::string g_exe_dir;

static std::string get_exe_dir(const char* argv0) {
    // Tenta via /proc/self/exe (Linux) ou argv[0] absoluto (Windows)
    #ifdef _WIN32
    {
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            fs::path p(buf);
            return p.parent_path().string();
        }
    }
    #else
    {
        // Linux: /proc/self/exe
        char buf[4096];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            fs::path p(buf);
            return p.parent_path().string();
        }
    }
    #endif

    // Fallback: usa argv[0]
    fs::path p(argv0);
    if (p.has_parent_path()) {
        return fs::absolute(p.parent_path()).string();
    }

    // Último recurso: diretório atual
    return fs::current_path().string();
}

// ============================================================================
// LEITURA DE ARQUIVO
// ============================================================================

static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Erro: Não foi possível abrir '" << path << "'" << std::endl;
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ============================================================================
// COMPILAÇÃO: fonte → .obj/.o
// ============================================================================

static bool compile_to_obj(const std::string& source,
                           const std::string& obj_path,
                           const std::string& base_dir,
                           const std::string& exe_dir,
                           std::vector<std::string>& extra_objs,
                           std::vector<std::string>& extra_libs,
                           std::vector<std::string>& extra_lib_paths,
                           std::vector<std::string>& extra_dlls) {
    jplang::Lexer lexer(source, base_dir);
    jplang::Parser parser(lexer, base_dir);

    auto program = parser.parse();
    if (!program.has_value()) {
        std::cerr << "Compilação abortada devido a erros de sintaxe." << std::endl;
        return false;
    }

    jplang::Codegen codegen;
    codegen.set_exe_dir(exe_dir);
    if (!codegen.compile(program.value(), obj_path, base_dir, parser.lang_config())) {
        std::cerr << "Erro na geração de código." << std::endl;
        return false;
    }

    extra_objs = codegen.extra_obj_paths();
    extra_libs = codegen.extra_libs();
    extra_lib_paths = codegen.extra_lib_paths();
    extra_dlls = codegen.extra_dll_paths();

    return true;
}

// ============================================================================
// MODO RUN: compila, linka, executa, apaga
// ============================================================================

static int mode_run(const std::string& input_path) {
    std::string source = read_file(input_path);
    if (source.empty()) return 1;

    std::string base_dir = fs::path(input_path).parent_path().string();

    fs::path temp_dir = "temp";
    fs::create_directories(temp_dir);

    fs::path stem = fs::path(input_path).stem();
    fs::path obj_path = temp_dir / (stem.string() + JP_OBJ_EXT);
    fs::path exe_path = temp_dir / (stem.string() + JP_EXE_EXT);

    std::vector<std::string> extra_objs;
    std::vector<std::string> extra_libs;
    std::vector<std::string> extra_lib_paths;
    std::vector<std::string> extra_dlls;
    if (!compile_to_obj(source, obj_path.string(), base_dir, g_exe_dir,
                        extra_objs, extra_libs, extra_lib_paths, extra_dlls)) {
        fs::remove_all(temp_dir);
        return 1;
    }

    if (!jplang::link_with_ld(obj_path.string(), exe_path.string(),
                               extra_objs, extra_libs, extra_lib_paths, extra_dlls)) {
        fs::remove_all(temp_dir);
        return 1;
    }

    #ifdef _WIN32
    int ret = std::system(exe_path.string().c_str());
    #else
    std::string run_cmd = "./" + exe_path.string();
    int ret = std::system(run_cmd.c_str());
    #endif

    fs::remove_all(temp_dir);
    return ret;
}

// ============================================================================
// MODO BUILD: compila e linka em output/
// ============================================================================

static int mode_build(const std::string& input_path, bool windowed = false) {
    std::string source = read_file(input_path);
    if (source.empty()) return 1;

    std::string base_dir = fs::path(input_path).parent_path().string();

    fs::path stem = fs::path(input_path).stem();
    fs::path out_dir = fs::path("output") / stem;
    fs::create_directories(out_dir);

    fs::path obj_path = out_dir / (stem.string() + JP_OBJ_EXT);
    fs::path exe_path = out_dir / (stem.string() + JP_EXE_EXT);

    std::vector<std::string> extra_objs;
    std::vector<std::string> extra_libs;
    std::vector<std::string> extra_lib_paths;
    std::vector<std::string> extra_dlls;
    if (!compile_to_obj(source, obj_path.string(), base_dir, g_exe_dir,
                        extra_objs, extra_libs, extra_lib_paths, extra_dlls)) {
        return 1;
    }

    std::cout << "Objeto gerado: " << obj_path.string() << std::endl;

    if (!jplang::link_with_ld(obj_path.string(), exe_path.string(),
                               extra_objs, extra_libs, extra_lib_paths, extra_dlls, windowed)) {
        return 1;
    }

    std::string modo = windowed ? " (GUI, sem console)" : "";
    std::cout << "Compilado: " << input_path << " -> " << exe_path.string() << modo << std::endl;
    return 0;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    g_exe_dir = get_exe_dir(argv[0]);

    if (argc < 2) {
        std::cerr << "JPLang Compiler v1.0 (" << JP_PLATFORM << ")" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Uso:" << std::endl;
        std::cerr << "  jp <arquivo.jp>             Compila, linka, executa e apaga" << std::endl;
        std::cerr << "  jp build <arquivo.jp>       Compila e linka em output/" << std::endl;
        std::cerr << "  jp build <arquivo.jp> -w    Compila como aplicativo GUI (sem console)" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Gerenciador de bibliotecas:" << std::endl;
        std::cerr << "  jp instalar <nome>          Instala biblioteca do repositorio" << std::endl;
        std::cerr << "  jp desinstalar <nome>       Remove biblioteca instalada" << std::endl;
        std::cerr << "  jp listar                   Lista bibliotecas instaladas" << std::endl;
        std::cerr << "  jp listar --remoto          Lista bibliotecas disponiveis" << std::endl;
        return 1;
    }

    std::string first_arg = argv[1];

    if (first_arg == "build") {
        if (argc < 3) {
            std::cerr << "Erro: Esperado arquivo após 'build'" << std::endl;
            return 1;
        }
        // Verifica flag -w (windowed, sem console)
        bool windowed = false;
        std::string build_file = argv[2];
        for (int i = 3; i < argc; i++) {
            std::string flag = argv[i];
            if (flag == "-w" || flag == "--windowed") {
                windowed = true;
            }
        }
        return mode_build(build_file, windowed);
    }

    if (first_arg == "instalar") {
        if (argc < 3) {
            std::cerr << "Erro: Esperado nome da biblioteca após 'instalar'" << std::endl;
            std::cerr << "Exemplo: jp instalar texto" << std::endl;
            return 1;
        }
        return jplang::install_lib(argv[2], g_exe_dir);
    }

    if (first_arg == "desinstalar") {
        if (argc < 3) {
            std::cerr << "Erro: Esperado nome da biblioteca após 'desinstalar'" << std::endl;
            return 1;
        }
        return jplang::uninstall_lib(argv[2], g_exe_dir);
    }

    if (first_arg == "listar") {
        bool show_remote = (argc >= 3 && std::string(argv[2]) == "--remoto");
        return jplang::list_libs(show_remote, g_exe_dir);
    }

    return mode_run(first_arg);
}