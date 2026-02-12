// paravoz.cpp
// Biblioteca de síntese de voz usando SAPI do Windows
// Compile DLL: g++ -shared -o bibliotecas/paravoz.jpd bibliotecas/paravoz.cpp -O3 -lole32 -loleaut32 -luuid
// Compile .a:  g++ -c bibliotecas/paravoz.cpp -o paravoz.o -O3 && ar rcs bibliotecas/libparavoz.a paravoz.o

#include <vector>
#include <variant>
#include <string>
#include <memory>
#include <map>
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <comdef.h>

// --- Tipos JPLang ---
struct Instancia;
using ValorVariant = std::variant<std::string, int, double, bool, std::shared_ptr<Instancia>>;

struct Instancia {
    std::string nomeClasse;
    std::map<std::string, ValorVariant> propriedades;
};

// --- Helpers (static para não conflitar com outras libs) ---
static std::string get_str(const ValorVariant& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "verdadeiro" : "falso";
    return "";
}

static int get_int(const ValorVariant& v) {
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    if (std::holds_alternative<double>(v)) return (int)std::get<double>(v);
    return 0;
}

// --- Estado Global ---
static ISpVoice* pVoice = nullptr;
static bool sapi_inicializado = false;
static int volume_atual = 100;
static int velocidade_atual = 0;

// --- Helper interno ---
static bool inicializar_sapi() {
    if (sapi_inicializado) return true;
    
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return false;
    
    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }
    
    pVoice->SetVolume(volume_atual);
    pVoice->SetRate(velocidade_atual);
    
    sapi_inicializado = true;
    return true;
}

// --- Funções Exportadas ---
extern "C" {

    // Falar texto (bloqueante)
    __declspec(dllexport) ValorVariant pv_falar(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        std::string texto = get_str(args[0]);
        if (texto.empty()) return 0;
        
        int tamanho = MultiByteToWideChar(CP_UTF8, 0, texto.c_str(), -1, nullptr, 0);
        if (tamanho == 0) return 0;
        
        wchar_t* wTexto = new wchar_t[tamanho];
        MultiByteToWideChar(CP_UTF8, 0, texto.c_str(), -1, wTexto, tamanho);
        
        HRESULT hr = pVoice->Speak(wTexto, SPF_DEFAULT, nullptr);
        
        delete[] wTexto;
        
        return SUCCEEDED(hr) ? 1 : 0;
    }

    // Falar texto (assíncrono)
    __declspec(dllexport) ValorVariant pv_falar_async(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        std::string texto = get_str(args[0]);
        if (texto.empty()) return 0;
        
        int tamanho = MultiByteToWideChar(CP_UTF8, 0, texto.c_str(), -1, nullptr, 0);
        if (tamanho == 0) return 0;
        
        wchar_t* wTexto = new wchar_t[tamanho];
        MultiByteToWideChar(CP_UTF8, 0, texto.c_str(), -1, wTexto, tamanho);
        
        HRESULT hr = pVoice->Speak(wTexto, SPF_ASYNC, nullptr);
        
        delete[] wTexto;
        
        return SUCCEEDED(hr) ? 1 : 0;
    }

    // Definir volume (0-100)
    __declspec(dllexport) ValorVariant pv_volume(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        int vol = get_int(args[0]);
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        
        volume_atual = vol;
        pVoice->SetVolume(vol);
        
        return vol;
    }

    // Definir velocidade (-10 a 10)
    __declspec(dllexport) ValorVariant pv_velocidade(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        int vel = get_int(args[0]);
        if (vel < -10) vel = -10;
        if (vel > 10) vel = 10;
        
        velocidade_atual = vel;
        pVoice->SetRate(vel);
        
        return vel;
    }

    // Pausar fala
    __declspec(dllexport) ValorVariant pv_pausar(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        HRESULT hr = pVoice->Pause();
        return SUCCEEDED(hr) ? 1 : 0;
    }

    // Continuar fala
    __declspec(dllexport) ValorVariant pv_continuar(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        HRESULT hr = pVoice->Resume();
        return SUCCEEDED(hr) ? 1 : 0;
    }

    // Parar fala
    __declspec(dllexport) ValorVariant pv_parar(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        HRESULT hr = pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
        return SUCCEEDED(hr) ? 1 : 0;
    }

    // Verificar se está falando
    __declspec(dllexport) ValorVariant pv_falando(const std::vector<ValorVariant>& args) {
        if (!inicializar_sapi()) return 0;
        
        SPVOICESTATUS status;
        HRESULT hr = pVoice->GetStatus(&status, nullptr);
        
        if (FAILED(hr)) return 0;
        
        return (status.dwRunningState == SPRS_IS_SPEAKING) ? 1 : 0;
    }

    // Obter volume atual
    __declspec(dllexport) ValorVariant pv_get_volume(const std::vector<ValorVariant>& args) {
        return volume_atual;
    }

    // Obter velocidade atual
    __declspec(dllexport) ValorVariant pv_get_velocidade(const std::vector<ValorVariant>& args) {
        return velocidade_atual;
    }

    // Limpar recursos
    __declspec(dllexport) ValorVariant pv_limpar(const std::vector<ValorVariant>& args) {
        if (pVoice) {
            pVoice->Release();
            pVoice = nullptr;
        }
        if (sapi_inicializado) {
            CoUninitialize();
            sapi_inicializado = false;
        }
        return 1;
    }
}