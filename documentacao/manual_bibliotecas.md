# Manual de Criação de Bibliotecas JPLang

## O que é uma biblioteca JPLang?

Uma biblioteca JPLang é um arquivo `.cpp` compilado como DLL (`.jpd`) que exporta funções em C puro. Qualquer função exportada fica automaticamente disponível no código JPLang.

---

## Estrutura obrigatória

Toda biblioteca JPLang **deve** seguir esta estrutura exata. Copie este template e adicione suas funções:

```cpp
// nome_da_biblioteca.cpp
// Descrição da biblioteca
//
// Compilar:
//   Windows: g++ -shared -static -o bibliotecas/nome_da_biblioteca/nome_da_biblioteca.jpd bibliotecas/nome_da_biblioteca/nome_da_biblioteca.cpp -O3
//   Linux:   g++ -shared -fPIC -o bibliotecas/nome_da_biblioteca/libnome_da_biblioteca.jpd bibliotecas/nome_da_biblioteca/nome_da_biblioteca.cpp -O3

#include <cstdint>
#include <cstring>
#include <cstdlib>

// === EXPORT ===
#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// === TIPOS (não alterar) ===
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
    } valor;
} JPValor;

// === HELPERS (copie os que precisar) ===

// Criar retorno inteiro
static JPValor jp_int(int64_t v) {
    JPValor r;
    r.tipo = JP_TIPO_INT;
    r.valor.inteiro = v;
    return r;
}

// Criar retorno decimal
static JPValor jp_double(double v) {
    JPValor r;
    r.tipo = JP_TIPO_DOUBLE;
    r.valor.decimal = v;
    return r;
}

// Criar retorno string
static JPValor jp_string(const char* s) {
    JPValor r;
    r.tipo = JP_TIPO_STRING;
    size_t len = strlen(s);
    r.valor.texto = (char*)malloc(len + 1);
    if (r.valor.texto) memcpy(r.valor.texto, s, len + 1);
    return r;
}

// Criar retorno booleano
static JPValor jp_bool(int v) {
    JPValor r;
    r.tipo = JP_TIPO_BOOL;
    r.valor.booleano = v;
    return r;
}

// Criar retorno nulo (para funções sem retorno útil)
static JPValor jp_nulo() {
    JPValor r;
    r.tipo = JP_TIPO_NULO;
    r.valor.inteiro = 0;
    return r;
}

// Ler argumento inteiro
static int64_t get_int(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int64_t)args[idx].valor.decimal;
    return 0;
}

// Ler argumento decimal
static double get_double(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_DOUBLE) return args[idx].valor.decimal;
    if (args[idx].tipo == JP_TIPO_INT) return (double)args[idx].valor.inteiro;
    return 0.0;
}

// Ler argumento string
static const char* get_string(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_STRING && args[idx].valor.texto) {
        return args[idx].valor.texto;
    }
    return "";
}

// Ler argumento booleano
static int get_bool(JPValor* args, int idx) {
    if (args[idx].tipo == JP_TIPO_BOOL) return args[idx].valor.booleano;
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro != 0;
    return 0;
}

// === SUAS FUNÇÕES AQUI ===
```

---

## Regras para criar funções

### Assinatura obrigatória

Toda função exportada **deve** ter esta assinatura exata:

```cpp
JP_EXPORT JPValor nome_da_funcao(JPValor* args, int numArgs) {
    // seu código aqui
    return jp_int(0); // ou jp_string(), jp_double(), jp_bool(), jp_nulo()
}
```

- `JP_EXPORT` — obrigatório, torna a função visível na DLL
- `JPValor` — tipo de retorno obrigatório (sempre retorna JPValor)
- `JPValor* args` — array com os argumentos passados pelo usuário
- `int numArgs` — quantidade de argumentos recebidos
- O nome da função no C será o mesmo nome usado no JPLang

### Lendo argumentos

Os argumentos chegam no array `args`, indexado a partir de 0:

```cpp
// funcao(42, "texto", 3.14)
int64_t primeiro = get_int(args, 0);     // 42
const char* segundo = get_string(args, 1); // "texto"
double terceiro = get_double(args, 2);    // 3.14
```

### Retornando valores

Use os helpers para retornar o tipo correto:

```cpp
return jp_int(42);              // retorna inteiro
return jp_double(3.14);         // retorna decimal
return jp_string("ola mundo");  // retorna string
return jp_bool(1);              // retorna verdadeiro
return jp_nulo();               // retorna nulo (sem valor)
```

### Strings com std::string

Se precisar manipular strings com `std::string`, adicione `#include <string>` e converta:

```cpp
#include <string>

JP_EXPORT JPValor juntar(JPValor* args, int numArgs) {
    std::string a = get_string(args, 0);
    std::string b = get_string(args, 1);
    std::string resultado = a + " " + b;
    return jp_string(resultado.c_str());
}
```

---

## Exemplos completos

### Exemplo 1: Função sem argumento

```cpp
JP_EXPORT JPValor pi(JPValor* args, int numArgs) {
    return jp_double(3.14159265358979);
}
```

Uso no JPLang: `saida(pi())`

### Exemplo 2: Função com argumentos e retorno

```cpp
JP_EXPORT JPValor somar(JPValor* args, int numArgs) {
    int64_t a = get_int(args, 0);
    int64_t b = get_int(args, 1);
    return jp_int(a + b);
}
```

Uso no JPLang: `saida(somar(10, 20))`

### Exemplo 3: Função que retorna string

```cpp
JP_EXPORT JPValor saudacao(JPValor* args, int numArgs) {
    std::string nome = get_string(args, 0);
    std::string msg = "Ola, " + nome + "!";
    return jp_string(msg.c_str());
}
```

Uso no JPLang: `saida(saudacao("Maria"))`

### Exemplo 4: Função com estado global

```cpp
static int contador = 0;

JP_EXPORT JPValor incrementar(JPValor* args, int numArgs) {
    contador++;
    return jp_int(contador);
}

JP_EXPORT JPValor resetar(JPValor* args, int numArgs) {
    contador = 0;
    return jp_nulo();
}
```

Uso no JPLang:
```
saida(incrementar())  // 1
saida(incrementar())  // 2
resetar()
saida(incrementar())  // 1
```

---

## Compilação

### Windows
```
g++ -shared -static -o bibliotecas/nome/nome.jpd bibliotecas/nome/nome.cpp -O3
```

### Linux
```
g++ -shared -fPIC -o bibliotecas/nome/libnome.jpd bibliotecas/nome/nome.cpp -O3
```

### Colocação dos arquivos

Coloque a DLL compilada na pasta `bibliotecas/nome/`:

```
bibliotecas/
  nome/
    nome.jpd        (Windows)
    libnome.jpd     (Linux)
```

---

## Uso no JPLang

```
importar nome

resultado = saudacao("Maria")
saida(resultado)
```

Se houver conflito de nomes entre bibliotecas, use importação seletiva:

```
de nome importar saudacao
```

---

## Regras importantes

1. **Toda função deve retornar `JPValor`** — mesmo que não tenha retorno útil, use `jp_nulo()`
2. **Toda função deve receber `(JPValor* args, int numArgs)`** — mesmo que não use argumentos
3. **Toda função deve ter `JP_EXPORT`** — sem isso, a função não é visível na DLL
4. **Strings retornadas devem ser alocadas com `malloc`** — o helper `jp_string()` já faz isso
5. **A biblioteca deve compilar tanto em Windows quanto Linux** — use o macro `JP_EXPORT` do template
6. **O nome da função no C++ será o nome usado no código JPLang** — use nomes descritivos
7. **Não use prefixo `jp_` nos nomes das funções** — apenas use o nome direto (ex: `somar`, não `jp_somar`)