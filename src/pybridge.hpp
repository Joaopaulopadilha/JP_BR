// pybridge.hpp
// Ponte entre JPLang e Python embedded - carrega python3.dll/libpython3.so dinamicamente
// Permite que qualquer biblioteca .jpd execute codigo Python sem dependencia estatica
// Uso: inclua este header na sua DLL e chame py_init(), py_exec(), py_finalizar()

#ifndef PYBRIDGE_HPP
#define PYBRIDGE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
    #include <windows.h>
    #define PY_LIB_HANDLE HMODULE
    #define PY_LOAD_LIB(path) LoadLibraryA(path)
    #define PY_GET_FUNC(h, name) GetProcAddress(h, name)
    #define PY_FREE_LIB(h) FreeLibrary(h)
    #define PY_PATH_SEP "\\"
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #define PY_LIB_HANDLE void*
    #define PY_LOAD_LIB(path) dlopen(path, RTLD_LAZY | RTLD_GLOBAL)
    #define PY_GET_FUNC(h, name) dlsym(h, name)
    #define PY_FREE_LIB(h) dlclose(h)
    #define PY_PATH_SEP "/"
#endif

// ============================================================
// TIPOS DA PYTHON C API (carregados dinamicamente)
// ============================================================

// Ponteiros de funcao da Python C API
typedef void  (*fn_Py_SetPythonHome)(const wchar_t*);
typedef void  (*fn_Py_SetPath)(const wchar_t*);
typedef void  (*fn_Py_Initialize)(void);
typedef void  (*fn_Py_Finalize)(void);
typedef int   (*fn_Py_IsInitialized)(void);
typedef int   (*fn_PyRun_SimpleString)(const char*);
typedef int   (*fn_PyRun_SimpleFile)(FILE*, const char*);
typedef void* (*fn_PyImport_ImportModule)(const char*);
typedef void* (*fn_PyObject_GetAttrString)(void*, const char*);
typedef void* (*fn_PyObject_CallObject)(void*, void*);
typedef void* (*fn_PyTuple_New)(long);
typedef int   (*fn_PyTuple_SetItem)(void*, long, void*);
typedef void* (*fn_PyUnicode_FromString)(const char*);
typedef void* (*fn_PyLong_FromLong)(long);
typedef void* (*fn_PyFloat_FromDouble)(double);
typedef const char* (*fn_PyUnicode_AsUTF8)(void*);
typedef long  (*fn_PyLong_AsLong)(void*);
typedef double(*fn_PyFloat_AsDouble)(void*);
typedef void* (*fn_PyErr_Occurred)(void);
typedef void  (*fn_PyErr_Print)(void);
typedef void  (*fn_PyErr_Clear)(void);
typedef void  (*fn_Py_DecRef)(void*);
typedef void  (*fn_Py_IncRef)(void*);
typedef void* (*fn_PyList_New)(long);
typedef int   (*fn_PyList_Append)(void*, void*);
typedef long  (*fn_PyList_Size)(void*);
typedef void* (*fn_PyList_GetItem)(void*, long);
typedef int   (*fn_PyObject_IsTrue)(void*);
typedef void* (*fn_Py_BuildValue)(const char*, ...);
typedef void* (*fn_PySys_SetObject)(const char*, void*);

// ============================================================
// ESTADO GLOBAL DO PYBRIDGE
// ============================================================

static PY_LIB_HANDLE g_pylib = NULL;
static int g_py_inicializado = 0;
static char g_py_base_path[1024] = {0};  // caminho base do Python embedded

// Ponteiros carregados
static fn_Py_SetPythonHome    p_Py_SetPythonHome    = NULL;
static fn_Py_SetPath          p_Py_SetPath          = NULL;
static fn_Py_Initialize       p_Py_Initialize       = NULL;
static fn_Py_Finalize         p_Py_Finalize         = NULL;
static fn_Py_IsInitialized    p_Py_IsInitialized    = NULL;
static fn_PyRun_SimpleString  p_PyRun_SimpleString   = NULL;
static fn_PyRun_SimpleFile    p_PyRun_SimpleFile     = NULL;
static fn_PyImport_ImportModule p_PyImport_ImportModule = NULL;
static fn_PyObject_GetAttrString p_PyObject_GetAttrString = NULL;
static fn_PyObject_CallObject p_PyObject_CallObject  = NULL;
static fn_PyTuple_New         p_PyTuple_New          = NULL;
static fn_PyTuple_SetItem     p_PyTuple_SetItem      = NULL;
static fn_PyUnicode_FromString p_PyUnicode_FromString = NULL;
static fn_PyLong_FromLong     p_PyLong_FromLong      = NULL;
static fn_PyFloat_FromDouble  p_PyFloat_FromDouble   = NULL;
static fn_PyUnicode_AsUTF8    p_PyUnicode_AsUTF8     = NULL;
static fn_PyLong_AsLong       p_PyLong_AsLong        = NULL;
static fn_PyFloat_AsDouble    p_PyFloat_AsDouble     = NULL;
static fn_PyErr_Occurred      p_PyErr_Occurred       = NULL;
static fn_PyErr_Print         p_PyErr_Print          = NULL;
static fn_PyErr_Clear         p_PyErr_Clear          = NULL;
static fn_Py_DecRef           p_Py_DecRef            = NULL;
static fn_Py_IncRef           p_Py_IncRef            = NULL;
static fn_PyList_New          p_PyList_New           = NULL;
static fn_PyList_Append       p_PyList_Append        = NULL;
static fn_PyList_Size         p_PyList_Size          = NULL;
static fn_PyList_GetItem      p_PyList_GetItem       = NULL;
static fn_PyObject_IsTrue     p_PyObject_IsTrue      = NULL;
static fn_Py_BuildValue       p_Py_BuildValue        = NULL;
static fn_PySys_SetObject     p_PySys_SetObject      = NULL;

// ============================================================
// FUNCOES INTERNAS
// ============================================================

// Carrega um simbolo da DLL do Python
static void* py_load_sym(const char* name) {
    void* sym = (void*)PY_GET_FUNC(g_pylib, name);
    if (!sym) {
        printf("[pybridge] Aviso: simbolo '%s' nao encontrado\n", name);
    }
    return sym;
}

// Carrega todos os simbolos necessarios
static int py_load_symbols(void) {
    #define LOAD(var, type, name) var = (type)py_load_sym(name); if (!var) falhas++
    
    int falhas = 0;
    
    LOAD(p_Py_SetPythonHome,     fn_Py_SetPythonHome,     "Py_SetPythonHome");
    LOAD(p_Py_SetPath,           fn_Py_SetPath,           "Py_SetPath");
    LOAD(p_Py_Initialize,        fn_Py_Initialize,        "Py_Initialize");
    LOAD(p_Py_Finalize,          fn_Py_Finalize,          "Py_Finalize");
    LOAD(p_Py_IsInitialized,     fn_Py_IsInitialized,     "Py_IsInitialized");
    LOAD(p_PyRun_SimpleString,   fn_PyRun_SimpleString,   "PyRun_SimpleString");
    LOAD(p_PyRun_SimpleFile,     fn_PyRun_SimpleFile,     "PyRun_SimpleFile");
    LOAD(p_PyImport_ImportModule,fn_PyImport_ImportModule, "PyImport_ImportModule");
    LOAD(p_PyObject_GetAttrString,fn_PyObject_GetAttrString,"PyObject_GetAttrString");
    LOAD(p_PyObject_CallObject,  fn_PyObject_CallObject,  "PyObject_CallObject");
    LOAD(p_PyTuple_New,          fn_PyTuple_New,          "PyTuple_New");
    LOAD(p_PyTuple_SetItem,      fn_PyTuple_SetItem,      "PyTuple_SetItem");
    LOAD(p_PyUnicode_FromString, fn_PyUnicode_FromString, "PyUnicode_FromString");
    LOAD(p_PyLong_FromLong,      fn_PyLong_FromLong,      "PyLong_FromLong");
    LOAD(p_PyFloat_FromDouble,   fn_PyFloat_FromDouble,   "PyFloat_FromDouble");
    LOAD(p_PyUnicode_AsUTF8,     fn_PyUnicode_AsUTF8,     "PyUnicode_AsUTF8");
    LOAD(p_PyLong_AsLong,        fn_PyLong_AsLong,        "PyLong_AsLong");
    LOAD(p_PyFloat_AsDouble,     fn_PyFloat_AsDouble,     "PyFloat_AsDouble");
    LOAD(p_PyErr_Occurred,       fn_PyErr_Occurred,       "PyErr_Occurred");
    LOAD(p_PyErr_Print,          fn_PyErr_Print,          "PyErr_Print");
    LOAD(p_PyErr_Clear,          fn_PyErr_Clear,          "PyErr_Clear");
    LOAD(p_Py_DecRef,            fn_Py_DecRef,            "Py_DecRef");
    LOAD(p_Py_IncRef,            fn_Py_IncRef,            "Py_IncRef");
    LOAD(p_PyList_New,           fn_PyList_New,           "PyList_New");
    LOAD(p_PyList_Append,        fn_PyList_Append,        "PyList_Append");
    LOAD(p_PyList_Size,          fn_PyList_Size,           "PyList_Size");
    LOAD(p_PyList_GetItem,       fn_PyList_GetItem,        "PyList_GetItem");
    LOAD(p_PyObject_IsTrue,      fn_PyObject_IsTrue,       "PyObject_IsTrue");
    LOAD(p_Py_BuildValue,        fn_Py_BuildValue,         "Py_BuildValue");
    LOAD(p_PySys_SetObject,      fn_PySys_SetObject,       "PySys_SetObject");
    
    #undef LOAD
    
    // Funcoes essenciais: Initialize, Finalize, RunSimpleString
    if (!p_Py_Initialize || !p_Py_Finalize || !p_PyRun_SimpleString) {
        printf("[pybridge] Erro: funcoes essenciais do Python nao encontradas\n");
        return 0;
    }
    
    return 1;
}

// Converte string char* para wchar_t* (necessario pra API do Python)
static wchar_t* py_char_to_wchar(const char* str) {
    if (!str) return NULL;
    
    #ifdef _WIN32
        size_t len = strlen(str) + 1;
        wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (!wstr) return NULL;
        MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, (int)len);
        return wstr;
    #else
        // Salva locale atual, força UTF-8 pra mbstowcs funcionar com acentos
        const char* old_locale = setlocale(LC_CTYPE, NULL);
        char saved_locale[256] = {0};
        if (old_locale) strncpy(saved_locale, old_locale, 255);
        
        setlocale(LC_CTYPE, "C.UTF-8");
        
        // Descobre tamanho necessario
        size_t needed = mbstowcs(NULL, str, 0);
        if (needed == (size_t)-1) {
            // Fallback: tenta locale generico
            setlocale(LC_CTYPE, "");
            needed = mbstowcs(NULL, str, 0);
        }
        if (needed == (size_t)-1) {
            // Ultimo recurso: conversao manual byte a byte
            if (saved_locale[0]) setlocale(LC_CTYPE, saved_locale);
            size_t len = strlen(str) + 1;
            wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
            if (!wstr) return NULL;
            for (size_t i = 0; i < len; i++) {
                wstr[i] = (wchar_t)(unsigned char)str[i];
            }
            return wstr;
        }
        
        wchar_t* wstr = (wchar_t*)malloc((needed + 1) * sizeof(wchar_t));
        if (!wstr) {
            if (saved_locale[0]) setlocale(LC_CTYPE, saved_locale);
            return NULL;
        }
        mbstowcs(wstr, str, needed + 1);
        
        // Restaura locale original
        if (saved_locale[0]) setlocale(LC_CTYPE, saved_locale);
        
        return wstr;
    #endif
}

// Descobre o caminho base do Python embedded relativo a biblioteca que chamou
// Procura em: bibliotecas/<lib>/python/ ou compilador/windows/py_win/ ou compilador/linux/py_linux/
// No Linux, a libpython fica em <base>/lib/libpython3.10.so (build standalone)
// No Windows, a dll fica em <base>/python310.dll (direto na raiz)
static int py_find_base_path(const char* lib_hint) {
    char tentativas[8][1024];
    int n = 0;
    
    // Se a biblioteca deu uma dica de onde esta
    if (lib_hint && lib_hint[0]) {
        snprintf(tentativas[n++], 1024, "bibliotecas%s%s%spython", 
                 PY_PATH_SEP, lib_hint, PY_PATH_SEP);
    }
    
    // Caminhos padrao
    #ifdef _WIN32
        snprintf(tentativas[n++], 1024, "compilador%swindows%spy_win", PY_PATH_SEP, PY_PATH_SEP);
        snprintf(tentativas[n++], 1024, "py_win");
        snprintf(tentativas[n++], 1024, "python");
    #else
        snprintf(tentativas[n++], 1024, "compilador%slinux%spy_linux", PY_PATH_SEP, PY_PATH_SEP);
        snprintf(tentativas[n++], 1024, "py_linux");
        snprintf(tentativas[n++], 1024, "python");
    #endif
    
    for (int i = 0; i < n; i++) {
        // Verifica se a DLL do Python existe nesse path
        char dll_path[1024];
        #ifdef _WIN32
            // Windows: dll na raiz da pasta (py_win/python310.dll)
            snprintf(dll_path, 1024, "%s%spython310.dll", tentativas[i], PY_PATH_SEP);
        #else
            // Linux: testa versoes de 3.14 ate 3.6 em lib/ e na raiz
            for (int minor = 14; minor >= 6; minor--) {
                // Tenta em lib/ (build standalone: py_linux/lib/libpython3.X.so)
                snprintf(dll_path, 1024, "%s/lib/libpython3.%d.so", tentativas[i], minor);
                {
                    FILE* fl = fopen(dll_path, "rb");
                    if (!fl) {
                        snprintf(dll_path, 1024, "%s/lib/libpython3.%d.so.1.0", tentativas[i], minor);
                        fl = fopen(dll_path, "rb");
                    }
                    if (fl) {
                        fclose(fl);
                        snprintf(g_py_base_path, 1024, "%s|3.%d", tentativas[i], minor);
                        return 1;
                    }
                }
                // Fallback: tenta na raiz da pasta (py_linux/libpython3.X.so)
                snprintf(dll_path, 1024, "%s/libpython3.%d.so", tentativas[i], minor);
                {
                    FILE* fl = fopen(dll_path, "rb");
                    if (!fl) {
                        snprintf(dll_path, 1024, "%s/libpython3.%d.so.1.0", tentativas[i], minor);
                        fl = fopen(dll_path, "rb");
                    }
                    if (fl) {
                        fclose(fl);
                        snprintf(g_py_base_path, 1024, "%s|3.%d", tentativas[i], minor);
                        return 1;
                    }
                }
            }
        #endif
        
        #ifdef _WIN32
        FILE* f = fopen(dll_path, "rb");
        if (f) {
            fclose(f);
            strncpy(g_py_base_path, tentativas[i], 1023);
            g_py_base_path[1023] = '\0';
            return 1;
        }
        #endif
    }
    
    #ifndef _WIN32
    // Fallback Linux: tenta encontrar libpython do sistema
    // Testa versoes de 3.14 ate 3.6
    {
        const char* lib_dirs[] = {
            "/usr/lib/x86_64-linux-gnu",
            "/usr/lib64",
            "/usr/lib",
            "/usr/lib/aarch64-linux-gnu",
            NULL
        };
        for (int minor = 14; minor >= 6; minor--) {
            // Tenta dlopen direto (pode funcionar se ldconfig esta ok)
            char so_name[256];
            snprintf(so_name, 256, "libpython3.%d.so.1.0", minor);
            void* test = dlopen(so_name, RTLD_LAZY);
            if (!test) {
                snprintf(so_name, 256, "libpython3.%d.so", minor);
                test = dlopen(so_name, RTLD_LAZY);
            }
            // Tenta paths absolutos comuns
            if (!test) {
                for (int d = 0; lib_dirs[d]; d++) {
                    char full[512];
                    snprintf(full, 512, "%s/libpython3.%d.so.1.0", lib_dirs[d], minor);
                    test = dlopen(full, RTLD_LAZY);
                    if (!test) {
                        snprintf(full, 512, "%s/libpython3.%d.so", lib_dirs[d], minor);
                        test = dlopen(full, RTLD_LAZY);
                    }
                    if (test) break;
                }
            }
            if (test) {
                dlclose(test);
                snprintf(g_py_base_path, 1024, "__system__:3.%d", minor);
                return 1;
            }
        }
    }
    #endif
    
    return 0;
}

// ============================================================
// API PUBLICA
// ============================================================

// Inicializa o Python embedded
// lib_hint: nome da biblioteca que esta chamando (ex: "visionInfer"), pode ser NULL
// Retorna: 1 = sucesso, 0 = erro
static int py_init(const char* lib_hint) {
    if (g_py_inicializado) return 1;
    
    // Encontra o Python embedded
    if (!py_find_base_path(lib_hint)) {
        printf("[pybridge] Erro: Python nao encontrado\n");
        printf("[pybridge] Instale python3-dev ou coloque python em compilador/windows/py_win/ ou compilador/linux/py_linux/\n");
        return 0;
    }
    
    // Verifica se esta usando Python do sistema (Linux)
    int usando_sistema = 0;
    char py_version[32] = "3.10";  // default fallback
    
    #ifndef _WIN32
    // Parseia formato "base_path|3.X" do py_find_base_path
    {
        char* sep = strchr(g_py_base_path, '|');
        if (sep) {
            *sep = '\0';
            strncpy(py_version, sep + 1, 31);
            py_version[31] = '\0';
        }
    }
    
    if (strncmp(g_py_base_path, "__system__:", 11) == 0) {
        usando_sistema = 1;
        strncpy(py_version, g_py_base_path + 11, 31);
        py_version[31] = '\0';
        printf("[pybridge] Usando Python %s do sistema\n", py_version);
        
        // Carrega libpython do sistema via dlopen
        char so_name[256];
        const char* sys_dirs[] = {
            "/usr/lib/x86_64-linux-gnu",
            "/usr/lib64",
            "/usr/lib",
            "/usr/lib/aarch64-linux-gnu",
            NULL
        };
        
        // Tenta dlopen por nome
        snprintf(so_name, 256, "libpython%s.so.1.0", py_version);
        g_pylib = dlopen(so_name, RTLD_LAZY | RTLD_GLOBAL);
        if (!g_pylib) {
            snprintf(so_name, 256, "libpython%s.so", py_version);
            g_pylib = dlopen(so_name, RTLD_LAZY | RTLD_GLOBAL);
        }
        // Tenta paths absolutos
        if (!g_pylib) {
            for (int d = 0; sys_dirs[d]; d++) {
                char full[512];
                snprintf(full, 512, "%s/libpython%s.so.1.0", sys_dirs[d], py_version);
                g_pylib = dlopen(full, RTLD_LAZY | RTLD_GLOBAL);
                if (!g_pylib) {
                    snprintf(full, 512, "%s/libpython%s.so", sys_dirs[d], py_version);
                    g_pylib = dlopen(full, RTLD_LAZY | RTLD_GLOBAL);
                }
                if (g_pylib) break;
            }
        }
        if (!g_pylib) {
            printf("[pybridge] Erro: nao foi possivel carregar libpython%s\n", py_version);
            printf("[pybridge] Instale: sudo apt install libpython%s-dev\n", py_version);
            printf("[pybridge] Ou no Fedora: sudo dnf install python3-devel\n");
            return 0;
        }
    }
    #endif
    
    if (!usando_sistema) {
        printf("[pybridge] Python %s encontrado em: %s\n", py_version, g_py_base_path);
    }
    
    // Carrega a DLL do Python (modo embutido)
    if (!usando_sistema) {
    char dll_path[2048];
    #ifdef _WIN32
        // Converte pra path absoluto pra resolver dependencias
        char abs_path[2048];
        GetFullPathNameA(g_py_base_path, 2048, abs_path, NULL);
        SetDllDirectoryA(abs_path);
        snprintf(dll_path, 2048, "%s\\python310.dll", abs_path);
    #else
        // Converte pra path absoluto usando realpath
        char abs_path[PATH_MAX];
        char* resolved = realpath(g_py_base_path, abs_path);
        if (!resolved) {
            // Fallback: monta manualmente
            if (g_py_base_path[0] == '/') {
                strncpy(abs_path, g_py_base_path, PATH_MAX - 1);
            } else {
                char cwd[PATH_MAX];
                if (getcwd(cwd, PATH_MAX)) {
                    snprintf(abs_path, PATH_MAX, "%s/%s", cwd, g_py_base_path);
                } else {
                    strncpy(abs_path, g_py_base_path, PATH_MAX - 1);
                }
            }
        }
        // Linux: libpython fica em <base>/lib/ (build standalone)
        snprintf(dll_path, 2048, "%s/lib/libpython%s.so", abs_path, py_version);
        // Se nao existe em lib/, tenta .so.1.0 e na raiz
        FILE* test_f = fopen(dll_path, "rb");
        if (!test_f) {
            snprintf(dll_path, 2048, "%s/lib/libpython%s.so.1.0", abs_path, py_version);
            test_f = fopen(dll_path, "rb");
        }
        if (!test_f) {
            snprintf(dll_path, 2048, "%s/libpython%s.so", abs_path, py_version);
            test_f = fopen(dll_path, "rb");
        }
        if (!test_f) {
            snprintf(dll_path, 2048, "%s/libpython%s.so.1.0", abs_path, py_version);
        } else {
            fclose(test_f);
        }
    #endif
    
    g_pylib = PY_LOAD_LIB(dll_path);
    if (!g_pylib) {
        // Tenta python3.dll como fallback (redirecionador)
        #ifdef _WIN32
            snprintf(dll_path, 1024, "%s\\python3.dll", abs_path);
            g_pylib = PY_LOAD_LIB(dll_path);
        #endif
        
        if (!g_pylib) {
            printf("[pybridge] Erro: nao foi possivel carregar %s\n", dll_path);
            #ifdef _WIN32
                printf("[pybridge] GetLastError: %lu\n", GetLastError());
            #else
                printf("[pybridge] dlerror: %s\n", dlerror());
            #endif
            return 0;
        }
    }
    } // fim if (!usando_sistema)
    
    // Carrega simbolos
    if (!py_load_symbols()) {
        PY_FREE_LIB(g_pylib);
        g_pylib = NULL;
        return 0;
    }
    
    // Configura o Python Home e paths
    if (!usando_sistema) {
        // Converte pra path absoluto de novo pra configurar PythonHome
        char abs_path[PATH_MAX];
        #ifdef _WIN32
            GetFullPathNameA(g_py_base_path, PATH_MAX, abs_path, NULL);
        #else
            char* resolved2 = realpath(g_py_base_path, abs_path);
            if (!resolved2) {
                if (g_py_base_path[0] == '/') {
                    strncpy(abs_path, g_py_base_path, PATH_MAX - 1);
                } else {
                    char cwd[PATH_MAX];
                    if (getcwd(cwd, PATH_MAX)) {
                        snprintf(abs_path, PATH_MAX, "%s/%s", cwd, g_py_base_path);
                    } else {
                        strncpy(abs_path, g_py_base_path, PATH_MAX - 1);
                    }
                }
            }
        #endif
        
        wchar_t* whome = py_char_to_wchar(abs_path);
        if (whome && p_Py_SetPythonHome) {
            p_Py_SetPythonHome(whome);
        }
        
        // Monta o sys.path incluindo site-packages
        // Windows: usa Lib/ e Lib/site-packages/
        // Linux: usa lib/python3.10/ e lib/python3.10/site-packages/
        char path_str[8192];
        #ifdef _WIN32
            snprintf(path_str, 8192, 
                     "%s;%s\\Lib;%s\\Lib\\site-packages;%s\\python310.zip",
                     abs_path, abs_path, abs_path, abs_path);
        #else
            snprintf(path_str, 8192, 
                     "%s:%s/lib/python%s:%s/lib/python%s/lib-dynload:%s/lib/python%s/site-packages",
                     abs_path, abs_path, py_version, abs_path, py_version, abs_path, py_version);
        #endif
        
        wchar_t* wpath = py_char_to_wchar(path_str);
        if (wpath && p_Py_SetPath) {
            p_Py_SetPath(wpath);
        }
        
        // Inicializa o interpretador
        p_Py_Initialize();
        
        if (p_Py_IsInitialized && !p_Py_IsInitialized()) {
            printf("[pybridge] Erro: Py_Initialize falhou\n");
            PY_FREE_LIB(g_pylib);
            g_pylib = NULL;
            free(whome);
            free(wpath);
            return 0;
        }
        
        // Adiciona o diretorio de scripts ao sys.path
        char setup[4096];
        #ifdef _WIN32
            snprintf(setup, 4096,
                "import sys\n"
                "sys.path.insert(0, '.')\n"
                "sys.path.insert(0, '%s')\n"
                "sys.path.insert(0, '%s/Lib')\n"
                "sys.path.insert(0, '%s/Lib/site-packages')\n",
                abs_path, abs_path, abs_path);
        #else
            snprintf(setup, 4096,
                "import sys\n"
                "sys.path.insert(0, '.')\n"
                "sys.path.insert(0, '%s')\n"
                "sys.path.insert(0, '%s/lib/python%s')\n"
                "sys.path.insert(0, '%s/lib/python%s/lib-dynload')\n"
                "sys.path.insert(0, '%s/lib/python%s/site-packages')\n",
                abs_path, abs_path, py_version, abs_path, py_version, abs_path, py_version);
        #endif
        
        // Troca \ por / no path pra nao dar problema em strings Python
        for (int i = 0; setup[i]; i++) {
            if (setup[i] == '\\') setup[i] = '/';
        }
        
        p_PyRun_SimpleString(setup);
        
        // Detecta e adiciona site-packages do Python do sistema
        // APENAS se a versao do sistema bater com a do embutido
        // Permite que modulos instalados via pip (global ou --user) sejam acessiveis
        {
            char py_setup_system[4096];
            snprintf(py_setup_system, 4096,
                "import sys as _sys, os as _os, subprocess as _sp\n"
                "def _pybridge_add_system_paths():\n"
                "    '''Descobre os paths do Python do sistema (mesma versao) e adiciona ao sys.path'''\n"
                "    _embed_ver = '%s'\n"
                "    # Tenta encontrar Python do sistema com a mesma versao\n"
                "    _py_names = ['python' + _embed_ver, 'python3', 'python']\n"
                "    _py_cmd = None\n"
                "    for _name in _py_names:\n"
                "        try:\n"
                "            _r = _sp.run([_name, '-c', 'import sys; print(f\"{sys.version_info.major}.{sys.version_info.minor}\")'],\n"
                "                         capture_output=True, text=True, timeout=5)\n"
                "            if _r.returncode == 0 and _r.stdout.strip() == _embed_ver:\n"
                "                _py_cmd = _name\n"
                "                break\n"
                "        except Exception:\n"
                "            continue\n"
                "    if not _py_cmd:\n"
                "        return\n"
                "    # Pega paths do Python do sistema (mesma versao)\n"
                "    try:\n"
                "        _r = _sp.run(\n"
                "            [_py_cmd, '-c', 'import sys,site,os;'\n"
                "             'paths=set(sys.path);'\n"
                "             'paths.add(site.getusersitepackages());'\n"
                "             '[paths.add(p) for p in site.getsitepackages()];'\n"
                "             'print(os.pathsep.join(p for p in paths if p and os.path.isdir(p)))'],\n"
                "            capture_output=True, text=True, timeout=10\n"
                "        )\n"
                "        if _r.returncode == 0 and _r.stdout.strip():\n"
                "            for _p in _r.stdout.strip().split(_os.pathsep):\n"
                "                _p = _p.strip()\n"
                "                if _p and _os.path.isdir(_p) and _p not in _sys.path:\n"
                "                    # Filtra: ignora paths de outra versao\n"
                "                    if '/python' in _p and '/python' + _embed_ver not in _p:\n"
                "                        continue\n"
                "                    _sys.path.append(_p)\n"
                "    except Exception:\n"
                "        pass\n"
                "_pybridge_add_system_paths()\n"
                "del _pybridge_add_system_paths\n",
                py_version
            );
            // Troca \ por /
            for (int i = 0; py_setup_system[i]; i++) {
                if (py_setup_system[i] == '\\') py_setup_system[i] = '/';
            }
            p_PyRun_SimpleString(py_setup_system);
        }
        
        free(whome);
        free(wpath);
    } else {
        // Modo sistema: apenas inicializa e adiciona diretorio atual ao path
        p_Py_Initialize();
        
        if (p_Py_IsInitialized && !p_Py_IsInitialized()) {
            printf("[pybridge] Erro: Py_Initialize falhou\n");
            PY_FREE_LIB(g_pylib);
            g_pylib = NULL;
            return 0;
        }
        
        p_PyRun_SimpleString(
            "import sys\n"
            "if '.' not in sys.path:\n"
            "    sys.path.insert(0, '.')\n"
        );
    }
    
    g_py_inicializado = 1;
    
    printf("[pybridge] Python inicializado com sucesso\n");
    return 1;
}

// Executa uma string de codigo Python
// Retorna: 0 = sucesso, -1 = erro
static int py_exec(const char* code) {
    if (!g_py_inicializado) {
        printf("[pybridge] Erro: Python nao inicializado. Chame py_init() primeiro\n");
        return -1;
    }
    
    int ret = p_PyRun_SimpleString(code);
    if (ret != 0) {
        printf("[pybridge] Erro ao executar codigo Python\n");
        return -1;
    }
    return 0;
}

// Executa um arquivo Python
// Retorna: 0 = sucesso, -1 = erro
static int py_exec_file(const char* filepath) {
    if (!g_py_inicializado) {
        printf("[pybridge] Erro: Python nao inicializado. Chame py_init() primeiro\n");
        return -1;
    }
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        printf("[pybridge] Erro: arquivo '%s' nao encontrado\n", filepath);
        return -1;
    }
    
    int ret = p_PyRun_SimpleFile(f, filepath);
    fclose(f);
    
    if (ret != 0) {
        printf("[pybridge] Erro ao executar '%s'\n", filepath);
        return -1;
    }
    return 0;
}

// Adiciona um diretorio ao sys.path do Python
// Util pra apontar pra pasta de scripts da biblioteca
static int py_add_path(const char* dir) {
    if (!g_py_inicializado) return -1;
    
    char cmd[2048];
    snprintf(cmd, 2048, 
        "import sys\n"
        "if '%s' not in sys.path:\n"
        "    sys.path.insert(0, '%s')\n", dir, dir);
    
    // Troca \ por /
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] == '\\') cmd[i] = '/';
    }
    
    return py_exec(cmd);
}

// Instala um pacote pip no Python embedded
// Retorna: 0 = sucesso, -1 = erro
static int py_pip_install(const char* pacote) {
    if (!g_py_inicializado) return -1;
    
    char cmd[2048];
    snprintf(cmd, 2048,
        "import subprocess, sys\n"
        "subprocess.check_call([sys.executable, '-m', 'pip', 'install', '%s', '-q'])\n",
        pacote);
    
    return py_exec(cmd);
}

// Verifica se um modulo Python esta disponivel
// Retorna: 1 = disponivel, 0 = nao encontrado
static int py_modulo_disponivel(const char* modulo) {
    if (!g_py_inicializado) return 0;
    
    char cmd[1024];
    snprintf(cmd, 1024,
        "try:\n"
        "    import %s\n"
        "    _pybridge_result = 1\n"
        "except ImportError:\n"
        "    _pybridge_result = 0\n",
        modulo);
    
    py_exec(cmd);
    
    // Checa o resultado via PyRun
    char check[256];
    snprintf(check, 256,
        "import builtins\n"
        "builtins._pybridge_check = _pybridge_result\n");
    py_exec(check);
    
    // Simplificado: tenta importar e se nao der erro, esta disponivel
    char test[512];
    snprintf(test, 512, "import %s", modulo);
    int ret = p_PyRun_SimpleString(test);
    
    if (ret != 0 && p_PyErr_Clear) {
        p_PyErr_Clear();
    }
    
    return (ret == 0) ? 1 : 0;
}

// Finaliza o Python embedded
static void py_finalizar(void) {
    if (!g_py_inicializado) return;
    
    if (p_Py_Finalize) {
        p_Py_Finalize();
    }
    
    if (g_pylib) {
        PY_FREE_LIB(g_pylib);
        g_pylib = NULL;
    }
    
    g_py_inicializado = 0;
    printf("[pybridge] Python finalizado\n");
}

// Retorna se o Python esta inicializado
static int py_esta_ativo(void) {
    return g_py_inicializado;
}

// Retorna o caminho base do Python embedded
static const char* py_get_base_path(void) {
    return g_py_base_path;
}

// ============================================================
// HELPERS DE VERIFICACAO DE DEPENDENCIAS
// ============================================================

// Verifica se um comando/executavel existe no sistema
// Retorna: 1 = encontrado, 0 = nao encontrado
static int py_check_command(const char* cmd) {
    char check[512];
    #ifdef _WIN32
        snprintf(check, 512, "where %s >nul 2>nul", cmd);
    #else
        snprintf(check, 512, "which %s >/dev/null 2>/dev/null", cmd);
    #endif
    return (system(check) == 0) ? 1 : 0;
}

// Detecta a distro Linux para sugerir o comando correto de instalacao
// Retorna: "apt", "dnf", "pacman", "zypper" ou "pkg" (desconhecido)
static const char* py_detect_pkg_manager(void) {
    #ifdef _WIN32
        return "winget";
    #else
        if (py_check_command("apt"))    return "apt";
        if (py_check_command("dnf"))    return "dnf";
        if (py_check_command("pacman")) return "pacman";
        if (py_check_command("zypper")) return "zypper";
        return "pkg";
    #endif
}

// Monta e imprime o comando de instalacao correto para a distro
// pacotes: lista de nomes no formato "apt_nome|dnf_nome|pacman_nome"
// Se todos forem iguais, basta passar o nome simples ex: "ffmpeg"
static void py_print_install_cmd(const char* pkg_manager, const char* pacote) {
    if (strcmp(pkg_manager, "apt") == 0) {
        printf("  sudo apt install %s\n", pacote);
    } else if (strcmp(pkg_manager, "dnf") == 0) {
        printf("  sudo dnf install %s\n", pacote);
    } else if (strcmp(pkg_manager, "pacman") == 0) {
        printf("  sudo pacman -S %s\n", pacote);
    } else if (strcmp(pkg_manager, "zypper") == 0) {
        printf("  sudo zypper install %s\n", pacote);
    } else {
        printf("  Instale o pacote: %s\n", pacote);
    }
}

// Verifica uma lista de dependencias e mostra o que falta
// deps: array de {comando, pacote_apt, pacote_dnf, pacote_pacman}
// Retorna: numero de dependencias faltando
typedef struct {
    const char* comando;      // executavel a verificar (ex: "ffmpeg")
    const char* pkg_apt;      // nome do pacote no apt
    const char* pkg_dnf;      // nome do pacote no dnf
    const char* pkg_pacman;   // nome do pacote no pacman
} PyDependencia;

static int py_check_deps(const char* lib_nome, PyDependencia* deps, int num_deps) {
    const char* pm = py_detect_pkg_manager();
    int faltando = 0;
    
    for (int i = 0; i < num_deps; i++) {
        if (!py_check_command(deps[i].comando)) {
            if (faltando == 0) {
                printf("[%s] Dependencias faltando:\n", lib_nome);
            }
            faltando++;
            printf("  - %s nao encontrado. Instale com:\n", deps[i].comando);
            
            if (strcmp(pm, "apt") == 0) {
                py_print_install_cmd(pm, deps[i].pkg_apt);
            } else if (strcmp(pm, "dnf") == 0) {
                py_print_install_cmd(pm, deps[i].pkg_dnf);
            } else if (strcmp(pm, "pacman") == 0) {
                py_print_install_cmd(pm, deps[i].pkg_pacman);
            } else {
                py_print_install_cmd(pm, deps[i].comando);
            }
        }
    }
    
    return faltando;
}

// Verifica se um modulo pip esta instalado e mostra como instalar se faltar
// Retorna: 1 = disponivel, 0 = faltando
static int py_check_pip_module(const char* lib_nome, const char* modulo, const char* pip_nome) {
    if (!g_py_inicializado) return 0;
    
    char test[512];
    snprintf(test, 512, "import %s", modulo);
    int ret = p_PyRun_SimpleString(test);
    
    if (ret != 0) {
        if (p_PyErr_Clear) p_PyErr_Clear();
        const char* pkg = pip_nome ? pip_nome : modulo;
        printf("[%s] Modulo Python '%s' nao encontrado.\n", lib_nome, modulo);
        printf("[%s] Instale com:\n", lib_nome);
        
        // Se esta usando Python embutido, usa o binario dele pra instalar
        if (strncmp(g_py_base_path, "__system__", 10) != 0 && g_py_base_path[0] != '\0') {
            printf("  %s/bin/python3 -m pip install %s\n", g_py_base_path, pkg);
            printf("  Ou: %s/bin/python3 -m pip install %s -t %s/lib/python*/site-packages/\n", 
                   g_py_base_path, pkg, g_py_base_path);
        } else {
            printf("  pip install %s\n", pkg);
        }
        return 0;
    }
    return 1;
}

#endif // PYBRIDGE_HPP