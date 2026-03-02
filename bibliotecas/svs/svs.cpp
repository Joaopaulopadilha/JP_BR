// svs.cpp
// Biblioteca nativa SVS (Simple Virtual Server) para JPLang 3.0 - Convenção int64_t
//
// Servidor HTTP com pool de threads para conexões simultâneas.
// Todas as funções recebem/retornam int64_t. Strings = char* castado para int64_t.
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/svs/svs.jpd bibliotecas/svs/svs.cpp -O3 -lws2_32
//   Linux:   g++ -shared -fPIC -o bibliotecas/svs/libsvs.jpd bibliotecas/svs/svs.cpp -O3 -lpthread

#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif
#endif

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <cstdio>
#include <functional>

// =============================================================================
// PLATAFORMA E EXPORT
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SocketType;
    #define INVALID_SOCK INVALID_SOCKET
    #define CLOSE_SOCKET closesocket
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    typedef int SocketType;
    #define INVALID_SOCK -1
    #define CLOSE_SOCKET close
#endif

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================
static char str_bufs[8][4096];
static int str_buf_idx = 0;
static std::mutex str_buf_mutex;

static int64_t retorna_str(const std::string& s) {
    std::lock_guard<std::mutex> lock(str_buf_mutex);
    char* buf = str_bufs[str_buf_idx++ & 7];
    strncpy(buf, s.c_str(), 4095);
    buf[4095] = '\0';
    return (int64_t)buf;
}

// =============================================================================
// HELPERS
// =============================================================================
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::mutex rand_mutex;
static std::string gerar_sessao_id() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string s;
    s.reserve(32);
    std::lock_guard<std::mutex> lock(rand_mutex);
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = true; }
    for (int i = 0; i < 32; ++i)
        s += alphanum[rand() % (sizeof(alphanum) - 1)];
    return s;
}

// =============================================================================
// ESTRUTURAS DE DADOS
// =============================================================================
struct Sessao {
    std::string id;
    std::map<std::string, std::string> dados;
    time_t ultima_atividade;
};

struct Requisicao {
    int id;
    std::string metodo;
    std::string caminho;
    std::string corpo;
    std::string raw;
    std::map<std::string, std::string> headers_entrada;
    std::vector<std::string> headers_saida;
    SocketType cliente_socket;
};

struct ConexaoPendente {
    SocketType cliente_socket;
    int porta;
};

struct Servidor {
    SocketType socket;
    int porta;
    std::atomic<bool> ativo;
    std::vector<std::thread> workers;
    std::thread acceptor_thread;
    std::queue<ConexaoPendente> fila_conexoes;
    std::mutex fila_mutex;
    std::condition_variable fila_cv;
    std::queue<int> fila_requisicoes_prontas;
    std::mutex req_prontas_mutex;
    std::condition_variable req_prontas_cv;
    Servidor() : ativo(false) {}
};

// =============================================================================
// ESTADO GLOBAL
// =============================================================================
static std::unordered_map<int, Servidor*> servidores;
static std::mutex servidores_mutex;
static std::unordered_map<int, Requisicao> requisicoes;
static std::mutex requisicoes_mutex;
static std::unordered_map<std::string, Sessao> sessoes_ativas;
static std::mutex sessoes_mutex;
static std::atomic<int> proximo_req_id{1};
static bool wsa_iniciado = false;
static std::mutex wsa_mutex;
static std::unordered_map<int, std::unordered_map<std::string, std::string>> pastas_mapeadas;
static std::mutex pastas_mutex;
static const int NUM_WORKERS = 8;

// =============================================================================
// FUNÇÕES DE ARQUIVO (INTERNAS)
// =============================================================================
static std::string obter_mime_type(const std::string& caminho) {
    size_t pos = caminho.rfind('.');
    if (pos == std::string::npos) return "application/octet-stream";
    std::string ext = caminho.substr(pos);
    for (auto& c : ext) c = tolower(c);

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".xml") return "application/xml; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".webp") return "image/webp";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".ttf") return "font/ttf";
    if (ext == ".otf") return "font/otf";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".zip") return "application/zip";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".webm") return "video/webm";
    return "application/octet-stream";
}

static bool ler_arquivo(const std::string& caminho, std::vector<char>& conteudo) {
    std::ifstream arquivo(caminho, std::ios::binary | std::ios::ate);
    if (!arquivo.is_open()) return false;
    std::streamsize tamanho = arquivo.tellg();
    arquivo.seekg(0, std::ios::beg);
    conteudo.resize(tamanho);
    return (bool)arquivo.read(conteudo.data(), tamanho);
}

static void enviar_arquivo_impl(SocketType cliente_socket, const std::string& caminho, const std::vector<std::string>& headers_extras) {
    std::vector<char> conteudo;
    if (!ler_arquivo(caminho, conteudo)) {
        std::string resposta = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 18\r\nConnection: close\r\n\r\nArquivo nao existe";
        send(cliente_socket, resposta.c_str(), (int)resposta.size(), 0);
        return;
    }
    std::string mime = obter_mime_type(caminho);
    std::string cabecalho = "HTTP/1.1 200 OK\r\nContent-Type: " + mime + "\r\nContent-Length: " + std::to_string(conteudo.size()) + "\r\nConnection: close\r\n";
    for (const auto& h : headers_extras) cabecalho += h + "\r\n";
    cabecalho += "\r\n";
    send(cliente_socket, cabecalho.c_str(), (int)cabecalho.size(), 0);
    if (!conteudo.empty()) send(cliente_socket, conteudo.data(), (int)conteudo.size(), 0);
}

// =============================================================================
// FUNÇÕES AUXILIARES DE REDE (INTERNAS)
// =============================================================================
static void iniciar_wsa() {
#if JP_WINDOWS
    std::lock_guard<std::mutex> lock(wsa_mutex);
    if (!wsa_iniciado) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        wsa_iniciado = true;
    }
#endif
}

static Requisicao parsear_requisicao(const std::string& raw, SocketType cliente) {
    Requisicao req;
    req.raw = raw;
    req.cliente_socket = cliente;
    size_t pos = 0;
    size_t fim_linha = raw.find("\r\n");

    if (fim_linha != std::string::npos) {
        std::string primeira_linha = raw.substr(0, fim_linha);
        size_t espaco1 = primeira_linha.find(' ');
        if (espaco1 != std::string::npos) {
            req.metodo = primeira_linha.substr(0, espaco1);
            size_t espaco2 = primeira_linha.find(' ', espaco1 + 1);
            req.caminho = (espaco2 != std::string::npos) ?
                primeira_linha.substr(espaco1 + 1, espaco2 - espaco1 - 1) :
                primeira_linha.substr(espaco1 + 1);
        }
        pos = fim_linha + 2;
    }

    while (pos < raw.size()) {
        fim_linha = raw.find("\r\n", pos);
        if (fim_linha == std::string::npos) break;
        std::string linha = raw.substr(pos, fim_linha - pos);
        pos = fim_linha + 2;
        if (linha.empty()) break;
        size_t separador = linha.find(':');
        if (separador != std::string::npos) {
            std::string chave = trim(linha.substr(0, separador));
            std::string valor = trim(linha.substr(separador + 1));
            std::transform(chave.begin(), chave.end(), chave.begin(), ::tolower);
            req.headers_entrada[chave] = valor;
        }
    }

    if (pos < raw.size()) req.corpo = raw.substr(pos);
    return req;
}

static std::string ler_requisicao_completa(SocketType cliente_socket) {
    std::string raw;
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int bytes_lidos = recv(cliente_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_lidos <= 0) return "";
    raw.append(buffer, bytes_lidos);

    size_t header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        std::string raw_lower = raw;
        std::transform(raw_lower.begin(), raw_lower.end(), raw_lower.begin(), ::tolower);
        size_t cl_pos = raw_lower.find("content-length:");
        if (cl_pos != std::string::npos && cl_pos < header_end) {
            size_t valor_inicio = cl_pos + 15;
            size_t valor_fim = raw_lower.find("\r\n", valor_inicio);
            if (valor_fim != std::string::npos) {
                int content_length = std::atoi(raw.substr(valor_inicio, valor_fim - valor_inicio).c_str());
                size_t corpo_inicio = header_end + 4;
                int falta_ler = content_length - (int)(raw.size() - corpo_inicio);
                while (falta_ler > 0) {
                    memset(buffer, 0, sizeof(buffer));
                    int para_ler = (falta_ler < (int)sizeof(buffer) - 1) ? falta_ler : (int)sizeof(buffer) - 1;
                    bytes_lidos = recv(cliente_socket, buffer, para_ler, 0);
                    if (bytes_lidos <= 0) break;
                    raw.append(buffer, bytes_lidos);
                    falta_ler -= bytes_lidos;
                }
            }
        }
    }
    return raw;
}

static std::string obter_ip_local_impl() {
    std::string ip_result = "127.0.0.1";
    iniciar_wsa();
    SocketType sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock != INVALID_SOCK) {
        struct sockaddr_in serv;
        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = inet_addr("8.8.8.8");
        serv.sin_port = htons(53);
        if (connect(sock, (const struct sockaddr*)&serv, sizeof(serv)) != -1) {
            struct sockaddr_in nome;
            socklen_t namelen = sizeof(nome);
            if (getsockname(sock, (struct sockaddr*)&nome, &namelen) != -1) {
                char buf[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &nome.sin_addr, buf, INET_ADDRSTRLEN))
                    ip_result = std::string(buf);
            }
        }
        CLOSE_SOCKET(sock);
    }
    return ip_result;
}

// =============================================================================
// POOL DE THREADS
// =============================================================================
static void worker_thread_func(Servidor* srv) {
    while (srv->ativo.load()) {
        ConexaoPendente conn;
        {
            std::unique_lock<std::mutex> lock(srv->fila_mutex);
            srv->fila_cv.wait(lock, [srv]() {
                return !srv->fila_conexoes.empty() || !srv->ativo.load();
            });
            if (!srv->ativo.load() && srv->fila_conexoes.empty()) return;
            if (srv->fila_conexoes.empty()) continue;
            conn = srv->fila_conexoes.front();
            srv->fila_conexoes.pop();
        }

        std::string raw = ler_requisicao_completa(conn.cliente_socket);
        if (raw.empty()) { CLOSE_SOCKET(conn.cliente_socket); continue; }

        Requisicao req = parsear_requisicao(raw, conn.cliente_socket);
        req.id = proximo_req_id.fetch_add(1);

        { std::lock_guard<std::mutex> lock(requisicoes_mutex); requisicoes[req.id] = req; }
        { std::lock_guard<std::mutex> lock(srv->req_prontas_mutex); srv->fila_requisicoes_prontas.push(req.id); }
        srv->req_prontas_cv.notify_one();
    }
}

static void acceptor_thread_func(Servidor* srv) {
    while (srv->ativo.load()) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        SocketType cliente_socket = accept(srv->socket, (struct sockaddr*)&cliente_addr, &cliente_len);
        if (cliente_socket == INVALID_SOCK) { if (!srv->ativo.load()) break; continue; }
        ConexaoPendente conn; conn.cliente_socket = cliente_socket; conn.porta = srv->porta;
        { std::lock_guard<std::mutex> lock(srv->fila_mutex); srv->fila_conexoes.push(conn); }
        srv->fila_cv.notify_one();
    }
}

// =============================================================================
// FUNÇÕES EXPORTADAS - SERVIDOR
// =============================================================================

// svs_meu_ip() -> texto
JP_EXPORT int64_t svs_meu_ip() {
    return retorna_str(obter_ip_local_impl());
}

// svs_criar(porta) -> inteiro (porta ou -1)
JP_EXPORT int64_t svs_criar(int64_t porta) {
    int p = (int)porta;
    iniciar_wsa();

    { std::lock_guard<std::mutex> lock(servidores_mutex);
      if (servidores.find(p) != servidores.end()) return (int64_t)p; }

    SocketType srv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_socket == INVALID_SOCK) return -1;

    int opt = 1;
    setsockopt(srv_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in endereco;
    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(p);

    if (bind(srv_socket, (struct sockaddr*)&endereco, sizeof(endereco)) < 0) { CLOSE_SOCKET(srv_socket); return -1; }
    if (listen(srv_socket, 128) < 0) { CLOSE_SOCKET(srv_socket); return -1; }

    Servidor* srv = new Servidor();
    srv->socket = srv_socket;
    srv->porta = p;
    srv->ativo.store(true);
    for (int i = 0; i < NUM_WORKERS; i++) srv->workers.emplace_back(worker_thread_func, srv);
    srv->acceptor_thread = std::thread(acceptor_thread_func, srv);

    { std::lock_guard<std::mutex> lock(servidores_mutex); servidores[p] = srv; }
    printf("[SVS] Servidor iniciado na porta %d com %d workers\n", p, NUM_WORKERS);
    return (int64_t)p;
}

// svs_aguardar(porta) -> inteiro (req_id ou -1)
JP_EXPORT int64_t svs_aguardar(int64_t porta) {
    int p = (int)porta;
    Servidor* srv = nullptr;
    { std::lock_guard<std::mutex> lock(servidores_mutex);
      auto it = servidores.find(p);
      if (it == servidores.end()) return -1;
      srv = it->second; }

    int req_id = -1;
    { std::unique_lock<std::mutex> lock(srv->req_prontas_mutex);
      srv->req_prontas_cv.wait(lock, [srv]() {
          return !srv->fila_requisicoes_prontas.empty() || !srv->ativo.load();
      });
      if (!srv->ativo.load() && srv->fila_requisicoes_prontas.empty()) return -1;
      if (!srv->fila_requisicoes_prontas.empty()) {
          req_id = srv->fila_requisicoes_prontas.front();
          srv->fila_requisicoes_prontas.pop();
      }
    }
    return (int64_t)req_id;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - REQUISIÇÃO
// =============================================================================

// svs_metodo(req_id) -> texto
JP_EXPORT int64_t svs_metodo(int64_t req_id) {
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find((int)req_id);
    if (it == requisicoes.end()) return retorna_str("");
    return retorna_str(it->second.metodo);
}

// svs_caminho(req_id) -> texto
JP_EXPORT int64_t svs_caminho(int64_t req_id) {
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find((int)req_id);
    if (it == requisicoes.end()) return retorna_str("");
    return retorna_str(it->second.caminho);
}

// svs_corpo(req_id) -> texto
JP_EXPORT int64_t svs_corpo(int64_t req_id) {
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find((int)req_id);
    if (it == requisicoes.end()) return retorna_str("");
    return retorna_str(it->second.corpo);
}

// svs_fechar(req_id) -> inteiro
JP_EXPORT int64_t svs_fechar(int64_t req_id) {
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find((int)req_id);
    if (it != requisicoes.end()) {
        CLOSE_SOCKET(it->second.cliente_socket);
        requisicoes.erase(it);
    }
    return 1;
}

// svs_parar(porta) -> inteiro
JP_EXPORT int64_t svs_parar(int64_t porta) {
    int p = (int)porta;
    Servidor* srv = nullptr;
    { std::lock_guard<std::mutex> lock(servidores_mutex);
      auto it = servidores.find(p);
      if (it == servidores.end()) return 0;
      srv = it->second;
      servidores.erase(it); }

    srv->ativo.store(false);
    CLOSE_SOCKET(srv->socket);
    srv->fila_cv.notify_all();
    srv->req_prontas_cv.notify_all();
    if (srv->acceptor_thread.joinable()) srv->acceptor_thread.join();
    for (auto& w : srv->workers) if (w.joinable()) w.join();
    delete srv;
    printf("[SVS] Servidor na porta %d parado\n", p);
    return 1;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - RESPOSTA
// =============================================================================

// svs_responder(req_id, codigo, conteudo) -> inteiro
JP_EXPORT int64_t svs_responder(int64_t req_id, int64_t codigo, int64_t conteudo) {
    std::string corpo((const char*)conteudo);
    SocketType cliente_socket;
    std::vector<std::string> headers_saida;

    { std::lock_guard<std::mutex> lock(requisicoes_mutex);
      auto it = requisicoes.find((int)req_id);
      if (it == requisicoes.end()) return 0;
      cliente_socket = it->second.cliente_socket;
      headers_saida = it->second.headers_saida;
      requisicoes.erase(it); }

    int cod = (int)codigo;
    std::string status = (cod == 200) ? "OK" : (cod == 404) ? "Not Found" : (cod == 500) ? "Internal Server Error" : "Unknown";
    std::string resposta = "HTTP/1.1 " + std::to_string(cod) + " " + status + "\r\n";
    resposta += "Content-Type: text/plain; charset=utf-8\r\nContent-Length: " + std::to_string(corpo.size()) + "\r\nConnection: close\r\n";
    for (const auto& h : headers_saida) resposta += h + "\r\n";
    resposta += "\r\n" + corpo;
    send(cliente_socket, resposta.c_str(), (int)resposta.size(), 0);
    CLOSE_SOCKET(cliente_socket);
    return 1;
}

// svs_responder_html(req_id, codigo, conteudo) -> inteiro
JP_EXPORT int64_t svs_responder_html(int64_t req_id, int64_t codigo, int64_t conteudo) {
    std::string corpo((const char*)conteudo);
    SocketType cliente_socket;
    std::vector<std::string> headers_saida;

    { std::lock_guard<std::mutex> lock(requisicoes_mutex);
      auto it = requisicoes.find((int)req_id);
      if (it == requisicoes.end()) return 0;
      cliente_socket = it->second.cliente_socket;
      headers_saida = it->second.headers_saida;
      requisicoes.erase(it); }

    int cod = (int)codigo;
    std::string status = (cod == 200) ? "OK" : (cod == 404) ? "Not Found" : (cod == 500) ? "Internal Server Error" : "Unknown";
    std::string resposta = "HTTP/1.1 " + std::to_string(cod) + " " + status + "\r\n";
    resposta += "Content-Type: text/html; charset=utf-8\r\nContent-Length: " + std::to_string(corpo.size()) + "\r\nConnection: close\r\n";
    for (const auto& h : headers_saida) resposta += h + "\r\n";
    resposta += "\r\n" + corpo;
    send(cliente_socket, resposta.c_str(), (int)resposta.size(), 0);
    CLOSE_SOCKET(cliente_socket);
    return 1;
}

// svs_arquivo(req_id, caminho) -> inteiro
JP_EXPORT int64_t svs_arquivo(int64_t req_id, int64_t caminho) {
    std::string path((const char*)caminho);
    SocketType cliente_socket;
    std::vector<std::string> headers_saida;

    { std::lock_guard<std::mutex> lock(requisicoes_mutex);
      auto it = requisicoes.find((int)req_id);
      if (it == requisicoes.end()) return 0;
      cliente_socket = it->second.cliente_socket;
      headers_saida = it->second.headers_saida;
      requisicoes.erase(it); }

    enviar_arquivo_impl(cliente_socket, path, headers_saida);
    CLOSE_SOCKET(cliente_socket);
    return 1;
}

// svs_pasta(porta, rota, diretorio) -> inteiro
JP_EXPORT int64_t svs_pasta(int64_t porta, int64_t rota, int64_t diretorio) {
    std::string r((const char*)rota);
    std::string d((const char*)diretorio);
    if (r.empty() || r[0] != '/') r = "/" + r;
    if (!d.empty() && d.back() != '/' && d.back() != '\\') d += "/";
    std::lock_guard<std::mutex> lock(pastas_mutex);
    pastas_mapeadas[(int)porta][r] = d;
    return 1;
}

// svs_servir_estatico(req_id, porta) -> inteiro
JP_EXPORT int64_t svs_servir_estatico(int64_t req_id, int64_t porta) {
    SocketType cliente_socket;
    std::string caminho_req;
    std::vector<std::string> headers_saida;

    { std::lock_guard<std::mutex> lock(requisicoes_mutex);
      auto it = requisicoes.find((int)req_id);
      if (it == requisicoes.end()) return 0;
      cliente_socket = it->second.cliente_socket;
      caminho_req = it->second.caminho;
      headers_saida = it->second.headers_saida; }

    std::string diretorio_encontrado, resto;
    bool encontrado = false;

    { std::lock_guard<std::mutex> lock(pastas_mutex);
      auto pasta_it = pastas_mapeadas.find((int)porta);
      if (pasta_it == pastas_mapeadas.end()) return 0;
      for (const auto& par : pasta_it->second) {
          if (caminho_req.find(par.first) == 0) {
              resto = caminho_req.substr(par.first.size());
              if (!resto.empty() && resto[0] == '/') resto = resto.substr(1);
              diretorio_encontrado = par.second;
              encontrado = true;
              break;
          }
      }
    }

    if (!encontrado) return 0;

    { std::lock_guard<std::mutex> lock(requisicoes_mutex); requisicoes.erase((int)req_id); }
    enviar_arquivo_impl(cliente_socket, diretorio_encontrado + resto, headers_saida);
    CLOSE_SOCKET(cliente_socket);
    return 1;
}

// svs_ler_texto(caminho) -> texto
JP_EXPORT int64_t svs_ler_texto(int64_t caminho) {
    std::string path((const char*)caminho);
    std::vector<char> dados;
    if (ler_arquivo(path, dados)) {
        dados.push_back('\0');
        return retorna_str(dados.data());
    }
    return retorna_str("");
}

// =============================================================================
// FUNÇÕES EXPORTADAS - COOKIES E SESSÃO
// =============================================================================

// svs_obter_cookie(req_id, nome) -> texto
JP_EXPORT int64_t svs_obter_cookie(int64_t req_id, int64_t nome) {
    std::string nome_cookie((const char*)nome);
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find((int)req_id);
    if (it == requisicoes.end()) return retorna_str("");

    auto hit = it->second.headers_entrada.find("cookie");
    if (hit == it->second.headers_entrada.end()) return retorna_str("");

    std::string cookies = hit->second;
    size_t pos = cookies.find(nome_cookie + "=");
    if (pos == std::string::npos) {
        pos = cookies.find(" " + nome_cookie + "=");
        if (pos != std::string::npos) pos++;
    }
    if (pos == std::string::npos) return retorna_str("");

    pos += nome_cookie.size() + 1;
    size_t fim = cookies.find(';', pos);
    if (fim == std::string::npos) fim = cookies.size();
    return retorna_str(cookies.substr(pos, fim - pos));
}

// svs_definir_cookie(req_id, nome, valor) -> inteiro
JP_EXPORT int64_t svs_definir_cookie(int64_t req_id, int64_t nome, int64_t valor) {
    std::string n((const char*)nome);
    std::string v((const char*)valor);
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find((int)req_id);
    if (it == requisicoes.end()) return 0;
    it->second.headers_saida.push_back("Set-Cookie: " + n + "=" + v + "; Path=/; HttpOnly");
    return 1;
}

// svs_sessao_iniciar(req_id) -> texto (session id)
JP_EXPORT int64_t svs_sessao_iniciar(int64_t req_id) {
    std::string id = gerar_sessao_id();

    { std::lock_guard<std::mutex> lock(sessoes_mutex);
      Sessao s; s.id = id; s.ultima_atividade = time(NULL);
      sessoes_ativas[id] = s; }

    { std::lock_guard<std::mutex> lock(requisicoes_mutex);
      auto it = requisicoes.find((int)req_id);
      if (it == requisicoes.end()) return retorna_str("");
      it->second.headers_saida.push_back("Set-Cookie: JPSESSID=" + id + "; Path=/; HttpOnly"); }

    return retorna_str(id);
}

// svs_sessao_id(req_id) -> texto
JP_EXPORT int64_t svs_sessao_id(int64_t req_id) {
    // Reutiliza lógica de obter cookie JPSESSID
    return svs_obter_cookie(req_id, (int64_t)"JPSESSID");
}

// svs_sessao_set(sessao_id, chave, valor) -> inteiro
JP_EXPORT int64_t svs_sessao_set(int64_t sessao_id, int64_t chave, int64_t valor) {
    std::string sid((const char*)sessao_id);
    std::string k((const char*)chave);
    std::string v((const char*)valor);
    std::lock_guard<std::mutex> lock(sessoes_mutex);
    if (sessoes_ativas.find(sid) != sessoes_ativas.end()) {
        sessoes_ativas[sid].dados[k] = v;
        sessoes_ativas[sid].ultima_atividade = time(NULL);
        return 1;
    }
    return 0;
}

// svs_sessao_get(sessao_id, chave) -> texto
JP_EXPORT int64_t svs_sessao_get(int64_t sessao_id, int64_t chave) {
    std::string sid((const char*)sessao_id);
    std::string k((const char*)chave);
    std::lock_guard<std::mutex> lock(sessoes_mutex);
    if (sessoes_ativas.find(sid) != sessoes_ativas.end()) {
        sessoes_ativas[sid].ultima_atividade = time(NULL);
        return retorna_str(sessoes_ativas[sid].dados[k]);
    }
    return retorna_str("");
}