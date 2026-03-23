// lexer.hpp
// Tokenizador (lexer) com suporte a indentação para JPLang v1.0

#ifndef JPLANG_LEXER_HPP
#define JPLANG_LEXER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <fstream>

namespace jplang {

// ============================================================================
// TIPOS DE TOKEN
// ============================================================================

enum class TK {
    // Controle
    TK_EOF,
    ERROR,

    // Literais
    IDENT,
    STRING,
    STRING_INTERP,      // string com interpolação (partes separadas)
    NUMBER,
    BOOL,
    NULO,               // nulo (null)

    // Símbolos
    LPAREN,             // (
    RPAREN,             // )
    COMMA,              // ,
    COLON,              // :
    EQUALS,             // =
    PLUS,               // +
    MINUS,              // -
    STAR,               // *
    SLASH,              // /
    PERCENT,            // %
    DOT,                // .
    LBRACKET,           // [
    RBRACKET,           // ]

    // Comparação
    EQ,                 // ==
    NE,                 // !=
    GT,                 // >
    LT,                 // <
    GE,                 // >=
    LE,                 // <=

    // Lógicos
    AND,                // e
    OR,                 // ou

    // Controle de fluxo
    SE,                 // se
    OU_SE,              // ou_se
    SENAO,              // senao

    // Loops
    REPETIR,            // repetir
    ENQUANTO,           // enquanto
    PARA,               // para
    EM,                 // em
    INTERVALO,          // intervalo
    PARAR,              // parar
    CONTINUAR,          // continuar

    // Funções
    FUNCAO,             // funcao
    RETORNA,            // retorna

    // Classes
    CLASSE,             // classe
    AUTO,               // auto (self/this)

    // Nativos e importação
    NATIVO,             // nativo
    IMPORTAR,           // importar

    // Indentação
    NEWLINE,
    INDENT,
    DEDENT,
};

// ============================================================================
// PARTE DE INTERPOLAÇÃO (para STRING_INTERP)
// ============================================================================

struct TokenInterpPart {
    bool is_var;                // true = variável, false = texto
    std::string value;
};

// ============================================================================
// TOKEN
// ============================================================================

struct Token {
    TK type;
    std::string value;
    int line;

    // Partes de interpolação (só usado quando type == STRING_INTERP)
    std::vector<TokenInterpPart> interp_parts;

    Token() : type(TK::TK_EOF), value(""), line(1) {}

    Token(TK type, const std::string& value, int line)
        : type(type), value(value), line(line) {}

    Token(TK type, std::vector<TokenInterpPart>&& parts, int line)
        : type(type), value(""), line(line), interp_parts(std::move(parts)) {}
};

// ============================================================================
// MAPA DE PALAVRAS-CHAVE PADRÃO (português)
// ============================================================================

inline std::unordered_map<std::string, TK> default_keywords() {
    return {
        {"verdadeiro", TK::BOOL},
        {"falso",      TK::BOOL},
        {"nulo",       TK::NULO},
        {"se",         TK::SE},
        {"ou_se",      TK::OU_SE},
        {"senao",      TK::SENAO},
        {"repetir",    TK::REPETIR},
        {"enquanto",   TK::ENQUANTO},
        {"para",       TK::PARA},
        {"em",         TK::EM},
        {"intervalo",  TK::INTERVALO},
        {"parar",      TK::PARAR},
        {"continuar",  TK::CONTINUAR},
        {"funcao",     TK::FUNCAO},
        {"retorna",    TK::RETORNA},
        {"classe",     TK::CLASSE},
        {"auto",       TK::AUTO},
        {"nativo",     TK::NATIVO},
        {"importar",   TK::IMPORTAR},
    };
}

// ============================================================================
// MAPA TOKEN INTERNO → TK (para converter JSON "palavras" → TK)
// ============================================================================

inline TK tk_from_internal_name(const std::string& name) {
    static const std::unordered_map<std::string, TK> map = {
        {"TRUE",       TK::BOOL},
        {"FALSE",      TK::BOOL},
        {"NULO",       TK::NULO},
        {"SE",         TK::SE},
        {"SENAO",      TK::SENAO},
        {"OU_SE",      TK::OU_SE},
        {"AND",        TK::AND},
        {"OR",         TK::OR},
        {"LOOP",       TK::REPETIR},
        {"ENQUANTO",   TK::ENQUANTO},
        {"REPETIR",    TK::REPETIR},
        {"PARA",       TK::PARA},
        {"EM",         TK::EM},
        {"INTERVALO",  TK::INTERVALO},
        {"PARAR",      TK::PARAR},
        {"CONTINUAR",  TK::CONTINUAR},
        {"FUNCAO",     TK::FUNCAO},
        {"RETORNA",    TK::RETORNA},
        {"CLASSE",     TK::CLASSE},
        {"AUTO",       TK::AUTO},
        {"NATIVO",     TK::NATIVO},
        {"IMPORTAR",   TK::IMPORTAR},
    };
    auto it = map.find(name);
    if (it != map.end()) return it->second;
    return TK::IDENT;
}

// ============================================================================
// CONFIGURAÇÃO DE IDIOMA
// ============================================================================

struct LangConfig {
    std::string idioma = "portugues";
    std::unordered_map<std::string, TK> keywords;

    // builtins: palavra_no_idioma → nome_interno (ex: "entrada" → "entrada")
    std::unordered_map<std::string, std::string> builtins;

    // saida
    std::string saida_prefix = "saida";
    std::string saida_no_newline_suffix = "l";
    std::unordered_map<std::string, std::string> saida_cores;
    // saida_cores: sufixo_no_idioma → cor_interna
    // ex: "_rojo" → "vermelho", "_amarillo" → "amarelo"

    // tipos: nome_interno → nome_no_idioma (para saída em runtime)
    // ex (ES): "inteiro" → "entero", "texto" → "cadena"
    // ex (EN): "inteiro" → "integer", "texto" → "string"
    std::unordered_map<std::string, std::string> tipos;

    // diagnostico: chave_msg → mensagem traduzida (para sistema de diagnostico FFI)
    std::unordered_map<std::string, std::string> diagnostico;

    // Nomes de booleanos no idioma (para conversão texto→bool em runtime)
    // ex (PT): "verdadeiro" / "falso", (ES): "verdadero" / "falso", (EN): "true" / "false"
    std::string bool_true = "verdadeiro";
    std::string bool_false = "falso";
    std::string null_keyword = "nulo";

    // Mapa de cor interna → Cor (para o parser)
    // ex: "YELLOW" → "amarelo", "RED" → "vermelho"
    static std::string internal_cor_to_pt(const std::string& cor) {
        if (cor == "YELLOW") return "amarelo";
        if (cor == "RED")    return "vermelho";
        if (cor == "BLUE")   return "azul";
        if (cor == "GREEN")  return "verde";
        return "";
    }
};

// ============================================================================
// PARSER JSON MÍNIMO (sem dependências externas)
// ============================================================================

inline std::string json_extract_string(const std::string& json, size_t& pos) {
    // Pula espaços e ':'
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' ||
           json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++; // pula "
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
        }
        result += json[pos];
        pos++;
    }
    if (pos < json.size()) pos++; // pula " final
    return result;
}

// Extrai todos os pares chave:valor de um bloco { ... }
// Retorna posição após o }
inline std::unordered_map<std::string, std::string> json_extract_object(const std::string& json, size_t& pos) {
    std::unordered_map<std::string, std::string> result;

    // Encontrar {
    while (pos < json.size() && json[pos] != '{') pos++;
    if (pos >= json.size()) return result;
    pos++; // pula {

    while (pos < json.size()) {
        // Pula espaços
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
               json[pos] == '\n' || json[pos] == '\r' || json[pos] == ',')) {
            pos++;
        }
        if (pos >= json.size() || json[pos] == '}') { pos++; break; }

        // Chave
        if (json[pos] != '"') break;
        std::string key = json_extract_string(json, pos);

        // Valor — pode ser string ou sub-objeto
        // Pula espaços e ':'
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' ||
               json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }

        if (pos < json.size() && json[pos] == '"') {
            std::string val = json_extract_string(json, pos);
            result[key] = val;
        } else if (pos < json.size() && json[pos] == '{') {
            // Sub-objeto: pular (será parseado separadamente)
            int depth = 0;
            size_t start = pos;
            while (pos < json.size()) {
                if (json[pos] == '{') depth++;
                else if (json[pos] == '}') { depth--; if (depth == 0) { pos++; break; } }
                pos++;
            }
            // Guardar como string bruta para re-parsear
            result[key] = json.substr(start, pos - start);
        }
    }
    return result;
}

// ============================================================================
// CARREGAMENTO DE CONFIGURAÇÃO DE IDIOMA
// ============================================================================

inline LangConfig load_lang_config(const std::string& lang_name, const std::string& base_dir = "") {
    LangConfig config;

    // Tentar encontrar o arquivo JSON
    std::vector<std::string> paths_to_try;
    if (!base_dir.empty()) {
        paths_to_try.push_back(base_dir + "/src/lang/" + lang_name + ".json");
        paths_to_try.push_back(base_dir + "/lang/" + lang_name + ".json");
    }
    paths_to_try.push_back("src/lang/" + lang_name + ".json");
    paths_to_try.push_back("lang/" + lang_name + ".json");

    std::string json_content;
    for (auto& path : paths_to_try) {
        std::ifstream file(path);
        if (file.is_open()) {
            json_content = std::string(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            break;
        }
    }

    if (json_content.empty()) {
        // Fallback: retorna config padrão (português)
        config.keywords = default_keywords();
        config.saida_prefix = "saida";
        config.saida_no_newline_suffix = "l";
        config.saida_cores = {
            {"_amarelo", "amarelo"}, {"_vermelho", "vermelho"},
            {"_azul", "azul"}, {"_verde", "verde"}
        };
        config.tipos = {
            {"inteiro", "inteiro"}, {"decimal", "decimal"},
            {"texto", "texto"}, {"booleano", "booleano"},
            {"lista", "lista"}, {"objeto", "objeto"},
            {"ponteiro", "ponteiro"}, {"nulo", "nulo"}
        };
        return config;
    }

    // Parsear JSON
    size_t pos = 0;
    auto root = json_extract_object(json_content, pos);

    config.idioma = root.count("idioma") ? root["idioma"] : lang_name;

    // Parsear "palavras" → keywords
    if (root.count("palavras")) {
        size_t ppos = 0;
        std::string palavras_json = root["palavras"];
        auto palavras = json_extract_object(palavras_json, ppos);
        for (auto& [word, internal] : palavras) {
            TK tk = tk_from_internal_name(internal);
            if (tk != TK::IDENT) {
                config.keywords[word] = tk;
            }
            // Capturar nomes de booleanos no idioma
            if (internal == "TRUE")  config.bool_true = word;
            if (internal == "FALSE") config.bool_false = word;
            if (internal == "NULO")  config.null_keyword = word;
        }
    }

    // Parsear "builtins"
    if (root.count("builtins")) {
        size_t bpos = 0;
        std::string builtins_json = root["builtins"];
        auto builtins = json_extract_object(builtins_json, bpos);
        for (auto& [word, internal] : builtins) {
            config.builtins[word] = internal;
        }
    }

    // Parsear "saida"
    if (root.count("saida")) {
        size_t spos = 0;
        std::string saida_json = root["saida"];
        auto saida = json_extract_object(saida_json, spos);

        if (saida.count("prefixo"))
            config.saida_prefix = saida["prefixo"];
        if (saida.count("sufixo_sem_quebra"))
            config.saida_no_newline_suffix = saida["sufixo_sem_quebra"];

        if (saida.count("cores")) {
            size_t cpos = 0;
            auto cores = json_extract_object(saida["cores"], cpos);
            for (auto& [suffix, internal_cor] : cores) {
                // suffix = "_rojo", internal_cor = "RED"
                // Converter para nome PT: "RED" → "vermelho"
                std::string pt_cor = LangConfig::internal_cor_to_pt(internal_cor);
                if (!pt_cor.empty()) {
                    config.saida_cores[suffix] = pt_cor;
                }
            }
        }
    }

    // Parsear "tipos" → nomes de tipos no idioma
    if (root.count("tipos")) {
        size_t tpos = 0;
        std::string tipos_json = root["tipos"];
        auto tipos = json_extract_object(tipos_json, tpos);
        for (auto& [interno, traduzido] : tipos) {
            config.tipos[interno] = traduzido;
        }
    }

    // Parsear "diagnostico" → mensagens do sistema de diagnostico FFI
    if (root.count("diagnostico")) {
        size_t dpos = 0;
        std::string diag_json = root["diagnostico"];
        auto diag = json_extract_object(diag_json, dpos);
        for (auto& [chave, msg] : diag) {
            config.diagnostico[chave] = msg;
        }
    }

    return config;
}

// ============================================================================
// DETECÇÃO DE FLAG DE IDIOMA NA PRIMEIRA LINHA
// Formato: $espanol, $english, etc.
// Retorna o nome do idioma e a posição após a flag
// ============================================================================

inline std::string detect_lang_flag(const std::string& source, size_t& skip_pos) {
    skip_pos = 0;
    size_t pos = 0;

    // Pular BOM UTF-8 se presente
    if (source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        pos = 3;
    }

    // Pular espaços iniciais
    while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t')) {
        pos++;
    }

    // Verificar se começa com $
    if (pos < source.size() && source[pos] == '$') {
        pos++; // pula $
        std::string lang;
        while (pos < source.size() && source[pos] != '\n' && source[pos] != '\r'
               && source[pos] != ' ' && source[pos] != '\t') {
            lang += source[pos];
            pos++;
        }
        // Pular até o fim da linha
        while (pos < source.size() && source[pos] != '\n') pos++;
        if (pos < source.size() && source[pos] == '\n') pos++;

        if (!lang.empty()) {
            skip_pos = pos;
            return lang;
        }
    }

    return "portugues";
}

// ============================================================================
// LEXER
// ============================================================================

class Lexer {
public:
    // Construtor padrão: detecta idioma pela flag $idioma na primeira linha
    explicit Lexer(const std::string& source, const std::string& base_dir = "")
        : src_(source), pos_(0), line_(1),
          pending_dedents_(0), at_line_start_(true), pending_newline_(false),
          bracket_depth_(0)
    {
        indent_stack_.push_back(0);
        // Pular UTF-8 BOM se presente (EF BB BF)
        if (src_.size() >= 3 &&
            static_cast<unsigned char>(src_[0]) == 0xEF &&
            static_cast<unsigned char>(src_[1]) == 0xBB &&
            static_cast<unsigned char>(src_[2]) == 0xBF) {
            pos_ = 3;
        }

        // Detectar flag de idioma
        size_t skip = 0;
        std::string lang = detect_lang_flag(src_, skip);
        lang_config_ = load_lang_config(lang, base_dir);

        // Se encontrou flag, pular a linha da flag
        if (lang != "portugues" && skip > 0) {
            pos_ = skip;
            // Recalcular line_ para a posição correta
            line_ = 2; // flag estava na linha 1
        }
    }

    // Construtor com config pré-carregada (usado por sub-parsers de interpolação)
    Lexer(const std::string& source, const LangConfig& config)
        : src_(source), pos_(0), line_(1),
          pending_dedents_(0), at_line_start_(true), pending_newline_(false),
          bracket_depth_(0), lang_config_(config)
    {
        indent_stack_.push_back(0);
        if (src_.size() >= 3 &&
            static_cast<unsigned char>(src_[0]) == 0xEF &&
            static_cast<unsigned char>(src_[1]) == 0xBB &&
            static_cast<unsigned char>(src_[2]) == 0xBF) {
            pos_ = 3;
        }
    }

    // Acesso à configuração de idioma (para o parser)
    const LangConfig& lang_config() const { return lang_config_; }

    // ========================================================================
    // PRÓXIMO TOKEN
    // ========================================================================

    Token next() {
        // Dedents pendentes
        if (pending_dedents_ > 0) {
            pending_dedents_--;
            return Token(TK::DEDENT, "", line_);
        }

        // Newline pendente
        if (pending_newline_) {
            pending_newline_ = false;
            return Token(TK::NEWLINE, "", line_);
        }

        // Indentação no início da linha (só quando fora de brackets)
        if (at_line_start_) {
            at_line_start_ = false;

            // Dentro de brackets: ignora indentação, só pula whitespace
            if (bracket_depth_ > 0) {
                skip_whitespace();
                return next();
            }

            int indent = count_indent();

            if (indent == -1) {
                skip_empty_line();
                return next();
            }

            int current = indent_stack_.back();

            if (indent > current) {
                indent_stack_.push_back(indent);
                return Token(TK::INDENT, "", line_);
            } else if (indent < current) {
                int dedents = 0;
                while (indent_stack_.size() > 1 && indent_stack_.back() > indent) {
                    indent_stack_.pop_back();
                    dedents++;
                }
                if (indent_stack_.back() != indent) {
                    return Token(TK::ERROR, "Indentação inválida", line_);
                }
                if (dedents > 0) {
                    pending_dedents_ = dedents - 1;
                    return Token(TK::DEDENT, "", line_);
                }
            }
        }

        skip_whitespace();
        skip_comment();

        if (at_end()) {
            if (indent_stack_.size() > 1) {
                pending_dedents_ = static_cast<int>(indent_stack_.size()) - 2;
                indent_stack_ = {0};
                return Token(TK::DEDENT, "", line_);
            }
            return Token(TK::TK_EOF, "", line_);
        }

        char c = peek();

        // Newlines
        if (c == '\n') {
            advance();
            at_line_start_ = true;
            int ln = line_;
            line_++;
            // Dentro de brackets: suprime NEWLINE, continua lendo
            if (bracket_depth_ > 0) {
                return next();
            }
            return Token(TK::NEWLINE, "", ln);
        }
        if (c == '\r') {
            advance();
            if (peek() == '\n') advance();
            at_line_start_ = true;
            int ln = line_;
            line_++;
            // Dentro de brackets: suprime NEWLINE, continua lendo
            if (bracket_depth_ > 0) {
                return next();
            }
            return Token(TK::NEWLINE, "", ln);
        }

        // Símbolos simples (com rastreamento de profundidade de brackets)
        switch (c) {
            case '(':
                advance();
                bracket_depth_++;
                return Token(TK::LPAREN,  "(", line_);
            case ')':
                advance();
                if (bracket_depth_ > 0) bracket_depth_--;
                return Token(TK::RPAREN,  ")", line_);
            case ',': advance(); return Token(TK::COMMA,   ",", line_);
            case ':': advance(); return Token(TK::COLON,   ":", line_);
            case '+': advance(); return Token(TK::PLUS,    "+", line_);
            case '-': advance(); return Token(TK::MINUS,   "-", line_);
            case '*': advance(); return Token(TK::STAR,    "*", line_);
            case '/': advance(); return Token(TK::SLASH,   "/", line_);
            case '%': advance(); return Token(TK::PERCENT, "%", line_);
            case '.': break;    // tratado abaixo
            case '[':
                advance();
                bracket_depth_++;
                return Token(TK::LBRACKET, "[", line_);
            case ']':
                advance();
                if (bracket_depth_ > 0) bracket_depth_--;
                return Token(TK::RBRACKET, "]", line_);
            default: break;
        }

        // Ponto (ou número decimal)
        if (c == '.') {
            if (peek_next_is_digit()) {
                return scan_number();
            }
            advance();
            return Token(TK::DOT, ".", line_);
        }

        // Operadores compostos
        if (c == '=') {
            advance();
            if (peek() == '=') {
                advance();
                return Token(TK::EQ, "==", line_);
            }
            return Token(TK::EQUALS, "=", line_);
        }

        if (c == '!') {
            advance();
            if (peek() == '=') {
                advance();
                return Token(TK::NE, "!=", line_);
            }
            return Token(TK::ERROR, "Esperado '=' após '!'", line_);
        }

        if (c == '>') {
            advance();
            if (peek() == '=') {
                advance();
                return Token(TK::GE, ">=", line_);
            }
            return Token(TK::GT, ">", line_);
        }

        if (c == '<') {
            advance();
            if (peek() == '=') {
                advance();
                return Token(TK::LE, "<=", line_);
            }
            return Token(TK::LT, "<", line_);
        }

        // Operadores lógicos simbólicos
        if (c == '&') {
            advance();
            if (peek() == '&') {
                advance();
                return Token(TK::AND, "&&", line_);
            }
            return Token(TK::ERROR, "Esperado '&' apos '&'", line_);
        }

        if (c == '|') {
            advance();
            if (peek() == '|') {
                advance();
                return Token(TK::OR, "||", line_);
            }
            return Token(TK::ERROR, "Esperado '|' apos '|'", line_);
        }

        // String
        if (c == '"') {
            return scan_string();
        }

        // Número
        if (is_digit(c)) {
            return scan_number();
        }

        // Identificador / palavra-chave (inclui UTF-8 para suporte a acentos em nomes)
        if (is_alpha(c) || c == '_' || is_utf8_lead(c)) {
            return scan_ident();
        }

        // Caractere desconhecido (pular bytes UTF-8 de continuação órfãos)
        if (is_utf8_cont(c)) {
            advance();
            return next();
        }
        advance();
        return Token(TK::ERROR, std::string(1, c), line_);
    }

private:
    std::string src_;
    size_t pos_;
    int line_;
    std::vector<int> indent_stack_;
    int pending_dedents_;
    bool at_line_start_;
    bool pending_newline_;
    int bracket_depth_;             // profundidade de ( ) [ ] — suprime NEWLINE/INDENT/DEDENT quando > 0
    LangConfig lang_config_;        // configuração do idioma ativo

    // ========================================================================
    // AUXILIARES
    // ========================================================================

    bool at_end() const {
        return pos_ >= src_.size();
    }

    char peek() const {
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }

    char peek_next() const {
        return (pos_ + 1) < src_.size() ? src_[pos_ + 1] : '\0';
    }

    bool peek_next_is_digit() const {
        return is_digit(peek_next());
    }

    char advance() {
        return src_[pos_++];
    }

    static bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }

    static bool is_alpha(char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               c == '_';
    }

    static bool is_alnum(char c) {
        return is_digit(c) || is_alpha(c);
    }

    // Verifica se é byte inicial de sequência UTF-8 multibyte (>= 0x80)
    static bool is_utf8_lead(char c) {
        unsigned char u = static_cast<unsigned char>(c);
        return u >= 0xC0;
    }

    // Verifica se é byte de continuação UTF-8 (10xxxxxx)
    static bool is_utf8_cont(char c) {
        unsigned char u = static_cast<unsigned char>(c);
        return (u & 0xC0) == 0x80;
    }

    // Retorna o número de bytes na sequência UTF-8 a partir do byte líder
    static int utf8_seq_len(char c) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) return 1;
        if ((u & 0xE0) == 0xC0) return 2;
        if ((u & 0xF0) == 0xE0) return 3;
        if ((u & 0xF8) == 0xF0) return 4;
        return 1;
    }

    void skip_whitespace() {
        while (peek() == ' ' || peek() == '\t') {
            advance();
        }
    }

    void skip_comment() {
        if (peek() == '#') {
            while (!at_end() && peek() != '\n') {
                advance();
            }
        }
    }

    // ========================================================================
    // INDENTAÇÃO
    // ========================================================================

    int count_indent() {
        int indent = 0;
        size_t start = pos_;

        while (peek() == ' ' || peek() == '\t') {
            if (peek() == ' ') {
                indent++;
            } else {
                indent = (indent + 4) & ~3;
            }
            advance();
        }

        // Linha vazia ou comentário - ignora
        if (peek() == '\n' || peek() == '\r' || peek() == '#' || at_end()) {
            pos_ = start;
            return -1;
        }
        return indent;
    }

    void skip_empty_line() {
        while (peek() == ' ' || peek() == '\t') {
            advance();
        }
        if (peek() == '#') {
            while (!at_end() && peek() != '\n') {
                advance();
            }
        }
        if (peek() == '\n') {
            advance();
            line_++;
            at_line_start_ = true;
        } else if (peek() == '\r') {
            advance();
            if (peek() == '\n') advance();
            line_++;
            at_line_start_ = true;
        }
    }

    // ========================================================================
    // SCANNERS
    // ========================================================================

    Token scan_string() {
        // Verifica se é string multilinha (""")
        // pos_ aponta para o primeiro ". Precisamos de mais 2 aspas consecutivas.
        if ((pos_ + 2) < src_.size() && src_[pos_] == '"' &&
            src_[pos_ + 1] == '"' && src_[pos_ + 2] == '"') {
            // Triple quote: """..."""
            advance();  // pula primeiro "
            advance();  // pula segundo "
            advance();  // pula terceiro "

            // Pula newline imediato após """ (opcional)
            if (!at_end() && peek() == '\r') advance();
            if (!at_end() && peek() == '\n') { advance(); line_++; }

            std::string value;
            bool has_interp = false;
            std::vector<TokenInterpPart> parts;

            while (!at_end()) {
                // Verifica fechamento """
                if (peek() == '"' && (pos_ + 1) < src_.size() && src_[pos_ + 1] == '"'
                    && (pos_ + 2) < src_.size() && src_[pos_ + 2] == '"') {
                    advance(); advance(); advance(); // pula """

                    if (has_interp) {
                        if (!value.empty()) parts.push_back({false, value});
                        return Token(TK::STRING_INTERP, std::move(parts), line_);
                    }
                    return Token(TK::STRING, value, line_);
                }

                // Escapes: \n \t \r \\ \" \{ \}
                if (peek() == '\\' && (pos_ + 1) < src_.size()) {
                    advance();
                    char ch = peek();
                    switch (ch) {
                        case 'n':  value += '\n'; break;
                        case 't':  value += '\t'; break;
                        case 'r':  value += '\r'; break;
                        case '\\': value += '\\'; break;
                        case '"':  value += '"';  break;
                        case '{':  value += '{';  break;
                        case '}':  value += '}';  break;
                        default:   value += '\\'; value += ch; break;
                    }
                    advance();
                    continue;
                }

                // Interpolação {var} - só se conteúdo for identificador válido
                if (peek() == '{') {
                    // Olha adiante pra ver se é interpolação real
                    size_t scan = pos_ + 1;
                    bool valid_ident = (scan < src_.size()) &&
                        (is_alpha(src_[scan]) || src_[scan] == '_' || is_utf8_lead(src_[scan]));
                    if (valid_ident) {
                        size_t ident_start = scan;
                        while (scan < src_.size() && (is_alnum(src_[scan]) || src_[scan] == '_'
                               || src_[scan] == '.' || is_utf8_lead(src_[scan]) || is_utf8_cont(src_[scan]))) scan++;
                        // Deve fechar com } imediatamente após o identificador
                        valid_ident = (scan < src_.size() && src_[scan] == '}' && scan > ident_start);
                    }

                    if (valid_ident) {
                        has_interp = true;
                        if (!value.empty()) { parts.push_back({false, value}); value.clear(); }
                        advance(); // pula {
                        std::string var_name;
                        while (!at_end() && peek() != '}') var_name += advance();
                        advance(); // pula }
                        parts.push_back({true, var_name});
                        continue;
                    }
                    // Não é interpolação, trata { como texto literal
                }

                if (peek() == '\n') line_++;
                value += peek();
                advance();
            }
            return Token(TK::ERROR, "String multilinha não terminada", line_);
        }

        // String simples "..."
        advance();  // pula "
        std::string value;
        bool has_interp = false;
        std::vector<TokenInterpPart> parts;

        while (!at_end() && peek() != '"') {
            if (peek() == '\\' && (pos_ + 1) < src_.size()) {
                advance();
                char ch = peek();
                switch (ch) {
                    case 'n':  value += '\n'; break;
                    case 't':  value += '\t'; break;
                    case 'r':  value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"':  value += '"';  break;
                    case '{':  value += '{';  break;
                    case '}':  value += '}';  break;
                    default:   value += '\\'; value += ch; break;
                }
                advance();
                continue;
            }

            if (peek() == '{') {
                has_interp = true;
                // Salva texto acumulado
                if (!value.empty()) {
                    parts.push_back({false, value});
                    value.clear();
                }
                advance();  // pula {
                std::string var_name;
                while (!at_end() && peek() != '}') {
                    var_name += advance();
                }
                if (at_end()) {
                    return Token(TK::ERROR, "Interpolação não terminada", line_);
                }
                advance();  // pula }
                parts.push_back({true, var_name});
                continue;
            }

            value += peek();
            advance();
        }

        if (at_end()) {
            return Token(TK::ERROR, "String não terminada", line_);
        }
        advance();  // pula " final

        if (has_interp) {
            // Salva texto restante
            if (!value.empty()) {
                parts.push_back({false, value});
            }
            return Token(TK::STRING_INTERP, std::move(parts), line_);
        }

        return Token(TK::STRING, value, line_);
    }

    Token scan_number() {
        std::string value;
        while (is_digit(peek())) {
            value += advance();
        }
        if (peek() == '.' && is_digit(peek_next())) {
            value += advance();  // pula .
            while (is_digit(peek())) {
                value += advance();
            }
        }
        return Token(TK::NUMBER, value, line_);
    }

    Token scan_ident() {
        std::string value;
        while (is_alnum(peek()) || peek() == '_' || is_utf8_lead(peek()) || is_utf8_cont(peek())) {
            value += advance();
        }

        // Verificar keywords do idioma configurado
        auto& kw = lang_config_.keywords;
        auto it = kw.find(value);
        if (it != kw.end()) {
            return Token(it->second, value, line_);
        }

        // Verificar builtins: traduzir para nome interno
        // Ex: "entrada" (espanhol) → "entrada" (interno), "input" (inglês) → "entrada"
        auto bit = lang_config_.builtins.find(value);
        if (bit != lang_config_.builtins.end()) {
            return Token(TK::IDENT, bit->second, line_);
        }

        return Token(TK::IDENT, value, line_);
    }
};

} // namespace jplang

#endif // JPLANG_LEXER_HPP