// jp_ollama.cpp
// Biblioteca nativa Ollama/LLM para JPLang - Controle do Ollama e comunicação com modelos de linguagem
//
// Compilar:
//   Windows: g++ -c -o bibliotecas/jp_ollama/jp_ollama.obj bibliotecas/jp_ollama/jp_ollama.cpp -O3 -lws2_32
//   Linux:   g++ -c -fPIC -o bibliotecas/jp_ollama/jp_ollama.o bibliotecas/jp_ollama/jp_ollama.cpp -O3

#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif
#endif

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>
#include <sstream>

// =============================================================================
// PLATAFORMA E EXPORT
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <tlhelp32.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SocketType;
    #define INVALID_SOCK INVALID_SOCKET
    #define CLOSE_SOCKET closesocket
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/wait.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <errno.h>
    #include <signal.h>
    typedef int SocketType;
    #define INVALID_SOCK -1
    #define CLOSE_SOCKET close
#endif

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================
static const int NUM_BUFS = 8;
static const int BUF_PEQUENO = 4096;

// Buffer grande para respostas do Ollama (pode ser vários KB)
static std::string bufs_grandes[NUM_BUFS];
static int buf_grande_idx = 0;
static std::mutex buf_mutex;

static int64_t retorna_str(const std::string& s) {
    std::lock_guard<std::mutex> lock(buf_mutex);
    int idx = buf_grande_idx++ & (NUM_BUFS - 1);
    bufs_grandes[idx] = s;
    return (int64_t)bufs_grandes[idx].c_str();
}

// =============================================================================
// ESTADO GLOBAL
// =============================================================================
static bool wsa_iniciado = false;
static std::mutex wsa_mutex;

// Configurações padrão do Ollama
static std::string ollama_host = "127.0.0.1";
static int ollama_porta = 11434;
static std::string ollama_modelo = "qwen3";
static double ollama_temperatura = 0.2;
static int timeout_segundos = 120;

// Último erro
static std::string ultimo_erro = "";
static std::mutex erro_mutex;

static void set_erro(const std::string& msg) {
    std::lock_guard<std::mutex> lock(erro_mutex);
    ultimo_erro = msg;
}

// Controle de processo do Ollama
#if JP_WINDOWS
static HANDLE ollama_processo = NULL;
static DWORD ollama_pid = 0;
#else
static pid_t ollama_pid = 0;
#endif
static std::mutex processo_mutex;

// =============================================================================
// INICIALIZAÇÃO DE REDE
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

// =============================================================================
// FUNÇÕES INTERNAS DE HTTP
// =============================================================================

// Parseia uma URL simples: http://host:porta/caminho
static bool parsear_url(const std::string& url, std::string& host, int& porta, std::string& caminho) {
    // Remove http://
    std::string trabalho = url;
    if (trabalho.find("http://") == 0) {
        trabalho = trabalho.substr(7);
    } else if (trabalho.find("https://") == 0) {
        // HTTPS não suportado (requer SSL)
        return false;
    }

    // Separa host:porta do caminho
    size_t barra = trabalho.find('/');
    std::string host_porta;
    if (barra != std::string::npos) {
        host_porta = trabalho.substr(0, barra);
        caminho = trabalho.substr(barra);
    } else {
        host_porta = trabalho;
        caminho = "/";
    }

    // Separa host da porta
    size_t dois_pontos = host_porta.find(':');
    if (dois_pontos != std::string::npos) {
        host = host_porta.substr(0, dois_pontos);
        porta = std::atoi(host_porta.substr(dois_pontos + 1).c_str());
    } else {
        host = host_porta;
        porta = 80;
    }

    return true;
}

// Conecta num socket TCP
static SocketType conectar_tcp(const std::string& host, int porta) {
    iniciar_wsa();

    struct addrinfo hints, *resultado = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string porta_str = std::to_string(porta);
    int status = getaddrinfo(host.c_str(), porta_str.c_str(), &hints, &resultado);
    if (status != 0 || !resultado) {
        set_erro("Erro DNS: nao foi possivel resolver " + host);
        return INVALID_SOCK;
    }

    SocketType sock = socket(resultado->ai_family, resultado->ai_socktype, resultado->ai_protocol);
    if (sock == INVALID_SOCK) {
        freeaddrinfo(resultado);
        set_erro("Erro ao criar socket");
        return INVALID_SOCK;
    }

    // Timeout de conexão e recebimento
#if JP_WINDOWS
    DWORD tv = timeout_segundos * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_segundos;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif

    if (connect(sock, resultado->ai_addr, (int)resultado->ai_addrlen) != 0) {
        CLOSE_SOCKET(sock);
        freeaddrinfo(resultado);
        set_erro("Erro ao conectar em " + host + ":" + std::to_string(porta));
        return INVALID_SOCK;
    }

    freeaddrinfo(resultado);
    return sock;
}

// Envia dados completos pelo socket
static bool enviar_tudo(SocketType sock, const std::string& dados) {
    size_t total = 0;
    while (total < dados.size()) {
        int enviado = send(sock, dados.c_str() + total, (int)(dados.size() - total), 0);
        if (enviado <= 0) return false;
        total += enviado;
    }
    return true;
}

// Recebe resposta HTTP completa
static std::string receber_resposta(SocketType sock) {
    std::string resposta;
    char buffer[8192];

    // Recebe headers primeiro
    while (true) {
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        resposta.append(buffer, bytes);

        // Verifica se já recebeu todos os headers
        if (resposta.find("\r\n\r\n") != std::string::npos) break;
    }

    if (resposta.empty()) return "";

    // Encontra fim dos headers
    size_t header_fim = resposta.find("\r\n\r\n");
    if (header_fim == std::string::npos) return resposta;

    std::string headers = resposta.substr(0, header_fim);
    size_t corpo_inicio = header_fim + 4;

    // Verifica Content-Length
    std::string headers_lower = headers;
    for (auto& c : headers_lower) c = tolower(c);

    size_t cl_pos = headers_lower.find("content-length:");
    if (cl_pos != std::string::npos) {
        size_t val_inicio = cl_pos + 15;
        size_t val_fim = headers_lower.find("\r\n", val_inicio);
        if (val_fim == std::string::npos) val_fim = headers_lower.size();
        int content_length = std::atoi(headers.substr(val_inicio, val_fim - val_inicio).c_str());

        // Recebe o resto do corpo se necessário
        int corpo_atual = (int)(resposta.size() - corpo_inicio);
        int falta = content_length - corpo_atual;
        while (falta > 0) {
            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            resposta.append(buffer, bytes);
            falta -= bytes;
        }
    }
    // Transfer-Encoding: chunked
    else if (headers_lower.find("transfer-encoding: chunked") != std::string::npos) {
        // Lê até o servidor fechar a conexão ou chunk final (0\r\n\r\n)
        while (true) {
            if (resposta.find("\r\n0\r\n\r\n") != std::string::npos) break;
            if (resposta.find("\r\n0\r\n") != std::string::npos &&
                resposta.back() == '\n') break;

            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            resposta.append(buffer, bytes);
        }

        // Decodifica chunks
        std::string corpo_raw = resposta.substr(corpo_inicio);
        std::string corpo_decodificado;
        size_t pos = 0;

        while (pos < corpo_raw.size()) {
            size_t fim_tamanho = corpo_raw.find("\r\n", pos);
            if (fim_tamanho == std::string::npos) break;

            std::string hex_str = corpo_raw.substr(pos, fim_tamanho - pos);
            // Remove extensões de chunk se existirem
            size_t ext_pos = hex_str.find(';');
            if (ext_pos != std::string::npos) hex_str = hex_str.substr(0, ext_pos);

            long chunk_size = strtol(hex_str.c_str(), nullptr, 16);
            if (chunk_size <= 0) break;

            pos = fim_tamanho + 2; // pula \r\n após tamanho
            if (pos + chunk_size <= corpo_raw.size()) {
                corpo_decodificado.append(corpo_raw, pos, chunk_size);
            }
            pos += chunk_size + 2; // pula dados + \r\n
        }

        // Reconstrói resposta com corpo decodificado
        resposta = resposta.substr(0, corpo_inicio) + corpo_decodificado;
    }
    else {
        // Sem Content-Length nem chunked: lê até fechar
        while (true) {
            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            resposta.append(buffer, bytes);
        }
    }

    return resposta;
}

// Extrai apenas o corpo da resposta HTTP (sem headers)
static std::string extrair_corpo(const std::string& resposta) {
    size_t pos = resposta.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return resposta.substr(pos + 4);
}

// Extrai código de status HTTP
static int extrair_status(const std::string& resposta) {
    // HTTP/1.1 200 OK
    size_t espaco = resposta.find(' ');
    if (espaco == std::string::npos) return 0;
    return std::atoi(resposta.c_str() + espaco + 1);
}

// Extrai valor de um campo JSON simples (sem parser completo)
// Funciona para: "campo": "valor" e "campo": numero
static std::string json_extrair(const std::string& json, const std::string& campo) {
    std::string busca = "\"" + campo + "\"";
    size_t pos = json.find(busca);
    if (pos == std::string::npos) return "";

    pos += busca.size();

    // Pula espaços e ':'
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) pos++;

    if (pos >= json.size()) return "";

    // String entre aspas
    if (json[pos] == '"') {
        pos++; // pula aspas inicial
        std::string valor;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                char next = json[pos + 1];
                if (next == '"') { valor += '"'; pos += 2; continue; }
                if (next == '\\') { valor += '\\'; pos += 2; continue; }
                if (next == 'n') { valor += '\n'; pos += 2; continue; }
                if (next == 't') { valor += '\t'; pos += 2; continue; }
                if (next == 'r') { valor += '\r'; pos += 2; continue; }
                if (next == '/') { valor += '/'; pos += 2; continue; }
                // \uXXXX — unicode escape (ex: \u003c = '<', \u003e = '>')
                if (next == 'u' && pos + 5 < json.size()) {
                    std::string hex = json.substr(pos + 2, 4);
                    unsigned long codepoint = strtoul(hex.c_str(), nullptr, 16);
                    if (codepoint > 0) {
                        // ASCII direto (cobre \u003c, \u003e, \u0026 etc.)
                        if (codepoint < 0x80) {
                            valor += (char)codepoint;
                        }
                        // UTF-8 2 bytes
                        else if (codepoint < 0x800) {
                            valor += (char)(0xC0 | (codepoint >> 6));
                            valor += (char)(0x80 | (codepoint & 0x3F));
                        }
                        // UTF-8 3 bytes
                        else {
                            valor += (char)(0xE0 | (codepoint >> 12));
                            valor += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            valor += (char)(0x80 | (codepoint & 0x3F));
                        }
                    }
                    pos += 6; // pula \uXXXX
                    continue;
                }
            }
            valor += json[pos];
            pos++;
        }
        return valor;
    }

    // Número, bool, null
    std::string valor;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']'
           && json[pos] != ' ' && json[pos] != '\n' && json[pos] != '\r') {
        valor += json[pos];
        pos++;
    }
    return valor;
}

// Monta um JSON simples para enviar ao Ollama
static std::string json_montar_generate(const std::string& modelo, const std::string& prompt, double temperatura) {
    std::string json = "{";
    json += "\"model\":\"" + modelo + "\",";
    json += "\"prompt\":\"";

    // Escapa o prompt
    for (char c : prompt) {
        switch (c) {
            case '"':  json += "\\\""; break;
            case '\\': json += "\\\\"; break;
            case '\n': json += "\\n"; break;
            case '\r': json += "\\r"; break;
            case '\t': json += "\\t"; break;
            default:   json += c; break;
        }
    }

    json += "\",";

    // Temperatura como string
    char temp_buf[32];
    snprintf(temp_buf, sizeof(temp_buf), "%.2f", temperatura);
    json += "\"temperature\":" + std::string(temp_buf) + ",";
    json += "\"stream\":false";
    json += "}";

    return json;
}

// Monta JSON para chat (com mensagens)
static std::string json_montar_chat(const std::string& modelo, const std::string& mensagens_json, double temperatura) {
    std::string json = "{";
    json += "\"model\":\"" + modelo + "\",";
    json += "\"messages\":" + mensagens_json + ",";

    char temp_buf[32];
    snprintf(temp_buf, sizeof(temp_buf), "%.2f", temperatura);
    json += "\"temperature\":" + std::string(temp_buf) + ",";
    json += "\"stream\":false";
    json += "}";

    return json;
}

// Faz uma requisição HTTP genérica
static std::string http_request(const std::string& metodo, const std::string& url,
                                 const std::string& corpo, const std::string& content_type) {
    std::string host;
    int porta;
    std::string caminho;

    if (!parsear_url(url, host, porta, caminho)) {
        set_erro("URL invalida ou HTTPS nao suportado: " + url);
        return "";
    }

    SocketType sock = conectar_tcp(host, porta);
    if (sock == INVALID_SOCK) return "";

    // Monta request HTTP
    std::string request = metodo + " " + caminho + " HTTP/1.1\r\n";
    request += "Host: " + host + ":" + std::to_string(porta) + "\r\n";
    request += "Connection: close\r\n";

    if (!corpo.empty()) {
        request += "Content-Type: " + content_type + "\r\n";
        request += "Content-Length: " + std::to_string(corpo.size()) + "\r\n";
    }

    request += "\r\n";

    if (!corpo.empty()) {
        request += corpo;
    }

    if (!enviar_tudo(sock, request)) {
        CLOSE_SOCKET(sock);
        set_erro("Erro ao enviar request para " + host);
        return "";
    }

    std::string resposta = receber_resposta(sock);
    CLOSE_SOCKET(sock);

    if (resposta.empty()) {
        set_erro("Resposta vazia de " + host);
        return "";
    }

    int status = extrair_status(resposta);
    if (status < 200 || status >= 300) {
        set_erro("HTTP " + std::to_string(status) + " de " + url);
    }

    return extrair_corpo(resposta);
}

// =============================================================================
// FUNÇÕES EXPORTADAS - CONTROLE DO PROCESSO OLLAMA
// =============================================================================

// llm_iniciar() -> inteiro
// Inicia o processo 'ollama serve' em background
// Retorna 1 se iniciou com sucesso, 0 se falhou, 2 se já estava rodando
JP_EXPORT int64_t llm_iniciar() {
    std::lock_guard<std::mutex> lock(processo_mutex);

    // Verifica se já tem um processo nosso rodando
#if JP_WINDOWS
    if (ollama_processo != NULL) {
        DWORD exit_code;
        if (GetExitCodeProcess(ollama_processo, &exit_code) && exit_code == STILL_ACTIVE) {
            return 2; // já rodando
        }
        // Processo morreu, limpa handle
        CloseHandle(ollama_processo);
        ollama_processo = NULL;
        ollama_pid = 0;
    }

    // Tenta verificar se o Ollama já está respondendo na porta
    // (pode ter sido iniciado externamente)
    {
        std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/";
        std::string resp = http_request("GET", url, "", "");
        if (!resp.empty()) {
            return 2; // já rodando externamente
        }
    }

    // Inicia ollama serve via CreateProcess
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // janela oculta
    ZeroMemory(&pi, sizeof(pi));

    char cmd[] = "ollama serve";

    if (!CreateProcessA(
        NULL,           // sem caminho do executável (usa PATH)
        cmd,            // linha de comando
        NULL, NULL,     // sem atributos de segurança
        FALSE,          // sem herança de handles
        CREATE_NO_WINDOW | DETACHED_PROCESS, // sem janela
        NULL,           // ambiente do pai
        NULL,           // diretório do pai
        &si, &pi
    )) {
        set_erro("Erro ao iniciar ollama serve (CreateProcess falhou, codigo " + std::to_string(GetLastError()) + ")");
        return 0;
    }

    ollama_processo = pi.hProcess;
    ollama_pid = pi.dwProcessId;
    CloseHandle(pi.hThread); // não precisamos da thread

#else
    if (ollama_pid > 0) {
        // Verifica se o processo ainda existe
        if (kill(ollama_pid, 0) == 0) {
            return 2; // já rodando
        }
        // Processo morreu, limpa
        ollama_pid = 0;
    }

    // Tenta verificar se o Ollama já está respondendo na porta
    {
        std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/";
        std::string resp = http_request("GET", url, "", "");
        if (!resp.empty()) {
            return 2; // já rodando externamente
        }
    }

    // Inicia ollama serve via fork + exec
    pid_t pid = fork();
    if (pid < 0) {
        set_erro("Erro ao iniciar ollama serve (fork falhou)");
        return 0;
    }

    if (pid == 0) {
        // Processo filho
        // Redireciona stdout/stderr para /dev/null
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        // Cria nova sessão para desacoplar do terminal
        setsid();

        execlp("ollama", "ollama", "serve", (char*)NULL);

        // Se chegou aqui, exec falhou
        _exit(1);
    }

    // Processo pai
    ollama_pid = pid;
#endif

    // Aguarda até 10 segundos para o Ollama ficar pronto
    for (int i = 0; i < 20; i++) {
#if JP_WINDOWS
        Sleep(500);
#else
        usleep(500000);
#endif
        std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/";
        std::string resp = http_request("GET", url, "", "");
        if (!resp.empty()) {
            set_erro(""); // limpa erro
            return 1; // sucesso
        }
    }

    set_erro("Ollama iniciado mas nao respondeu em 10 segundos");
    return 1; // processo foi criado, mas pode estar lento
}

// llm_encerrar() -> inteiro
// Encerra o processo do Ollama
// Retorna 1 se encerrou, 0 se falhou, 2 se não havia processo para encerrar
JP_EXPORT int64_t llm_encerrar() {
    std::lock_guard<std::mutex> lock(processo_mutex);

#if JP_WINDOWS
    if (ollama_processo == NULL || ollama_pid == 0) {
        return 2; // nada para encerrar
    }

    // Tenta encerrar a árvore de processos (ollama pode ter filhos)
    // Primeiro tenta terminar graciosamente via GenerateConsoleCtrlEvent
    // Se não funcionar, usa TerminateProcess

    // Enumera e mata processos filhos
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snapshot, &pe)) {
            do {
                if (pe.th32ParentProcessID == ollama_pid) {
                    HANDLE filho = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (filho) {
                        TerminateProcess(filho, 0);
                        CloseHandle(filho);
                    }
                }
            } while (Process32Next(snapshot, &pe));
        }
        CloseHandle(snapshot);
    }

    // Mata o processo principal
    BOOL resultado = TerminateProcess(ollama_processo, 0);
    WaitForSingleObject(ollama_processo, 3000); // espera até 3s
    CloseHandle(ollama_processo);
    ollama_processo = NULL;
    ollama_pid = 0;

    if (!resultado) {
        set_erro("Erro ao encerrar processo Ollama");
        return 0;
    }

#else
    if (ollama_pid <= 0) {
        return 2; // nada para encerrar
    }

    // Envia SIGTERM primeiro (encerramento gracioso)
    if (kill(ollama_pid, SIGTERM) != 0) {
        set_erro("Erro ao enviar SIGTERM para Ollama (pid " + std::to_string(ollama_pid) + ")");
        ollama_pid = 0;
        return 0;
    }

    // Aguarda até 5 segundos pelo encerramento gracioso
    for (int i = 0; i < 10; i++) {
        usleep(500000);
        int status;
        pid_t result = waitpid(ollama_pid, &status, WNOHANG);
        if (result == ollama_pid || result == -1) {
            // Processo encerrou
            ollama_pid = 0;
            return 1;
        }
    }

    // Se não encerrou, força com SIGKILL
    kill(ollama_pid, SIGKILL);
    usleep(500000);
    waitpid(ollama_pid, nullptr, WNOHANG);
    ollama_pid = 0;

#endif

    return 1;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - CONFIGURAÇÃO
// =============================================================================

// llm_config_host(host) -> inteiro
// Define o host do Ollama (padrão: 127.0.0.1)
JP_EXPORT int64_t llm_config_host(int64_t host_ptr) {
    ollama_host = std::string((const char*)host_ptr);
    return 1;
}

// llm_config_porta(porta) -> inteiro
// Define a porta do Ollama (padrão: 11434)
JP_EXPORT int64_t llm_config_porta(int64_t porta) {
    ollama_porta = (int)porta;
    return 1;
}

// llm_config_modelo(modelo) -> inteiro
// Define o modelo padrão (padrão: qwen3)
JP_EXPORT int64_t llm_config_modelo(int64_t modelo_ptr) {
    ollama_modelo = std::string((const char*)modelo_ptr);
    return 1;
}

// llm_config_temperatura(temp * 100) -> inteiro
// Define a temperatura (passar multiplicado por 100, ex: 20 = 0.20)
JP_EXPORT int64_t llm_config_temperatura(int64_t temp100) {
    ollama_temperatura = (double)temp100 / 100.0;
    return 1;
}

// llm_config_timeout(segundos) -> inteiro
// Define timeout em segundos (padrão: 120)
JP_EXPORT int64_t llm_config_timeout(int64_t segundos) {
    timeout_segundos = (int)segundos;
    return 1;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - OLLAMA
// =============================================================================

// llm_status() -> inteiro
// Verifica se o Ollama está rodando (1 = sim, 0 = não)
JP_EXPORT int64_t llm_status() {
    std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/";
    std::string resp = http_request("GET", url, "", "");
    return resp.empty() ? 0 : 1;
}

// llm_gerar(prompt) -> texto
// Envia um prompt simples e retorna a resposta do modelo
JP_EXPORT int64_t llm_gerar(int64_t prompt_ptr) {
    std::string prompt((const char*)prompt_ptr);
    std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/api/generate";
    std::string corpo = json_montar_generate(ollama_modelo, prompt, ollama_temperatura);

    std::string resp = http_request("POST", url, corpo, "application/json");
    if (resp.empty()) return retorna_str("");

    return retorna_str(json_extrair(resp, "response"));
}

// llm_chat(mensagens_json) -> texto
// Envia mensagens no formato chat e retorna a resposta
// mensagens_json deve ser: [{"role":"user","content":"ola"}]
JP_EXPORT int64_t llm_chat(int64_t mensagens_ptr) {
    std::string mensagens((const char*)mensagens_ptr);
    std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/api/chat";
    std::string corpo = json_montar_chat(ollama_modelo, mensagens, ollama_temperatura);

    std::string resp = http_request("POST", url, corpo, "application/json");
    if (resp.empty()) return retorna_str("");

    // Extrai message.content da resposta
    // A resposta tem: {"message":{"role":"assistant","content":"..."}}
    std::string message = json_extrair(resp, "content");
    return retorna_str(message);
}

// llm_listar() -> texto
// Lista modelos disponíveis (retorna JSON bruto)
JP_EXPORT int64_t llm_listar() {
    std::string url = "http://" + ollama_host + ":" + std::to_string(ollama_porta) + "/api/tags";
    std::string resp = http_request("GET", url, "", "");
    return retorna_str(resp);
}

// =============================================================================
// FUNÇÕES EXPORTADAS - HTTP GENÉRICO
// =============================================================================

// llm_get(url) -> texto
// Faz um GET e retorna o corpo da resposta
JP_EXPORT int64_t llm_get(int64_t url_ptr) {
    std::string url((const char*)url_ptr);
    std::string resp = http_request("GET", url, "", "");
    return retorna_str(resp);
}

// llm_post(url, corpo) -> texto
// Faz um POST com corpo JSON e retorna a resposta
JP_EXPORT int64_t llm_post(int64_t url_ptr, int64_t corpo_ptr) {
    std::string url((const char*)url_ptr);
    std::string corpo((const char*)corpo_ptr);
    std::string resp = http_request("POST", url, corpo, "application/json");
    return retorna_str(resp);
}

// llm_post_texto(url, corpo) -> texto
// Faz um POST com corpo text/plain e retorna a resposta
JP_EXPORT int64_t llm_post_texto(int64_t url_ptr, int64_t corpo_ptr) {
    std::string url((const char*)url_ptr);
    std::string corpo((const char*)corpo_ptr);
    std::string resp = http_request("POST", url, corpo, "text/plain");
    return retorna_str(resp);
}

// =============================================================================
// FUNÇÕES EXPORTADAS - JSON SIMPLES
// =============================================================================

// llm_json_campo(json, campo) -> texto
// Extrai o valor de um campo de um JSON
JP_EXPORT int64_t llm_json_campo(int64_t json_ptr, int64_t campo_ptr) {
    std::string json((const char*)json_ptr);
    std::string campo((const char*)campo_ptr);
    return retorna_str(json_extrair(json, campo));
}

// =============================================================================
// FUNÇÕES EXPORTADAS - UTILITÁRIOS
// =============================================================================

// llm_erro() -> texto
// Retorna o último erro ocorrido
JP_EXPORT int64_t llm_erro() {
    std::lock_guard<std::mutex> lock(erro_mutex);
    return retorna_str(ultimo_erro);
}

// llm_versao() -> texto
// Retorna a versão da biblioteca
JP_EXPORT int64_t llm_versao() {
    return retorna_str("jp_ollama 2.0 - JPLang LLM/Ollama Client");
}
