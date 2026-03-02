// linker_windows.hpp
// Linkagem Windows x64 — resolve caminhos do ld.exe embutido, monta comando COFF/PE, suporta DLLs .jpd

#ifndef JPLANG_LINKER_WINDOWS_HPP
#define JPLANG_LINKER_WINDOWS_HPP

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

// ============================================================================
// DECLARAÇÃO DIRETA WIN32 (evita incluir windows.h)
// ============================================================================

#ifdef _WIN32
extern "C" {
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
}
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#endif

namespace jplang {

// ============================================================================
// HELPERS
// ============================================================================

// Retorna o diretório do executável do compilador
static std::string get_exe_dir() {
    #ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path().string();
    #else
    return ".";
    #endif
}

// Normaliza barras para o padrão do sistema
static std::string normalize_path(const std::string& path) {
    std::string result = path;
    #ifdef _WIN32
    for (auto& c : result) {
        if (c == '/') c = '\\';
    }
    #endif
    return result;
}

// ============================================================================
// CÓPIA DE DLLs .jpd PARA O DIRETÓRIO DO EXECUTÁVEL
// ============================================================================

static void copy_dlls_to_exe_dir(const std::string& exe_path,
                                  const std::vector<std::string>& dll_paths) {
    if (dll_paths.empty()) return;

    fs::path exe_dir = fs::path(exe_path).parent_path();
    if (exe_dir.empty()) exe_dir = ".";

    for (auto& dll : dll_paths) {
        fs::path src(dll);
        if (!fs::exists(src)) {
            std::cerr << "Aviso: DLL não encontrada para copiar: " << dll << std::endl;
            continue;
        }
        fs::path dst = exe_dir / src.filename();
        try {
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            std::cerr << "Aviso: Falha ao copiar DLL '" << dll << "': " << e.what() << std::endl;
        }
    }
}

// ============================================================================
// LINKAGEM: .obj → .exe via ld embutido (src/backend_windows/ld_linker/)
// ============================================================================

static bool link_with_ld(const std::string& obj_path, const std::string& exe_path,
                          const std::vector<std::string>& extra_objs = {},
                          const std::vector<std::string>& extra_libs = {},
                          const std::vector<std::string>& extra_lib_paths = {},
                          const std::vector<std::string>& extra_dlls = {}) {

    // Procurar ld.exe relativo ao diretório atual
    std::string ld_dir = "src\\backend_windows\\ld_linker";
    std::string ld_exe = ld_dir + "\\ld.exe";

    if (!fs::exists(ld_exe)) {
        // Tentar relativo ao executável do compilador
        #ifdef _WIN32
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string exe_dir = fs::path(buf).parent_path().string();
        ld_dir = normalize_path(exe_dir + "\\src\\backend_windows\\ld_linker");
        ld_exe = ld_dir + "\\ld.exe";
        #endif
    }

    if (!fs::exists(ld_exe)) {
        std::cerr << "Erro: ld.exe não encontrado em 'src/backend_windows/ld_linker/'" << std::endl;
        return false;
    }

    std::string norm_obj = normalize_path(obj_path);
    std::string norm_exe = normalize_path(exe_path);

    // Montar comando replicando a ordem exata do g++ -static
    std::string cmd = "\"" + ld_exe + "\"";
    cmd += " -m i386pep -Bstatic";
    cmd += " -o \"" + norm_exe + "\"";

    // CRT startup objects
    cmd += " \"" + ld_dir + "\\crt2.o\"";
    cmd += " \"" + ld_dir + "\\crtbegin.o\"";

    // Objeto principal do programa
    cmd += " \"" + norm_obj + "\"";

    // Objetos extras (bibliotecas .obj compiladas, ex: aleatorio.obj)
    for (auto& obj : extra_objs) {
        if (fs::exists(obj)) {
            cmd += " \"" + normalize_path(obj) + "\"";
        }
    }

    cmd += " -L \"" + ld_dir + "\"";

    // Libs na ordem exata do g++ (primeiro bloco)
    cmd += " -lstdc++ -lmingw32 -lgcc -lgcc_eh -lmingwex -lmsvcrt";
    cmd += " -lkernel32 -lpthread -ladvapi32 -lshell32 -luser32 -lkernel32";

    // Caminhos extras de busca de libs (-L)
    for (auto& lpath : extra_lib_paths) {
        cmd += " -L \"" + normalize_path(lpath) + "\"";
    }

    // Libs extras declaradas nos JSONs das bibliotecas nativas
    for (auto& lib : extra_libs) {
        cmd += " -l" + lib;
    }

    // DLLs .jpd — o ld do MinGW aceita DLLs diretamente como input
    for (auto& dll : extra_dlls) {
        if (fs::exists(dll)) {
            cmd += " \"" + normalize_path(dll) + "\"";
        } else {
            std::cerr << "Aviso: DLL não encontrada: " << dll << std::endl;
        }
    }

    // Segundo bloco (repetido para resolver dependências circulares)
    cmd += " -lmingw32 -lgcc -lgcc_eh -lmingwex -lmsvcrt -lkernel32";

    // CRT finalization objects
    cmd += " \"" + ld_dir + "\\default-manifest.o\"";
    cmd += " \"" + ld_dir + "\\crtend.o\"";

    cmd += " -e main";

    int ret = std::system(("\"" + cmd + "\"").c_str());

    if (ret != 0) {
        std::cerr << "Erro: Linkagem falhou (código " << ret << ")" << std::endl;
        return false;
    }

    // Copiar DLLs .jpd para o diretório do executável
    copy_dlls_to_exe_dir(exe_path, extra_dlls);

    return true;
}

} // namespace jplang

#endif // JPLANG_LINKER_WINDOWS_HPP
//comentario