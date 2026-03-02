// linker_linux.hpp
// Linkagem Linux x86-64 — resolve caminhos do ld embutido, suporta .o estático e .jpd dinâmico

#ifndef JPLANG_LINKER_LINUX_HPP
#define JPLANG_LINKER_LINUX_HPP

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <climits>
#include <unistd.h>

namespace fs = std::filesystem;

namespace jplang {

// ============================================================================
// HELPERS
// ============================================================================

// Retorna o diretório do executável do compilador
static std::string get_exe_dir() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return fs::path(buf).parent_path().string();
    }
    return ".";
}

// ============================================================================
// LINKAGEM: .o → executável via ld embutido (src/backend_linux/ld_linker/)
//
// Suporta:
//   - Bibliotecas estáticas (.o) via extra_objs
//   - Bibliotecas dinâmicas (.jpd) via extra_dlls (passadas direto ao ld)
//   - Diretórios de busca em runtime via extra_lib_paths (-rpath)
//   - Libs do sistema via extra_libs (-lX11, -lpthread, etc.)
//
// Quando há .jpd, a linkagem é dinâmica (sem -static).
// Quando não há .jpd, a linkagem é estática (-static) como antes.
// ============================================================================

static bool link_with_ld(const std::string& obj_path, const std::string& exe_path,
                          const std::vector<std::string>& extra_objs = {},
                          const std::vector<std::string>& extra_libs = {},
                          const std::vector<std::string>& extra_lib_paths = {},
                          const std::vector<std::string>& extra_dlls = {}) {

    // Procurar ld relativo ao diretório atual
    std::string ld_dir = "src/backend_linux/ld_linker";
    std::string ld_exe = ld_dir + "/ld";

    if (!fs::exists(ld_exe)) {
        // Tentar relativo ao executável do compilador
        std::string exe_dir = get_exe_dir();
        ld_dir = exe_dir + "/src/backend_linux/ld_linker";
        ld_exe = ld_dir + "/ld";
    }

    if (!fs::exists(ld_exe)) {
        std::cerr << "Erro: ld não encontrado em 'src/backend_linux/ld_linker/'" << std::endl;
        return false;
    }

    bool has_dynamic = !extra_dlls.empty();

    // LD_LIBRARY_PATH para as dependências dinâmicas do próprio ld
    std::string cmd = "LD_LIBRARY_PATH=\"" + ld_dir + "\" ";

    // Comando do linker
    cmd += ld_exe;
    cmd += " -m elf_x86_64 --hash-style=gnu";

    if (has_dynamic) {
        // ================================================================
        // LINKAGEM DINÂMICA
        // Usa CRT dinâmicos (_dyn.o) e libs .so do ld_linker/
        // ================================================================
        cmd += " -z relro";
        cmd += " --dynamic-linker /lib64/ld-linux-x86-64.so.2";
        cmd += " -o " + exe_path;

        // CRT startup dinâmico
        std::string crt1 = ld_dir + "/crt1_dyn.o";
        std::string crti = ld_dir + "/crti_dyn.o";
        cmd += " " + (fs::exists(crt1) ? crt1 : ld_dir + "/crt1.o");
        cmd += " " + (fs::exists(crti) ? crti : ld_dir + "/crti.o");
        std::string crtbeginS = ld_dir + "/crtbeginS.o";
        if (fs::exists(crtbeginS)) {
            cmd += " " + crtbeginS;
        }

        // Search path para libs — ld_linker/ primeiro
        cmd += " -L" + ld_dir;

        // Objeto principal
        cmd += " " + obj_path;

        // Objetos extras (bibliotecas .o estáticas)
        for (auto& obj : extra_objs) {
            if (fs::exists(obj)) {
                cmd += " " + obj;
            }
        }

        // Bibliotecas dinâmicas .jpd: passadas pelo caminho relativo
        // O ld grava o caminho no NEEDED, permitindo que o executável
        // encontre a dll em bibliotecas/<nome>/lib<nome>.jpd relativo ao CWD
        for (auto& dll : extra_dlls) {
            if (fs::exists(dll)) {
                cmd += " " + dll;
            } else {
                std::cerr << "Aviso: Biblioteca dinâmica não encontrada: " << dll << std::endl;
            }
        }

        // libstdc++ estático (evita dependência de .so em runtime)
        cmd += " " + ld_dir + "/libstdc++.a";
        cmd += " -lm";

        // Libs extras declaradas nos JSONs
        for (auto& lib : extra_libs) {
            cmd += " -l" + lib;
        }

        cmd += " --start-group -lgcc -lgcc_s -lc --end-group";

        // CRT finalization dinâmico
        std::string crtend = ld_dir + "/crtend_dyn.o";
        if (fs::exists(crtend)) {
            cmd += " " + crtend;
        } else {
            std::string crtendS = ld_dir + "/crtendS.o";
            cmd += " " + (fs::exists(crtendS) ? crtendS : ld_dir + "/crtend.o");
        }
        std::string crtn = ld_dir + "/crtn_dyn.o";
        cmd += " " + (fs::exists(crtn) ? crtn : ld_dir + "/crtn.o");

    } else {
        // ================================================================
        // LINKAGEM ESTÁTICA (como antes)
        // ================================================================
        cmd += " -static -z relro";
        cmd += " -o " + exe_path;

        // CRT startup estático
        cmd += " " + ld_dir + "/crt1.o";
        cmd += " " + ld_dir + "/crti.o";
        cmd += " " + ld_dir + "/crtbeginT.o";

        // Search path
        cmd += " -L" + ld_dir;

        // Objeto principal
        cmd += " " + obj_path;

        // Objetos extras
        for (auto& obj : extra_objs) {
            if (fs::exists(obj)) {
                cmd += " " + obj;
            }
        }

        // Libs estáticas
        cmd += " -lstdc++ -lm";

        // Libs extras
        for (auto& lib : extra_libs) {
            cmd += " -l" + lib;
        }

        cmd += " --start-group -lgcc -lgcc_eh -lc --end-group";

        // CRT finalization estático
        cmd += " " + ld_dir + "/crtend.o";
        cmd += " " + ld_dir + "/crtn.o";
    }

    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        std::cerr << "Erro: Linkagem falhou (código " << ret << ")" << std::endl;
        return false;
    }

    return true;
}

} // namespace jplang

#endif // JPLANG_LINKER_LINUX_HPP