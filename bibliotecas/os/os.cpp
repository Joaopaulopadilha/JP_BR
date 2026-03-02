/*===================================================================*/
/*  Biblioteca “os” – versão corrigida (nome + build)                */
/*===================================================================*/
#if defined(_WIN32)
#include <windows.h>
#include <winreg.h>
#include <cstdio>
#include <cstring>

extern "C" const char* os_info()
{
    static char buffer[512] = {0};

    /*--- 1. Tenta ler a informação do registro (visualização 64‑bits) --------*/
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0,
                      KEY_READ | KEY_WOW64_64KEY,   /* <--- 64‑bit view */
                      &hKey) == ERROR_SUCCESS)
    {
        WCHAR prodNameW[512];
        DWORD type, len = sizeof(prodNameW);

        if (RegQueryValueExW(hKey, L"ProductName", nullptr, &type,
                             (LPBYTE)prodNameW, &len) == ERROR_SUCCESS
            && type == REG_SZ)
        {
            /* converte UTF‑16 (WCHAR) para UTF‑8 */
            int needed = WideCharToMultiByte(CP_UTF8, 0,
                                            prodNameW, -1,
                                            buffer, sizeof(buffer),
                                            nullptr, nullptr);
            if (needed > 0)
            {
                /* lê o número do build (CurrentBuildNumber) */
                DWORD build = 0;
                len = sizeof(DWORD);
                if (RegQueryValueExW(hKey, L"CurrentBuildNumber", nullptr, nullptr,
                                     (LPBYTE)&build, &len) == ERROR_SUCCESS)
                {
                    snprintf(buffer + needed,
                             sizeof(buffer) - needed,
                             " (Build %lu)", build);
                }
            }
        }
        RegCloseKey(hKey);
    }

    /*--- 2. Se não conseguiu, volta para RtlGetVersion (fallback) ----------*/
    if (!buffer[0])
    {
        HMODULE hNtdll = LoadLibraryW(L"ntdll.dll");
        if (hNtdll)
        {
            typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
            RtlGetVersionPtr rtl = (RtlGetVersionPtr)GetProcAddress(hNtdll,"RtlGetVersion");
            if (rtl)
            {
                RTL_OSVERSIONINFOW ver{ sizeof(ver) };
                if (rtl(&ver) == 0)
                {
                    snprintf(buffer, sizeof(buffer),
                             "Windows %lu.%lu (Build %lu)",
                             ver.dwMajorVersion, ver.dwMinorVersion,
                             ver.dwBuildNumber);
                }
            }
            FreeLibrary(hNtdll);
        }

        if (!buffer[0])
            snprintf(buffer, sizeof(buffer), "Windows Unknown");
    }

    return buffer; /* string estática persiste entre chamadas */
}
#endif  /* -----  FIM WINDOWS  ----- */

#if !defined(_WIN32) /* ----- POSIX ----- */
#include <sys/utsname.h>
extern "C" const char* os_info()
{
    static char buffer[256] = {0};
    struct utsname sys;
    if (uname(&sys) == 0)
        snprintf(buffer, sizeof(buffer), "%s %s", sys.sysname, sys.release);
    else
        snprintf(buffer, sizeof(buffer), "Unix Unknown");
    return buffer;
}
#endif
