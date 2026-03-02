Manual de Criação de Bibliotecas para a Linguagem JPLang
Objetivo – Fornecer uma documentação completa, passo‑a‑passo, para quem quer criar bibliotecas estáticas/dinâmicas (.obj ou .dll) que possam ser importadas na sua linguagem, usando a estrutura de descrição em JSON que você já utiliza.

O manual inclui também um guia prático de como usar uma IA (ex.: ChatGPT, GPT‑4) para gerar código‑fonte de forma automática.

Índice
Seção	Tema
1	Visão geral da arquitetura de bibliotecas
2	Estrutura de um módulo JPLang
3	Especificação da interface: tipos, parâmetros e retorno
4	Arquivo de fonte: C/C++ puro + extern "C"
5	Arquivo de descrição JSON
6	Compilação & linkagem
7	Carregamento e uso em JPLang
8	Exemplo completo – “tempo”
9	Usando IA para gerar bibliotecas
10	Boas práticas, erros comuns e depuração
11	FAQ
1. Visão geral da arquitetura de bibliotecas
Conceito	O que é	Como é usado no JPLang
Obj	Arquivo objeto (.obj) contendo código compilado	O compilador do JPLang liga o objeto ao seu código de runtime.
DLL	Biblioteca dinâmica (.dll)	Opcional – útil se a biblioteca precisar ser atualizada sem recompilar o runtime.
JSON	Descrição estática da API	O parser de JPLang lê o arquivo para gerar wrappers em tempo de execução.
extern "C"	Evita name‑mangling	Garantia de que a função apareça com o nome exato no arquivo objeto.
Calling convention	Stdcall (__stdcall)	Compatível com Win32 API e a maioria das linguagens que interagem com DLLs.
Nota: Se você quiser usar tipos de retorno int64_t ou ponteiros, certifique-se de que a ABI do JPLang suporte essa convenção.

2. Estrutura de um módulo JPLang
biblioteca/
├── nome_lib.cpp          ← Código‑fonte (C/C++)
├── nome_lib.json         ← Descrição da API
└── Makefile / CMakeLists ← Build (opcional)
2.1 Arquivo de fonte
Deve conter extern "C" em torno das funções exportadas.
Se usar C++, garanta que não haja mangling de nomes.
Prefira __stdcall ou a convenção padrão do JPLang (normalmente __cdecl – consulte a documentação).
Se precisar de recursos do Windows, inclua <windows.h> (ou outro header específico).
2.2 Arquivo JSON
O JSON descreve quais funções estão disponíveis, seus tipos de retorno e parâmetros.
Formato típico:

{
  "funcoes": [
    { "nome": "minha_funcao", "retorno": "inteiro", "params": ["texto", "inteiro"] },
    { "nome": "outra",       "retorno": "texto",   "params": [] }
  ]
}
nome → nome literal usado no JPLang (my_func).
retorno → "inteiro", "texto", "nulo", "vetor", etc.
params → array na mesma ordem que aparecem no C/C++.
Tip: Se a linguagem possui tipos customizados (ex.: datetime), inclua um mapeamento em um arquivo separado ou use extensões do JSON.

3. Especificação da interface
Tipo	Representação em C/C++	Representação em JSON	Observações
inteiro	int64_t (ou long long)	"inteiro"	Use int64_t para garantir compatibilidade 64‑bit.
texto	const char* (UTF‑8)	"texto"	A biblioteca deve converter para UTF‑16 quando necessário.
ponteiro	void*	"nulo"	Use apenas quando a API de JPLang for capaz de manipular ponteiros.
vetor	int* + tamanho	"vetor"	Defina um par de funções: size e data.
Boa prática: todas as funções que retornam ponteiros devem ser acompanhadas de uma função de liberação (ex.: free_<func_)._

4. Arquivo de fonte – exemplos
4.1 Biblioteca “tempo”
Já foi apresentado. Ele demonstra:

uso de extern "C*
exportação de funções de retorno int64_t e const char*
chamadas diretas de API do Windows
4.2 Biblioteca “janela” (exemplo completo)
// janela.cpp
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

extern "C" {

//--- Dados estáticos -----------------
static HINSTANCE hInst = nullptr;
static HWND hWnd = nullptr;
static HWND hBtn = nullptr;
static const int BUTTON_ID = 101;

//--- Função de criação ----------------
int64_t jn_criar(const char* titulo, int larg, int alt) {
    if (!hInst) hInst = GetModuleHandle(nullptr);
    if (!hWnd) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
            switch (m) {
                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
                case WM_COMMAND:
                    if (HIWORD(w)==BN_CLICKED && LOWORD(w)==BUTTON_ID)
                        buttonClicked = true;
                    return 0;
            }
            return DefWindowProcW(h, m, w, l);
        };
        wc.hInstance = hInst;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName = L"JN_WINDOW_CLASS";
        if (!RegisterClassExW(&wc)) return -1;
    }
    std::wstring wTitle = utf8_to_wide(titulo);
    hWnd = CreateWindowExW(0, L"JN_WINDOW_CLASS", wTitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        larg, alt, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return -1;
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    return 0;
}

//--- Exibir --------------------------
int64_t jn_exibir() {
    if (!hWnd) return -1;
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return 1;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

//--- Botão ----------------------------
int64_t jn_botao(const char* texto, int x, int y) {
    if (!hWnd) return -1;
    std::wstring wTxt = utf8_to_wide(texto);
    hBtn = CreateWindowExW(WS_EX_CLIENTEDGE, L"BUTTON", wTxt.c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, 100, 30, hWnd, (HMENU)(DWORD_PTR)BUTTON_ID,
        hInst, nullptr);
    return hBtn ? 0 : -1;
}

//--- Botão clicado --------------------
int64_t jn_botao_clicado() {
    if (buttonClicked) { buttonClicked = false; return 1; }
    return 0;
}

} // extern "C"
Observação: utf8_to_wide é a mesma função usada em tempo.cpp.

5. Compilação & linkagem
5.1 MinGW (Windows)
# 1. Compila o arquivo fonte → objeto
g++ -c -O3 -DUNICODE -D_UNICODE \
    -o biblioteca/obj/janela.obj \
    biblioteca/janela.cpp

# 2. (Opcional) Cria um .dll
g++ -shared -O3 -DUNICODE -D_UNICODE \
    -o biblioteca/bin/janela.dll \
    biblioteca/obj/janela.obj \
    -luser32 -lkernel32
Se você usar Visual Studio, crie um projeto “Static Library” ou “Dynamic Library” e adicione janela.cpp.

5.2 CMake (portável)
cmake_minimum_required(VERSION 3.16)
project(janela LANGUAGES CXX)

add_library(janela SHARED biblioteca/janela.cpp)  # ou STATIC
target_compile_definitions(janela PRIVATE UNICODE _UNICODE)
target_link_libraries(janela PRIVATE user32 kernel32)
Dica: Adicione -fno-exceptions se seu runtime JPLang não suporta exceções C++.

6. Carregamento e uso em JPLang
Coloque o arquivo .obj (ou .dll) e o .json na mesma pasta onde o JPLang executa.
No seu código JPLang, faça:
importar janela          // carrega biblioteca e lê janela.json

jn_criar("título", 600, 600)
jn_botao("clique aqui", 270, 270)

enquanto verdadeiro:
    jn_exibir()
    se jn_botao_clicado():
        saida("botão foi clicado")
Obs: Se a biblioteca for uma DLL, o runtime deve ser configurado para usar LoadLibrary + GetProcAddress. Se for .obj, o JPLang liga direto no momento da compilação do runtime.

7. Exemplo completo – “tempo”
O exemplo já aparece em sua pergunta. Apenas recapitulando:

// tempo.cpp
#include <ctime>
extern "C" {
    typedef long long time64_t;
    time64_t _time64(time64_t*);
    struct tm* _localtime64(const time64_t*);
    void __stdcall Sleep(unsigned long);

    int64_t tm_dia()          { ... }
    int64_t tm_timestamp()     { ... }
    const char* tm_completo()  { ... }
    int64_t tm_sleep(int64_t ms) { Sleep((unsigned long)ms); return 0; }
}
JSON:

{
  "funcoes": [
    { "nome": "tm_dia", "retorno": "inteiro", "params": [] },
    ...
    { "nome": "tm_sleep", "retorno": "inteiro", "params": ["inteiro"] }
  ]
}
