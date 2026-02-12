// fonte.hpp
// Módulo de fontes de vídeo para biblioteca CVN (Portável Windows/Linux)

#ifndef CVN_FONTE_HPP
#define CVN_FONTE_HPP

#include <string>
#include <vector>
#include <functional>

// === PLATAFORMA WINDOWS ===
#ifdef _WIN32
#include <windows.h>
#else
// === PLATAFORMA LINUX ===
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

namespace cvn {

// Tipos de fonte
enum class TipoFonte {
    NENHUM,
    CAMERA,
    TELA,
    REGIAO
};

// Tipos de filtro
enum class TipoFiltro {
    NENHUM,
    CINZA,
    INVERTER,
    BRILHO,
    CONTRASTE,
    LIMIAR,
    BLUR,
    BORDAS,
    SEPIA,
    SATURACAO,
    FLIP_H,
    FLIP_V,
    ROTACIONAR,
    REDIMENSIONAR,
    LIMIAR_FILTRO
};

// Configuração de filtro
struct ConfigFiltro {
    TipoFiltro tipo = TipoFiltro::NENHUM;
    float valor1 = 0;
    float valor2 = 0;
};

// Estrutura de fonte de vídeo
struct Fonte {
    TipoFonte tipo = TipoFonte::NENHUM;
    int indice = 0;
    int x = 0, y = 0;
    int largura = 0;
    int altura = 0;
    std::vector<ConfigFiltro> filtros;
    bool valida = false;
    
    Fonte() = default;
    
    static Fonte criar_camera(int cam_indice) {
        Fonte f;
        f.tipo = TipoFonte::CAMERA;
        f.indice = cam_indice;
        f.valida = true;
        return f;
    }
    
    static Fonte criar_tela(int monitor_indice) {
        Fonte f;
        f.tipo = TipoFonte::TELA;
        f.indice = monitor_indice;
        f.valida = true;
        return f;
    }
    
    static Fonte criar_regiao(int monitor_indice, int rx, int ry, int rlargura, int raltura) {
        Fonte f;
        f.tipo = TipoFonte::REGIAO;
        f.indice = monitor_indice;
        f.x = rx;
        f.y = ry;
        f.largura = rlargura;
        f.altura = raltura;
        f.valida = true;
        return f;
    }
    
    Fonte com_filtro(TipoFiltro tipo, float v1 = 0, float v2 = 0) const {
        Fonte nova = *this;
        ConfigFiltro cf;
        cf.tipo = tipo;
        cf.valor1 = v1;
        cf.valor2 = v2;
        nova.filtros.push_back(cf);
        return nova;
    }
};

// Info do monitor
struct MonitorInfo {
    int indice;
    int x, y;
    int largura, altura;
    bool primario;
};

// === IMPLEMENTAÇÃO WINDOWS ===
#ifdef _WIN32

static std::vector<MonitorInfo> g_monitores;

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MONITORINFOEX mi;
    mi.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &mi);
    
    MonitorInfo info;
    info.indice = (int)g_monitores.size();
    info.x = mi.rcMonitor.left;
    info.y = mi.rcMonitor.top;
    info.largura = mi.rcMonitor.right - mi.rcMonitor.left;
    info.altura = mi.rcMonitor.bottom - mi.rcMonitor.top;
    info.primario = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    
    g_monitores.push_back(info);
    return TRUE;
}

class GerenciadorFontes {
private:
    std::vector<Fonte*> fontes;
    
public:
    static GerenciadorFontes& instancia() {
        static GerenciadorFontes inst;
        return inst;
    }
    
    std::string listar_telas() {
        g_monitores.clear();
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
        
        std::string resultado;
        for (const auto& m : g_monitores) {
            if (!resultado.empty()) resultado += ",";
            resultado += std::to_string(m.indice) + ":" + 
                        std::to_string(m.largura) + "x" + 
                        std::to_string(m.altura);
        }
        return resultado;
    }
    
    bool obter_monitor(int indice, int& x, int& y, int& largura, int& altura) {
        g_monitores.clear();
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
        
        if (indice < 0 || indice >= (int)g_monitores.size()) {
            x = GetSystemMetrics(SM_XVIRTUALSCREEN);
            y = GetSystemMetrics(SM_YVIRTUALSCREEN);
            largura = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            altura = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            return false;
        }
        
        x = g_monitores[indice].x;
        y = g_monitores[indice].y;
        largura = g_monitores[indice].largura;
        altura = g_monitores[indice].altura;
        return true;
    }
    
    int criar(const Fonte& fonte) {
        Fonte* f = new Fonte(fonte);
        int id = (int)fontes.size();
        fontes.push_back(f);
        return id;
    }
    
    Fonte* obter(int id) {
        if (id < 0 || id >= (int)fontes.size()) return nullptr;
        return fontes[id];
    }
    
    int clonar_com_filtro(int id, TipoFiltro tipo, float v1 = 0, float v2 = 0) {
        Fonte* f = obter(id);
        if (!f) return -1;
        
        Fonte nova = f->com_filtro(tipo, v1, v2);
        return criar(nova);
    }
    
    void liberar(int id) {
        if (id < 0 || id >= (int)fontes.size()) return;
        if (fontes[id]) {
            delete fontes[id];
            fontes[id] = nullptr;
        }
    }
    
    void liberar_todas() {
        for (auto f : fontes) {
            if (f) delete f;
        }
        fontes.clear();
    }
    
    ~GerenciadorFontes() {
        liberar_todas();
    }
};

// === IMPLEMENTAÇÃO LINUX ===
#else

class GerenciadorFontes {
private:
    std::vector<Fonte*> fontes;
    std::vector<MonitorInfo> monitores;
    Display* display = nullptr;
    
    void atualizar_monitores() {
        monitores.clear();
        
        if (!display) {
            display = XOpenDisplay(nullptr);
        }
        if (!display) return;
        
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);
        
        // Tenta usar XRandR para múltiplos monitores
        int num_monitors = 0;
        XRRMonitorInfo* xrr_monitors = XRRGetMonitors(display, root, True, &num_monitors);
        
        if (xrr_monitors && num_monitors > 0) {
            for (int i = 0; i < num_monitors; i++) {
                MonitorInfo info;
                info.indice = i;
                info.x = xrr_monitors[i].x;
                info.y = xrr_monitors[i].y;
                info.largura = xrr_monitors[i].width;
                info.altura = xrr_monitors[i].height;
                info.primario = xrr_monitors[i].primary;
                monitores.push_back(info);
            }
            XRRFreeMonitors(xrr_monitors);
        } else {
            // Fallback: tela única
            MonitorInfo info;
            info.indice = 0;
            info.x = 0;
            info.y = 0;
            info.largura = DisplayWidth(display, screen);
            info.altura = DisplayHeight(display, screen);
            info.primario = true;
            monitores.push_back(info);
        }
    }
    
public:
    static GerenciadorFontes& instancia() {
        static GerenciadorFontes inst;
        return inst;
    }
    
    std::string listar_telas() {
        atualizar_monitores();
        
        std::string resultado;
        for (const auto& m : monitores) {
            if (!resultado.empty()) resultado += ",";
            resultado += std::to_string(m.indice) + ":" + 
                        std::to_string(m.largura) + "x" + 
                        std::to_string(m.altura);
        }
        return resultado;
    }
    
    bool obter_monitor(int indice, int& x, int& y, int& largura, int& altura) {
        atualizar_monitores();
        
        if (indice < 0 || indice >= (int)monitores.size()) {
            // Retorna tela completa
            if (!display) display = XOpenDisplay(nullptr);
            if (display) {
                int screen = DefaultScreen(display);
                x = 0;
                y = 0;
                largura = DisplayWidth(display, screen);
                altura = DisplayHeight(display, screen);
            } else {
                x = y = 0;
                largura = altura = 0;
            }
            return false;
        }
        
        x = monitores[indice].x;
        y = monitores[indice].y;
        largura = monitores[indice].largura;
        altura = monitores[indice].altura;
        return true;
    }
    
    int criar(const Fonte& fonte) {
        Fonte* f = new Fonte(fonte);
        int id = (int)fontes.size();
        fontes.push_back(f);
        return id;
    }
    
    Fonte* obter(int id) {
        if (id < 0 || id >= (int)fontes.size()) return nullptr;
        return fontes[id];
    }
    
    int clonar_com_filtro(int id, TipoFiltro tipo, float v1 = 0, float v2 = 0) {
        Fonte* f = obter(id);
        if (!f) return -1;
        
        Fonte nova = f->com_filtro(tipo, v1, v2);
        return criar(nova);
    }
    
    void liberar(int id) {
        if (id < 0 || id >= (int)fontes.size()) return;
        if (fontes[id]) {
            delete fontes[id];
            fontes[id] = nullptr;
        }
    }
    
    void liberar_todas() {
        for (auto f : fontes) {
            if (f) delete f;
        }
        fontes.clear();
    }
    
    ~GerenciadorFontes() {
        liberar_todas();
        if (display) {
            XCloseDisplay(display);
        }
    }
};

#endif // _WIN32

} // namespace cvn

#endif // CVN_FONTE_HPP