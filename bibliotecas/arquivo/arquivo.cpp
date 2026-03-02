// arquivo.cpp
// Biblioteca nativa de manipulação de arquivos e diretórios para JPLang
//
// Compilar:
//   Windows: g++ -c -o bibliotecas/arquivo/arquivo.obj bibliotecas/arquivo/arquivo.cpp -O3
//   Linux:   g++ -c -fPIC -o bibliotecas/arquivo/arquivo.o bibliotecas/arquivo/arquivo.cpp -O3

#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <mutex>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <direct.h>
    #include <io.h>
    #define MKDIR(path) _mkdir(path)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <unistd.h>
    #define MKDIR(path) mkdir(path, 0755)
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

// Último erro
static std::string ultimo_erro = "";
static std::mutex erro_mutex;

static void set_erro(const std::string& msg) {
    std::lock_guard<std::mutex> lock(erro_mutex);
    ultimo_erro = msg;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - LEITURA
// =============================================================================

// arq_ler(caminho) -> texto
// Lê o conteúdo inteiro de um arquivo texto
JP_EXPORT int64_t arq_ler(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);
    std::ifstream arquivo(caminho, std::ios::binary);
    if (!arquivo.is_open()) {
        set_erro("Nao foi possivel abrir: " + caminho);
        return retorna_str("");
    }
    std::ostringstream ss;
    ss << arquivo.rdbuf();
    arquivo.close();
    return retorna_str(ss.str());
}

// arq_ler_linhas(caminho, linha_inicio, quantidade) -> texto
// Lê um trecho específico de linhas de um arquivo
JP_EXPORT int64_t arq_ler_linhas(int64_t caminho_ptr, int64_t inicio, int64_t quantidade) {
    std::string caminho((const char*)caminho_ptr);
    std::ifstream arquivo(caminho);
    if (!arquivo.is_open()) {
        set_erro("Nao foi possivel abrir: " + caminho);
        return retorna_str("");
    }

    std::string resultado;
    std::string linha;
    int linha_atual = 1;
    int ini = (int)inicio;
    int qtd = (int)quantidade;

    while (std::getline(arquivo, linha)) {
        if (linha_atual >= ini && linha_atual < ini + qtd) {
            if (!resultado.empty()) resultado += "\n";
            resultado += linha;
        }
        if (linha_atual >= ini + qtd) break;
        linha_atual++;
    }

    arquivo.close();
    return retorna_str(resultado);
}

// arq_contar_linhas(caminho) -> inteiro
// Conta o número de linhas de um arquivo
JP_EXPORT int64_t arq_contar_linhas(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);
    std::ifstream arquivo(caminho);
    if (!arquivo.is_open()) {
        set_erro("Nao foi possivel abrir: " + caminho);
        return 0;
    }

    int64_t count = 0;
    std::string linha;
    while (std::getline(arquivo, linha)) {
        count++;
    }

    arquivo.close();
    return count;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - ESCRITA
// =============================================================================

// arq_escrever(caminho, conteudo) -> inteiro
// Escreve conteúdo em um arquivo (sobrescreve se existir)
JP_EXPORT int64_t arq_escrever(int64_t caminho_ptr, int64_t conteudo_ptr) {
    std::string caminho((const char*)caminho_ptr);
    std::string conteudo((const char*)conteudo_ptr);

    std::ofstream arquivo(caminho, std::ios::binary | std::ios::trunc);
    if (!arquivo.is_open()) {
        set_erro("Nao foi possivel criar: " + caminho);
        return 0;
    }

    arquivo.write(conteudo.c_str(), conteudo.size());
    arquivo.close();
    return 1;
}

// arq_adicionar(caminho, conteudo) -> inteiro
// Adiciona conteúdo ao final de um arquivo (cria se não existir)
JP_EXPORT int64_t arq_adicionar(int64_t caminho_ptr, int64_t conteudo_ptr) {
    std::string caminho((const char*)caminho_ptr);
    std::string conteudo((const char*)conteudo_ptr);

    std::ofstream arquivo(caminho, std::ios::binary | std::ios::app);
    if (!arquivo.is_open()) {
        set_erro("Nao foi possivel abrir para adicionar: " + caminho);
        return 0;
    }

    arquivo.write(conteudo.c_str(), conteudo.size());
    arquivo.close();
    return 1;
}

// arq_escrever_linha(caminho, conteudo) -> inteiro
// Adiciona uma linha ao final de um arquivo (com \n)
JP_EXPORT int64_t arq_escrever_linha(int64_t caminho_ptr, int64_t conteudo_ptr) {
    std::string caminho((const char*)caminho_ptr);
    std::string conteudo((const char*)conteudo_ptr);
    conteudo += "\n";

    std::ofstream arquivo(caminho, std::ios::binary | std::ios::app);
    if (!arquivo.is_open()) {
        set_erro("Nao foi possivel abrir para adicionar: " + caminho);
        return 0;
    }

    arquivo.write(conteudo.c_str(), conteudo.size());
    arquivo.close();
    return 1;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - VERIFICAÇÃO
// =============================================================================

// arq_existe(caminho) -> inteiro
// Verifica se um arquivo ou diretório existe (1 = sim, 0 = não)
JP_EXPORT int64_t arq_existe(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);
    struct stat info;
    return (stat(caminho.c_str(), &info) == 0) ? 1 : 0;
}

// arq_tamanho(caminho) -> inteiro
// Retorna o tamanho do arquivo em bytes (0 se não existir)
JP_EXPORT int64_t arq_tamanho(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);
    struct stat info;
    if (stat(caminho.c_str(), &info) != 0) {
        set_erro("Arquivo nao encontrado: " + caminho);
        return 0;
    }
    return (int64_t)info.st_size;
}

// arq_eh_diretorio(caminho) -> inteiro
// Verifica se o caminho é um diretório (1 = sim, 0 = não)
JP_EXPORT int64_t arq_eh_diretorio(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);
    struct stat info;
    if (stat(caminho.c_str(), &info) != 0) return 0;
#if JP_WINDOWS
    return (info.st_mode & _S_IFDIR) ? 1 : 0;
#else
    return S_ISDIR(info.st_mode) ? 1 : 0;
#endif
}

// =============================================================================
// FUNÇÕES EXPORTADAS - MANIPULAÇÃO
// =============================================================================

// arq_apagar(caminho) -> inteiro
// Apaga um arquivo (1 = sucesso, 0 = falha)
JP_EXPORT int64_t arq_apagar(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);
    if (remove(caminho.c_str()) == 0) return 1;
    set_erro("Nao foi possivel apagar: " + caminho);
    return 0;
}

// arq_renomear(antigo, novo) -> inteiro
// Renomeia ou move um arquivo (1 = sucesso, 0 = falha)
JP_EXPORT int64_t arq_renomear(int64_t antigo_ptr, int64_t novo_ptr) {
    std::string antigo((const char*)antigo_ptr);
    std::string novo((const char*)novo_ptr);
    if (rename(antigo.c_str(), novo.c_str()) == 0) return 1;
    set_erro("Nao foi possivel renomear: " + antigo + " -> " + novo);
    return 0;
}

// arq_copiar(origem, destino) -> inteiro
// Copia um arquivo (1 = sucesso, 0 = falha)
JP_EXPORT int64_t arq_copiar(int64_t origem_ptr, int64_t destino_ptr) {
    std::string origem((const char*)origem_ptr);
    std::string destino((const char*)destino_ptr);

    std::ifstream src(origem, std::ios::binary);
    if (!src.is_open()) {
        set_erro("Nao foi possivel abrir origem: " + origem);
        return 0;
    }

    std::ofstream dst(destino, std::ios::binary | std::ios::trunc);
    if (!dst.is_open()) {
        set_erro("Nao foi possivel criar destino: " + destino);
        return 0;
    }

    dst << src.rdbuf();
    src.close();
    dst.close();
    return 1;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - DIRETÓRIOS
// =============================================================================

// arq_criar_dir(caminho) -> inteiro
// Cria um diretório (1 = sucesso ou já existe, 0 = falha)
JP_EXPORT int64_t arq_criar_dir(int64_t caminho_ptr) {
    std::string caminho((const char*)caminho_ptr);

    // Verifica se já existe
    struct stat info;
    if (stat(caminho.c_str(), &info) == 0) return 1;

    if (MKDIR(caminho.c_str()) == 0) return 1;
    set_erro("Nao foi possivel criar diretorio: " + caminho);
    return 0;
}

// =============================================================================
// FUNÇÕES EXPORTADAS - TEMPORÁRIOS
// =============================================================================

// arq_temp(extensao) -> texto
// Gera um nome de arquivo temporário único com a extensão dada
JP_EXPORT int64_t arq_temp(int64_t ext_ptr) {
    std::string ext((const char*)ext_ptr);
    static int counter = 0;
    static std::mutex temp_mutex;

    std::lock_guard<std::mutex> lock(temp_mutex);
    counter++;

    char buf[256];
    snprintf(buf, sizeof(buf), "_jp_temp_%d_%d%s", (int)time(nullptr), counter, ext.c_str());
    return retorna_str(std::string(buf));
}

// =============================================================================
// FUNÇÕES EXPORTADAS - UTILITÁRIOS
// =============================================================================

// arq_erro() -> texto
// Retorna o último erro ocorrido
JP_EXPORT int64_t arq_erro() {
    std::lock_guard<std::mutex> lock(erro_mutex);
    return retorna_str(ultimo_erro);
}

// arq_versao() -> texto
// Retorna a versão da biblioteca
JP_EXPORT int64_t arq_versao() {
    return retorna_str("arquivo.obj 1.0 - JPLang File I/O");
}
