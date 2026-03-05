// jp_extrator.cpp
// Biblioteca nativa para extrair funcoes JP_EXPORT de codigo C++ e gerar JSON de biblioteca JPLang
//
// Compilar:
//   Windows: g++ -c -o bibliotecas/jp_extrator/jp_extrator.obj bibliotecas/jp_extrator/jp_extrator.cpp -O3
//   Linux:   g++ -c -fPIC -o bibliotecas/jp_extrator/jp_extrator.o bibliotecas/jp_extrator/jp_extrator.cpp -O3

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
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================
static const int NUM_BUFS = 8;
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
// ESTRUTURAS INTERNAS
// =============================================================================
struct FuncaoExportada {
    std::string nome;
    std::string retorno_jp;
    std::vector<std::string> params_jp;
};

// Último resultado (persistente entre chamadas)
static std::vector<FuncaoExportada> ultimas_funcoes;
static std::string ultimo_json;

// =============================================================================
// CONVERSÃO DE TIPOS C -> JPLang
// =============================================================================
static std::string tipo_c_para_jp(const std::string& tipo) {
    std::string t = tipo;

    // Remove espaços extras
    while (!t.empty() && t[0] == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    if (t == "int64_t" || t == "int" || t == "long" || t == "long long")
        return "inteiro";
    if (t == "double" || t == "float")
        return "float";
    if (t == "void")
        return "vazio";
    if (t == "bool")
        return "bool";
    if (t.find("char") != std::string::npos && t.find("*") != std::string::npos)
        return "texto";
    if (t.find("char") != std::string::npos)
        return "texto";

    return "inteiro"; // fallback
}

// =============================================================================
// PARSER DE PARÂMETROS
// =============================================================================
static std::vector<std::string> parsear_params(const std::string& params_str) {
    std::vector<std::string> resultado;

    std::string s = params_str;
    // Trim
    while (!s.empty() && s[0] == ' ') s.erase(0, 1);
    while (!s.empty() && s.back() == ' ') s.pop_back();

    if (s.empty() || s == "void") return resultado;

    // Divide por vírgula
    std::stringstream ss(s);
    std::string param;
    while (std::getline(ss, param, ',')) {
        // Trim
        while (!param.empty() && param[0] == ' ') param.erase(0, 1);
        while (!param.empty() && param.back() == ' ') param.pop_back();

        if (param.empty()) continue;

        // Extrai o tipo (tudo menos a última palavra que é o nome)
        size_t ultimo_espaco = param.rfind(' ');
        std::string tipo_param;
        if (ultimo_espaco != std::string::npos) {
            tipo_param = param.substr(0, ultimo_espaco);
            // Remove * do nome se ficou no tipo
            while (!tipo_param.empty() && tipo_param.back() == ' ') tipo_param.pop_back();
        } else {
            tipo_param = param;
        }

        resultado.push_back(tipo_c_para_jp(tipo_param));
    }

    return resultado;
}

// =============================================================================
// EXTRATOR DE FUNÇÕES JP_EXPORT
// =============================================================================
static std::vector<FuncaoExportada> extrair_funcoes(const std::string& codigo) {
    std::vector<FuncaoExportada> funcoes;
    size_t pos = 0;

    while (pos < codigo.size()) {
        // Procura JP_EXPORT
        size_t idx = codigo.find("JP_EXPORT", pos);
        if (idx == std::string::npos) break;

        pos = idx + 9; // pula "JP_EXPORT"

        // Pula espaços
        while (pos < codigo.size() && codigo[pos] == ' ') pos++;

        // Encontra o parêntese de abertura
        size_t paren = codigo.find('(', pos);
        if (paren == std::string::npos) break;

        // Tudo entre pos e paren é "tipo nome"
        std::string antes_paren = codigo.substr(pos, paren - pos);
        // Trim
        while (!antes_paren.empty() && antes_paren[0] == ' ') antes_paren.erase(0, 1);
        while (!antes_paren.empty() && antes_paren.back() == ' ') antes_paren.pop_back();

        // Separa tipo e nome pelo último espaço
        size_t ultimo_espaco = antes_paren.rfind(' ');
        if (ultimo_espaco == std::string::npos) {
            pos = paren + 1;
            continue;
        }

        std::string tipo_ret = antes_paren.substr(0, ultimo_espaco);
        std::string nome_func = antes_paren.substr(ultimo_espaco + 1);

        // Trim
        while (!tipo_ret.empty() && tipo_ret.back() == ' ') tipo_ret.pop_back();
        while (!nome_func.empty() && nome_func[0] == ' ') nome_func.erase(0, 1);

        // Ignora funções internas (retorna_str, etc)
        if (nome_func.empty() || nome_func[0] == '_') {
            pos = paren + 1;
            continue;
        }

        // Encontra o parêntese de fechamento
        size_t fecha = codigo.find(')', paren);
        if (fecha == std::string::npos) break;

        std::string params_str = codigo.substr(paren + 1, fecha - paren - 1);

        FuncaoExportada f;
        f.nome = nome_func;
        f.retorno_jp = tipo_c_para_jp(tipo_ret);
        f.params_jp = parsear_params(params_str);

        funcoes.push_back(f);
        pos = fecha + 1;
    }

    return funcoes;
}

// =============================================================================
// GERADOR DE JSON
// =============================================================================
static std::string gerar_json(const std::vector<FuncaoExportada>& funcoes) {
    std::string json = "{\n";
    json += "    \"tipo\": \"estatico\",\n";
    json += "    \"funcoes\": [\n";

    for (size_t i = 0; i < funcoes.size(); i++) {
        const auto& f = funcoes[i];

        json += "        { \"nome\": \"" + f.nome + "\", ";
        json += "\"retorno\": \"" + f.retorno_jp + "\", ";
        json += "\"params\": [";

        for (size_t p = 0; p < f.params_jp.size(); p++) {
            if (p > 0) json += ", ";
            json += "\"" + f.params_jp[p] + "\"";
        }

        json += "] }";
        if (i < funcoes.size() - 1) json += ",";
        json += "\n";
    }

    json += "    ],\n";
    json += "    \"libs\": [],\n";
    json += "    \"libs_linux\": []\n";
    json += "}";

    return json;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// ext_extrair(codigo) -> inteiro
// Extrai funções JP_EXPORT do código C++ e armazena internamente
// Retorna o número de funções encontradas
JP_EXPORT int64_t ext_extrair(int64_t codigo_ptr) {
    std::string codigo((const char*)codigo_ptr);
    ultimas_funcoes = extrair_funcoes(codigo);
    ultimo_json = gerar_json(ultimas_funcoes);
    return (int64_t)ultimas_funcoes.size();
}

// ext_json() -> texto
// Retorna o JSON gerado pela última extração
JP_EXPORT int64_t ext_json() {
    return retorna_str(ultimo_json);
}

// ext_total() -> inteiro
// Retorna o total de funções da última extração
JP_EXPORT int64_t ext_total() {
    return (int64_t)ultimas_funcoes.size();
}

// ext_nome(indice) -> texto
// Retorna o nome da função no índice especificado
JP_EXPORT int64_t ext_nome(int64_t indice) {
    int idx = (int)indice;
    if (idx < 0 || idx >= (int)ultimas_funcoes.size()) return retorna_str("");
    return retorna_str(ultimas_funcoes[idx].nome);
}

// ext_retorno(indice) -> texto
// Retorna o tipo de retorno JPLang da função no índice especificado
JP_EXPORT int64_t ext_retorno(int64_t indice) {
    int idx = (int)indice;
    if (idx < 0 || idx >= (int)ultimas_funcoes.size()) return retorna_str("");
    return retorna_str(ultimas_funcoes[idx].retorno_jp);
}

// ext_params(indice) -> texto
// Retorna os parâmetros da função como string separada por vírgula
JP_EXPORT int64_t ext_params(int64_t indice) {
    int idx = (int)indice;
    if (idx < 0 || idx >= (int)ultimas_funcoes.size()) return retorna_str("");

    std::string resultado;
    for (size_t i = 0; i < ultimas_funcoes[idx].params_jp.size(); i++) {
        if (i > 0) resultado += ",";
        resultado += ultimas_funcoes[idx].params_jp[i];
    }
    return retorna_str(resultado);
}

// ext_versao() -> texto
JP_EXPORT int64_t ext_versao() {
    return retorna_str("jp_extrator 1.0 - JPLang Function Extractor");
}
