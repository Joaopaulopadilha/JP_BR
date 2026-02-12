// jprasp.cpp
// Biblioteca de raspagem web (web scraping) para JPLang - estilo BeautifulSoup

#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "winhttp.lib")

static const wchar_t* USER_AGENT = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) Gecko/20100101 Firefox/120.0";

// === TIPOS JPLANG ===
struct Instancia;
using ValorVariant = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

struct Instancia {
    std::string nomeClasse;
    std::map<std::string, ValorVariant> propriedades;
};

typedef enum {
    JP_TIPO_NULO = 0, JP_TIPO_INT = 1, JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3, JP_TIPO_BOOL = 4, JP_TIPO_OBJETO = 5,
    JP_TIPO_LISTA = 6, JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro; double decimal; char* texto;
        int booleano; void* objeto; void* lista; void* ponteiro;
    } valor;
} JPValor;

// === CONVERSORES ===
static ValorVariant jp_para_variant(const JPValor& jp) {
    switch (jp.tipo) {
        case JP_TIPO_INT:    return (int)jp.valor.inteiro;
        case JP_TIPO_DOUBLE: return jp.valor.decimal;
        case JP_TIPO_BOOL:   return (bool)jp.valor.booleano;
        case JP_TIPO_STRING: return jp.valor.texto ? std::string(jp.valor.texto) : std::string("");
        default:             return std::string("");
    }
}

static JPValor variant_para_jp(const ValorVariant& var) {
    JPValor jp;
    memset(&jp, 0, sizeof(jp));
    if (std::holds_alternative<int>(var)) {
        jp.tipo = JP_TIPO_INT;
        jp.valor.inteiro = std::get<int>(var);
    } else if (std::holds_alternative<double>(var)) {
        jp.tipo = JP_TIPO_DOUBLE;
        jp.valor.decimal = std::get<double>(var);
    } else if (std::holds_alternative<bool>(var)) {
        jp.tipo = JP_TIPO_BOOL;
        jp.valor.booleano = std::get<bool>(var) ? 1 : 0;
    } else if (std::holds_alternative<std::string>(var)) {
        jp.tipo = JP_TIPO_STRING;
        const std::string& s = std::get<std::string>(var);
        jp.valor.texto = (char*)malloc(s.size() + 1);
        if (jp.valor.texto) memcpy(jp.valor.texto, s.c_str(), s.size() + 1);
    } else {
        jp.tipo = JP_TIPO_NULO;
    }
    return jp;
}

static std::vector<ValorVariant> jp_array_para_vector(JPValor* args, int numArgs) {
    std::vector<ValorVariant> result;
    if (args == nullptr || numArgs <= 0) return result;
    for (int i = 0; i < numArgs; i++) result.push_back(jp_para_variant(args[i]));
    return result;
}

static std::string get_str(const ValorVariant& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    return "";
}

static int get_int(const ValorVariant& v) {
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    if (std::holds_alternative<double>(v)) return (int)std::get<double>(v);
    return 0;
}

// === FUNCOES AUXILIARES ===

static std::string para_minusculo(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static std::wstring str_para_wstr(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size <= 0) return std::wstring();
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

static bool parse_url(const std::string& url, std::wstring& host, std::wstring& path, bool& https) {
    std::string urlCopy = url;
    https = true;
    if (urlCopy.find("https://") == 0) urlCopy = urlCopy.substr(8);
    else if (urlCopy.find("http://") == 0) { urlCopy = urlCopy.substr(7); https = false; }
    
    size_t pathPos = urlCopy.find('/');
    std::string hostStr = (pathPos != std::string::npos) ? urlCopy.substr(0, pathPos) : urlCopy;
    std::string pathStr = (pathPos != std::string::npos) ? urlCopy.substr(pathPos) : "/";
    
    if (hostStr.empty()) return false;
    host = str_para_wstr(hostStr);
    path = str_para_wstr(pathStr);
    return !host.empty();
}

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// === FUNCOES HTTP ===

static std::string rasp_buscar_interno(const std::string& url) {
    if (url.empty()) return "ERRO: URL vazia";
    
    std::wstring host, path;
    bool https;
    if (!parse_url(url, host, path, https)) return "ERRO: URL invalida";
    
    HINTERNET hSession = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "ERRO: Falha ao iniciar sessao HTTP";
    
    DWORD timeout = 30000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    
    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return "ERRO: Falha ao conectar"; }
    
    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL,
                                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "ERRO: Falha ao criar requisicao";
    }
    
    WinHttpAddRequestHeaders(hRequest, 
        L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        L"Accept-Language: pt-BR,pt;q=0.9,en-US;q=0.8,en;q=0.7\r\n",
        -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "ERRO: Falha ao enviar requisicao";
    }
    
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "ERRO: Falha ao receber resposta";
    }
    
    std::string resultado;
    resultado.reserve(65536);
    DWORD bytesDisponiveis = 0, bytesLidos = 0;
    char buffer[8192];
    
    while (WinHttpQueryDataAvailable(hRequest, &bytesDisponiveis) && bytesDisponiveis > 0) {
        DWORD toRead = (bytesDisponiveis > sizeof(buffer) - 1) ? sizeof(buffer) - 1 : bytesDisponiveis;
        if (WinHttpReadData(hRequest, buffer, toRead, &bytesLidos) && bytesLidos > 0) {
            resultado.append(buffer, bytesLidos);
        } else break;
        if (resultado.size() > 5 * 1024 * 1024) break;
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return resultado.empty() ? "ERRO: Resposta vazia" : resultado;
}

static int rasp_status_interno(const std::string& url) {
    if (url.empty()) return -1;
    
    std::wstring host, path;
    bool https;
    if (!parse_url(url, host, path, https)) return -1;
    
    HINTERNET hSession = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return -1;
    
    DWORD timeout = 10000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    
    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }
    
    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", path.c_str(), NULL,
                                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return -1;
    }
    
    DWORD statusCode = 0, size = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return (int)statusCode;
}

// === FUNCOES DE PARSING HTML ===

static size_t encontrar_fechamento_tag(const std::string& html, const std::string& tag, size_t inicio) {
    int nivel = 1;
    size_t pos = inicio;
    size_t len = html.size();
    size_t tagLen = tag.size();
    
    while (pos < len && nivel > 0) {
        // Busca próxima tag de fechamento </tag>
        size_t proxFechar = std::string::npos;
        for (size_t i = pos; i + tagLen + 3 <= len; i++) {
            if (html[i] == '<' && html[i+1] == '/') {
                bool match = true;
                for (size_t j = 0; j < tagLen && match; j++) {
                    if (tolower(html[i+2+j]) != tolower(tag[j])) match = false;
                }
                if (match && html[i+2+tagLen] == '>') { 
                    proxFechar = i; 
                    break; 
                }
            }
        }
        
        // Busca próxima tag de abertura <tag> ou <tag ...>
        size_t proxAbrir = std::string::npos;
        for (size_t i = pos; i + tagLen + 1 < len; i++) {
            if (html[i] == '<' && html[i+1] != '/' && html[i+1] != '!') {
                bool match = true;
                for (size_t j = 0; j < tagLen && match; j++) {
                    if (tolower(html[i+1+j]) != tolower(tag[j])) match = false;
                }
                if (match) {
                    char c = html[i+1+tagLen];
                    if (c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/') { 
                        proxAbrir = i; 
                        break; 
                    }
                }
            }
        }
        
        if (proxFechar == std::string::npos) break;
        
        // Se encontrou abertura antes do fechamento, incrementa nível
        if (proxAbrir != std::string::npos && proxAbrir < proxFechar) {
            size_t fimTag = html.find('>', proxAbrir);
            // Não incrementa se for self-closing (/>)
            if (fimTag != std::string::npos && !(fimTag > 0 && html[fimTag - 1] == '/')) {
                nivel++;
            }
            pos = fimTag + 1;
        } else {
            // Encontrou fechamento
            nivel--;
            if (nivel == 0) {
                return proxFechar + tagLen + 3; // Retorna posição após </tag>
            }
            pos = proxFechar + tagLen + 3;
        }
    }
    return std::string::npos;
}

static bool eh_self_closing(const std::string& tag) {
    return (tag == "img" || tag == "br" || tag == "hr" || tag == "input" || 
            tag == "meta" || tag == "link" || tag == "area" || tag == "base" || 
            tag == "col" || tag == "embed" || tag == "source" || tag == "track" || tag == "wbr");
}

// Tags que podem ter fechamento implícito em HTML5
static bool pode_ter_fechamento_implicito(const std::string& tag) {
    return (tag == "p" || tag == "li" || tag == "dt" || tag == "dd" || 
            tag == "tr" || tag == "td" || tag == "th" || tag == "option" ||
            tag == "thead" || tag == "tbody" || tag == "tfoot");
}

// Encontra o fim de uma tag que pode ter fechamento implícito
static size_t encontrar_fim_tag_implicita(const std::string& html, const std::string& tag, size_t inicio) {
    size_t tagLen = tag.size();
    
    // Primeiro busca </tag> explícito
    for (size_t i = inicio; i + tagLen + 3 <= html.size(); i++) {
        if (html[i] == '<' && html[i+1] == '/') {
            bool match = true;
            for (size_t j = 0; j < tagLen && match; j++) {
                if (tolower(html[i+2+j]) != tolower(tag[j])) match = false;
            }
            if (match && html[i+2+tagLen] == '>') {
                return i + tagLen + 3; // Retorna posição após </tag>
            }
            
            // Verifica se é fechamento de tag de bloco (fecha implicitamente)
            // Mas ignora tags inline como </a>, </span>, </em>, </strong>
            std::string closeTag;
            size_t k = i + 2;
            while (k < html.size() && html[k] != '>' && html[k] != ' ') {
                closeTag += tolower(html[k]);
                k++;
            }
            
            // Tags de bloco que fecham <p> implicitamente
            if (closeTag == "div" || closeTag == "section" || closeTag == "article" ||
                closeTag == "header" || closeTag == "footer" || closeTag == "nav" ||
                closeTag == "aside" || closeTag == "main" || closeTag == "body" ||
                closeTag == "html" || closeTag == "ul" || closeTag == "ol" ||
                closeTag == "table" || closeTag == "form" || closeTag == "blockquote") {
                return i; // Fecha implicitamente antes desta tag
            }
        }
        
        // Verifica se é abertura de mesma tag ou tag de bloco
        if (html[i] == '<' && html[i+1] != '/' && html[i+1] != '!') {
            bool match = true;
            for (size_t j = 0; j < tagLen && match && i+1+j < html.size(); j++) {
                if (tolower(html[i+1+j]) != tolower(tag[j])) match = false;
            }
            if (match && i+1+tagLen < html.size()) {
                char c = html[i+1+tagLen];
                if (c == '>' || c == ' ' || c == '\t' || c == '\n') {
                    return i; // Próxima tag do mesmo tipo = fechamento implícito
                }
            }
            
            // Tags de bloco que abrem também fecham <p> implicitamente
            std::string openTag;
            size_t k = i + 1;
            while (k < html.size() && html[k] != '>' && html[k] != ' ' && html[k] != '/') {
                openTag += tolower(html[k]);
                k++;
            }
            
            if (openTag == "div" || openTag == "p" || openTag == "h1" || openTag == "h2" ||
                openTag == "h3" || openTag == "h4" || openTag == "h5" || openTag == "h6" ||
                openTag == "ul" || openTag == "ol" || openTag == "table" || openTag == "form" ||
                openTag == "blockquote" || openTag == "pre" || openTag == "hr") {
                return i;
            }
        }
    }
    
    return html.size(); // Vai até o fim
}

static std::string rasp_buscar_tag_interno(const std::string& html, const std::string& tag) {
    if (html.empty() || tag.empty()) return "";
    
    std::string tag_lower = para_minusculo(tag);
    bool selfClosing = eh_self_closing(tag_lower);
    size_t tagLen = tag.size();
    
    for (size_t i = 0; i + tagLen + 1 < html.size(); i++) {
        if (html[i] == '<') {
            bool match = true;
            for (size_t j = 0; j < tagLen && match; j++)
                if (tolower(html[i+1+j]) != tolower(tag[j])) match = false;
            if (match) {
                char c = html[i+1+tagLen];
                if (c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '/') {
                    size_t fimAbertura = html.find('>', i);
                    if (fimAbertura == std::string::npos) return "";
                    
                    if (selfClosing || (fimAbertura > 0 && html[fimAbertura - 1] == '/'))
                        return html.substr(i, fimAbertura - i + 1);
                    
                    size_t fimTag = encontrar_fechamento_tag(html, tag_lower, fimAbertura + 1);
                    return (fimTag == std::string::npos) ? 
                           html.substr(i, fimAbertura - i + 1) : html.substr(i, fimTag - i);
                }
            }
        }
    }
    return "";
}

static std::string rasp_buscar_todas_interno(const std::string& html, const std::string& tag) {
    if (html.empty() || tag.empty()) return "";
    
    std::string tag_lower = para_minusculo(tag);
    bool selfClosing = eh_self_closing(tag_lower);
    bool fechamentoImplicito = pode_ter_fechamento_implicito(tag_lower);
    std::string resultado;
    size_t pos = 0, tagLen = tag.size();
    int count = 0;
    
    while (pos < html.size() && count < 1000) {
        size_t inicio = std::string::npos;
        
        for (size_t i = pos; i + tagLen + 1 < html.size(); i++) {
            if (html[i] == '<') {
                bool match = true;
                for (size_t j = 0; j < tagLen && match; j++)
                    if (tolower(html[i+1+j]) != tolower(tag[j])) match = false;
                if (match) {
                    char c = html[i+1+tagLen];
                    if (c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '/') { inicio = i; break; }
                }
            }
        }
        
        if (inicio == std::string::npos) break;
        
        size_t fimAbertura = html.find('>', inicio);
        if (fimAbertura == std::string::npos) break;
        
        std::string elemento;
        if (selfClosing || (fimAbertura > 0 && html[fimAbertura - 1] == '/')) {
            elemento = html.substr(inicio, fimAbertura - inicio + 1);
            pos = fimAbertura + 1;
        } else {
            size_t fimTag;
            if (fechamentoImplicito) {
                fimTag = encontrar_fim_tag_implicita(html, tag_lower, fimAbertura + 1);
            } else {
                fimTag = encontrar_fechamento_tag(html, tag_lower, fimAbertura + 1);
            }
            
            if (fimTag == std::string::npos) {
                elemento = html.substr(inicio, fimAbertura - inicio + 1);
                pos = fimAbertura + 1;
            } else {
                elemento = html.substr(inicio, fimTag - inicio);
                pos = fimTag;
            }
        }
        
        if (!resultado.empty()) resultado += "||";
        resultado += elemento;
        count++;
    }
    return resultado;
}

static std::string rasp_buscar_classe_interno(const std::string& html, const std::string& classe) {
    if (html.empty() || classe.empty()) return "";
    
    std::string resultado;
    size_t pos = 0;
    int count = 0;
    
    while (pos < html.size() && count < 1000) {
        size_t classPos = html.find("class=", pos);
        if (classPos == std::string::npos) classPos = html.find("CLASS=", pos);
        if (classPos == std::string::npos) break;
        
        size_t tagInicio = html.rfind('<', classPos);
        if (tagInicio == std::string::npos) { pos = classPos + 1; continue; }
        
        size_t aspas = classPos + 6;
        if (aspas >= html.size()) break;
        
        char delim = html[aspas];
        if (delim != '"' && delim != '\'') { pos = classPos + 1; continue; }
        
        size_t fimAspas = html.find(delim, aspas + 1);
        if (fimAspas == std::string::npos) { pos = classPos + 1; continue; }
        
        std::string classes = html.substr(aspas + 1, fimAspas - aspas - 1);
        
        bool encontrou = false;
        size_t cpos = 0;
        while (cpos < classes.size()) {
            size_t espaco = classes.find(' ', cpos);
            std::string c = (espaco == std::string::npos) ? classes.substr(cpos) : classes.substr(cpos, espaco - cpos);
            if (c == classe) { encontrou = true; break; }
            cpos = (espaco == std::string::npos) ? classes.size() : espaco + 1;
        }
        
        if (!encontrou) { pos = classPos + 1; continue; }
        
        size_t tagFimNome = html.find_first_of(" \t\n>", tagInicio + 1);
        if (tagFimNome == std::string::npos) { pos = classPos + 1; continue; }
        
        std::string tag = para_minusculo(html.substr(tagInicio + 1, tagFimNome - tagInicio - 1));
        
        size_t fimAbertura = html.find('>', tagInicio);
        if (fimAbertura == std::string::npos) { pos = classPos + 1; continue; }
        
        std::string elemento;
        if (eh_self_closing(tag) || (fimAbertura > 0 && html[fimAbertura - 1] == '/')) {
            elemento = html.substr(tagInicio, fimAbertura - tagInicio + 1);
            pos = fimAbertura + 1;
        } else {
            size_t fimTag = encontrar_fechamento_tag(html, tag, fimAbertura + 1);
            if (fimTag == std::string::npos) {
                elemento = html.substr(tagInicio, fimAbertura - tagInicio + 1);
                pos = fimAbertura + 1;
            } else {
                elemento = html.substr(tagInicio, fimTag - tagInicio);
                pos = fimTag;
            }
        }
        
        if (!resultado.empty()) resultado += "||";
        resultado += elemento;
        count++;
    }
    return resultado;
}

static std::string rasp_buscar_id_interno(const std::string& html, const std::string& id) {
    if (html.empty() || id.empty()) return "";
    
    std::string busca1 = "id=\"" + id + "\"";
    std::string busca2 = "id='" + id + "'";
    
    size_t pos = html.find(busca1);
    if (pos == std::string::npos) pos = html.find(busca2);
    if (pos == std::string::npos) {
        std::string busca3 = "ID=\"" + id + "\"";
        std::string busca4 = "ID='" + id + "'";
        pos = html.find(busca3);
        if (pos == std::string::npos) pos = html.find(busca4);
    }
    if (pos == std::string::npos) return "";
    
    size_t tagInicio = html.rfind('<', pos);
    if (tagInicio == std::string::npos) return "";
    
    size_t tagFimNome = html.find_first_of(" \t\n>", tagInicio + 1);
    if (tagFimNome == std::string::npos) return "";
    
    std::string tag = para_minusculo(html.substr(tagInicio + 1, tagFimNome - tagInicio - 1));
    
    size_t fimAbertura = html.find('>', tagInicio);
    if (fimAbertura == std::string::npos) return "";
    
    if (eh_self_closing(tag) || (fimAbertura > 0 && html[fimAbertura - 1] == '/'))
        return html.substr(tagInicio, fimAbertura - tagInicio + 1);
    
    size_t fimTag = encontrar_fechamento_tag(html, tag, fimAbertura + 1);
    return (fimTag == std::string::npos) ? 
           html.substr(tagInicio, fimAbertura - tagInicio + 1) : html.substr(tagInicio, fimTag - tagInicio);
}

static std::string rasp_pegar_texto_interno(const std::string& html) {
    if (html.empty()) return "";
    
    std::string resultado;
    resultado.reserve(html.size());
    bool dentroTag = false;
    
    for (char c : html) {
        if (c == '<') dentroTag = true;
        else if (c == '>') dentroTag = false;
        else if (!dentroTag) resultado += c;
    }
    
    size_t p;
    while ((p = resultado.find("&nbsp;")) != std::string::npos) resultado.replace(p, 6, " ");
    while ((p = resultado.find("&amp;")) != std::string::npos) resultado.replace(p, 5, "&");
    while ((p = resultado.find("&lt;")) != std::string::npos) resultado.replace(p, 4, "<");
    while ((p = resultado.find("&gt;")) != std::string::npos) resultado.replace(p, 4, ">");
    while ((p = resultado.find("&quot;")) != std::string::npos) resultado.replace(p, 6, "\"");
    while ((p = resultado.find("&#39;")) != std::string::npos) resultado.replace(p, 5, "'");
    while ((p = resultado.find("&apos;")) != std::string::npos) resultado.replace(p, 6, "'");
    
    return trim(resultado);
}

static std::string rasp_pegar_atributo_interno(const std::string& html, const std::string& atributo) {
    if (html.empty() || atributo.empty()) return "";
    
    std::string atributo_lower = para_minusculo(atributo);
    std::string html_lower = para_minusculo(html);
    
    std::string busca = atributo_lower + "=";
    size_t pos = html_lower.find(busca);
    if (pos == std::string::npos) return "";
    
    size_t inicio = pos + busca.size();
    if (inicio >= html.size()) return "";
    
    char delim = html[inicio];
    if (delim == '"' || delim == '\'') {
        size_t fim = html.find(delim, inicio + 1);
        return (fim == std::string::npos) ? "" : html.substr(inicio + 1, fim - inicio - 1);
    } else {
        size_t fim = html.find_first_of(" \t\n>", inicio);
        return (fim == std::string::npos) ? "" : html.substr(inicio, fim - inicio);
    }
}

static int rasp_contar_interno(const std::string& html, const std::string& tag) {
    if (html.empty() || tag.empty()) return 0;
    
    int count = 0;
    size_t tagLen = tag.size();
    
    for (size_t i = 0; i + tagLen + 1 < html.size(); i++) {
        if (html[i] == '<') {
            bool match = true;
            for (size_t j = 0; j < tagLen && match; j++)
                if (tolower(html[i+1+j]) != tolower(tag[j])) match = false;
            if (match) {
                char c = html[i+1+tagLen];
                if (c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '/') count++;
            }
        }
    }
    return count;
}

// === WRAPPERS ===

ValorVariant rasp_buscar_impl(const std::vector<ValorVariant>& args) {
    if (args.empty()) return std::string("ERRO: URL nao fornecida");
    return rasp_buscar_interno(get_str(args[0]));
}

ValorVariant rasp_status_impl(const std::vector<ValorVariant>& args) {
    if (args.empty()) return -1;
    return rasp_status_interno(get_str(args[0]));
}

ValorVariant rasp_disponivel_impl(const std::vector<ValorVariant>& args) {
    if (args.empty()) return false;
    int code = rasp_status_interno(get_str(args[0]));
    return (code >= 200 && code < 400);
}

ValorVariant rasp_buscar_tag_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return std::string("");
    return rasp_buscar_tag_interno(get_str(args[0]), get_str(args[1]));
}

ValorVariant rasp_buscar_todas_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return std::string("");
    return rasp_buscar_todas_interno(get_str(args[0]), get_str(args[1]));
}

ValorVariant rasp_buscar_classe_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return std::string("");
    return rasp_buscar_classe_interno(get_str(args[0]), get_str(args[1]));
}

ValorVariant rasp_buscar_id_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return std::string("");
    return rasp_buscar_id_interno(get_str(args[0]), get_str(args[1]));
}

ValorVariant rasp_pegar_texto_impl(const std::vector<ValorVariant>& args) {
    if (args.empty()) return std::string("");
    return rasp_pegar_texto_interno(get_str(args[0]));
}

ValorVariant rasp_pegar_atributo_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return std::string("");
    return rasp_pegar_atributo_interno(get_str(args[0]), get_str(args[1]));
}

ValorVariant rasp_contar_impl(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0;
    return rasp_contar_interno(get_str(args[0]), get_str(args[1]));
}

// === EXPORTS TCC ===
extern "C" {
    __declspec(dllexport) JPValor jp_rasp_buscar(JPValor* args, int n) {
        return variant_para_jp(rasp_buscar_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_status(JPValor* args, int n) {
        return variant_para_jp(rasp_status_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_disponivel(JPValor* args, int n) {
        return variant_para_jp(rasp_disponivel_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_buscar_tag(JPValor* args, int n) {
        return variant_para_jp(rasp_buscar_tag_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_buscar_todas(JPValor* args, int n) {
        return variant_para_jp(rasp_buscar_todas_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_buscar_classe(JPValor* args, int n) {
        return variant_para_jp(rasp_buscar_classe_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_buscar_id(JPValor* args, int n) {
        return variant_para_jp(rasp_buscar_id_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_pegar_texto(JPValor* args, int n) {
        return variant_para_jp(rasp_pegar_texto_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_pegar_atributo(JPValor* args, int n) {
        return variant_para_jp(rasp_pegar_atributo_impl(jp_array_para_vector(args, n)));
    }
    
    __declspec(dllexport) JPValor jp_rasp_contar(JPValor* args, int n) {
        return variant_para_jp(rasp_contar_impl(jp_array_para_vector(args, n)));
    }
}