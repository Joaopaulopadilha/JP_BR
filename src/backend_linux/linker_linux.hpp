// linker_linux.hpp
// Linkagem Linux x86-64 — resolve caminhos do ld embutido, suporta .o estatico e .jpd dinamico

#ifndef JPLANG_LINKER_LINUX_HPP
#define JPLANG_LINKER_LINUX_HPP

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <climits>
#include <unistd.h>
#include <array>

namespace fs = std::filesystem;

namespace jplang {

// ============================================================================
// DETECCAO DE CRT OBJECTS DO SISTEMA
//
// Os CRT objects (crt1.o, crti.o, crtn.o) precisam ser da mesma glibc
// instalada no sistema. Embutir CRT de outra distro causa:
//   "Inconsistency detected by ld.so: _dl_call_libc_early_init"
//
// Estrategia:
//   1. Pergunta ao cc/gcc com -print-file-name (solucao padrao da industria)
//   2. Fallback para caminhos conhecidos por distro
//   3. Ultimo recurso: CRT embutidos no ld_linker/
// ============================================================================

// Executa um comando e retorna a saida (trim)
static std::string ld_exec_cmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

// Tenta encontrar o diretorio dos CRT objects do sistema (glibc)
static std::string find_system_crt_dir() {
    // Metodo 1: perguntar ao cc (link simbolico padrao)
    std::string path = ld_exec_cmd("cc -print-file-name=crt1.o 2>/dev/null");
    if (!path.empty() && path != "crt1.o" && fs::exists(path)) {
        return fs::path(path).parent_path().string();
    }

    // Metodo 2: perguntar ao gcc
    path = ld_exec_cmd("gcc -print-file-name=crt1.o 2>/dev/null");
    if (!path.empty() && path != "crt1.o" && fs::exists(path)) {
        return fs::path(path).parent_path().string();
    }

    // Metodo 3: caminhos conhecidos por distro
    std::vector<std::string> known_paths = {
        "/usr/lib64",                       // Fedora, RHEL, openSUSE
        "/usr/lib/x86_64-linux-gnu",        // Ubuntu, Debian
        "/usr/lib",                         // Arch, Alpine
    };
    for (auto& p : known_paths) {
        if (fs::exists(p + "/crt1.o")) {
            return p;
        }
    }

    return "";
}

// Tenta encontrar o diretorio do crtbeginT.o / crtbeginS.o (vem do gcc, nao da glibc)
static std::string find_gcc_crt_dir() {
    std::string path = ld_exec_cmd("cc -print-file-name=crtbeginT.o 2>/dev/null");
    if (!path.empty() && path != "crtbeginT.o" && fs::exists(path)) {
        return fs::path(path).parent_path().string();
    }

    path = ld_exec_cmd("gcc -print-file-name=crtbeginT.o 2>/dev/null");
    if (!path.empty() && path != "crtbeginT.o" && fs::exists(path)) {
        return fs::path(path).parent_path().string();
    }

    return "";
}

// Helper: retorna o CRT do sistema se existir, senao o embutido
static std::string resolve_crt(const std::string& name,
                                const std::string& system_dir,
                                const std::string& gcc_dir,
                                const std::string& ld_dir) {
    // CRTs da glibc: crt1.o, crti.o, crtn.o
    if (name == "crt1.o" || name == "crti.o" || name == "crtn.o") {
        if (!system_dir.empty() && fs::exists(system_dir + "/" + name)) {
            return system_dir + "/" + name;
        }
    }

    // CRTs do gcc: crtbeginT.o, crtbeginS.o, crtend.o, crtendS.o
    if (name.find("crtbegin") != std::string::npos || name.find("crtend") != std::string::npos) {
        if (!gcc_dir.empty() && fs::exists(gcc_dir + "/" + name)) {
            return gcc_dir + "/" + name;
        }
    }

    // Fallback: embutido
    return ld_dir + "/" + name;
}

// ============================================================================
// LINKAGEM: .o -> executavel via ld embutido (src/backend_linux/ld_linker/)
// ============================================================================

static bool link_with_ld(const std::string& obj_path, const std::string& exe_path,
                          const std::vector<std::string>& extra_objs = {},
                          const std::vector<std::string>& extra_libs = {},
                          const std::vector<std::string>& extra_lib_paths = {},
                          const std::vector<std::string>& extra_dlls = {}) {

    // Procurar ld relativo ao diretorio atual
    std::string ld_dir = "src/backend_linux/ld_linker";
    std::string ld_exe = ld_dir + "/ld";

    if (!fs::exists(ld_exe)) {
        // Tentar relativo ao executavel do compilador
        char exe_buf[PATH_MAX];
        ssize_t exe_len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (exe_len > 0) {
            exe_buf[exe_len] = '\0';
            std::string exe_dir = fs::path(exe_buf).parent_path().string();
            ld_dir = exe_dir + "/src/backend_linux/ld_linker";
            ld_exe = ld_dir + "/ld";
        }
    }

    if (!fs::exists(ld_exe)) {
        std::cerr << "Erro: ld nao encontrado em 'src/backend_linux/ld_linker/'" << std::endl;
        return false;
    }

    // Detectar CRT objects do sistema
    std::string system_crt_dir = find_system_crt_dir();
    std::string gcc_crt_dir = find_gcc_crt_dir();

    bool has_dynamic = !extra_dlls.empty();

    // LD_LIBRARY_PATH para as dependencias dinamicas do proprio ld
    std::string cmd = "LD_LIBRARY_PATH=\"" + ld_dir + "\" ";

    // Comando do linker
    cmd += "\"" + ld_exe + "\"";
    cmd += " -m elf_x86_64 --hash-style=gnu";

    if (has_dynamic) {
        // ================================================================
        // LINKAGEM DINAMICA
        // ================================================================
        cmd += " -z relro";
        cmd += " --dynamic-linker /lib64/ld-linux-x86-64.so.2";
        cmd += " -o \"" + exe_path + "\"";

        // CRT startup dinamico (do sistema)
        cmd += " \"" + resolve_crt("crt1.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crti.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";

        std::string crtbeginS = resolve_crt("crtbeginS.o", system_crt_dir, gcc_crt_dir, ld_dir);
        if (fs::exists(crtbeginS)) {
            cmd += " \"" + crtbeginS + "\"";
        }

        // Search path para libs — ld_linker/ primeiro
        cmd += " -L\"" + ld_dir + "\"";

        // Diretorio do sistema como search path para libc, libm, etc.
        if (!system_crt_dir.empty()) {
            cmd += " -L\"" + system_crt_dir + "\"";
        }

        // Diretorios extras de busca em runtime (-rpath)
        for (auto& lpath : extra_lib_paths) {
            cmd += " -rpath \"" + lpath + "\"";
            cmd += " -L\"" + lpath + "\"";
        }

        // Objeto principal
        cmd += " \"" + obj_path + "\"";

        // Objetos extras (bibliotecas .o estaticas)
        for (auto& obj : extra_objs) {
            if (fs::exists(obj)) {
                cmd += " \"" + obj + "\"";
            }
        }

        // Bibliotecas dinamicas .jpd
        for (auto& dll : extra_dlls) {
            if (fs::exists(dll)) {
                cmd += " \"" + dll + "\"";
            } else {
                std::cerr << "Aviso: Biblioteca dinamica nao encontrada: " << dll << std::endl;
            }
        }

        // libstdc++ estatico (evita dependencia de .so em runtime)
        cmd += " \"" + ld_dir + "/libstdc++.a\"";
        cmd += " -lm";

        // Libs extras declaradas nos JSONs
        for (auto& lib : extra_libs) {
            cmd += " -l" + lib;
        }

        cmd += " --start-group -lgcc -lgcc_s -lc --end-group";

        // CRT finalization dinamico
        std::string crtendS = resolve_crt("crtendS.o", system_crt_dir, gcc_crt_dir, ld_dir);
        if (fs::exists(crtendS)) {
            cmd += " \"" + crtendS + "\"";
        } else {
            cmd += " \"" + resolve_crt("crtend.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";
        }
        cmd += " \"" + resolve_crt("crtn.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";

    } else {
        // ================================================================
        // LINKAGEM ESTATICA
        // ================================================================
        cmd += " -static -z relro";
        cmd += " -o \"" + exe_path + "\"";

        // CRT startup estatico (do sistema)
        cmd += " \"" + resolve_crt("crt1.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crti.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crtbeginT.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";

        // Search path
        cmd += " -L\"" + ld_dir + "\"";

        // Diretorio do sistema como search path
        if (!system_crt_dir.empty()) {
            cmd += " -L\"" + system_crt_dir + "\"";
        }
        if (!gcc_crt_dir.empty() && gcc_crt_dir != system_crt_dir) {
            cmd += " -L\"" + gcc_crt_dir + "\"";
        }

        // Diretorios extras de busca de libs
        for (auto& lpath : extra_lib_paths) {
            cmd += " -L\"" + lpath + "\"";
        }

        // Objeto principal
        cmd += " \"" + obj_path + "\"";

        // Objetos extras
        for (auto& obj : extra_objs) {
            if (fs::exists(obj)) {
                cmd += " \"" + obj + "\"";
            }
        }

        // Libs estaticas
        cmd += " -lstdc++ -lm";

        // Libs extras
        for (auto& lib : extra_libs) {
            cmd += " -l" + lib;
        }

        cmd += " --start-group -lgcc -lgcc_eh -lc --end-group";

        // CRT finalization estatico (do sistema)
        cmd += " \"" + resolve_crt("crtend.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crtn.o", system_crt_dir, gcc_crt_dir, ld_dir) + "\"";
    }

    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        std::cerr << "Erro: Linkagem falhou (codigo " << ret << ")" << std::endl;
        return false;
    }

    return true;
}

} // namespace jplang

#endif // JPLANG_LINKER_LINUX_HPP