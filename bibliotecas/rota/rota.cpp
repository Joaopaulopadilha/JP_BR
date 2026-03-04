// rota.cpp
// Biblioteca de rotas para JPLang — calcula distância (km) e tempo (min) entre cidades via OSRM + Nominatim
//
// Multiplataforma: WinHTTP (Windows) / libcurl (Linux), linkagem estática via .obj/.o, extern "C" puro
//
// Compilação:
//   Windows: g++ -std=c++17 -c -o bibliotecas/rota/rota.obj bibliotecas/rota/rota.cpp
//   Linux:   g++ -std=c++17 -c -o bibliotecas/rota/rota.o   bibliotecas/rota/rota.cpp
//
// Linkagem automática via rota.json:
//   Windows: -lwinhttp
//   Linux:   -lcurl

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

// =============================================================================
// PLATAFORMA
// =============================================================================

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>

#else

#include <curl/curl.h>

#endif

// =============================================================================
// ESTADO INTERNO
// =============================================================================

static int64_t resultado_distancia_km = 0;   // última distância calculada (km)
static int64_t resultado_tempo_min    = 0;   // último tempo calculado (minutos)

// =============================================================================
// HTTP GET - ABSTRAÇÃO POR PLATAFORMA
// =============================================================================

#ifdef _WIN32

// ---- Windows: WinHTTP ----

static std::string http_get(const std::string& url)
{
    std::string result;

    // Separar host e path da URL
    std::string host, path;
    bool is_https = false;

    size_t proto_end = url.find("://");
    if (proto_end == std::string::npos) return "";

    std::string scheme = url.substr(0, proto_end);
    is_https = (scheme == "https");

    size_t host_start = proto_end + 3;
    size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        host = url.substr(host_start);
        path = "/";
    } else {
        host = url.substr(host_start, path_start - host_start);
        path = url.substr(path_start);
    }

    // Converter host e path para wide string
    int whost_len = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (whost_len == 0 || wpath_len == 0) return "";

    std::wstring whost(whost_len, 0);
    std::wstring wpath(wpath_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, &whost[0], whost_len);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wpath_len);

    HINTERNET hSession = WinHttpOpen(L"JPLang-Rota/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    // Habilitar TLS 1.1 + 1.2 + 1.3 na sessão (precisa ser antes do Connect)
    if (is_https) {
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                          WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        protocols |= 0x00002000;  // TLS 1.3 (Windows 10 1903+)
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
                         &protocols, sizeof(protocols));
    }

    INTERNET_PORT port = is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Relaxar verificação de certificado (alguns servidores precisam)
    if (is_https) {
        DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                          SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                         &sec_flags, sizeof(sec_flags));
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    char buf[4096];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        result.append(buf, bytesRead);
        bytesRead = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

#else

// ---- Linux: libcurl ----

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t total = size * nmemb;
    ((std::string*)userp)->append((char*)contents, total);
    return total;
}

static std::string http_get(const std::string& url)
{
    std::string result;

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "JPLang-Rota/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";
    return result;
}

#endif

// =============================================================================
// URL ENCODING
// =============================================================================

static std::string url_encode(const std::string& str)
{
    std::string encoded;
    char hex[4];
    for (unsigned char c : str) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += (char)c;
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", c);
            encoded += hex;
        }
    }
    return encoded;
}

// =============================================================================
// JSON PARSER MINIMALISTA (sem dependências externas)
// =============================================================================

static std::string json_get_string(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;

    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']'
               && json[end] != ' ' && json[end] != '\n' && json[end] != '\r')
            end++;
        return json.substr(pos, end - pos);
    }
}

static double json_get_double(const std::string& json, const std::string& key)
{
    std::string val = json_get_string(json, key);
    if (val.empty()) return 0.0;
    return atof(val.c_str());
}

// =============================================================================
// GEOCODING via Nominatim (OpenStreetMap)
// =============================================================================

struct Coordenada {
    double lat;
    double lon;
    bool ok;
};

static Coordenada geocode(const std::string& cidade)
{
    Coordenada c = {0.0, 0.0, false};

    std::string url = "https://nominatim.openstreetmap.org/search?q="
                    + url_encode(cidade)
                    + "&format=json&limit=1";

    std::string resp = http_get(url);
    if (resp.empty() || resp[0] != '[') return c;

    size_t obj_start = resp.find('{');
    size_t obj_end   = resp.find('}', obj_start);
    if (obj_start == std::string::npos || obj_end == std::string::npos) return c;

    std::string obj = resp.substr(obj_start, obj_end - obj_start + 1);

    std::string lat_str = json_get_string(obj, "lat");
    std::string lon_str = json_get_string(obj, "lon");

    if (lat_str.empty() || lon_str.empty()) return c;

    c.lat = atof(lat_str.c_str());
    c.lon = atof(lon_str.c_str());
    c.ok  = true;
    return c;
}

// =============================================================================
// ROTA via OSRM
// =============================================================================

static bool calcular_rota_osrm(double lat1, double lon1, double lat2, double lon2)
{
    char url_buf[512];
    snprintf(url_buf, sizeof(url_buf),
             "https://router.project-osrm.org/route/v1/driving/%.6f,%.6f;%.6f,%.6f?overview=false",
             lon1, lat1, lon2, lat2);

    std::string resp = http_get(std::string(url_buf));
    if (resp.empty()) return false;

    std::string code = json_get_string(resp, "code");
    if (code != "Ok") return false;

    size_t routes_pos = resp.find("\"routes\"");
    if (routes_pos == std::string::npos) return false;

    std::string routes_str = resp.substr(routes_pos);

    double distance_m  = json_get_double(routes_str, "distance");
    double duration_s  = json_get_double(routes_str, "duration");

    if (distance_m <= 0.0 && duration_s <= 0.0) return false;

    resultado_distancia_km = (int64_t)(distance_m / 1000.0 + 0.5);
    resultado_tempo_min    = (int64_t)(duration_s / 60.0 + 0.5);

    return true;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

extern "C" int64_t rota_calcular(const char* origem, const char* destino)
{
    if (!origem || !destino) return -1;

    Coordenada c1 = geocode(std::string(origem));
    if (!c1.ok) return -1;

    Coordenada c2 = geocode(std::string(destino));
    if (!c2.ok) return -2;

    if (!calcular_rota_osrm(c1.lat, c1.lon, c2.lat, c2.lon))
        return -3;

    return 0;
}

extern "C" int64_t rota_distancia()
{
    return resultado_distancia_km;
}

extern "C" int64_t rota_tempo()
{
    return resultado_tempo_min;
}