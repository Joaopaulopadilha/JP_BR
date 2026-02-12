// lang_loader.hpp
// Carregador de idiomas - lê arquivos JSON de lang/ e monta mapas de keywords, builtins e saída

#ifndef LANG_LOADER_HPP
#define LANG_LOADER_HPP

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include "opcodes.hpp"

namespace fs = std::filesystem;

// === MAPAS GLOBAIS DE IDIOMA ===

// Mapa: palavra no idioma -> TokenType
inline std::unordered_map<std::string, TokenType> langKeywords;

// Mapa: nome builtin no idioma -> nome interno (entrada, inteiro, decimal, texto, booleano, tipo)
inline std::unordered_map<std::string, std::string> langBuiltins;

// Configuração de saída
inline std::string langSaidaPrefixo = "saida";
inline std::string langSaidaSufixoSemQuebra = "l";
inline std::unordered_map<std::string, std::string> langSaidaCores;

// Nomes dos tipos no idioma (chave interna -> nome no idioma)
inline std::unordered_map<std::string, std::string> langTipos;

// Mensagens de erro no idioma
inline std::unordered_map<std::string, std::string> langErros;

// Idioma atual
inline std::string langIdioma = "portugues";

// Helper: retorna mensagem de erro traduzida, substituindo placeholders
// Uso: langErro("expressao_invalida", {{"valor", "xyz"}})
inline std::string langErro(const std::string& chave, 
    const std::unordered_map<std::string, std::string>& params = {}) {
    
    auto it = langErros.find(chave);
    std::string msg = (it != langErros.end()) ? it->second : chave;
    
    // Substitui placeholders {nome} pelos valores
    for (const auto& [nome, valor] : params) {
        std::string placeholder = "{" + nome + "}";
        size_t pos = msg.find(placeholder);
        while (pos != std::string::npos) {
            msg.replace(pos, placeholder.length(), valor);
            pos = msg.find(placeholder, pos + valor.length());
        }
    }
    
    return msg;
}

// Helper: formata erro com número da linha
inline std::string langErroLinha(int linha, const std::string& chave,
    const std::unordered_map<std::string, std::string>& params = {}) {
    
    std::string prefixo = langErro("linha", {{"num", std::to_string(linha)}});
    std::string msg = langErro(chave, params);
    return prefixo + ": " + msg;
}

// === SNAPSHOT DO ESTADO DE IDIOMA ===
// Permite salvar e restaurar todo o estado de idioma ao parsear módulos importados
struct LangSnapshot {
    std::string idioma;
    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<std::string, std::string> builtins;
    std::string saidaPrefixo;
    std::string saidaSufixoSemQuebra;
    std::unordered_map<std::string, std::string> saidaCores;
    std::unordered_map<std::string, std::string> tipos;
    std::unordered_map<std::string, std::string> erros;
};

// Salva o estado atual de idioma em um snapshot
inline LangSnapshot langSalvarEstado() {
    LangSnapshot snap;
    snap.idioma = langIdioma;
    snap.keywords = langKeywords;
    snap.builtins = langBuiltins;
    snap.saidaPrefixo = langSaidaPrefixo;
    snap.saidaSufixoSemQuebra = langSaidaSufixoSemQuebra;
    snap.saidaCores = langSaidaCores;
    snap.tipos = langTipos;
    snap.erros = langErros;
    return snap;
}

// Restaura o estado de idioma a partir de um snapshot
inline void langRestaurarEstado(const LangSnapshot& snap) {
    langIdioma = snap.idioma;
    langKeywords = snap.keywords;
    langBuiltins = snap.builtins;
    langSaidaPrefixo = snap.saidaPrefixo;
    langSaidaSufixoSemQuebra = snap.saidaSufixoSemQuebra;
    langSaidaCores = snap.saidaCores;
    langTipos = snap.tipos;
    langErros = snap.erros;
}

class LangLoader {
public:

    // Detecta diretiva de idioma na primeira linha do source (ex: #english)
    // Retorna o nome do idioma ou "portugues" se não encontrar
    static std::string detectarIdioma(const std::string& source) {
        if (source.empty() || source[0] != '$') return "portugues";
        
        // Lê a primeira linha
        size_t fim = source.find('\n');
        std::string primeiraLinha = (fim != std::string::npos) 
            ? source.substr(0, fim) 
            : source;
        
        // Remove o $ e espaços
        std::string idioma = primeiraLinha.substr(1);
        
        // Trim
        while (!idioma.empty() && idioma[0] == ' ') idioma = idioma.substr(1);
        while (!idioma.empty() && idioma.back() == ' ') idioma.pop_back();
        while (!idioma.empty() && idioma.back() == '\r') idioma.pop_back();
        
        if (idioma.empty()) return "";
        
        return idioma;
    }

    // Carrega um arquivo de idioma JSON
    // baseDir: diretório base onde está a pasta lang/
    static bool carregar(const std::string& idioma, const std::string& baseDir) {
        // Limpa mapas anteriores
        langKeywords.clear();
        langBuiltins.clear();
        langSaidaCores.clear();
        langTipos.clear();
        langErros.clear();
        langSaidaPrefixo = "saida";
        langSaidaSufixoSemQuebra = "l";
        langIdioma = idioma;
        
        // Tenta encontrar o arquivo JSON
        std::string jsonPath = encontrarJSON(idioma, baseDir);
        if (jsonPath.empty()) {
            return false;
        }
        
        // Lê o arquivo
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();
        
        // Parseia o JSON
        return parsearJSON(json);
    }
    
    // Carrega o idioma padrão (português hardcoded) sem precisar de arquivo
    static void carregarPadrao() {
        langKeywords.clear();
        langBuiltins.clear();
        langSaidaCores.clear();
        langTipos.clear();
        langErros.clear();
        langIdioma = "portugues";
        
        // Keywords
        langKeywords["verdadeiro"] = TokenType::TRUE;
        langKeywords["falso"] = TokenType::FALSE;
        langKeywords["se"] = TokenType::SE;
        langKeywords["senao"] = TokenType::SENAO;
        langKeywords["ou_se"] = TokenType::OU_SE;
        langKeywords["e"] = TokenType::AND;
        langKeywords["ou"] = TokenType::OR;
        langKeywords["loop"] = TokenType::LOOP;
        langKeywords["enquanto"] = TokenType::ENQUANTO;
        langKeywords["repetir"] = TokenType::REPETIR;
        langKeywords["para"] = TokenType::PARA;
        langKeywords["em"] = TokenType::EM;
        langKeywords["intervalo"] = TokenType::INTERVALO;
        langKeywords["parar"] = TokenType::PARAR;
        langKeywords["continuar"] = TokenType::CONTINUAR;
        langKeywords["funcao"] = TokenType::FUNCAO;
        langKeywords["retorna"] = TokenType::RETORNA;
        langKeywords["classe"] = TokenType::CLASSE;
        langKeywords["auto"] = TokenType::AUTO;
        langKeywords["int"] = TokenType::TYPE_INT;
        langKeywords["float"] = TokenType::TYPE_FLOAT;
        langKeywords["str"] = TokenType::TYPE_STR;
        langKeywords["bool"] = TokenType::TYPE_BOOL;
        langKeywords["nativo"] = TokenType::NATIVO;
        langKeywords["importar"] = TokenType::IMPORTAR;
        langKeywords["de"] = TokenType::DE;
        langKeywords["como"] = TokenType::COMO;
        
        // Builtins
        langBuiltins["entrada"] = "entrada";
        langBuiltins["inteiro"] = "inteiro";
        langBuiltins["int"] = "inteiro";
        langBuiltins["decimal"] = "decimal";
        langBuiltins["dec"] = "decimal";
        langBuiltins["texto"] = "texto";
        langBuiltins["booleano"] = "booleano";
        langBuiltins["bool"] = "booleano";
        langBuiltins["tipo"] = "tipo";
        
        // Saída
        langSaidaPrefixo = "saida";
        langSaidaSufixoSemQuebra = "l";
        langSaidaCores["_amarelo"] = "YELLOW";
        langSaidaCores["_vermelho"] = "RED";
        langSaidaCores["_azul"] = "BLUE";
        langSaidaCores["_verde"] = "GREEN";
        
        // Tipos
        langTipos["inteiro"] = "inteiro";
        langTipos["decimal"] = "decimal";
        langTipos["texto"] = "texto";
        langTipos["booleano"] = "booleano";
        langTipos["lista"] = "lista";
        langTipos["objeto"] = "objeto";
        langTipos["ponteiro"] = "ponteiro";
        langTipos["nulo"] = "nulo";
        
        // Erros
        langErros["linha"] = "Linha {num}";
        langErros["encontrado"] = "Encontrado: '{valor}'";
        langErros["esperado"] = "Esperado '{valor}'";
        langErros["esperado_apos_args"] = "Esperado ')' apos argumentos";
        langErros["esperado_apos_indice"] = "Esperado ']' apos indice";
        langErros["esperado_apos_lista"] = "Esperado ']' apos elementos da lista";
        langErros["esperado_nome_atributo"] = "Esperado nome do atributo";
        langErros["esperado_nome_membro"] = "Esperado nome do membro";
        langErros["esperado_comando_saida"] = "Esperado comando de saida";
        langErros["builtin_espera_args"] = "{funcao}() espera {num} argumento(s)";
        langErros["expressao_invalida"] = "Expressao invalida: {valor}";
        langErros["comando_desconhecido"] = "Comando desconhecido ou inesperado: {valor}";
        langErros["indentacao_invalida"] = "Erro de indentacao invalida";
        langErros["caractere_inesperado"] = "Caractere inesperado: {valor}";
        langErros["arquivo_nao_encontrado"] = "Arquivo nao encontrado: {valor}";
        langErros["divisao_por_zero"] = "Divisao por zero";
        langErros["funcao_nao_definida"] = "Funcao nao definida: {valor}";
        langErros["funcao_colisao"] = "Funcao '{funcao}' encontrada em: {libs}. Usando: {usada}";
    }

private:

    // Encontra o arquivo JSON do idioma
    static std::string encontrarJSON(const std::string& idioma, const std::string& baseDir) {
        std::vector<fs::path> caminhos = {
            fs::path(baseDir) / "lang" / (idioma + ".json"),
            fs::path("lang") / (idioma + ".json"),
            fs::path(baseDir) / (idioma + ".json")
        };
        
        for (const auto& caminho : caminhos) {
            if (fs::exists(caminho)) {
                return caminho.string();
            }
        }
        return "";
    }

    // === PARSER JSON MINIMALISTA ===
    // Suporta apenas a estrutura esperada: objetos com strings aninhados 1 nível
    
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || 
               json[pos] == '\r' || json[pos] == '\t')) {
            pos++;
        }
    }
    
    static std::string readString(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '"') return "";
        pos++; // pula "
        
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        
        if (pos < json.size()) pos++; // pula "
        return result;
    }
    
    // Converte string do JSON para TokenType
    static TokenType stringParaTokenType(const std::string& s) {
        if (s == "TRUE") return TokenType::TRUE;
        if (s == "FALSE") return TokenType::FALSE;
        if (s == "SE") return TokenType::SE;
        if (s == "SENAO") return TokenType::SENAO;
        if (s == "OU_SE") return TokenType::OU_SE;
        if (s == "AND") return TokenType::AND;
        if (s == "OR") return TokenType::OR;
        if (s == "LOOP") return TokenType::LOOP;
        if (s == "ENQUANTO") return TokenType::ENQUANTO;
        if (s == "REPETIR") return TokenType::REPETIR;
        if (s == "PARA") return TokenType::PARA;
        if (s == "EM") return TokenType::EM;
        if (s == "INTERVALO") return TokenType::INTERVALO;
        if (s == "PARAR") return TokenType::PARAR;
        if (s == "CONTINUAR") return TokenType::CONTINUAR;
        if (s == "FUNCAO") return TokenType::FUNCAO;
        if (s == "RETORNA") return TokenType::RETORNA;
        if (s == "CLASSE") return TokenType::CLASSE;
        if (s == "AUTO") return TokenType::AUTO;
        if (s == "TYPE_INT") return TokenType::TYPE_INT;
        if (s == "TYPE_FLOAT") return TokenType::TYPE_FLOAT;
        if (s == "TYPE_STR") return TokenType::TYPE_STR;
        if (s == "TYPE_BOOL") return TokenType::TYPE_BOOL;
        if (s == "NATIVO") return TokenType::NATIVO;
        if (s == "IMPORTAR") return TokenType::IMPORTAR;
        if (s == "DE") return TokenType::DE;
        if (s == "COMO") return TokenType::COMO;
        return TokenType::ID;
    }
    
    // Parseia um objeto simples { "key": "value", ... }
    static std::unordered_map<std::string, std::string> readObject(const std::string& json, size_t& pos) {
        std::unordered_map<std::string, std::string> result;
        
        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != '{') return result;
        pos++; // pula {
        
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            if (json[pos] == '}') { pos++; break; }
            if (json[pos] == ',') { pos++; continue; }
            
            std::string key = readString(json, pos);
            skipWhitespace(json, pos);
            
            if (pos < json.size() && json[pos] == ':') pos++; // pula :
            skipWhitespace(json, pos);
            
            if (pos < json.size() && json[pos] == '{') {
                // Sub-objeto: pula (será tratado por seção específica)
                // Guarda posição para re-parsear
                size_t subStart = pos;
                int depth = 1;
                pos++;
                while (pos < json.size() && depth > 0) {
                    if (json[pos] == '{') depth++;
                    if (json[pos] == '}') depth--;
                    pos++;
                }
                result[key] = json.substr(subStart, pos - subStart);
            } else if (pos < json.size() && json[pos] == '"') {
                result[key] = readString(json, pos);
            }
        }
        
        return result;
    }
    
    // Parseia o JSON completo do idioma
    static bool parsearJSON(const std::string& json) {
        size_t pos = 0;
        skipWhitespace(json, pos);
        
        if (pos >= json.size() || json[pos] != '{') return false;
        pos++; // pula { raiz
        
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            if (json[pos] == '}') break;
            if (json[pos] == ',') { pos++; continue; }
            
            std::string secao = readString(json, pos);
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ':') pos++;
            skipWhitespace(json, pos);
            
            if (secao == "idioma") {
                langIdioma = readString(json, pos);
            }
            else if (secao == "palavras") {
                auto mapa = readObject(json, pos);
                for (const auto& [palavra, tokenStr] : mapa) {
                    TokenType tt = stringParaTokenType(tokenStr);
                    if (tt != TokenType::ID) {
                        langKeywords[palavra] = tt;
                    }
                }
            }
            else if (secao == "builtins") {
                auto mapa = readObject(json, pos);
                for (const auto& [nome, interno] : mapa) {
                    langBuiltins[nome] = interno;
                }
            }
            else if (secao == "saida") {
                auto mapa = readObject(json, pos);
                
                if (mapa.find("prefixo") != mapa.end()) {
                    langSaidaPrefixo = mapa["prefixo"];
                }
                if (mapa.find("sufixo_sem_quebra") != mapa.end()) {
                    langSaidaSufixoSemQuebra = mapa["sufixo_sem_quebra"];
                }
                if (mapa.find("cores") != mapa.end()) {
                    // Parseia sub-objeto de cores
                    std::string coresJson = mapa["cores"];
                    size_t cpos = 0;
                    auto cores = readObject(coresJson, cpos);
                    for (const auto& [sufixo, cor] : cores) {
                        langSaidaCores[sufixo] = cor;
                    }
                }
            }
            else if (secao == "tipos") {
                auto mapa = readObject(json, pos);
                for (const auto& [interno, nomeIdioma] : mapa) {
                    langTipos[interno] = nomeIdioma;
                }
            }
            else if (secao == "erros") {
                auto mapa = readObject(json, pos);
                for (const auto& [chave, mensagem] : mapa) {
                    langErros[chave] = mensagem;
                }
            }
            else {
                // Seção desconhecida: pula valor
                if (pos < json.size() && json[pos] == '"') {
                    readString(json, pos);
                } else if (pos < json.size() && json[pos] == '{') {
                    int depth = 1;
                    pos++;
                    while (pos < json.size() && depth > 0) {
                        if (json[pos] == '{') depth++;
                        if (json[pos] == '}') depth--;
                        pos++;
                    }
                }
            }
        }
        
        return !langKeywords.empty();
    }
};

#endif