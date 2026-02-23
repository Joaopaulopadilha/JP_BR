// jp_install.hpp
// Sistema de instalacao/desinstalacao de bibliotecas JPLang via GitHub
// Comandos: jp instalar <lib>, jp desinstalar <lib>, jp --listar, jp <lib> (info/funcoes)
// Usa WinHTTP (Windows) e OpenSSL+sockets (Linux) para HTTPS

#ifndef JP_INSTALL_HPP
#define JP_INSTALL_HPP

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

// =============================================================================
// CONFIGURACAO DOS REPOSITORIOS
// =============================================================================

#ifdef _WIN32
    static const std::string REPO_OWNER = "Joaopaulopadilha";
    static const std::string REPO_NAME  = "bibliotecas_windows";
#else
    static const std::string REPO_OWNER = "Joaopaulopadilha";
    static const std::string REPO_NAME  = "bibliotecas_linux";
#endif

static const std::string GITHUB_API_HOST = "api.github.com";
static const std::string GITHUB_RAW_HOST = "raw.githubusercontent.com";
static const std::string BRANCH = "main";

// =============================================================================
// HTTP CLIENT - WINDOWS (WinHTTP)
// =============================================================================
#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace jphttp {

// Faz uma requisicao HTTPS GET e retorna o body como string
static std::string httpsGet(const std::string& host, const std::string& path) {
    std::string resultado;
    
    // Converte host para wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, NULL, 0);
    std::vector<wchar_t> whost(wlen);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, whost.data(), wlen);
    
    // Converte path para wide string
    wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wpath(wlen);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
    
    HINTERNET hSession = WinHttpOpen(L"JPLang/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (!hSession) return "";
    
    HINTERNET hConnect = WinHttpConnect(hSession, whost.data(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.data(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    // Adiciona header User-Agent (GitHub API exige)
    WinHttpAddRequestHeaders(hRequest, 
        L"User-Agent: JPLang/1.0\r\nAccept: application/vnd.github.v3+json\r\n",
        -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    if (bResults) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize > 0) {
                std::vector<char> buffer(dwSize + 1, 0);
                DWORD dwDownloaded = 0;
                WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded);
                resultado.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return resultado;
}

// Baixa um arquivo binario via HTTPS
static bool httpsDownload(const std::string& host, const std::string& path, const std::string& destino) {
    // Converte host para wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, NULL, 0);
    std::vector<wchar_t> whost(wlen);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, whost.data(), wlen);
    
    // Converte path para wide string
    wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wpath(wlen);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
    
    HINTERNET hSession = WinHttpOpen(L"JPLang/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (!hSession) return false;
    
    HINTERNET hConnect = WinHttpConnect(hSession, whost.data(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.data(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    WinHttpAddRequestHeaders(hRequest,
        L"User-Agent: JPLang/1.0\r\n",
        -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    bool ok = false;
    if (bResults) {
        // Verifica status code
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX);
        
        if (statusCode == 200) {
            std::ofstream out(destino, std::ios::binary);
            if (out.is_open()) {
                DWORD dwSize = 0;
                do {
                    dwSize = 0;
                    WinHttpQueryDataAvailable(hRequest, &dwSize);
                    if (dwSize > 0) {
                        std::vector<char> buffer(dwSize);
                        DWORD dwDownloaded = 0;
                        WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded);
                        out.write(buffer.data(), dwDownloaded);
                    }
                } while (dwSize > 0);
                out.close();
                ok = true;
            }
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return ok;
}

} // namespace jphttp

// =============================================================================
// HTTP CLIENT - LINUX (OpenSSL + sockets)
// =============================================================================
#else

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace jphttp {

static bool g_ssl_init = false;

static void ssl_init() {
    if (g_ssl_init) return;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    g_ssl_init = true;
}

// Le toda a resposta HTTP de um SSL connection
static std::string ssl_read_all(SSL* ssl) {
    std::string resultado;
    char buffer[8192];
    int bytes;
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
        resultado.append(buffer, bytes);
    }
    return resultado;
}

// Extrai o body de uma resposta HTTP (pula headers)
static std::string extract_body(const std::string& response) {
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) return response;
    return response.substr(pos + 4);
}

// Extrai status code da resposta HTTP
static int extract_status(const std::string& response) {
    // "HTTP/1.1 200 OK"
    size_t pos = response.find(' ');
    if (pos == std::string::npos) return 0;
    return std::stoi(response.substr(pos + 1, 3));
}

// Conecta via HTTPS e retorna o body
static std::string httpsGet(const std::string& host, const std::string& path) {
    ssl_init();
    
    // Resolve host
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host.c_str(), "443", &hints, &res) != 0) {
        return "";
    }
    
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return "";
    }
    
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        return "";
    }
    freeaddrinfo(res);
    
    // SSL
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        close(sock);
        return "";
    }
    
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, host.c_str());
    
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return "";
    }
    
    // Monta request
    std::string request = 
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "User-Agent: JPLang/1.0\r\n"
        "Accept: application/vnd.github.v3+json\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    SSL_write(ssl, request.c_str(), request.size());
    
    std::string response = ssl_read_all(ssl);
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
    
    return extract_body(response);
}

// Baixa arquivo binario via HTTPS
static bool httpsDownload(const std::string& host, const std::string& path, const std::string& destino) {
    ssl_init();
    
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host.c_str(), "443", &hints, &res) != 0) return false;
    
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return false; }
    
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock); freeaddrinfo(res); return false;
    }
    freeaddrinfo(res);
    
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { close(sock); return false; }
    
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, host.c_str());
    
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl); SSL_CTX_free(ctx); close(sock); return false;
    }
    
    std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "User-Agent: JPLang/1.0\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    SSL_write(ssl, request.c_str(), request.size());
    
    std::string response = ssl_read_all(ssl);
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
    
    int status = extract_status(response);
    if (status != 200) return false;
    
    std::string body = extract_body(response);
    
    std::ofstream out(destino, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(body.data(), body.size());
    out.close();
    
    return true;
}

} // namespace jphttp

#endif // _WIN32 / Linux

// =============================================================================
// PARSER JSON MINIMO (para resposta da GitHub API)
// =============================================================================

namespace jpjson {

// Estrutura simples para um arquivo retornado pela API
struct GitHubFile {
    std::string name;
    std::string download_url;
    std::string type;   // "file" ou "dir"
    size_t size = 0;
};

// Extrai string de um campo JSON: "campo": "valor"
static std::string getField(const std::string& json, size_t start, const std::string& campo) {
    std::string busca = "\"" + campo + "\"";
    size_t pos = json.find(busca, start);
    if (pos == std::string::npos) return "";
    
    pos = json.find(':', pos + busca.size());
    if (pos == std::string::npos) return "";
    pos++;
    
    // Pula espacos
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    
    if (pos >= json.size()) return "";
    
    // Null
    if (json.substr(pos, 4) == "null") return "";
    
    // String
    if (json[pos] == '"') {
        pos++;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                if (json[pos] == 'n') val += '\n';
                else if (json[pos] == 't') val += '\t';
                else val += json[pos];
            } else {
                val += json[pos];
            }
            pos++;
        }
        return val;
    }
    
    // Numero
    std::string val;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ' ' && json[pos] != '\n') {
        val += json[pos];
        pos++;
    }
    return val;
}

// Parseia a resposta da GitHub API contents (array de objetos)
static std::vector<GitHubFile> parseContents(const std::string& json) {
    std::vector<GitHubFile> arquivos;
    
    // Procura cada objeto { ... } no array
    size_t pos = 0;
    while (pos < json.size()) {
        size_t objStart = json.find('{', pos);
        if (objStart == std::string::npos) break;
        
        // Encontra o fim do objeto (nivel 1)
        int nivel = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < json.size() && nivel > 0) {
            if (json[objEnd] == '{') nivel++;
            else if (json[objEnd] == '}') nivel--;
            objEnd++;
        }
        
        std::string obj = json.substr(objStart, objEnd - objStart);
        
        GitHubFile f;
        f.name = getField(obj, 0, "name");
        f.download_url = getField(obj, 0, "download_url");
        f.type = getField(obj, 0, "type");
        std::string sizeStr = getField(obj, 0, "size");
        if (!sizeStr.empty()) {
            try { f.size = std::stoull(sizeStr); } catch (...) {}
        }
        
        if (!f.name.empty()) {
            arquivos.push_back(f);
        }
        
        pos = objEnd;
    }
    
    return arquivos;
}

// Verifica se a resposta eh um erro 404 do GitHub
static bool isNotFound(const std::string& json) {
    return json.find("\"Not Found\"") != std::string::npos ||
           json.find("\"message\"") != std::string::npos;
}

} // namespace jpjson

// =============================================================================
// FUNCOES DE INSTALACAO
// =============================================================================

namespace jpinstall {

// Retorna o caminho da pasta bibliotecas/ relativo ao executavel
static fs::path getBibliotecasDir(const std::string& exeDir) {
    fs::path baseDir = exeDir;
    
    // Tenta encontrar a pasta bibliotecas/
    std::vector<fs::path> caminhos = {
        baseDir / "bibliotecas",
        fs::path("bibliotecas"),
    };
    
    for (const auto& c : caminhos) {
        if (fs::exists(c) && fs::is_directory(c)) {
            return fs::absolute(c);
        }
    }
    
    // Se nao existe, cria
    fs::path padrao = baseDir / "bibliotecas";
    fs::create_directories(padrao);
    return fs::absolute(padrao);
}

// Formata tamanho em bytes pra legivel
static std::string formatSize(size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        snprintf(buf, 32, "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32];
    snprintf(buf, 32, "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

// Lista arquivos de uma biblioteca no GitHub
static std::vector<jpjson::GitHubFile> listarArquivosRemoto(const std::string& lib) {
    std::string path = "/repos/" + REPO_OWNER + "/" + REPO_NAME + "/contents/" + lib;
    
    std::string json = jphttp::httpsGet(GITHUB_API_HOST, path);
    
    if (json.empty() || jpjson::isNotFound(json)) {
        return {};
    }
    
    auto arquivos = jpjson::parseContents(json);
    
    // Filtra apenas arquivos (ignora subdiretorios por enquanto)
    std::vector<jpjson::GitHubFile> resultado;
    for (const auto& f : arquivos) {
        if (f.type == "file") {
            resultado.push_back(f);
        }
    }
    
    return resultado;
}

// Instala uma biblioteca: baixa todos os arquivos do GitHub
static int instalar(const std::string& lib, const std::string& exeDir) {
    fs::path bibDir = getBibliotecasDir(exeDir);
    fs::path libDir = bibDir / lib;
    
    // Verifica se ja esta instalada
    if (fs::exists(libDir)) {
        std::cout << "[JP] Biblioteca '" << lib << "' ja esta instalada em: " << libDir << std::endl;
        std::cout << "[JP] Use 'jp desinstalar " << lib << "' primeiro para reinstalar." << std::endl;
        return 1;
    }
    
    std::cout << "[JP] Buscando biblioteca '" << lib << "'..." << std::endl;
    
    // Lista arquivos no GitHub
    auto arquivos = listarArquivosRemoto(lib);
    
    if (arquivos.empty()) {
        #ifdef _WIN32
        std::cout << "[JP] Biblioteca '" << lib << "' nao encontrada para Windows." << std::endl;
        #else
        std::cout << "[JP] Biblioteca '" << lib << "' nao encontrada para Linux." << std::endl;
        #endif
        std::cout << "[JP] Verifique o nome ou acesse: https://github.com/" 
                  << REPO_OWNER << "/" << REPO_NAME << std::endl;
        return 1;
    }
    
    // Calcula tamanho total
    size_t totalSize = 0;
    for (const auto& f : arquivos) totalSize += f.size;
    
    std::cout << "[JP] Encontrado: " << arquivos.size() << " arquivo(s) (" 
              << formatSize(totalSize) << ")" << std::endl;
    
    // Cria diretorio
    fs::create_directories(libDir);
    
    // Baixa cada arquivo
    int baixados = 0;
    for (const auto& f : arquivos) {
        std::cout << "[JP] Baixando: " << f.name << " (" << formatSize(f.size) << ")..." << std::flush;
        
        // Monta path para download raw
        std::string rawPath = "/" + REPO_OWNER + "/" + REPO_NAME + "/" + BRANCH + "/" + lib + "/" + f.name;
        std::string destino = (libDir / f.name).string();
        
        bool ok = jphttp::httpsDownload(GITHUB_RAW_HOST, rawPath, destino);
        
        if (ok) {
            std::cout << " OK" << std::endl;
            baixados++;
            
            // Em Linux, da permissao de execucao para arquivos sem extensao e .jpd
            #ifndef _WIN32
            fs::path p(f.name);
            std::string ext = p.extension().string();
            if (ext.empty() || ext == ".jpd") {
                fs::permissions(destino,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
            }
            #endif
        } else {
            std::cout << " ERRO" << std::endl;
        }
    }
    
    if (baixados == (int)arquivos.size()) {
        std::cout << "[JP] Biblioteca '" << lib << "' instalada com sucesso!" << std::endl;
        return 0;
    } else {
        std::cout << "[JP] Atencao: " << baixados << "/" << arquivos.size() 
                  << " arquivos baixados." << std::endl;
        return 1;
    }
}

// Desinstala uma biblioteca: remove a pasta
static int desinstalar(const std::string& lib, const std::string& exeDir) {
    fs::path bibDir = getBibliotecasDir(exeDir);
    fs::path libDir = bibDir / lib;
    
    if (!fs::exists(libDir)) {
        std::cout << "[JP] Biblioteca '" << lib << "' nao esta instalada." << std::endl;
        return 1;
    }
    
    // Conta arquivos
    int count = 0;
    for (const auto& entry : fs::directory_iterator(libDir)) {
        if (entry.is_regular_file()) count++;
    }
    
    std::cout << "[JP] Removendo biblioteca '" << lib << "' (" << count << " arquivos)..." << std::endl;
    
    try {
        fs::remove_all(libDir);
        std::cout << "[JP] Biblioteca '" << lib << "' desinstalada com sucesso!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[JP] Erro ao remover: " << e.what() << std::endl;
        return 1;
    }
}

// Mostra informacoes e funcoes exportadas de uma biblioteca
static int info(const std::string& lib, const std::string& exeDir) {
    fs::path bibDir = getBibliotecasDir(exeDir);
    fs::path libDir = bibDir / lib;
    
    bool instalada = fs::exists(libDir);
    
    std::cout << "=== Biblioteca: " << lib << " ===" << std::endl;
    
    if (instalada) {
        std::cout << "Status: Instalada" << std::endl;
        std::cout << "Local: " << libDir << std::endl;
        
        // Lista arquivos
        std::cout << "\nArquivos:" << std::endl;
        for (const auto& entry : fs::directory_iterator(libDir)) {
            if (entry.is_regular_file()) {
                std::cout << "  " << entry.path().filename().string() 
                          << " (" << formatSize(entry.file_size()) << ")" << std::endl;
            }
        }
        
        // Busca funcoes exportadas no .cpp
        fs::path cppFile = libDir / (lib + ".cpp");
        if (fs::exists(cppFile)) {
            std::cout << "\nFuncoes disponiveis:" << std::endl;
            
            std::ifstream in(cppFile);
            std::string line;
            bool encontrou = false;
            
            while (std::getline(in, line)) {
                // Busca linhas com JP_EXPORT
                if (line.find("JP_EXPORT") != std::string::npos) {
                    // Extrai nome da funcao
                    // Formato tipico: JP_EXPORT JPValor nome_func(JPValor* args, int numArgs) {
                    size_t pos = line.find("JP_EXPORT");
                    std::string resto = line.substr(pos + 9); // pula "JP_EXPORT"
                    
                    // Pula tipo de retorno (JPValor, void, etc)
                    // Procura o nome da funcao antes do (
                    size_t parenPos = resto.find('(');
                    if (parenPos != std::string::npos) {
                        std::string antes = resto.substr(0, parenPos);
                        
                        // Remove espacos e pega a ultima palavra (nome da funcao)
                        // Trim
                        while (!antes.empty() && antes.back() == ' ') antes.pop_back();
                        
                        size_t spacePos = antes.rfind(' ');
                        std::string nomeFuncao;
                        if (spacePos != std::string::npos) {
                            nomeFuncao = antes.substr(spacePos + 1);
                        } else {
                            nomeFuncao = antes;
                        }
                        
                        if (!nomeFuncao.empty()) {
                            // Busca comentario na linha anterior ou mesma linha
                            std::cout << "  " << nomeFuncao << "()" << std::endl;
                            encontrou = true;
                        }
                    }
                }
                
                // Tambem busca comentarios com descricao das funcoes
                // Formato: // nome_func(args)
                // Descricao da funcao
                if (line.find("// " + lib + "_") != std::string::npos || 
                    line.find("// Retorna:") != std::string::npos) {
                    // Pula - sao comentarios auxiliares
                }
            }
            
            if (!encontrou) {
                std::cout << "  (nenhuma funcao exportada encontrada no fonte)" << std::endl;
            }
        } else {
            std::cout << "\nFonte (.cpp) nao disponivel para listar funcoes." << std::endl;
        }
        
    } else {
        // Nao instalada - busca no GitHub
        std::cout << "Status: Nao instalada" << std::endl;
        std::cout << "\nBuscando no repositorio..." << std::endl;
        
        auto arquivos = listarArquivosRemoto(lib);
        
        if (arquivos.empty()) {
            #ifdef _WIN32
            std::cout << "Biblioteca nao encontrada para Windows." << std::endl;
            #else
            std::cout << "Biblioteca nao encontrada para Linux." << std::endl;
            #endif
        } else {
            size_t totalSize = 0;
            std::cout << "Disponivel para download (" << arquivos.size() << " arquivos):" << std::endl;
            for (const auto& f : arquivos) {
                std::cout << "  " << f.name << " (" << formatSize(f.size) << ")" << std::endl;
                totalSize += f.size;
            }
            std::cout << "\nTamanho total: " << formatSize(totalSize) << std::endl;
            std::cout << "Para instalar: jp instalar " << lib << std::endl;
        }
    }
    
    return 0;
}

// Lista todas as bibliotecas instaladas
static int listar(const std::string& exeDir) {
    fs::path bibDir = getBibliotecasDir(exeDir);
    
    std::cout << "[JP] Bibliotecas instaladas:" << std::endl;
    
    int count = 0;
    if (fs::exists(bibDir)) {
        for (const auto& entry : fs::directory_iterator(bibDir)) {
            if (entry.is_directory()) {
                std::string nome = entry.path().filename().string();
                std::cout << "  " << nome << std::endl;
                count++;
            }
        }
    }
    
    if (count == 0) {
        std::cout << "  (nenhuma biblioteca instalada)" << std::endl;
    } else {
        std::cout << "\nTotal: " << count << " biblioteca(s)" << std::endl;
    }
    
    return 0;
}

// Processa comandos de instalacao
// Retorna: -1 = nao eh comando de instalacao, 0 = sucesso, 1 = erro
static int processarComando(int argc, char* argv[]) {
    if (argc < 2) return -1;
    
    std::string cmd = argv[1];
    
    // Diretorio do executavel
    std::string exeDir = fs::path(argv[0]).parent_path().string();
    if (exeDir.empty()) exeDir = fs::current_path().string();
    
    // jp instalar <lib>
    if (cmd == "instalar" || cmd == "install") {
        if (argc < 3) {
            std::cerr << "[JP] Uso: jp instalar <biblioteca>" << std::endl;
            std::cerr << "[JP] Exemplo: jp instalar yt" << std::endl;
            return 1;
        }
        return instalar(argv[2], exeDir);
    }
    
    // jp desinstalar <lib>
    if (cmd == "desinstalar" || cmd == "uninstall" || cmd == "remover") {
        if (argc < 3) {
            std::cerr << "[JP] Uso: jp desinstalar <biblioteca>" << std::endl;
            return 1;
        }
        return desinstalar(argv[2], exeDir);
    }
    
    // jp bibliotecas (lista todas)
    if (cmd == "bibliotecas" || cmd == "libs" || cmd == "--listar") {
        return listar(exeDir);
    }
    
    // jp <lib> - info se nao for um arquivo .jp
    if (argc == 2) {
        std::string arg = argv[1];
        
        // Se termina com .jp, nao eh comando de info
        if (arg.size() > 3 && arg.substr(arg.size() - 3) == ".jp") {
            return -1;
        }
        
        // Se eh um dos comandos conhecidos, ignora
        if (arg == "build" || arg == "compilar" || arg == "debug" || 
            arg == "help" || arg == "ajuda" || arg == "--help" || arg == "-h" ||
            arg == "--listar") {
            return -1;
        }
        
        // Se o arquivo existe no disco (pode ser um .jp sem extensao?), ignora
        if (fs::exists(arg)) {
            return -1;
        }
        
        // Trata como pedido de info da biblioteca
        return info(arg, exeDir);
    }
    
    return -1; // Nao eh comando de instalacao
}

} // namespace jpinstall

#endif // JP_INSTALL_HPP