// hardinfo.cpp
// Biblioteca de informações de hardware para JPLang (Windows + Linux)
// Windows: g++ -std=c++17 -shared -o bibliotecas/hardinfo/hardinfo.jpd hardinfo.cpp -O3 -lpdh -lpsapi -static-libgcc -static-libstdc++
// Linux:   g++ -std=c++17 -shared -fPIC -o bibliotecas/hardinfo/libhardinfo.jpd hardinfo.cpp -O3 -static-libgcc -static-libstdc++

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>

#ifdef _WIN32
    #include <windows.h>
    #include <intrin.h>
    #include <pdh.h>
    #include <pdhmsg.h>
    #include <psapi.h>
    #pragma comment(lib, "pdh.lib")
    #pragma comment(lib, "psapi.lib")
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <sys/statvfs.h>
    #include <sys/utsname.h>
    #include <pwd.h>
    #include <mntent.h>
    #include <cpuid.h>
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// --- Tipos do runtime JPLang (compatível com jpruntime.h) ---
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4,
    JP_TIPO_OBJETO = 5,
    JP_TIPO_LISTA = 6,
    JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
        void* lista;
        void* ponteiro;
    } valor;
} JPValor;

// --- Helpers para criar JPValor ---
static JPValor jp_nulo() {
    JPValor v;
    v.tipo = JP_TIPO_NULO;
    v.valor.inteiro = 0;
    return v;
}

static JPValor jp_int(int64_t i) {
    JPValor v;
    v.tipo = JP_TIPO_INT;
    v.valor.inteiro = i;
    return v;
}

static JPValor jp_double(double d) {
    JPValor v;
    v.tipo = JP_TIPO_DOUBLE;
    v.valor.decimal = d;
    return v;
}

static JPValor jp_string(const char* s) {
    JPValor v;
    v.tipo = JP_TIPO_STRING;
    if (s) {
        size_t len = strlen(s);
        v.valor.texto = (char*)malloc(len + 1);
        if (v.valor.texto) memcpy(v.valor.texto, s, len + 1);
    } else {
        v.valor.texto = NULL;
    }
    return v;
}

static const char* jp_get_string(JPValor v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto != NULL) return v.valor.texto;
    return "";
}

// --- Variáveis globais para monitoramento de CPU ---
#ifdef _WIN32
static PDH_HQUERY g_cpuQuery = NULL;
static PDH_HCOUNTER g_cpuTotal = NULL;
#else
static unsigned long long g_prevIdle = 0;
static unsigned long long g_prevTotal = 0;
#endif
static bool g_cpuIniciado = false;

// --- Helpers Internos ---

static std::string format_bytes(unsigned long long bytes) {
    const double gb = 1024.0 * 1024.0 * 1024.0;
    const double mb = 1024.0 * 1024.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bytes >= (unsigned long long)gb) {
        ss << (bytes / gb) << " GB";
    } else {
        ss << (bytes / mb) << " MB";
    }
    return ss.str();
}

static double bytes_to_gb(unsigned long long bytes) {
    return (double)bytes / (1024.0 * 1024.0 * 1024.0);
}

#ifdef _WIN32
static void iniciar_cpu_counter() {
    if (g_cpuIniciado) return;
    PdhOpenQuery(NULL, 0, &g_cpuQuery);
    PdhAddEnglishCounterA(g_cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &g_cpuTotal);
    PdhCollectQueryData(g_cpuQuery);
    g_cpuIniciado = true;
}
#else
static std::string ler_proc_linha(const std::string& arquivo, const std::string& chave) {
    std::ifstream f(arquivo);
    std::string linha;
    while (std::getline(f, linha)) {
        if (linha.find(chave) == 0) {
            size_t pos = linha.find(':');
            if (pos != std::string::npos) {
                std::string valor = linha.substr(pos + 1);
                size_t start = valor.find_first_not_of(" \t");
                if (start != std::string::npos) valor = valor.substr(start);
                return valor;
            }
        }
    }
    return "";
}

static void ler_cpu_stat(unsigned long long& idle, unsigned long long& total) {
    std::ifstream f("/proc/stat");
    std::string linha;
    std::getline(f, linha);
    std::istringstream iss(linha);
    std::string cpu;
    iss >> cpu;
    unsigned long long values[10] = {0};
    total = 0;
    for (int i = 0; i < 10 && iss >> values[i]; i++) {
        total += values[i];
    }
    idle = values[3] + values[4];
}

static void iniciar_cpu_counter() {
    if (g_cpuIniciado) return;
    ler_cpu_stat(g_prevIdle, g_prevTotal);
    g_cpuIniciado = true;
}

static int contar_processos() {
    int count = 0;
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            bool isProc = true;
            for (int i = 0; entry->d_name[i]; i++) {
                if (entry->d_name[i] < '0' || entry->d_name[i] > '9') {
                    isProc = false;
                    break;
                }
            }
            if (isProc && entry->d_name[0] != '\0') count++;
        }
        closedir(dir);
    }
    return count;
}
#endif

// =====================
// INFORMAÇÕES ESTÁTICAS
// =====================

JP_EXPORT JPValor jp_hardinfo_cpu(JPValor* args, int numArgs) {
#ifdef _WIN32
    int cpuInfo[4] = { -1 };
    char cpuBrandString[0x40];
    memset(cpuBrandString, 0, sizeof(cpuBrandString));
    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];
    if (nExIds >= 0x80000004) {
        for (int i = 0x80000002; i <= 0x80000004; ++i) {
            __cpuid(cpuInfo, i);
            memcpy(cpuBrandString + (i - 0x80000002) * 16, cpuInfo, sizeof(cpuInfo));
        }
        return jp_string(cpuBrandString);
    }
#else
    unsigned int eax, ebx, ecx, edx;
    char brand[49];
    memset(brand, 0, sizeof(brand));
    if (__get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx) && eax >= 0x80000004) {
        for (unsigned int i = 0x80000002; i <= 0x80000004; i++) {
            __get_cpuid(i, &eax, &ebx, &ecx, &edx);
            int offset = (i - 0x80000002) * 16;
            memcpy(brand + offset, &eax, 4);
            memcpy(brand + offset + 4, &ebx, 4);
            memcpy(brand + offset + 8, &ecx, 4);
            memcpy(brand + offset + 12, &edx, 4);
        }
        char* p = brand;
        while (*p == ' ') p++;
        return jp_string(p);
    }
    std::string nome = ler_proc_linha("/proc/cpuinfo", "model name");
    if (!nome.empty()) return jp_string(nome.c_str());
#endif
    return jp_string("CPU Desconhecida");
}

JP_EXPORT JPValor jp_hardinfo_gpu(JPValor* args, int numArgs) {
#ifdef _WIN32
    DISPLAY_DEVICEA dd;
    dd.cb = sizeof(dd);
    int i = 0;
    while (EnumDisplayDevicesA(NULL, i, &dd, 0)) {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
            return jp_string(dd.DeviceString);
        }
        i++;
    }
#else
    {
        std::ifstream f("/proc/driver/nvidia/gpus/0/information");
        if (f.is_open()) {
            std::string linha;
            while (std::getline(f, linha)) {
                if (linha.find("Model:") == 0) {
                    size_t pos = linha.find(':');
                    if (pos != std::string::npos) {
                        std::string nome = linha.substr(pos + 1);
                        size_t start = nome.find_first_not_of(" \t");
                        if (start != std::string::npos) nome = nome.substr(start);
                        return jp_string(nome.c_str());
                    }
                }
            }
        }
    }
    {
        std::ifstream f("/sys/class/drm/card0/device/label");
        if (f.is_open()) {
            std::string nome;
            std::getline(f, nome);
            if (!nome.empty()) return jp_string(nome.c_str());
        }
    }
    {
        FILE* pipe = popen("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | sed 's/.*: //'", "r");
        if (pipe) {
            char buffer[256];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                result.pop_back();
            if (!result.empty()) return jp_string(result.c_str());
        }
    }
#endif
    return jp_string("GPU Desconhecida");
}

JP_EXPORT JPValor jp_hardinfo_memoria(JPValor* args, int numArgs) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        std::string s = format_bytes(memInfo.ullTotalPhys);
        return jp_string(s.c_str());
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        std::string s = format_bytes((unsigned long long)info.totalram * info.mem_unit);
        return jp_string(s.c_str());
    }
#endif
    return jp_string("0 GB");
}

JP_EXPORT JPValor jp_hardinfo_hd(JPValor* args, int numArgs) {
    std::string resultado;
#ifdef _WIN32
    DWORD drives = GetLogicalDrives();
    char driveLetter[] = "A:\\";
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            driveLetter[0] = 'A' + i;
            ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
            if (GetDiskFreeSpaceExA(driveLetter, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                if (!resultado.empty()) resultado += "; ";
                std::string volName = std::string(driveLetter);
                volName.pop_back();
                resultado += volName + " [" + format_bytes(totalNumberOfBytes.QuadPart) + "]";
            }
        }
    }
#else
    FILE* mtab = setmntent("/etc/mtab", "r");
    if (mtab) {
        struct mntent* entry;
        while ((entry = getmntent(mtab)) != NULL) {
            if (std::string(entry->mnt_fsname).find("/dev/") != 0) continue;
            if (std::string(entry->mnt_fsname).find("/dev/loop") == 0) continue;
            struct statvfs stat;
            if (statvfs(entry->mnt_dir, &stat) == 0) {
                unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
                if (total == 0) continue;
                if (!resultado.empty()) resultado += "; ";
                resultado += std::string(entry->mnt_dir) + " [" + format_bytes(total) + "]";
            }
        }
        endmntent(mtab);
    }
#endif
    if (resultado.empty()) return jp_string("Nenhum disco encontrado");
    return jp_string(resultado.c_str());
}

JP_EXPORT JPValor jp_hardinfo_cpu_nucleos(JPValor* args, int numArgs) {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return jp_int((int)sysInfo.dwNumberOfProcessors);
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return jp_int(nprocs > 0 ? (int)nprocs : 1);
#endif
}

// No Linux retorna info do kernel (Ex: "Linux 6.5.0-44-generic")
JP_EXPORT JPValor jp_hardinfo_windows(JPValor* args, int numArgs) {
#ifdef _WIN32
    OSVERSIONINFOEXA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    #pragma warning(disable : 4996)
    GetVersionExA((OSVERSIONINFOA*)&osvi);
    #pragma warning(default : 4996)
    std::stringstream ss;
    ss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
    ss << " (Build " << osvi.dwBuildNumber << ")";
    return jp_string(ss.str().c_str());
#else
    struct utsname buf;
    if (uname(&buf) == 0) {
        std::stringstream ss;
        ss << buf.sysname << " " << buf.release;
        return jp_string(ss.str().c_str());
    }
    return jp_string("Linux Desconhecido");
#endif
}

JP_EXPORT JPValor jp_hardinfo_hostname(JPValor* args, int numArgs) {
#ifdef _WIN32
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return jp_string(buffer);
    }
#else
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        return jp_string(buffer);
    }
#endif
    return jp_string("Desconhecido");
}

JP_EXPORT JPValor jp_hardinfo_usuario(JPValor* args, int numArgs) {
#ifdef _WIN32
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (GetUserNameA(buffer, &size)) {
        return jp_string(buffer);
    }
#else
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) return jp_string(pw->pw_name);
    const char* user = getenv("USER");
    if (user) return jp_string(user);
#endif
    return jp_string("Desconhecido");
}

// ========================
// MONITORAMENTO TEMPO REAL
// ========================

JP_EXPORT JPValor jp_hardinfo_cpu_percent(JPValor* args, int numArgs) {
    iniciar_cpu_counter();
#ifdef _WIN32
    PdhCollectQueryData(g_cpuQuery);
    PDH_FMT_COUNTERVALUE counterVal;
    PdhGetFormattedCounterValue(g_cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    return jp_int((int)counterVal.doubleValue);
#else
    unsigned long long idle, total;
    ler_cpu_stat(idle, total);
    unsigned long long diffIdle = idle - g_prevIdle;
    unsigned long long diffTotal = total - g_prevTotal;
    g_prevIdle = idle;
    g_prevTotal = total;
    if (diffTotal == 0) return jp_int(0);
    int percent = (int)(100.0 * (1.0 - ((double)diffIdle / (double)diffTotal)));
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return jp_int(percent);
#endif
}

JP_EXPORT JPValor jp_hardinfo_memoria_total(JPValor* args, int numArgs) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        std::string s = format_bytes(memInfo.ullTotalPhys);
        return jp_string(s.c_str());
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        std::string s = format_bytes((unsigned long long)info.totalram * info.mem_unit);
        return jp_string(s.c_str());
    }
#endif
    return jp_string("0 GB");
}

JP_EXPORT JPValor jp_hardinfo_memoria_usada(JPValor* args, int numArgs) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        unsigned long long usada = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        std::string s = format_bytes(usada);
        return jp_string(s.c_str());
    }
#else
    unsigned long long total = 0, available = 0;
    std::ifstream f("/proc/meminfo");
    std::string linha;
    while (std::getline(f, linha)) {
        if (linha.find("MemTotal:") == 0) {
            std::istringstream iss(linha);
            std::string key; unsigned long long val;
            iss >> key >> val;
            total = val * 1024;
        }
        if (linha.find("MemAvailable:") == 0) {
            std::istringstream iss(linha);
            std::string key; unsigned long long val;
            iss >> key >> val;
            available = val * 1024;
        }
    }
    if (total > 0) {
        std::string s = format_bytes(total - available);
        return jp_string(s.c_str());
    }
#endif
    return jp_string("0 GB");
}

JP_EXPORT JPValor jp_hardinfo_memoria_livre(JPValor* args, int numArgs) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        std::string s = format_bytes(memInfo.ullAvailPhys);
        return jp_string(s.c_str());
    }
#else
    std::ifstream f("/proc/meminfo");
    std::string linha;
    while (std::getline(f, linha)) {
        if (linha.find("MemAvailable:") == 0) {
            std::istringstream iss(linha);
            std::string key; unsigned long long val;
            iss >> key >> val;
            std::string s = format_bytes(val * 1024);
            return jp_string(s.c_str());
        }
    }
#endif
    return jp_string("0 GB");
}

JP_EXPORT JPValor jp_hardinfo_memoria_percent(JPValor* args, int numArgs) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return jp_int((int)memInfo.dwMemoryLoad);
    }
#else
    unsigned long long total = 0, available = 0;
    std::ifstream f("/proc/meminfo");
    std::string linha;
    while (std::getline(f, linha)) {
        if (linha.find("MemTotal:") == 0) {
            std::istringstream iss(linha);
            std::string key; unsigned long long val;
            iss >> key >> val;
            total = val;
        }
        if (linha.find("MemAvailable:") == 0) {
            std::istringstream iss(linha);
            std::string key; unsigned long long val;
            iss >> key >> val;
            available = val;
        }
    }
    if (total > 0) return jp_int((int)(((total - available) * 100) / total));
#endif
    return jp_int(0);
}

JP_EXPORT JPValor jp_hardinfo_uptime(JPValor* args, int numArgs) {
#ifdef _WIN32
    return jp_int((int)(GetTickCount64() / 1000));
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) return jp_int((int)info.uptime);
    return jp_int(0);
#endif
}

JP_EXPORT JPValor jp_hardinfo_uptime_fmt(JPValor* args, int numArgs) {
#ifdef _WIN32
    unsigned long long segundos = GetTickCount64() / 1000;
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) return jp_string("0s");
    long segundos = info.uptime;
#endif
    int dias = segundos / 86400;
    segundos %= 86400;
    int horas = segundos / 3600;
    segundos %= 3600;
    int minutos = segundos / 60;
    segundos %= 60;
    std::stringstream ss;
    if (dias > 0) ss << dias << "d ";
    if (horas > 0 || dias > 0) ss << horas << "h ";
    if (minutos > 0 || horas > 0 || dias > 0) ss << minutos << "m ";
    ss << segundos << "s";
    return jp_string(ss.str().c_str());
}

// Windows: recebe letra do drive (ex: "C")
// Linux: recebe caminho do mount (ex: "/" ou "/home")
JP_EXPORT JPValor jp_hardinfo_hd_livre(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_double(0.0);
    const char* arg = jp_get_string(args[0]);
    if (!arg || !arg[0]) return jp_double(0.0);
#ifdef _WIN32
    char path[4] = { arg[0], ':', '\\', '\0' };
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        return jp_double(bytes_to_gb(totalNumberOfFreeBytes.QuadPart));
    }
#else
    struct statvfs stat;
    if (statvfs(arg, &stat) == 0) {
        return jp_double(bytes_to_gb((unsigned long long)stat.f_bavail * stat.f_frsize));
    }
#endif
    return jp_double(0.0);
}

JP_EXPORT JPValor jp_hardinfo_hd_usado(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_double(0.0);
    const char* arg = jp_get_string(args[0]);
    if (!arg || !arg[0]) return jp_double(0.0);
#ifdef _WIN32
    char path[4] = { arg[0], ':', '\\', '\0' };
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        unsigned long long usado = totalNumberOfBytes.QuadPart - totalNumberOfFreeBytes.QuadPart;
        return jp_double(bytes_to_gb(usado));
    }
#else
    struct statvfs stat;
    if (statvfs(arg, &stat) == 0) {
        unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
        unsigned long long livre = (unsigned long long)stat.f_bavail * stat.f_frsize;
        return jp_double(bytes_to_gb(total - livre));
    }
#endif
    return jp_double(0.0);
}

JP_EXPORT JPValor jp_hardinfo_hd_percent(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_int(0);
    const char* arg = jp_get_string(args[0]);
    if (!arg || !arg[0]) return jp_int(0);
#ifdef _WIN32
    char path[4] = { arg[0], ':', '\\', '\0' };
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        if (totalNumberOfBytes.QuadPart == 0) return jp_int(0);
        unsigned long long usado = totalNumberOfBytes.QuadPart - totalNumberOfFreeBytes.QuadPart;
        return jp_int((int)((usado * 100) / totalNumberOfBytes.QuadPart));
    }
#else
    struct statvfs stat;
    if (statvfs(arg, &stat) == 0) {
        unsigned long long total = stat.f_blocks;
        unsigned long long livre = stat.f_bavail;
        if (total == 0) return jp_int(0);
        return jp_int((int)(((total - livre) * 100) / total));
    }
#endif
    return jp_int(0);
}

JP_EXPORT JPValor jp_hardinfo_hd_total(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_double(0.0);
    const char* arg = jp_get_string(args[0]);
    if (!arg || !arg[0]) return jp_double(0.0);
#ifdef _WIN32
    char path[4] = { arg[0], ':', '\\', '\0' };
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        return jp_double(bytes_to_gb(totalNumberOfBytes.QuadPart));
    }
#else
    struct statvfs stat;
    if (statvfs(arg, &stat) == 0) {
        return jp_double(bytes_to_gb((unsigned long long)stat.f_blocks * stat.f_frsize));
    }
#endif
    return jp_double(0.0);
}

JP_EXPORT JPValor jp_hardinfo_processos(JPValor* args, int numArgs) {
#ifdef _WIN32
    DWORD aProcesses[1024], cbNeeded;
    if (EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return jp_int((int)(cbNeeded / sizeof(DWORD)));
    }
    return jp_int(0);
#else
    return jp_int(contar_processos());
#endif
}