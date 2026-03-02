// cmd.cpp
// Biblioteca nativa de execução de comandos e subprocessos para JPLang
//
// Compilar:
//   Windows: g++ -c -o bibliotecas/cmd/cmd.obj bibliotecas/cmd/cmd.cpp -O3
//   Linux:   g++ -c -fPIC -o bibliotecas/cmd/cmd.o bibliotecas/cmd/cmd.cpp -O3

#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <array>

#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <windows.h>
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <unistd.h>
    #include <sys/wait.h>
#endif

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================
static const int NUM_BUFS = 8;
static std::string bufs[NUM_BUFS];
static int buf_idx = 0;
static std::mutex buf_mutex;

static int64_t retorna_str(const std::string& s) {
    std::lock_guard<std::mutex> lock(buf_mutex);
    int idx = buf_idx++ & (NUM_BUFS - 1);
    bufs[idx] = s;
    return (int64_t)bufs[idx].c_str();
}

// =============================================================================
// ESTADO GLOBAL - ÚLTIMO RESULTADO
// =============================================================================
static std::string ultimo_stdout = "";
static std::string ultimo_stderr = "";
static int64_t ultimo_retorno = -1;
static std::string ultimo_erro = "";
static std::mutex estado_mutex;

static void limpar_estado() {
    ultimo_stdout = "";
    ultimo_stderr = "";
    ultimo_retorno = -1;
    ultimo_erro = "";
}

// =============================================================================
// EXECUÇÃO DE COMANDOS - WINDOWS
// =============================================================================
#if JP_WINDOWS

static int64_t executar_windows(const std::string& comando) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Pipes para stdout
    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        ultimo_erro = "Falha ao criar pipe stdout";
        return -1;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    // Pipes para stderr
    HANDLE stderr_read = nullptr, stderr_write = nullptr;
    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        ultimo_erro = "Falha ao criar pipe stderr";
        return -1;
    }
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // Configurar processo
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    // Montar comando via cmd /c para suportar pipes, redirecionamentos etc
    std::string cmd_line = "cmd /c " + comando;

    // Criar array mutável (CreateProcessA exige)
    char* cmd_buf = new char[cmd_line.size() + 1];
    strcpy(cmd_buf, cmd_line.c_str());

    BOOL ok = CreateProcessA(
        nullptr,        // lpApplicationName
        cmd_buf,        // lpCommandLine
        nullptr,        // lpProcessAttributes
        nullptr,        // lpThreadAttributes
        TRUE,           // bInheritHandles
        CREATE_NO_WINDOW, // dwCreationFlags
        nullptr,        // lpEnvironment
        nullptr,        // lpCurrentDirectory
        &si,            // lpStartupInfo
        &pi             // lpProcessInformation
    );

    delete[] cmd_buf;

    // Fechar extremidades de escrita dos pipes (senão a leitura não termina)
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!ok) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        ultimo_erro = "Falha ao criar processo: " + comando;
        return -1;
    }

    // Ler stdout
    std::string out_result;
    char buffer[4096];
    DWORD bytes_lidos;
    while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_lidos, nullptr) && bytes_lidos > 0) {
        buffer[bytes_lidos] = '\0';
        out_result.append(buffer, bytes_lidos);
    }
    CloseHandle(stdout_read);

    // Ler stderr
    std::string err_result;
    while (ReadFile(stderr_read, buffer, sizeof(buffer) - 1, &bytes_lidos, nullptr) && bytes_lidos > 0) {
        buffer[bytes_lidos] = '\0';
        err_result.append(buffer, bytes_lidos);
    }
    CloseHandle(stderr_read);

    // Aguardar fim do processo
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Remover \r\n final
    while (!out_result.empty() && (out_result.back() == '\n' || out_result.back() == '\r'))
        out_result.pop_back();
    while (!err_result.empty() && (err_result.back() == '\n' || err_result.back() == '\r'))
        err_result.pop_back();

    ultimo_stdout = out_result;
    ultimo_stderr = err_result;
    ultimo_retorno = (int64_t)exit_code;

    return ultimo_retorno;
}

#else
// =============================================================================
// EXECUÇÃO DE COMANDOS - LINUX
// =============================================================================

static int64_t executar_linux(const std::string& comando) {
    // Pipes para stdout
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        ultimo_erro = "Falha ao criar pipes";
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        ultimo_erro = "Falha no fork";
        return -1;
    }

    if (pid == 0) {
        // Processo filho
        close(stdout_pipe[0]); // fecha leitura
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        execl("/bin/sh", "sh", "-c", comando.c_str(), nullptr);
        _exit(127);
    }

    // Processo pai
    close(stdout_pipe[1]); // fecha escrita
    close(stderr_pipe[1]);

    // Ler stdout
    std::string out_result;
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        out_result.append(buffer, bytes);
    }
    close(stdout_pipe[0]);

    // Ler stderr
    std::string err_result;
    while ((bytes = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        err_result.append(buffer, bytes);
    }
    close(stderr_pipe[0]);

    // Aguardar fim do processo
    int status;
    waitpid(pid, &status, 0);

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    // Remover \n final
    while (!out_result.empty() && (out_result.back() == '\n' || out_result.back() == '\r'))
        out_result.pop_back();
    while (!err_result.empty() && (err_result.back() == '\n' || err_result.back() == '\r'))
        err_result.pop_back();

    ultimo_stdout = out_result;
    ultimo_stderr = err_result;
    ultimo_retorno = (int64_t)exit_code;

    return ultimo_retorno;
}

#endif

// =============================================================================
// FUNÇÕES EXPORTADAS - EXECUÇÃO
// =============================================================================

// cmd_executar(comando) -> inteiro
// Executa um comando e retorna o código de saída (0 = sucesso)
// Após chamar, use cmd_stdout() e cmd_stderr() para ler a saída
JP_EXPORT int64_t cmd_executar(int64_t comando_ptr) {
    std::string comando((const char*)comando_ptr);
    std::lock_guard<std::mutex> lock(estado_mutex);
    limpar_estado();

#if JP_WINDOWS
    return executar_windows(comando);
#else
    return executar_linux(comando);
#endif
}

// cmd_stdout() -> texto
// Retorna a saída padrão (stdout) do último comando executado
JP_EXPORT int64_t cmd_stdout() {
    std::lock_guard<std::mutex> lock(estado_mutex);
    return retorna_str(ultimo_stdout);
}

// cmd_stderr() -> texto
// Retorna a saída de erro (stderr) do último comando executado
JP_EXPORT int64_t cmd_stderr() {
    std::lock_guard<std::mutex> lock(estado_mutex);
    return retorna_str(ultimo_stderr);
}

// cmd_retorno() -> inteiro
// Retorna o código de saída do último comando executado
JP_EXPORT int64_t cmd_retorno() {
    std::lock_guard<std::mutex> lock(estado_mutex);
    return ultimo_retorno;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - ATALHOS
// =============================================================================

// cmd_compilar_c(arquivo_c, arquivo_saida) -> inteiro
// Atalho: gcc arquivo.c -o saida
JP_EXPORT int64_t cmd_compilar_c(int64_t src_ptr, int64_t out_ptr) {
    std::string src((const char*)src_ptr);
    std::string out((const char*)out_ptr);
    std::string comando = "gcc \"" + src + "\" -o \"" + out + "\"";

    std::lock_guard<std::mutex> lock(estado_mutex);
    limpar_estado();

#if JP_WINDOWS
    return executar_windows(comando);
#else
    return executar_linux(comando);
#endif
}

// cmd_compilar_cpp(arquivo_cpp, arquivo_saida) -> inteiro
// Atalho: g++ arquivo.cpp -o saida -O3
JP_EXPORT int64_t cmd_compilar_cpp(int64_t src_ptr, int64_t out_ptr) {
    std::string src((const char*)src_ptr);
    std::string out((const char*)out_ptr);
    std::string comando = "g++ \"" + src + "\" -o \"" + out + "\" -O3";

    std::lock_guard<std::mutex> lock(estado_mutex);
    limpar_estado();

#if JP_WINDOWS
    return executar_windows(comando);
#else
    return executar_linux(comando);
#endif
}

// cmd_compilar_flags(arquivo, arquivo_saida, flags) -> inteiro
// Compila com flags customizadas: g++ arquivo -o saida flags
JP_EXPORT int64_t cmd_compilar_flags(int64_t src_ptr, int64_t out_ptr, int64_t flags_ptr) {
    std::string src((const char*)src_ptr);
    std::string out((const char*)out_ptr);
    std::string flags((const char*)flags_ptr);
    std::string comando = "g++ \"" + src + "\" -o \"" + out + "\" " + flags;

    std::lock_guard<std::mutex> lock(estado_mutex);
    limpar_estado();

#if JP_WINDOWS
    return executar_windows(comando);
#else
    return executar_linux(comando);
#endif
}

// =============================================================================
// FUNÇÕES EXPORTADAS - UTILITÁRIOS
// =============================================================================

// cmd_erro() -> texto
// Retorna o último erro interno da biblioteca
JP_EXPORT int64_t cmd_erro() {
    std::lock_guard<std::mutex> lock(estado_mutex);
    return retorna_str(ultimo_erro);
}

// cmd_versao() -> texto
// Retorna a versão da biblioteca
JP_EXPORT int64_t cmd_versao() {
    return retorna_str("cmd.obj 1.0 - JPLang Command Executor");
}
