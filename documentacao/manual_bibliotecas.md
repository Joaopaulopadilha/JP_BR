# Manual de Criação de Bibliotecas para JPLang

## Visão Geral

A linguagem JPLang possui dois modos de execução:

1. **Modo Interpretado** (`./jp programa.jp`): Executa diretamente via VM
2. **Modo Compilado** (`./jp build programa.jp`): Transpila para C e compila com TCC

**Ambos os modos usam o mesmo formato de biblioteca!** Você só precisa exportar funções uma vez, usando a interface `JPValor`.

---

## Suporte Multiplataforma (Windows/Linux)

O JPLang suporta bibliotecas nativas em Windows e Linux. Para manter um único repositório com ambas as versões:

| Plataforma | Nome do Arquivo | Extensão |
|------------|-----------------|----------|
| Windows | `nome.jpd` | `.jpd` ou `.dll` |
| Linux | `libnome.jpd` | `.jpd` ou `.so` |

**O prefixo `lib` no Linux evita conflitos!** Ambos os arquivos podem coexistir na mesma pasta.

### Ordem de Busca

**Windows** procura (em ordem):
1. `bibliotecas/nome/nome.jpd`
2. `bibliotecas/nome/nome.dll`

**Linux** procura (em ordem):
1. `bibliotecas/nome/libnome.jpd` ← prioridade
2. `bibliotecas/nome/libnome.so`
3. `bibliotecas/nome/nome.jpd`
4. `bibliotecas/nome/nome.so`

---

## Estrutura de uma Biblioteca

Uma biblioteca JPLang consiste em:

```
bibliotecas/
└── minhaBiblioteca/
    ├── minhaBiblioteca.cpp      # Código fonte (único para ambas plataformas)
    ├── minhaBiblioteca.jpd      # Compilado Windows
    ├── libminhaBiblioteca.jpd   # Compilado Linux
    └── minhaBiblioteca.jp       # Interface JPLang
```

---

## Compilação Multiplataforma

**IMPORTANTE**: Todos os comandos de compilação são executados da **raiz do projeto**, especificando caminhos relativos completos.

### Windows (MinGW)
```bash
g++ -shared -o bibliotecas/nome/nome.jpd bibliotecas/nome/nome.cpp -O3
```

### Linux
```bash
g++ -shared -fPIC -o bibliotecas/nome/libnome.jpd bibliotecas/nome/nome.cpp -O3
```

### Exemplos Reais

```bash
# Windows - biblioteca tempo
g++ -shared -o bibliotecas/tempo/tempo.jpd bibliotecas/tempo/tempo.cpp -O3

# Linux - biblioteca tempo
g++ -shared -fPIC -o bibliotecas/tempo/libtempo.jpd bibliotecas/tempo/tempo.cpp -O3

# Windows - biblioteca com dependências (ex: overlay com GDI)
g++ -shared -o bibliotecas/overlay/overlay.jpd bibliotecas/overlay/overlay.cpp -lgdi32 -lgdiplus -mwindows -O3

# Linux - biblioteca com dependências (ex: matemática)
g++ -shared -fPIC -o bibliotecas/matematica/libmatematica.jpd bibliotecas/matematica/matematica.cpp -lm -O3
```

---

## Macro de Export Multiplataforma

Use esta macro no início de toda biblioteca para funcionar em ambas plataformas:

```cpp
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif
```

Depois, exporte as funções assim:

```cpp
JP_EXPORT JPValor jp_minha_funcao(JPValor* args, int numArgs) {
    // implementação
}
```

---

## Tipos Obrigatórios

Todas as bibliotecas devem usar estes tipos para comunicação com JPLang:

```cpp
#include <cstdint>
#include <cstring>
#include <cstdlib>

// Tipos de valor suportados
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

// Estrutura de valor variant
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
```

---

## Assinatura das Funções

Todas as funções exportadas devem seguir esta assinatura:

```cpp
JP_EXPORT JPValor jp_nomeFuncao(JPValor* args, int numArgs);
```

- **Prefixo `jp_`**: Obrigatório para todas as funções
- **args**: Array de argumentos passados pelo JPLang
- **numArgs**: Quantidade de argumentos no array
- **Retorno**: Sempre retorna um `JPValor`

---

## Helpers Úteis

```cpp
// Criar valores de retorno
static inline JPValor jp_nulo() {
    JPValor v;
    v.tipo = JP_TIPO_NULO;
    v.valor.inteiro = 0;
    return v;
}

static inline JPValor jp_int(int64_t i) {
    JPValor v;
    v.tipo = JP_TIPO_INT;
    v.valor.inteiro = i;
    return v;
}

static inline JPValor jp_double(double d) {
    JPValor v;
    v.tipo = JP_TIPO_DOUBLE;
    v.valor.decimal = d;
    return v;
}

static inline JPValor jp_bool(int b) {
    JPValor v;
    v.tipo = JP_TIPO_BOOL;
    v.valor.booleano = b ? 1 : 0;
    return v;
}

static inline JPValor jp_string(const char* s) {
    JPValor v;
    v.tipo = JP_TIPO_STRING;
    if (s) {
        size_t len = strlen(s);
        v.valor.texto = (char*)malloc(len + 1);
        if (v.valor.texto) {
            memcpy(v.valor.texto, s, len + 1);
        }
    } else {
        v.valor.texto = NULL;
    }
    return v;
}

// Extrair valores dos argumentos
static inline int64_t jp_get_int(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_INT: return v.valor.inteiro;
        case JP_TIPO_DOUBLE: return (int64_t)v.valor.decimal;
        case JP_TIPO_BOOL: return v.valor.booleano;
        default: return 0;
    }
}

static inline double jp_get_double(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_DOUBLE: return v.valor.decimal;
        case JP_TIPO_INT: return (double)v.valor.inteiro;
        default: return 0.0;
    }
}

static inline const char* jp_get_string(JPValor v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto) {
        return v.valor.texto;
    }
    return "";
}

static inline int jp_get_bool(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_BOOL: return v.valor.booleano;
        case JP_TIPO_INT: return v.valor.inteiro != 0;
        default: return 0;
    }
}
```

---

## Código Específico de Plataforma

Quando precisar de código diferente para cada plataforma:

```cpp
#if JP_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif

JP_EXPORT JPValor jp_esperar(JPValor* args, int numArgs) {
    int ms = (int)jp_get_int(args[0]);
    
    #if JP_WINDOWS
        Sleep(ms);
    #else
        usleep(ms * 1000);
    #endif
    
    return jp_nulo();
}
```

### Funções de Tempo Thread-Safe

```cpp
static std::tm pegar_tempo_local() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm time_info;
    
    #if JP_WINDOWS
        localtime_s(&time_info, &now_c);  // Windows: localtime_s
    #else
        localtime_r(&now_c, &time_info);  // Linux: localtime_r
    #endif
    
    return time_info;
}
```

---

## Exemplo Completo Multiplataforma

### stop.cpp

```cpp
// stop.cpp
// Biblioteca de espera/sleep para JPLang - Multiplataforma
//
// Compilar:
//   Windows: g++ -shared -o bibliotecas/stop/stop.jpd bibliotecas/stop/stop.cpp -O3
//   Linux:   g++ -shared -fPIC -o bibliotecas/stop/libstop.jpd bibliotecas/stop/stop.cpp -O3

#include <cstdint>
#include <cstring>

// === DETECÇÃO DE PLATAFORMA ===
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
    #include <windows.h>
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
    #include <unistd.h>
#endif

// === TIPOS JPLANG ===
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4
} JPTipo;

typedef struct {
    int tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* ponteiro;
    } valor;
} JPValor;

// === HELPERS ===
static JPValor jp_nulo() {
    JPValor v;
    v.tipo = JP_TIPO_NULO;
    v.valor.inteiro = 0;
    return v;
}

static int jp_get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return (int)args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int)args[idx].valor.decimal;
    return 0;
}

// === FUNÇÕES EXPORTADAS ===

// Espera por N milissegundos
JP_EXPORT JPValor jp_stop_esperar(JPValor* args, int numArgs) {
    if (numArgs < 1) return jp_nulo();
    
    int ms = jp_get_int(args, 0);
    
    if (ms > 0) {
        #if JP_WINDOWS
            Sleep(ms);
        #else
            usleep(ms * 1000);
        #endif
    }
    
    return jp_nulo();
}
```

### stop.jp

```jplang
# stop.jp
# Biblioteca de espera/sleep para JPLang

# Importa a função nativa da DLL
# Sintaxe: nativo "caminho.jpd" importar funcao(numArgs)
nativo "bibliotecas/stop/stop.jpd" importar stop_esperar(1)

# Classe wrapper para acesso simplificado
classe stop:
    # Espera por N milissegundos
    # Uso: stop.esperar(1000)  # espera 1 segundo
    funcao esperar(ms):
        retorna stop_esperar(ms)
```

### Uso em JPLang

```jplang
importar stop

saida("Esperando 2 segundos...")
stop.esperar(2000)
saida("Pronto!")
```

---

## Exemplo em C Puro

### matematica.c

```c
// matematica.c
// Biblioteca de funções matemáticas para JPLang
//
// Compilar:
//   Windows: gcc -shared -o bibliotecas/matematica/matematica.jpd bibliotecas/matematica/matematica.c -lm -O3
//   Linux:   gcc -shared -fPIC -o bibliotecas/matematica/libmatematica.jpd bibliotecas/matematica/matematica.c -lm -O3

#include <stdint.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT __declspec(dllexport)
#else
    #define JP_EXPORT __attribute__((visibility("default")))
#endif

// === TIPOS JPLANG ===
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2
} JPTipo;

typedef struct {
    int tipo;
    union {
        int64_t inteiro;
        double decimal;
    } valor;
} JPValor;

// Helper para extrair double
static double get_num(JPValor v) {
    if (v.tipo == JP_TIPO_DOUBLE) return v.valor.decimal;
    if (v.tipo == JP_TIPO_INT) return (double)v.valor.inteiro;
    return 0.0;
}

// Helper para retornar double
static JPValor ret_double(double d) {
    JPValor v;
    v.tipo = JP_TIPO_DOUBLE;
    v.valor.decimal = d;
    return v;
}

// === FUNÇÕES EXPORTADAS ===

JP_EXPORT JPValor jp_mat_raiz(JPValor* args, int n) {
    if (n < 1) return ret_double(0);
    return ret_double(sqrt(get_num(args[0])));
}

JP_EXPORT JPValor jp_mat_potencia(JPValor* args, int n) {
    if (n < 2) return ret_double(0);
    return ret_double(pow(get_num(args[0]), get_num(args[1])));
}

JP_EXPORT JPValor jp_mat_seno(JPValor* args, int n) {
    if (n < 1) return ret_double(0);
    return ret_double(sin(get_num(args[0])));
}

JP_EXPORT JPValor jp_mat_cosseno(JPValor* args, int n) {
    if (n < 1) return ret_double(0);
    return ret_double(cos(get_num(args[0])));
}

JP_EXPORT JPValor jp_mat_absoluto(JPValor* args, int n) {
    if (n < 1) return ret_double(0);
    return ret_double(fabs(get_num(args[0])));
}
```

---

## Exemplo em Rust

### stop.rs

```rust
// stop.rs
// Biblioteca de espera/sleep para JPLang
//
// Compilar:
//   Windows: rustc --crate-type cdylib -o bibliotecas/stop/stop.jpd bibliotecas/stop/stop.rs
//   Linux:   rustc --crate-type cdylib -o bibliotecas/stop/libstop.jpd bibliotecas/stop/stop.rs

use std::ffi::c_void;
use std::thread;
use std::time::Duration;

// === TIPOS JPLANG ===
#[repr(C)]
#[derive(Copy, Clone)]
pub union JPValorUnion {
    pub inteiro: i64,
    pub decimal: f64,
    pub texto: *mut i8,
    pub booleano: i32,
    pub ponteiro: *mut c_void,
}

#[repr(C)]
pub struct JPValor {
    pub tipo: i32,
    pub valor: JPValorUnion,
}

const JP_TIPO_NULO: i32 = 0;
const JP_TIPO_INT: i32 = 1;
const JP_TIPO_DOUBLE: i32 = 2;

// === FUNÇÃO EXPORTADA ===
#[no_mangle]
pub extern "C" fn jp_stop_esperar(args: *const JPValor, num_args: i32) -> JPValor {
    let mut ms: u64 = 0;
    
    if !args.is_null() && num_args > 0 {
        unsafe {
            let arg0 = &*args;
            match arg0.tipo {
                JP_TIPO_INT => {
                    if arg0.valor.inteiro > 0 {
                        ms = arg0.valor.inteiro as u64;
                    }
                }
                JP_TIPO_DOUBLE => {
                    if arg0.valor.decimal > 0.0 {
                        ms = arg0.valor.decimal as u64;
                    }
                }
                _ => {}
            }
        }
    }
    
    if ms > 0 {
        thread::sleep(Duration::from_millis(ms));
    }
    
    // Retorna nulo
    JPValor {
        tipo: JP_TIPO_NULO,
        valor: JPValorUnion { inteiro: 0 },
    }
}
```

---

## Arquivo Interface JPLang (.jp)

O arquivo `.jp` é a interface entre JPLang e a DLL. Ele contém:

1. **Declaração `nativo`** com as funções importadas
2. **Classe wrapper** para acesso simplificado (opcional, mas recomendado)

### Sintaxe do `nativo`

```jplang
nativo "caminho/para/biblioteca.jpd" importar funcao1(numArgs1), funcao2(numArgs2)
```

- **caminho**: Caminho relativo para o arquivo `.jpd`
- **funcao(N)**: Nome da função (sem prefixo `jp_`) e quantidade de argumentos

**Nota**: O caminho no arquivo `.jp` sempre usa `nome.jpd`. O JPLang automaticamente procura `libnome.jpd` no Linux.

---

## Checklist para Criar uma Biblioteca

- [ ] Usar macro `JP_EXPORT` para portabilidade
- [ ] Definir tipos `JPTipo` e `JPValor`
- [ ] Criar funções com prefixo `jp_` e assinatura `JPValor func(JPValor* args, int numArgs)`
- [ ] Usar `#if JP_WINDOWS` para código específico de plataforma
- [ ] Compilar da **raiz do projeto** com caminhos relativos
- [ ] No Windows: gerar `nome.jpd`
- [ ] No Linux: gerar `libnome.jpd`
- [ ] Criar arquivo `.jp` com `nativo` e classe wrapper
- [ ] Testar em ambas plataformas

---

## Tabela de Compilação por Linguagem

| Linguagem | Windows | Linux |
|-----------|---------|-------|
| C++ (g++) | `g++ -shared -o bibliotecas/nome/nome.jpd bibliotecas/nome/nome.cpp -O3` | `g++ -shared -fPIC -o bibliotecas/nome/libnome.jpd bibliotecas/nome/nome.cpp -O3` |
| C (gcc) | `gcc -shared -o bibliotecas/nome/nome.jpd bibliotecas/nome/nome.c -O3` | `gcc -shared -fPIC -o bibliotecas/nome/libnome.jpd bibliotecas/nome/nome.c -O3` |
| Rust | `rustc --crate-type cdylib -o bibliotecas/nome/nome.jpd bibliotecas/nome/nome.rs` | `rustc --crate-type cdylib -o bibliotecas/nome/libnome.jpd bibliotecas/nome/nome.rs` |

---

## Erros Comuns

### Erro: "invalid ELF header" (Linux)

**Causa**: Tentando carregar uma DLL Windows (.jpd compilada no Windows) no Linux

**Solução**: Compile a versão Linux com prefixo `lib`:
```bash
g++ -shared -fPIC -o bibliotecas/nome/libnome.jpd bibliotecas/nome/nome.cpp -O3
```

### Erro: "Funcao nao encontrada: jp_xxx"

**Causa**: A função não foi exportada com prefixo `jp_` ou falta `extern "C"`

**Solução**: Verifique se está usando `JP_EXPORT` corretamente:
```cpp
JP_EXPORT JPValor jp_minha_funcao(JPValor* args, int n) { ... }
```

### Erro: "Falha ao carregar biblioteca"

**Causa**: A biblioteca não foi encontrada no caminho esperado

**Solução**: Verifique se o arquivo está em:
- Windows: `bibliotecas/nome/nome.jpd`
- Linux: `bibliotecas/nome/libnome.jpd`

### Erro: Crash ao chamar função

**Causa**: Assinatura incorreta ou tipos incompatíveis

**Solução**: Verifique:
1. Está usando `JP_EXPORT` (que inclui `extern "C"`)
2. A assinatura é exatamente `JPValor func(JPValor* args, int numArgs)`
3. Os tipos `JPValor` e `JPTipo` estão definidos corretamente

---

## Resumo

1. **Uma única interface**: Todas as funções usam `JPValor jp_func(JPValor* args, int numArgs)`
2. **Prefixo obrigatório**: Todas as funções exportadas devem começar com `jp_`
3. **Multiplataforma**: Use `JP_EXPORT` e compile `nome.jpd` (Windows) e `libnome.jpd` (Linux)
4. **Compile da raiz**: Sempre use caminhos relativos completos nos comandos de compilação
5. **Funciona em ambos os modos**: Interpretado e compilado usam o mesmo formato