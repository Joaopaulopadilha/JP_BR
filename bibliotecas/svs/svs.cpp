// svs.cpp
// Biblioteca nativa SVS (Simple Virtual Server) para JPLang
// Versão com POOL DE THREADS para conexões simultâneas
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
#include <iostream>
#include <functional>

// =============================================================================
// DETECÇÃO DE PLATAFORMA
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
// TIPOS C PUROS (INTERFACE JPLang)
// =============================================================================
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
    } valor;
} JPValor;

// =============================================================================
// HELPERS GERAIS
// =============================================================================
static JPValor jp_string(const std::string& s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    r.valor.texto = (char*)malloc(s.size() + 1);
    if (r.valor.texto) {
        memcpy(r.valor.texto, s.c_str(), s.size() + 1);
    }
    return r;
}

static JPValor jp_int(int64_t v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

static std::string jp_get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) {
        return std::string(args[idx].valor.texto);
    }
    return "";
}

// Remove espaços do início e fim da string
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Gera um ID aleatório para a sessão (thread-safe)
static std::mutex rand_mutex;
static std::string gerar_sessao_id() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string s;
    s.reserve(32);
    
    std::lock_guard<std::mutex> lock(rand_mutex);
    static bool seeded = false;
    if(!seeded) { 
        srand((unsigned)time(NULL)); 
        seeded = true; 
    }
    
    for (int i = 0; i < 32; ++i) 
        s += alphanum[rand() % (sizeof(alphanum) - 1)];
    
    return s;
}

// =============================================================================
// ESTRUTURAS DE DADOS
// =============================================================================

// Estrutura para manter o estado da sessão no servidor
struct Sessao {
    std::string id;
    std::map<std::string, std::string> dados;
    time_t ultima_atividade;
};

// Estrutura da Requisição HTTP
struct Requisicao {
    int id;
    std::string metodo;
    std::string caminho;
    std::string corpo;
    std::string raw;
    
    // Novos campos para Headers e Cookies
    std::map<std::string, std::string> headers_entrada; // Headers recebidos (lowercase)
    std::vector<std::string> headers_saida;             // Headers a serem enviados (Set-Cookie etc)
    
    SocketType cliente_socket;
};

// Estrutura de conexão pendente (socket aceito, aguardando processamento)
struct ConexaoPendente {
    SocketType cliente_socket;
    int porta;
};

// Estrutura do Servidor
struct Servidor {
    SocketType socket;
    int porta;
    std::atomic<bool> ativo;
    
    // Pool de threads
    std::vector<std::thread> workers;
    std::thread acceptor_thread;
    
    // Fila de conexões pendentes
    std::queue<ConexaoPendente> fila_conexoes;
    std::mutex fila_mutex;
    std::condition_variable fila_cv;
    
    // Fila de requisições prontas (parseadas, aguardando JPLang processar)
    std::queue<int> fila_requisicoes_prontas;
    std::mutex req_prontas_mutex;
    std::condition_variable req_prontas_cv;
    
    Servidor() : ativo(false) {}
};

// =============================================================================
// ESTADO GLOBAL (PROTEGIDO POR MUTEX)
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

// Mapeamento de pastas: porta -> (rota -> diretorio)
static std::unordered_map<int, std::unordered_map<std::string, std::string>> pastas_mapeadas;
static std::mutex pastas_mutex;

// Número de workers no pool (ajustável)
static const int NUM_WORKERS = 8;

// =============================================================================
// FUNÇÕES DE ARQUIVO
// =============================================================================
static std::string obter_mime_type(const std::string& caminho) {
    size_t pos = caminho.rfind('.');
    if (pos == std::string::npos) return "application/octet-stream";
    
    std::string ext = caminho.substr(pos);
    
    // Converte para minúsculo
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
    if (!arquivo.read(conteudo.data(), tamanho)) {
        return false;
    }
    
    return true;
}

// Atualizado para aceitar headers extras (Cookies)
static void enviar_arquivo_impl(SocketType cliente_socket, const std::string& caminho, const std::vector<std::string>& headers_extras) {
    std::vector<char> conteudo;
    
    if (!ler_arquivo(caminho, conteudo)) {
        // Arquivo não encontrado
        std::string resposta = "HTTP/1.1 404 Not Found\r\n";
        resposta += "Content-Type: text/plain; charset=utf-8\r\n";
        resposta += "Content-Length: 18\r\n";
        resposta += "Connection: close\r\n";
        resposta += "\r\n";
        resposta += "Arquivo nao existe";
        send(cliente_socket, resposta.c_str(), (int)resposta.size(), 0);
        return;
    }
    
    std::string mime = obter_mime_type(caminho);
    
    // Monta cabeçalho
    std::string cabecalho = "HTTP/1.1 200 OK\r\n";
    cabecalho += "Content-Type: " + mime + "\r\n";
    cabecalho += "Content-Length: " + std::to_string(conteudo.size()) + "\r\n";
    cabecalho += "Connection: close\r\n";
    
    // Injeta headers extras (Cookies/Sessão)
    for (const auto& h : headers_extras) {
        cabecalho += h + "\r\n";
    }
    
    cabecalho += "\r\n";
    
    // Envia cabeçalho
    send(cliente_socket, cabecalho.c_str(), (int)cabecalho.size(), 0);
    
    // Envia conteúdo
    if (!conteudo.empty()) {
        send(cliente_socket, conteudo.data(), (int)conteudo.size(), 0);
    }
}

// =============================================================================
// FUNÇÕES AUXILIARES DE REDE
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

// Parser atualizado para ler headers
static Requisicao parsear_requisicao(const std::string& raw, SocketType cliente) {
    Requisicao req;
    req.raw = raw;
    req.cliente_socket = cliente;
    req.metodo = "";
    req.caminho = "";
    req.corpo = "";
    
    size_t pos = 0;
    size_t fim_linha = raw.find("\r\n");
    
    // 1. Linha de Requisição (GET /caminho HTTP/1.1)
    if (fim_linha != std::string::npos) {
        std::string primeira_linha = raw.substr(0, fim_linha);
        
        size_t espaco1 = primeira_linha.find(' ');
        if (espaco1 != std::string::npos) {
            req.metodo = primeira_linha.substr(0, espaco1);
            
            size_t espaco2 = primeira_linha.find(' ', espaco1 + 1);
            if (espaco2 != std::string::npos) {
                req.caminho = primeira_linha.substr(espaco1 + 1, espaco2 - espaco1 - 1);
            } else {
                req.caminho = primeira_linha.substr(espaco1 + 1);
            }
        }
        pos = fim_linha + 2;
    }
    
    // 2. Headers
    while (pos < raw.size()) {
        fim_linha = raw.find("\r\n", pos);
        if (fim_linha == std::string::npos) break;
        
        std::string linha = raw.substr(pos, fim_linha - pos);
        pos = fim_linha + 2; // Pula \r\n
        
        if (linha.empty()) break; // Fim dos headers
        
        size_t separador = linha.find(':');
        if (separador != std::string::npos) {
            std::string chave = trim(linha.substr(0, separador));
            std::string valor = trim(linha.substr(separador + 1));
            
            // Converte chave para minúsculo para facilitar busca
            std::transform(chave.begin(), chave.end(), chave.begin(), ::tolower);
            req.headers_entrada[chave] = valor;
        }
    }
    
    // 3. Corpo
    if (pos < raw.size()) {
        req.corpo = raw.substr(pos);
    }
    
    return req;
}

// Lê dados completos da requisição HTTP
static std::string ler_requisicao_completa(SocketType cliente_socket) {
    std::string raw;
    char buffer[4096];
    int bytes_lidos;
    
    // Primeira leitura
    memset(buffer, 0, sizeof(buffer));
    bytes_lidos = recv(cliente_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_lidos <= 0) {
        return "";
    }
    
    raw.append(buffer, bytes_lidos);
    
    // Verifica se tem Content-Length e se precisa ler mais
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
                int corpo_lido = (int)(raw.size() - corpo_inicio);
                int falta_ler = content_length - corpo_lido;
                
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
                char buffer[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &nome.sin_addr, buffer, INET_ADDRSTRLEN)) {
                    ip_result = std::string(buffer);
                }
            }
        }
        CLOSE_SOCKET(sock);
    }
    return ip_result;
}

// =============================================================================
// POOL DE THREADS - FUNÇÕES
// =============================================================================

// Worker thread: processa conexões da fila
static void worker_thread_func(Servidor* srv) {
    while (srv->ativo.load()) {
        ConexaoPendente conn;
        
        // Aguarda uma conexão na fila
        {
            std::unique_lock<std::mutex> lock(srv->fila_mutex);
            srv->fila_cv.wait(lock, [srv]() {
                return !srv->fila_conexoes.empty() || !srv->ativo.load();
            });
            
            if (!srv->ativo.load() && srv->fila_conexoes.empty()) {
                return; // Servidor parou
            }
            
            if (srv->fila_conexoes.empty()) {
                continue;
            }
            
            conn = srv->fila_conexoes.front();
            srv->fila_conexoes.pop();
        }
        
        // Lê e parseia a requisição
        std::string raw = ler_requisicao_completa(conn.cliente_socket);
        
        if (raw.empty()) {
            CLOSE_SOCKET(conn.cliente_socket);
            continue;
        }
        
        // Parseia a requisição
        Requisicao req = parsear_requisicao(raw, conn.cliente_socket);
        req.id = proximo_req_id.fetch_add(1);
        
        // Armazena requisição (thread-safe)
        {
            std::lock_guard<std::mutex> lock(requisicoes_mutex);
            requisicoes[req.id] = req;
        }
        
        // Coloca na fila de requisições prontas para JPLang
        {
            std::lock_guard<std::mutex> lock(srv->req_prontas_mutex);
            srv->fila_requisicoes_prontas.push(req.id);
        }
        srv->req_prontas_cv.notify_one();
    }
}

// Acceptor thread: aceita conexões e coloca na fila
static void acceptor_thread_func(Servidor* srv) {
    while (srv->ativo.load()) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        
        SocketType cliente_socket = accept(srv->socket, (struct sockaddr*)&cliente_addr, &cliente_len);
        
        if (cliente_socket == INVALID_SOCK) {
            if (!srv->ativo.load()) break; // Servidor foi parado
            continue;
        }
        
        // Coloca na fila para os workers processarem
        ConexaoPendente conn;
        conn.cliente_socket = cliente_socket;
        conn.porta = srv->porta;
        
        {
            std::lock_guard<std::mutex> lock(srv->fila_mutex);
            srv->fila_conexoes.push(conn);
        }
        srv->fila_cv.notify_one();
    }
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// Retorna o IP local da máquina
JP_EXPORT JPValor jp_svs_meu_ip(JPValor* args, int numArgs) {
    return jp_string(obter_ip_local_impl());
}

// Cria um servidor na porta especificada
// Retorna: porta se sucesso, -1 se erro
JP_EXPORT JPValor jp_svs_criar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(-1);
    int porta = jp_get_int(args, 0);
    
    iniciar_wsa();
    
    // Se já existe servidor nessa porta, retorna
    {
        std::lock_guard<std::mutex> lock(servidores_mutex);
        if (servidores.find(porta) != servidores.end()) {
            return jp_int(porta);
        }
    }
    
    // Cria socket
    SocketType srv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_socket == INVALID_SOCK) {
        return jp_int(-1);
    }
    
    // Permite reusar a porta
    int opt = 1;
    setsockopt(srv_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    // Configura endereço
    struct sockaddr_in endereco;
    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(porta);
    
    // Bind
    if (bind(srv_socket, (struct sockaddr*)&endereco, sizeof(endereco)) < 0) {
        CLOSE_SOCKET(srv_socket);
        return jp_int(-1);
    }
    
    // Listen com backlog maior para suportar mais conexões
    if (listen(srv_socket, 128) < 0) {
        CLOSE_SOCKET(srv_socket);
        return jp_int(-1);
    }
    
    // Cria servidor
    Servidor* srv = new Servidor();
    srv->socket = srv_socket;
    srv->porta = porta;
    srv->ativo.store(true);
    
    // Inicia workers do pool
    for (int i = 0; i < NUM_WORKERS; i++) {
        srv->workers.emplace_back(worker_thread_func, srv);
    }
    
    // Inicia thread acceptor
    srv->acceptor_thread = std::thread(acceptor_thread_func, srv);
    
    // Salva servidor
    {
        std::lock_guard<std::mutex> lock(servidores_mutex);
        servidores[porta] = srv;
    }
    
    printf("[SVS] Servidor iniciado na porta %d com %d workers\n", porta, NUM_WORKERS);
    
    return jp_int(porta);
}

// Aguarda uma requisição chegar (bloqueante)
// Retorna: ID da requisição
JP_EXPORT JPValor jp_svs_aguardar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(-1);
    int porta = jp_get_int(args, 0);
    
    Servidor* srv = nullptr;
    {
        std::lock_guard<std::mutex> lock(servidores_mutex);
        auto it = servidores.find(porta);
        if (it == servidores.end()) {
            return jp_int(-1);
        }
        srv = it->second;
    }
    
    // Aguarda uma requisição pronta na fila
    int req_id = -1;
    {
        std::unique_lock<std::mutex> lock(srv->req_prontas_mutex);
        srv->req_prontas_cv.wait(lock, [srv]() {
            return !srv->fila_requisicoes_prontas.empty() || !srv->ativo.load();
        });
        
        if (!srv->ativo.load() && srv->fila_requisicoes_prontas.empty()) {
            return jp_int(-1);
        }
        
        if (!srv->fila_requisicoes_prontas.empty()) {
            req_id = srv->fila_requisicoes_prontas.front();
            srv->fila_requisicoes_prontas.pop();
        }
    }
    
    return jp_int(req_id);
}

// Retorna o método HTTP da requisição (GET, POST, etc)
JP_EXPORT JPValor jp_svs_metodo(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    int req_id = jp_get_int(args, 0);
    
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find(req_id);
    if (it == requisicoes.end()) {
        return jp_string("");
    }
    
    return jp_string(it->second.metodo);
}

// Retorna o caminho da requisição (/, /pagina, etc)
JP_EXPORT JPValor jp_svs_caminho(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    int req_id = jp_get_int(args, 0);
    
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find(req_id);
    if (it == requisicoes.end()) {
        return jp_string("");
    }
    
    return jp_string(it->second.caminho);
}

// Retorna o corpo da requisição (dados enviados via POST, PUT, etc)
JP_EXPORT JPValor jp_svs_corpo(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    int req_id = jp_get_int(args, 0);
    
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find(req_id);
    if (it == requisicoes.end()) {
        return jp_string("");
    }
    
    return jp_string(it->second.corpo);
}

// Fecha a conexão de uma requisição (sem responder)
JP_EXPORT JPValor jp_svs_fechar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    int req_id = jp_get_int(args, 0);
    
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find(req_id);
    if (it != requisicoes.end()) {
        CLOSE_SOCKET(it->second.cliente_socket);
        requisicoes.erase(it);
    }
    
    return jp_int(1);
}

// Para o servidor na porta especificada
JP_EXPORT JPValor jp_svs_parar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    int porta = jp_get_int(args, 0);
    
    Servidor* srv = nullptr;
    {
        std::lock_guard<std::mutex> lock(servidores_mutex);
        auto it = servidores.find(porta);
        if (it == servidores.end()) {
            return jp_int(0);
        }
        srv = it->second;
        servidores.erase(it);
    }
    
    // Para o servidor
    srv->ativo.store(false);
    
    // Fecha o socket para desbloquear accept()
    CLOSE_SOCKET(srv->socket);
    
    // Notifica workers para sair
    srv->fila_cv.notify_all();
    srv->req_prontas_cv.notify_all();
    
    // Aguarda acceptor terminar
    if (srv->acceptor_thread.joinable()) {
        srv->acceptor_thread.join();
    }
    
    // Aguarda workers terminarem
    for (auto& w : srv->workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    
    delete srv;
    
    printf("[SVS] Servidor na porta %d parado\n", porta);
    
    return jp_int(1);
}

// =============================================================================
// FUNÇÕES DE COOKIES E SESSÃO
// =============================================================================

// Obtém valor de um cookie da requisição
JP_EXPORT JPValor jp_svs_obter_cookie(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_string("");
    int req_id = jp_get_int(args, 0);
    std::string nome_cookie = jp_get_string(args, 1);
    
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find(req_id);
    if (it == requisicoes.end()) return jp_string("");
    
    // Procura o header 'cookie' (já está em minúsculo pelo parser)
    auto hit = it->second.headers_entrada.find("cookie");
    if (hit == it->second.headers_entrada.end()) return jp_string("");
    
    std::string cookies = hit->second;
    
    // Busca simples: "nome="
    size_t pos = cookies.find(nome_cookie + "=");
    if (pos == std::string::npos) {
        // Tenta achar com espaço antes "; nome="
        pos = cookies.find(" " + nome_cookie + "=");
        if (pos != std::string::npos) pos++; // Avança o espaço
    }
    
    if (pos == std::string::npos) return jp_string("");
    
    pos += nome_cookie.size() + 1; // Pula nome e =
    size_t fim = cookies.find(';', pos);
    if (fim == std::string::npos) fim = cookies.size();
    
    return jp_string(cookies.substr(pos, fim - pos));
}

// Define um cookie para ser enviado na resposta
JP_EXPORT JPValor jp_svs_definir_cookie(JPValor* args, int numArgs) {
    if (numArgs < 3) return jp_int(0);
    int req_id = jp_get_int(args, 0);
    std::string nome = jp_get_string(args, 1);
    std::string valor = jp_get_string(args, 2);
    
    std::lock_guard<std::mutex> lock(requisicoes_mutex);
    auto it = requisicoes.find(req_id);
    if (it == requisicoes.end()) return jp_int(0);
    
    std::string header = "Set-Cookie: " + nome + "=" + valor + "; Path=/; HttpOnly";
    it->second.headers_saida.push_back(header);
    
    return jp_int(1);
}

// Inicia uma sessão (Gera ID e define cookie JPSESSID)
JP_EXPORT JPValor jp_svs_sessao_iniciar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    int req_id = jp_get_int(args, 0);
    
    std::string id = gerar_sessao_id();
    
    // Cria sessão
    {
        std::lock_guard<std::mutex> lock(sessoes_mutex);
        Sessao s;
        s.id = id;
        s.ultima_atividade = time(NULL);
        sessoes_ativas[id] = s;
    }
    
    // Define cookie na requisição
    {
        std::lock_guard<std::mutex> lock(requisicoes_mutex);
        auto it = requisicoes.find(req_id);
        if (it == requisicoes.end()) return jp_string("");
        
        std::string header = "Set-Cookie: JPSESSID=" + id + "; Path=/; HttpOnly";
        it->second.headers_saida.push_back(header);
    }
    
    return jp_string(id);
}

// Obtém o ID da sessão atual
JP_EXPORT JPValor jp_svs_sessao_id(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    // Reutiliza a lógica de obter cookie
    JPValor args_temp[2];
    args_temp[0] = args[0];
    args_temp[1] = jp_string("JPSESSID");
    JPValor res = jp_svs_obter_cookie(args_temp, 2);
    free(args_temp[1].valor.texto);
    return res;
}

// Define valor na sessão
JP_EXPORT JPValor jp_svs_sessao_set(JPValor* args, int numArgs) {
    if (numArgs < 3) return jp_int(0);
    std::string sess_id = jp_get_string(args, 0);
    std::string chave = jp_get_string(args, 1);
    std::string valor = jp_get_string(args, 2);
    
    std::lock_guard<std::mutex> lock(sessoes_mutex);
    if (sessoes_ativas.find(sess_id) != sessoes_ativas.end()) {
        sessoes_ativas[sess_id].dados[chave] = valor;
        sessoes_ativas[sess_id].ultima_atividade = time(NULL);
        return jp_int(1);
    }
    return jp_int(0);
}

// Obtém valor da sessão
JP_EXPORT JPValor jp_svs_sessao_get(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_string("");
    std::string sess_id = jp_get_string(args, 0);
    std::string chave = jp_get_string(args, 1);
    
    std::lock_guard<std::mutex> lock(sessoes_mutex);
    if (sessoes_ativas.find(sess_id) != sessoes_ativas.end()) {
        sessoes_ativas[sess_id].ultima_atividade = time(NULL);
        return jp_string(sessoes_ativas[sess_id].dados[chave]);
    }
    return jp_string("");
}

// =============================================================================
// FUNÇÕES DE RESPOSTA
// =============================================================================

// Envia resposta HTTP com texto simples
JP_EXPORT JPValor jp_svs_responder(JPValor* args, int numArgs) {
    if (numArgs < 3) return jp_int(0);
    int req_id = jp_get_int(args, 0);
    int codigo = jp_get_int(args, 1);
    std::string conteudo = jp_get_string(args, 2);
    
    SocketType cliente_socket;
    std::vector<std::string> headers_saida;
    
    {
        std::lock_guard<std::mutex> lock(requisicoes_mutex);
        auto it = requisicoes.find(req_id);
        if (it == requisicoes.end()) {
            return jp_int(0);
        }
        cliente_socket = it->second.cliente_socket;
        headers_saida = it->second.headers_saida;
        requisicoes.erase(it);
    }
    
    // Monta resposta HTTP
    std::string status_texto = (codigo == 200) ? "OK" : 
                               (codigo == 404) ? "Not Found" :
                               (codigo == 500) ? "Internal Server Error" : "Unknown";
    
    std::string resposta = "HTTP/1.1 " + std::to_string(codigo) + " " + status_texto + "\r\n";
    resposta += "Content-Type: text/plain; charset=utf-8\r\n";
    resposta += "Content-Length: " + std::to_string(conteudo.size()) + "\r\n";
    resposta += "Connection: close\r\n";
    
    // Injeção de Cookies/Headers Extras
    for (const auto& h : headers_saida) {
        resposta += h + "\r\n";
    }
    
    resposta += "\r\n";
    resposta += conteudo;
    
    // Envia
    send(cliente_socket, resposta.c_str(), (int)resposta.size(), 0);
    
    // Fecha conexão
    CLOSE_SOCKET(cliente_socket);
    
    return jp_int(1);
}

// Envia resposta HTTP com HTML
JP_EXPORT JPValor jp_svs_responder_html(JPValor* args, int numArgs) {
    if (numArgs < 3) return jp_int(0);
    int req_id = jp_get_int(args, 0);
    int codigo = jp_get_int(args, 1);
    std::string conteudo = jp_get_string(args, 2);
    
    SocketType cliente_socket;
    std::vector<std::string> headers_saida;
    
    {
        std::lock_guard<std::mutex> lock(requisicoes_mutex);
        auto it = requisicoes.find(req_id);
        if (it == requisicoes.end()) {
            return jp_int(0);
        }
        cliente_socket = it->second.cliente_socket;
        headers_saida = it->second.headers_saida;
        requisicoes.erase(it);
    }
    
    // Monta resposta HTTP
    std::string status_texto = (codigo == 200) ? "OK" : 
                               (codigo == 404) ? "Not Found" :
                               (codigo == 500) ? "Internal Server Error" : "Unknown";
    
    std::string resposta = "HTTP/1.1 " + std::to_string(codigo) + " " + status_texto + "\r\n";
    resposta += "Content-Type: text/html; charset=utf-8\r\n";
    resposta += "Content-Length: " + std::to_string(conteudo.size()) + "\r\n";
    resposta += "Connection: close\r\n";
    
    // Injeção de Cookies/Headers Extras
    for (const auto& h : headers_saida) {
        resposta += h + "\r\n";
    }
    
    resposta += "\r\n";
    resposta += conteudo;
    
    // Envia
    send(cliente_socket, resposta.c_str(), (int)resposta.size(), 0);
    
    // Fecha conexão
    CLOSE_SOCKET(cliente_socket);
    
    return jp_int(1);
}

// Serve um arquivo específico
JP_EXPORT JPValor jp_svs_arquivo(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_int(0);
    int req_id = jp_get_int(args, 0);
    std::string caminho = jp_get_string(args, 1);
    
    SocketType cliente_socket;
    std::vector<std::string> headers_saida;
    
    {
        std::lock_guard<std::mutex> lock(requisicoes_mutex);
        auto it = requisicoes.find(req_id);
        if (it == requisicoes.end()) {
            return jp_int(0);
        }
        cliente_socket = it->second.cliente_socket;
        headers_saida = it->second.headers_saida;
        requisicoes.erase(it);
    }
    
    enviar_arquivo_impl(cliente_socket, caminho, headers_saida);
    
    // Fecha conexão
    CLOSE_SOCKET(cliente_socket);
    
    return jp_int(1);
}

// Mapeia uma rota para uma pasta de arquivos estáticos
JP_EXPORT JPValor jp_svs_pasta(JPValor* args, int numArgs) {
    if (numArgs < 3) return jp_int(0);
    int porta = jp_get_int(args, 0);
    std::string rota = jp_get_string(args, 1);
    std::string diretorio = jp_get_string(args, 2);
    
    // Garante que rota começa com /
    if (rota.empty() || rota[0] != '/') {
        rota = "/" + rota;
    }
    
    // Garante que diretório termina com /
    if (!diretorio.empty() && diretorio.back() != '/' && diretorio.back() != '\\') {
        diretorio += "/";
    }
    
    std::lock_guard<std::mutex> lock(pastas_mutex);
    pastas_mapeadas[porta][rota] = diretorio;
    
    return jp_int(1);
}

// Tenta servir arquivo de uma pasta mapeada
JP_EXPORT JPValor jp_svs_servir_estatico(JPValor* args, int numArgs) {
    if (numArgs < 2) return jp_int(0);
    int req_id = jp_get_int(args, 0);
    int porta = jp_get_int(args, 1);
    
    SocketType cliente_socket;
    std::string caminho_req;
    std::vector<std::string> headers_saida;
    
    {
        std::lock_guard<std::mutex> lock(requisicoes_mutex);
        auto req_it = requisicoes.find(req_id);
        if (req_it == requisicoes.end()) {
            return jp_int(0);
        }
        cliente_socket = req_it->second.cliente_socket;
        caminho_req = req_it->second.caminho;
        headers_saida = req_it->second.headers_saida;
    }
    
    std::string diretorio_encontrado;
    std::string resto;
    bool encontrado = false;
    
    {
        std::lock_guard<std::mutex> lock(pastas_mutex);
        auto pasta_it = pastas_mapeadas.find(porta);
        if (pasta_it == pastas_mapeadas.end()) {
            return jp_int(0);
        }
        
        // Procura um mapeamento que corresponda
        for (const auto& par : pasta_it->second) {
            const std::string& rota = par.first;
            const std::string& diretorio = par.second;
            
            // Verifica se o caminho começa com a rota
            if (caminho_req.find(rota) == 0) {
                // Extrai o resto do caminho após a rota
                resto = caminho_req.substr(rota.size());
                
                // Remove / inicial se houver
                if (!resto.empty() && resto[0] == '/') {
                    resto = resto.substr(1);
                }
                
                diretorio_encontrado = diretorio;
                encontrado = true;
                break;
            }
        }
    }
    
    if (!encontrado) {
        return jp_int(0);
    }
    
    // Monta caminho completo do arquivo
    std::string caminho_arquivo = diretorio_encontrado + resto;
    
    // Remove da lista de requisições
    {
        std::lock_guard<std::mutex> lock(requisicoes_mutex);
        requisicoes.erase(req_id);
    }
    
    // Envia arquivo
    enviar_arquivo_impl(cliente_socket, caminho_arquivo, headers_saida);
    
    // Fecha conexão
    CLOSE_SOCKET(cliente_socket);
    
    return jp_int(1);
}

// =============================================================================
// FUNÇÃO: LER TEXTO
// =============================================================================
JP_EXPORT JPValor jp_svs_ler_texto(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_string("");
    
    std::string caminho = jp_get_string(args, 0);
    
    std::vector<char> dados;
    if (ler_arquivo(caminho, dados)) {
        // Adiciona terminador nulo para garantir que é string válida
        dados.push_back('\0'); 
        return jp_string(dados.data());
    }
    
    // Retorna string vazia em caso de erro
    return jp_string("");
}