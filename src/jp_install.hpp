// jp_install.hpp
// Gerenciador de bibliotecas JPLang — instalar, desinstalar, listar via GitHub

#ifndef JPLANG_INSTALL_HPP
#define JPLANG_INSTALL_HPP

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

namespace jplang {

// ============================================================================
// CONFIGURAÇÃO
// ============================================================================

#ifdef _WIN32
    static const std::string REPO_OWNER = "Joaopaulopadilha";
    static const std::string REPO_NAME  = "bibliotecas_windows";
#else
    static const std::string REPO_OWNER = "Joaopaulopadilha";
    static const std::string REPO_NAME  = "bibliotecas_linux";
#endif

static const std::string REPO_BRANCH = "main";

// Resolve diretório de bibliotecas: exe_dir/bibliotecas (se exe_dir fornecido)
// Caso contrário, fallback para "bibliotecas" (relativo ao CWD)
static std::string resolve_lib_dir(const std::string& exe_dir) {
    if (!exe_dir.empty()) {
        return exe_dir + "/bibliotecas";
    }
    return "bibliotecas";
}

// ============================================================================
// HELPERS
// ============================================================================

// Executa comando e captura stdout
static std::string exec_cmd(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

// Verifica se curl está disponível
static bool has_curl() {
    std::string out = exec_cmd("curl --version 2>&1");
    return out.find("curl") != std::string::npos;
}

// ============================================================================
// JSON MINI-PARSER (para resposta da GitHub API)
// Extrai valores de "name" e "download_url" e "type" de arrays JSON
// ============================================================================

struct GitHubEntry {
    std::string name;
    std::string download_url;
    std::string type;  // "file" ou "dir"
};

static std::string json_extract_value(const std::string& json, size_t& pos, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t found = json.find(search, pos);
    if (found == std::string::npos) return "";

    // Avança até o valor
    size_t colon = json.find(':', found + search.size());
    if (colon == std::string::npos) return "";

    size_t start = colon + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;

    if (start >= json.size()) return "";

    if (json[start] == '"') {
        // String value
        start++;
        size_t end = json.find('"', start);
        if (end == std::string::npos) return "";
        pos = end + 1;
        return json.substr(start, end - start);
    }
    else if (json.substr(start, 4) == "null") {
        pos = start + 4;
        return "";
    }

    // Outro tipo (número etc)
    size_t end = start;
    while (end < json.size() && json[end] != ',' && json[end] != '}') end++;
    pos = end;
    return json.substr(start, end - start);
}

static std::vector<GitHubEntry> parse_github_api_response(const std::string& json) {
    std::vector<GitHubEntry> entries;

    // Procurar cada objeto { ... } no array
    size_t pos = 0;
    while (pos < json.size()) {
        size_t obj_start = json.find('{', pos);
        if (obj_start == std::string::npos) break;

        size_t obj_end = json.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        // Extrair campos deste objeto
        std::string obj = json.substr(obj_start, obj_end - obj_start + 1);
        size_t opos = 0;

        GitHubEntry entry;

        // Extrair "name"
        opos = 0;
        entry.name = json_extract_value(obj, opos, "name");

        // Extrair "type"
        opos = 0;
        entry.type = json_extract_value(obj, opos, "type");

        // Extrair "download_url"
        opos = 0;
        entry.download_url = json_extract_value(obj, opos, "download_url");

        if (!entry.name.empty() && !entry.type.empty()) {
            entries.push_back(entry);
        }

        pos = obj_end + 1;
    }

    return entries;
}

// ============================================================================
// DOWNLOAD DE ARQUIVO VIA CURL
// ============================================================================

static bool download_file(const std::string& url, const std::string& dest_path) {
    // Criar diretório pai se necessário
    fs::path parent = fs::path(dest_path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }

    std::string cmd = "curl -sL -o \"" + dest_path + "\" \"" + url + "\" 2>&1";
    int ret = std::system(cmd.c_str());
    return ret == 0 && fs::exists(dest_path) && fs::file_size(dest_path) > 0;
}

// ============================================================================
// LISTAR CONTEÚDO DE PASTA NO GITHUB VIA API
// ============================================================================

static std::vector<GitHubEntry> github_list_dir(const std::string& path) {
    std::string api_url = "https://api.github.com/repos/"
                        + REPO_OWNER + "/" + REPO_NAME
                        + "/contents/" + path + "?ref=" + REPO_BRANCH;

    std::string cmd = "curl -sL -H \"Accept: application/vnd.github.v3+json\" \""
                    + api_url + "\" 2>&1";
    std::string response = exec_cmd(cmd);

    if (response.empty() || response[0] != '[') {
        return {};
    }

    return parse_github_api_response(response);
}

// ============================================================================
// LISTAR BIBLIOTECAS DISPONÍVEIS NO REPOSITÓRIO REMOTO
// ============================================================================

static std::vector<std::string> remote_list_libs() {
    auto entries = github_list_dir("");
    std::vector<std::string> libs;
    for (auto& e : entries) {
        if (e.type == "dir") {
            libs.push_back(e.name);
        }
    }
    return libs;
}

// ============================================================================
// DOWNLOAD RECURSIVO DE PASTA
// ============================================================================

static bool download_directory(const std::string& remote_path,
                               const std::string& local_base) {
    auto entries = github_list_dir(remote_path);

    if (entries.empty()) {
        return false;
    }

    for (auto& entry : entries) {
        std::string local_path = local_base + "/" + entry.name;

        if (entry.type == "file") {
            if (entry.download_url.empty()) {
                std::cerr << "  AVISO: sem URL para " << entry.name << std::endl;
                continue;
            }
            std::cout << "  Baixando " << entry.name << "..." << std::endl;
            if (!download_file(entry.download_url, local_path)) {
                std::cerr << "  ERRO: falha ao baixar " << entry.name << std::endl;
                return false;
            }
        }
        else if (entry.type == "dir") {
            std::string sub_remote = remote_path + "/" + entry.name;
            if (!download_directory(sub_remote, local_path)) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// INSTALAR BIBLIOTECA
// ============================================================================

static int install_lib(const std::string& lib_name, const std::string& exe_dir = "") {
    if (!has_curl()) {
        std::cerr << "Erro: 'curl' não encontrado. Instale curl para usar este comando." << std::endl;
        return 1;
    }

    std::string lib_dir = resolve_lib_dir(exe_dir);
    fs::path lib_path = fs::path(lib_dir) / lib_name;

    if (fs::exists(lib_path)) {
        std::cerr << "Biblioteca '" << lib_name << "' já está instalada." << std::endl;
        std::cerr << "Use 'jp desinstalar " << lib_name << "' primeiro para reinstalar." << std::endl;
        return 1;
    }

    std::cout << "Instalando '" << lib_name << "'..." << std::endl;

#ifdef _WIN32
    std::cout << "Repositorio: bibliotecas_windows/" << lib_name << std::endl;
#else
    std::cout << "Repositorio: bibliotecas_linux/" << lib_name << std::endl;
#endif

    // Verificar se existe no repositório
    auto entries = github_list_dir(lib_name);
    if (entries.empty()) {
        std::cerr << "Erro: biblioteca '" << lib_name << "' não encontrada no repositório." << std::endl;
        std::cerr << "Use 'jp listar --remoto' para ver bibliotecas disponíveis." << std::endl;
        return 1;
    }

    // Criar diretório e baixar
    fs::create_directories(lib_path);

    if (!download_directory(lib_name, lib_path.string())) {
        std::cerr << "Erro ao baixar biblioteca." << std::endl;
        fs::remove_all(lib_path);
        return 1;
    }

    std::cout << "Biblioteca '" << lib_name << "' instalada com sucesso em "
              << lib_path.string() << "/" << std::endl;
    return 0;
}

// ============================================================================
// DESINSTALAR BIBLIOTECA
// ============================================================================

static int uninstall_lib(const std::string& lib_name, const std::string& exe_dir = "") {
    std::string lib_dir = resolve_lib_dir(exe_dir);
    fs::path lib_path = fs::path(lib_dir) / lib_name;

    if (!fs::exists(lib_path)) {
        std::cerr << "Biblioteca '" << lib_name << "' não está instalada." << std::endl;
        return 1;
    }

    std::cout << "Desinstalando '" << lib_name << "'..." << std::endl;
    fs::remove_all(lib_path);
    std::cout << "Biblioteca '" << lib_name << "' removida." << std::endl;
    return 0;
}

// ============================================================================
// LISTAR BIBLIOTECAS
// ============================================================================

static int list_libs(bool show_remote, const std::string& exe_dir = "") {
    std::string lib_dir = resolve_lib_dir(exe_dir);

    // Listar instaladas
    std::cout << "Bibliotecas instaladas:" << std::endl;

    if (!fs::exists(lib_dir)) {
        std::cout << "  (nenhuma)" << std::endl;
    } else {
        bool found = false;
        for (auto& entry : fs::directory_iterator(lib_dir)) {
            if (entry.is_directory()) {
                std::cout << "  " << entry.path().filename().string() << std::endl;
                found = true;
            }
        }
        if (!found) {
            std::cout << "  (nenhuma)" << std::endl;
        }
    }

    // Listar remotas
    if (show_remote) {
        if (!has_curl()) {
            std::cerr << "\nErro: 'curl' necessário para listar remotas." << std::endl;
            return 1;
        }

        std::cout << "\nBibliotecas disponíveis no repositório:" << std::endl;
        auto libs = remote_list_libs();
        if (libs.empty()) {
            std::cout << "  (erro ao consultar repositório)" << std::endl;
        } else {
            for (auto& lib : libs) {
                // Marcar se já instalada
                fs::path lib_path = fs::path(lib_dir) / lib;
                std::string status = fs::exists(lib_path) ? " [instalada]" : "";
                std::cout << "  " << lib << status << std::endl;
            }
        }
    }

    return 0;
}

} // namespace jplang

#endif // JPLANG_INSTALL_HPP