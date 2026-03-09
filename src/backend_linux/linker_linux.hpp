// linker_linux.hpp
// Linkagem Linux x86-64 — usa ld embutido com fallback para ld do sistema

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
// UTILITARIOS
// ============================================================================

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

static bool test_executable(const std::string& exe, const std::string& env_prefix = "") {
    std::string cmd = env_prefix + "\"" + exe + "\" --version > /dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

// ============================================================================
// DETECCAO DE DIRETORIOS DO SISTEMA
// ============================================================================

// CRT da glibc: crt1.o, crti.o, crtn.o
static std::string find_system_crt_dir() {
    std::string path = ld_exec_cmd("cc -print-file-name=crt1.o 2>/dev/null");
    if (!path.empty() && path != "crt1.o" && fs::exists(path))
        return fs::path(path).parent_path().string();

    path = ld_exec_cmd("gcc -print-file-name=crt1.o 2>/dev/null");
    if (!path.empty() && path != "crt1.o" && fs::exists(path))
        return fs::path(path).parent_path().string();

    for (auto& p : {"/usr/lib64", "/usr/lib/x86_64-linux-gnu", "/usr/lib"}) {
        if (fs::exists(std::string(p) + "/crt1.o")) return p;
    }
    return "";
}

// CRT do gcc: crtbeginT.o, crtbeginS.o, crtend.o, crtendS.o
static std::string find_gcc_crt_dir() {
    std::string path = ld_exec_cmd("cc -print-file-name=crtbeginT.o 2>/dev/null");
    if (!path.empty() && path != "crtbeginT.o" && fs::exists(path))
        return fs::path(path).parent_path().string();

    path = ld_exec_cmd("gcc -print-file-name=crtbeginT.o 2>/dev/null");
    if (!path.empty() && path != "crtbeginT.o" && fs::exists(path))
        return fs::path(path).parent_path().string();

    return "";
}

// libgcc.a, libgcc_eh.a
static std::string find_gcc_lib_dir() {
    std::string path = ld_exec_cmd("cc -print-file-name=libgcc.a 2>/dev/null");
    if (!path.empty() && path != "libgcc.a" && fs::exists(path))
        return fs::path(path).parent_path().string();

    path = ld_exec_cmd("gcc -print-file-name=libgcc.a 2>/dev/null");
    if (!path.empty() && path != "libgcc.a" && fs::exists(path))
        return fs::path(path).parent_path().string();

    return "";
}

// Verifica se o sistema tem glibc-static (libc.a)
static bool has_static_libc(const std::string& system_crt_dir) {
    if (!system_crt_dir.empty() && fs::exists(system_crt_dir + "/libc.a"))
        return true;

    // Checar tambem via cc
    std::string path = ld_exec_cmd("cc -print-file-name=libc.a 2>/dev/null");
    if (!path.empty() && path != "libc.a" && fs::exists(path))
        return true;

    return false;
}

// ============================================================================
// DETECCAO DO LINKER
// ============================================================================

struct LinkerInfo {
    std::string ld_exe;
    std::string ld_dir;         // Diretorio do ld embutido (pra libstdc++.a)
    bool using_system_ld;
};

static LinkerInfo find_linker() {
    LinkerInfo info;
    info.using_system_ld = false;

    info.ld_dir = "src/backend_linux/ld_linker";
    info.ld_exe = info.ld_dir + "/ld";

    if (!fs::exists(info.ld_exe)) {
        char exe_buf[PATH_MAX];
        ssize_t exe_len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (exe_len > 0) {
            exe_buf[exe_len] = '\0';
            std::string exe_dir = fs::path(exe_buf).parent_path().string();
            info.ld_dir = exe_dir + "/src/backend_linux/ld_linker";
            info.ld_exe = info.ld_dir + "/ld";
        }
    }

    // Testar se o ld embutido funciona
    if (fs::exists(info.ld_exe)) {
        std::string env_prefix = "LD_LIBRARY_PATH=\"" + info.ld_dir + "\" ";
        if (test_executable(info.ld_exe, env_prefix))
            return info;
    }

    // Fallback: ld do sistema
    std::string system_ld = ld_exec_cmd("which ld 2>/dev/null");
    if (!system_ld.empty() && fs::exists(system_ld)) {
        info.ld_exe = system_ld;
        info.using_system_ld = true;
        return info;
    }

    info.ld_exe = "ld";
    info.using_system_ld = true;
    return info;
}

// Helper: CRT do sistema se existir, senao embutido
static std::string resolve_crt(const std::string& name,
                                const std::string& system_dir,
                                const std::string& gcc_dir,
                                const std::string& ld_dir) {
    if (name == "crt1.o" || name == "crti.o" || name == "crtn.o") {
        if (!system_dir.empty() && fs::exists(system_dir + "/" + name))
            return system_dir + "/" + name;
    }

    if (name.find("crtbegin") != std::string::npos || name.find("crtend") != std::string::npos) {
        if (!gcc_dir.empty() && fs::exists(gcc_dir + "/" + name))
            return gcc_dir + "/" + name;
    }

    return ld_dir + "/" + name;
}

// Adiciona -L para um diretorio se nao vazio e nao duplicado
static void add_L(std::string& cmd, const std::string& dir, std::vector<std::string>& added) {
    if (dir.empty()) return;
    for (auto& a : added) { if (a == dir) return; }
    cmd += " -L\"" + dir + "\"";
    added.push_back(dir);
}

// ============================================================================
// LINKAGEM: .o -> executavel
//
// Decisao de modo:
//   - Se tem .jpd (extra_dlls)       -> sempre dinamico
//   - Se ld embutido funciona        -> estatico com libs embutidas
//   - Se ld do sistema + libc.a      -> estatico com libs do sistema
//   - Se ld do sistema sem libc.a    -> dinamico automaticamente
// ============================================================================

static bool link_with_ld(const std::string& obj_path, const std::string& exe_path,
                          const std::vector<std::string>& extra_objs = {},
                          const std::vector<std::string>& extra_libs = {},
                          const std::vector<std::string>& extra_lib_paths = {},
                          const std::vector<std::string>& extra_dlls = {},
                          bool windowed = false) {

    (void)windowed; // Ignorado no Linux

    LinkerInfo linker = find_linker();

    if (!linker.using_system_ld && !fs::exists(linker.ld_exe)) {
        std::cerr << "Erro: ld nao encontrado" << std::endl;
        return false;
    }

    std::string system_crt_dir = find_system_crt_dir();
    std::string gcc_crt_dir = find_gcc_crt_dir();
    std::string gcc_lib_dir = find_gcc_lib_dir();

    bool has_jpd = !extra_dlls.empty();

    // Decidir se linkagem sera estatica ou dinamica
    bool force_dynamic = has_jpd;
    if (!force_dynamic && linker.using_system_ld && !has_static_libc(system_crt_dir)) {
        force_dynamic = true;
    }

    // Montar comando
    std::string cmd;

    if (!linker.using_system_ld) {
        cmd = "LD_LIBRARY_PATH=\"" + linker.ld_dir + "\" ";
    }

    cmd += "\"" + linker.ld_exe + "\"";
    cmd += " -m elf_x86_64 --hash-style=gnu";

    if (force_dynamic) {
        // ================================================================
        // LINKAGEM DINAMICA
        // ================================================================
        cmd += " -z relro";
        cmd += " --dynamic-linker /lib64/ld-linux-x86-64.so.2";
        cmd += " -o \"" + exe_path + "\"";

        // CRT startup
        cmd += " \"" + resolve_crt("crt1.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crti.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";

        std::string crtbeginS = resolve_crt("crtbeginS.o", system_crt_dir, gcc_crt_dir, linker.ld_dir);
        if (fs::exists(crtbeginS)) {
            cmd += " \"" + crtbeginS + "\"";
        }

        // Search paths
        std::vector<std::string> added_dirs;
        if (linker.using_system_ld) {
            add_L(cmd, system_crt_dir, added_dirs);
            add_L(cmd, gcc_lib_dir, added_dirs);
        } else {
            add_L(cmd, linker.ld_dir, added_dirs);
            add_L(cmd, system_crt_dir, added_dirs);
            add_L(cmd, gcc_lib_dir, added_dirs);
        }

        for (auto& lpath : extra_lib_paths) {
            cmd += " -rpath \"" + lpath + "\"";
            add_L(cmd, lpath, added_dirs);
        }

        // Objeto principal
        cmd += " \"" + obj_path + "\"";

        // Objetos extras
        for (auto& obj : extra_objs) {
            if (fs::exists(obj)) cmd += " \"" + obj + "\"";
        }

        // Bibliotecas dinamicas .jpd
        for (auto& dll : extra_dlls) {
            if (fs::exists(dll)) {
                cmd += " \"" + dll + "\"";
            } else {
                std::cerr << "Aviso: Biblioteca dinamica nao encontrada: " << dll << std::endl;
            }
        }

        // libstdc++: usar embutida (estatica) pra evitar dep de .so
        std::string libstdcpp_a = linker.ld_dir + "/libstdc++.a";
        if (fs::exists(libstdcpp_a)) {
            cmd += " \"" + libstdcpp_a + "\"";
        } else {
            cmd += " -lstdc++";
        }
        cmd += " -lm";

        for (auto& lib : extra_libs) {
            cmd += " -l" + lib;
        }

        cmd += " --start-group -lgcc -lgcc_s -lc --end-group";

        // CRT finalization
        std::string crtendS = resolve_crt("crtendS.o", system_crt_dir, gcc_crt_dir, linker.ld_dir);
        if (fs::exists(crtendS)) {
            cmd += " \"" + crtendS + "\"";
        } else {
            cmd += " \"" + resolve_crt("crtend.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";
        }
        cmd += " \"" + resolve_crt("crtn.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";

    } else {
        // ================================================================
        // LINKAGEM ESTATICA
        // (so chega aqui se ld embutido funciona OU sistema tem libc.a)
        // ================================================================
        cmd += " -static -z relro";
        cmd += " -o \"" + exe_path + "\"";

        // CRT startup
        cmd += " \"" + resolve_crt("crt1.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crti.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crtbeginT.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";

        // Search paths
        std::vector<std::string> added_dirs;
        if (linker.using_system_ld) {
            // ld do sistema com glibc-static: usar so libs do sistema
            add_L(cmd, system_crt_dir, added_dirs);
            add_L(cmd, gcc_crt_dir, added_dirs);
            add_L(cmd, gcc_lib_dir, added_dirs);
        } else {
            // ld embutido: libs embutidas primeiro, sistema como fallback
            add_L(cmd, linker.ld_dir, added_dirs);
            add_L(cmd, system_crt_dir, added_dirs);
            add_L(cmd, gcc_crt_dir, added_dirs);
            add_L(cmd, gcc_lib_dir, added_dirs);
        }

        for (auto& lpath : extra_lib_paths) {
            add_L(cmd, lpath, added_dirs);
        }

        // Objeto principal
        cmd += " \"" + obj_path + "\"";

        // Objetos extras
        for (auto& obj : extra_objs) {
            if (fs::exists(obj)) cmd += " \"" + obj + "\"";
        }

        // Libs estaticas
        if (linker.using_system_ld) {
            // Usando ld do sistema: libstdc++.a embutida se existir
            std::string libstdcpp_a = linker.ld_dir + "/libstdc++.a";
            if (fs::exists(libstdcpp_a)) {
                cmd += " \"" + libstdcpp_a + "\"";
            } else {
                cmd += " -lstdc++";
            }
            cmd += " -lm";
        } else {
            cmd += " -lstdc++ -lm";
        }

        for (auto& lib : extra_libs) {
            cmd += " -l" + lib;
        }

        cmd += " --start-group -lgcc -lgcc_eh -lc --end-group";

        // CRT finalization
        cmd += " \"" + resolve_crt("crtend.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";
        cmd += " \"" + resolve_crt("crtn.o", system_crt_dir, gcc_crt_dir, linker.ld_dir) + "\"";
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