// os.cpp
// Biblioteca de funções do sistema operacional para JPLang - Versão Unificada Windows/Linux
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/os/os.jpd bibliotecas/os/os.cpp -O3 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/os/libos.jpd bibliotecas/os/os.cpp -O3

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <thread>
#include <chrono>

// =============================================================================
// DETECÇÃO DE PLATAFORMA
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <windows.h>
    #include <direct.h>
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <pwd.h>
    #include <limits.h>
#endif

// =============================================================================
// TIPOS C PUROS (interface com JPLang)
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
// HELPERS
// =============================================================================
static JPValor jp_int(int v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

static JPValor jp_string(const std::string& s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    r.valor.texto = (char*)malloc(s.size() + 1);
    if (r.valor.texto) {
        memcpy(r.valor.texto, s.c_str(), s.size() + 1);
    }
    return r;
}

static std::string jp_get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) {
        return std::string(args[idx].valor.texto);
    }
    return "";
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// Limpar terminal
JP_EXPORT JPValor jp_os_limpar_terminal(JPValor* args, int numArgs) {
#if JP_WINDOWS
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD count;
    DWORD cellCount;
    COORD homeCoords = {0, 0};

    if (hConsole == INVALID_HANDLE_VALUE) return jp_int(0);
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return jp_int(0);

    cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, ' ', cellCount, homeCoords, &count);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cellCount, homeCoords, &count);
    SetConsoleCursorPosition(hConsole, homeCoords);
#else
    // Código ANSI para limpar terminal
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
    return jp_int(1);
}

// Pausar execução (milissegundos)
JP_EXPORT JPValor jp_os_dormir(JPValor* args, int numArgs) {
    int ms = jp_get_int(args, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return jp_int(1);
}

// Executar comando do sistema
JP_EXPORT JPValor jp_os_executar(JPValor* args, int numArgs) {
    std::string cmd = jp_get_string(args, 0);
    int resultado = system(cmd.c_str());
    return jp_int(resultado);
}

// Obter variável de ambiente
JP_EXPORT JPValor jp_os_getenv(JPValor* args, int numArgs) {
    std::string nome = jp_get_string(args, 0);
    char* valor = getenv(nome.c_str());
    if (valor) return jp_string(std::string(valor));
    return jp_string("");
}

// Definir variável de ambiente
JP_EXPORT JPValor jp_os_setenv(JPValor* args, int numArgs) {
    std::string nome = jp_get_string(args, 0);
    std::string valor = jp_get_string(args, 1);
#if JP_WINDOWS
    std::string cmd = nome + "=" + valor;
    int resultado = _putenv(cmd.c_str());
    return jp_int((resultado == 0) ? 1 : 0);
#else
    int resultado = setenv(nome.c_str(), valor.c_str(), 1);
    return jp_int((resultado == 0) ? 1 : 0);
#endif
}

// Obter diretório atual
JP_EXPORT JPValor jp_os_diretorio_atual(JPValor* args, int numArgs) {
#if JP_WINDOWS
    char buffer[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, buffer)) {
        return jp_string(std::string(buffer));
    }
#else
    char buffer[PATH_MAX];
    if (getcwd(buffer, PATH_MAX)) {
        return jp_string(std::string(buffer));
    }
#endif
    return jp_string("");
}

// Mudar diretório
JP_EXPORT JPValor jp_os_mudar_diretorio(JPValor* args, int numArgs) {
    std::string caminho = jp_get_string(args, 0);
#if JP_WINDOWS
    if (SetCurrentDirectoryA(caminho.c_str())) {
        return jp_int(1);
    }
#else
    if (chdir(caminho.c_str()) == 0) {
        return jp_int(1);
    }
#endif
    return jp_int(0);
}

// Verificar se arquivo/pasta existe
JP_EXPORT JPValor jp_os_existe(JPValor* args, int numArgs) {
    std::string caminho = jp_get_string(args, 0);
#if JP_WINDOWS
    DWORD attr = GetFileAttributesA(caminho.c_str());
    return jp_int((attr != INVALID_FILE_ATTRIBUTES) ? 1 : 0);
#else
    struct stat st;
    return jp_int((stat(caminho.c_str(), &st) == 0) ? 1 : 0);
#endif
}

// Verificar se é diretório
JP_EXPORT JPValor jp_os_eh_diretorio(JPValor* args, int numArgs) {
    std::string caminho = jp_get_string(args, 0);
#if JP_WINDOWS
    DWORD attr = GetFileAttributesA(caminho.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return jp_int(0);
    return jp_int((attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0);
#else
    struct stat st;
    if (stat(caminho.c_str(), &st) != 0) return jp_int(0);
    return jp_int(S_ISDIR(st.st_mode) ? 1 : 0);
#endif
}

// Criar diretório
JP_EXPORT JPValor jp_os_criar_diretorio(JPValor* args, int numArgs) {
    std::string caminho = jp_get_string(args, 0);
#if JP_WINDOWS
    if (CreateDirectoryA(caminho.c_str(), NULL)) {
        return jp_int(1);
    }
#else
    if (mkdir(caminho.c_str(), 0755) == 0) {
        return jp_int(1);
    }
#endif
    return jp_int(0);
}

// Remover arquivo
JP_EXPORT JPValor jp_os_remover_arquivo(JPValor* args, int numArgs) {
    std::string caminho = jp_get_string(args, 0);
#if JP_WINDOWS
    if (DeleteFileA(caminho.c_str())) {
        return jp_int(1);
    }
#else
    if (unlink(caminho.c_str()) == 0) {
        return jp_int(1);
    }
#endif
    return jp_int(0);
}

// Remover diretório (vazio)
JP_EXPORT JPValor jp_os_remover_diretorio(JPValor* args, int numArgs) {
    std::string caminho = jp_get_string(args, 0);
#if JP_WINDOWS
    if (RemoveDirectoryA(caminho.c_str())) {
        return jp_int(1);
    }
#else
    if (rmdir(caminho.c_str()) == 0) {
        return jp_int(1);
    }
#endif
    return jp_int(0);
}

// Renomear/mover arquivo ou pasta
JP_EXPORT JPValor jp_os_renomear(JPValor* args, int numArgs) {
    std::string origem = jp_get_string(args, 0);
    std::string destino = jp_get_string(args, 1);
#if JP_WINDOWS
    if (MoveFileA(origem.c_str(), destino.c_str())) {
        return jp_int(1);
    }
#else
    if (rename(origem.c_str(), destino.c_str()) == 0) {
        return jp_int(1);
    }
#endif
    return jp_int(0);
}

// Copiar arquivo
JP_EXPORT JPValor jp_os_copiar_arquivo(JPValor* args, int numArgs) {
    std::string origem = jp_get_string(args, 0);
    std::string destino = jp_get_string(args, 1);
#if JP_WINDOWS
    if (CopyFileA(origem.c_str(), destino.c_str(), FALSE)) {
        return jp_int(1);
    }
#else
    FILE* src = fopen(origem.c_str(), "rb");
    if (!src) return jp_int(0);
    
    FILE* dst = fopen(destino.c_str(), "wb");
    if (!dst) {
        fclose(src);
        return jp_int(0);
    }
    
    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    return jp_int(1);
#endif
    return jp_int(0);
}

// Obter nome do usuário
JP_EXPORT JPValor jp_os_usuario(JPValor* args, int numArgs) {
#if JP_WINDOWS
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (GetUserNameA(buffer, &size)) {
        return jp_string(std::string(buffer));
    }
#else
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        return jp_string(std::string(pw->pw_name));
    }
#endif
    return jp_string("");
}

// Obter nome do computador
JP_EXPORT JPValor jp_os_computador(JPValor* args, int numArgs) {
#if JP_WINDOWS
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return jp_string(std::string(buffer));
    }
#else
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        return jp_string(std::string(buffer));
    }
#endif
    return jp_string("");
}

// Obter timestamp (segundos desde epoch)
JP_EXPORT JPValor jp_os_timestamp(JPValor* args, int numArgs) {
    return jp_int((int)time(NULL));
}

// Obter tick count (ms desde boot)
JP_EXPORT JPValor jp_os_tick(JPValor* args, int numArgs) {
#if JP_WINDOWS
    return jp_int((int)GetTickCount());
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return jp_int((int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
#endif
}

// Emitir beep (frequencia, duracao_ms)
JP_EXPORT JPValor jp_os_beep(JPValor* args, int numArgs) {
    int freq = jp_get_int(args, 0);
    int duracao = jp_get_int(args, 1);
#if JP_WINDOWS
    Beep(freq, duracao);
#else
    // No Linux, tenta usar o beep do terminal (pode não funcionar em todos)
    (void)freq;
    (void)duracao;
    printf("\a");
    fflush(stdout);
#endif
    return jp_int(1);
}

// Sair do programa com código
JP_EXPORT JPValor jp_os_sair(JPValor* args, int numArgs) {
    int codigo = jp_get_int(args, 0);
    exit(codigo);
    return jp_int(0);
}

// Obter nome do sistema operacional
JP_EXPORT JPValor jp_os_nome(JPValor* args, int numArgs) {
#if JP_WINDOWS
    return jp_string("windows");
#else
    return jp_string("linux");
#endif
}