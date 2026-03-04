// hdw.cpp
// Biblioteca de hardware para JPLang — multiplataforma (Windows/Linux), linkagem estatica via .obj/.o, extern "C" puro
//
// Compilacao:
//   Windows: g++ -std=c++17 -c -o bibliotecas/hdw/hdw.obj bibliotecas/hdw/hdw.cpp
//   Linux:   g++ -std=c++17 -c -o bibliotecas/hdw/hdw.o   bibliotecas/hdw/hdw.cpp

#include <cstdint>
#include <cstring>
#include <cstdio>

// =============================================================================
// DECLARACOES POR PLATAFORMA
// =============================================================================

#ifdef _WIN32

extern "C" {
    // Kernel32
    typedef struct {
        unsigned long dwLength;
        unsigned long dwMemoryLoad;
        unsigned long long ullTotalPhys;
        unsigned long long ullAvailPhys;
        unsigned long long ullTotalPageFile;
        unsigned long long ullAvailPageFile;
        unsigned long long ullTotalVirtual;
        unsigned long long ullAvailVirtual;
        unsigned long long ullAvailExtendedVirtual;
    } MEMORYSTATUSEX;

    int __stdcall GlobalMemoryStatusEx(MEMORYSTATUSEX* lpBuffer);

    void __stdcall Sleep(unsigned long dwMilliseconds);

    typedef struct {
        unsigned long dwOemId;
        unsigned long dwPageSize;
        void* lpMinimumApplicationAddress;
        void* lpMaximumApplicationAddress;
        unsigned long long dwActiveProcessorMask;
        unsigned long dwNumberOfProcessors;
        unsigned long dwProcessorType;
        unsigned long dwAllocationGranularity;
        unsigned short wProcessorLevel;
        unsigned short wProcessorRevision;
    } SYSTEM_INFO;

    void __stdcall GetSystemInfo(SYSTEM_INFO* lpSystemInfo);

    int __stdcall GetDiskFreeSpaceExA(
        const char* lpDirectoryName,
        unsigned long long* lpFreeBytesAvailableToCaller,
        unsigned long long* lpTotalNumberOfBytes,
        unsigned long long* lpTotalNumberOfFreeBytes
    );

    unsigned long __stdcall GetTickCount64_();
    unsigned long long __stdcall GetTickCount64();

    int __stdcall GetComputerNameA(char* lpBuffer, unsigned long* nSize);

    typedef struct {
        unsigned long dwOSVersionInfoSize;
        unsigned long dwMajorVersion;
        unsigned long dwMinorVersion;
        unsigned long dwBuildNumber;
        unsigned long dwPlatformId;
        char szCSDVersion[128];
    } OSVERSIONINFOA;

    // Filetime para calculo de uso de CPU
    typedef struct { unsigned long dwLowDateTime; unsigned long dwHighDateTime; } FILETIME;
    int __stdcall GetSystemTimes(FILETIME* lpIdleTime, FILETIME* lpKernelTime, FILETIME* lpUserTime);

    unsigned long __stdcall GetCurrentDirectoryA(unsigned long nBufferLength, char* lpBuffer);
    unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
}

#else

#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <fstream>
#include <string>

#endif

// =============================================================================
// BUFFERS INTERNOS
// =============================================================================

static char str_buffer[512];
static char cpu_name_buffer[256];
static bool cpu_name_cached = false;

// =============================================================================
// PROCESSADOR
// =============================================================================

static void detect_cpu_name() {
    if (cpu_name_cached) return;

#ifdef _WIN32
    // Ler nome da CPU via CPUID
    int cpuInfo[4] = {0};
    char* p = cpu_name_buffer;

    // CPUID com EAX=0x80000002, 0x80000003, 0x80000004
    for (unsigned i = 0x80000002; i <= 0x80000004; i++) {
        __asm__ __volatile__(
            "cpuid"
            : "=a"(cpuInfo[0]), "=b"(cpuInfo[1]), "=c"(cpuInfo[2]), "=d"(cpuInfo[3])
            : "a"(i)
        );
        memcpy(p, cpuInfo, 16);
        p += 16;
    }
    cpu_name_buffer[48] = '\0';

    // Trim leading spaces
    char* start = cpu_name_buffer;
    while (*start == ' ') start++;
    if (start != cpu_name_buffer) {
        memmove(cpu_name_buffer, start, strlen(start) + 1);
    }

#else
    // Linux: ler de /proc/cpuinfo
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                pos++; // pular ':'
                while (pos < line.size() && line[pos] == ' ') pos++;
                strncpy(cpu_name_buffer, line.c_str() + pos, sizeof(cpu_name_buffer) - 1);
                cpu_name_buffer[sizeof(cpu_name_buffer) - 1] = '\0';
            }
            break;
        }
    }

    if (cpu_name_buffer[0] == '\0') {
        strcpy(cpu_name_buffer, "Desconhecido");
    }
#endif

    cpu_name_cached = true;
}

extern "C" const char* hdw_cpu_nome() {
    detect_cpu_name();
    return cpu_name_buffer;
}

extern "C" int64_t hdw_cpu_nucleos() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int64_t)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int64_t)n : 1;
#endif
}

extern "C" int64_t hdw_cpu_threads() {
    // Em C puro sem acesso a /sys detalhado, nucleos logicos = threads
    // No Windows, GetSystemInfo retorna processadores logicos (threads)
    // No Linux, _SC_NPROCESSORS_ONLN tambem retorna logicos
    return hdw_cpu_nucleos();
}

// Uso da CPU em porcentagem (0-100)
// Mede a diferenca entre duas leituras com um intervalo curto

extern "C" int64_t hdw_cpu_uso() {
#ifdef _WIN32
    FILETIME idle1, kern1, user1;
    FILETIME idle2, kern2, user2;

    GetSystemTimes(&idle1, &kern1, &user1);

    // Esperar 100ms para medir diferenca
    Sleep(100);

    GetSystemTimes(&idle2, &kern2, &user2);

    auto ft_to_u64 = [](FILETIME ft) -> uint64_t {
        return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    };

    uint64_t idle_diff = ft_to_u64(idle2) - ft_to_u64(idle1);
    uint64_t kern_diff = ft_to_u64(kern2) - ft_to_u64(kern1);
    uint64_t user_diff = ft_to_u64(user2) - ft_to_u64(user1);

    uint64_t total = kern_diff + user_diff;
    if (total == 0) return 0;

    uint64_t busy = total - idle_diff;
    return (int64_t)((busy * 100) / total);

#else
    // Linux: ler /proc/stat duas vezes
    auto read_cpu = [](uint64_t& idle_out, uint64_t& total_out) {
        std::ifstream f("/proc/stat");
        std::string line;
        std::getline(f, line);
        // "cpu  user nice system idle iowait irq softirq steal"
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
        sscanf(line.c_str(), "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        idle_out = idle + iowait;
        total_out = user + nice + system + idle + iowait + irq + softirq + steal;
    };

    uint64_t idle1, total1, idle2, total2;
    read_cpu(idle1, total1);
    usleep(100000); // 100ms
    read_cpu(idle2, total2);

    uint64_t total_diff = total2 - total1;
    uint64_t idle_diff = idle2 - idle1;

    if (total_diff == 0) return 0;
    return (int64_t)(((total_diff - idle_diff) * 100) / total_diff);
#endif
}

// =============================================================================
// MEMORIA (valores em MB)
// =============================================================================

extern "C" int64_t hdw_mem_total() {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    return (int64_t)(ms.ullTotalPhys / (1024 * 1024));
#else
    struct sysinfo si;
    sysinfo(&si);
    return (int64_t)((uint64_t)si.totalram * si.mem_unit / (1024 * 1024));
#endif
}

extern "C" int64_t hdw_mem_livre() {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    return (int64_t)(ms.ullAvailPhys / (1024 * 1024));
#else
    // Usar MemAvailable de /proc/meminfo (mais preciso que sysinfo.freeram)
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("MemAvailable:") != std::string::npos) {
            uint64_t kb = 0;
            sscanf(line.c_str(), "MemAvailable: %lu", &kb);
            return (int64_t)(kb / 1024);
        }
    }
    // Fallback
    struct sysinfo si;
    sysinfo(&si);
    return (int64_t)((uint64_t)si.freeram * si.mem_unit / (1024 * 1024));
#endif
}

extern "C" int64_t hdw_mem_usada() {
    return hdw_mem_total() - hdw_mem_livre();
}

extern "C" int64_t hdw_mem_uso() {
    int64_t total = hdw_mem_total();
    if (total == 0) return 0;
    return (hdw_mem_usada() * 100) / total;
}

// =============================================================================
// ARMAZENAMENTO (valores em MB, disco onde o programa esta rodando)
// =============================================================================

extern "C" int64_t hdw_disco_total() {
#ifdef _WIN32
    unsigned long long total = 0;
    GetDiskFreeSpaceExA(nullptr, nullptr, &total, nullptr);
    return (int64_t)(total / (1024 * 1024));
#else
    struct statvfs sv;
    if (statvfs(".", &sv) != 0) return 0;
    return (int64_t)((uint64_t)sv.f_blocks * sv.f_frsize / (1024 * 1024));
#endif
}

extern "C" int64_t hdw_disco_livre() {
#ifdef _WIN32
    unsigned long long free_bytes = 0;
    GetDiskFreeSpaceExA(nullptr, &free_bytes, nullptr, nullptr);
    return (int64_t)(free_bytes / (1024 * 1024));
#else
    struct statvfs sv;
    if (statvfs(".", &sv) != 0) return 0;
    return (int64_t)((uint64_t)sv.f_bavail * sv.f_frsize / (1024 * 1024));
#endif
}

extern "C" int64_t hdw_disco_usado() {
    return hdw_disco_total() - hdw_disco_livre();
}

extern "C" int64_t hdw_disco_uso() {
    int64_t total = hdw_disco_total();
    if (total == 0) return 0;
    return (hdw_disco_usado() * 100) / total;
}

// =============================================================================
// SISTEMA
// =============================================================================

extern "C" const char* hdw_os_nome() {
#ifdef _WIN32
    strcpy(str_buffer, "Windows");
    return str_buffer;
#else
    struct utsname un;
    if (uname(&un) == 0) {
        snprintf(str_buffer, sizeof(str_buffer), "%s %s", un.sysname, un.release);
    } else {
        strcpy(str_buffer, "Linux");
    }
    return str_buffer;
#endif
}

extern "C" const char* hdw_hostname() {
#ifdef _WIN32
    unsigned long size = sizeof(str_buffer);
    if (!GetComputerNameA(str_buffer, &size)) {
        strcpy(str_buffer, "desconhecido");
    }
    return str_buffer;
#else
    if (gethostname(str_buffer, sizeof(str_buffer)) != 0) {
        strcpy(str_buffer, "desconhecido");
    }
    return str_buffer;
#endif
}

extern "C" int64_t hdw_uptime() {
#ifdef _WIN32
    return (int64_t)(GetTickCount64() / 1000);
#else
    struct sysinfo si;
    sysinfo(&si);
    return (int64_t)si.uptime;
#endif
}

extern "C" const char* hdw_uptime_str() {
    int64_t secs = hdw_uptime();
    int64_t dias = secs / 86400;
    int64_t horas = (secs % 86400) / 3600;
    int64_t mins = (secs % 3600) / 60;
    int64_t segs = secs % 60;

    if (dias > 0) {
        snprintf(str_buffer, sizeof(str_buffer), "%ldd %02lldh %02lldm %02llds",
                 (long)dias, (long long)horas, (long long)mins, (long long)segs);
    } else {
        snprintf(str_buffer, sizeof(str_buffer), "%02lldh %02lldm %02llds",
                 (long long)horas, (long long)mins, (long long)segs);
    }
    return str_buffer;
}