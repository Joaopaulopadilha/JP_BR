# Mapa do Projeto

Estrutura completa do projeto gerada automaticamente.

**Diretório:** `C:\Users\jpp\Desktop\jp_git_uni\core_2026\src`
**Data de geração:** 2026-02-08 20:08:00
**Total de itens:** 47

## Estrutura

```
├── 📁 ast_src
│   ├── 📄 ast_builtins.hpp - Nós de funções built-in do AST (entrada, inteiro, decimal, texto)
│   ├── 📄 ast_classes.hpp - Nós de classes do AST (declaração, métodos, atributos, auto)
│   ├── 📄 ast_condicionais.hpp - Nós de condicionais do AST (se, senao, ou_se)
│   ├── 📄 ast_expressoes.hpp - Nós de expressões do AST (literais, variáveis, operações binárias)
│   ├── 📄 ast_funcoes.hpp - Nós de funções do AST (declaração, chamada, retorno)
│   ├── 📄 ast_importar.hpp - Nós de importação do AST (importar módulos e arquivos .jp)
│   ├── 📄 ast_listas.hpp - Nós AST para operações com listas
│   ├── 📄 ast_nativos.hpp - Nós de módulos nativos do AST (importação e chamada de funções nativas)
│   ├── 📄 ast_repeticoes.hpp - Nós de repetição do AST (loop, enquanto, repetir, para, parar, continuar)
│   └── 📄 ast_statements.hpp - Nós de comandos básicos do AST (atribuição, saída, bloco, expressão como statement)
├── 📁 jpc_src
│   ├── 📄 jpc_gerador.hpp - Geração de código C para cada OpCode
│   └── 📄 jpc_runtime.hpp - Código C do runtime gerado pelo compilador JPLang
├── 📁 lexer_src
│   ├── 📄 lexer_classes.hpp - Módulo do lexer para reconhecimento de palavras-chave de classes (classe, auto)
│   ├── 📄 lexer_condicionais.hpp - Módulo do lexer para reconhecimento de palavras-chave de condicionais (se, senao, ou_se, e, ou)
│   ├── 📄 lexer_expressoes.hpp - Módulo do lexer para leitura de strings e números
│   ├── 📄 lexer_funcoes.hpp - Módulo do lexer para reconhecimento de palavras-chave de funções (funcao, retorna)
│   ├── 📄 lexer_importar.hpp - Módulo do lexer para reconhecimento de palavras-chave de importação
│   ├── 📄 lexer_nativos.hpp - Módulo do lexer para reconhecimento de palavras-chave de módulos nativos
│   ├── 📄 lexer_repeticoes.hpp - Módulo do lexer para reconhecimento de palavras-chave de repetição (loop, enquanto, repetir, para, parar, continuar)
│   ├── 📄 lexer_saida.hpp - Módulo do lexer para verificação de palavras-chave gerais (verdadeiro, falso) e cadeia de verificação
│   └── 📄 lexer_variaveis.hpp - Módulo do lexer para leitura de identificadores e palavras reservadas
├── 📁 parser_src
│   ├── 📄 parser_classes.hpp - Módulo do parser para análise de classes (declaração, métodos, atributos)
│   ├── 📄 parser_condicionais.hpp - define PARSER_SRC_PARSER_CONDICIONAIS_HPP
│   ├── 📄 parser_expressoes.hpp - Módulo do parser para análise de expressões (literais, variáveis, operações, chamadas de função)
│   ├── 📄 parser_funcoes.hpp - Módulo do parser para análise de funções (declaração, chamada, retorno)
│   ├── 📄 parser_importar.hpp - Módulo do parser para análise de importações
│   ├── 📄 parser_nativos.hpp - Módulo do parser para análise de importações nativas
│   ├── 📄 parser_repeticoes.hpp - Módulo do parser para análise de estruturas de repetição (loop, enquanto, repetir, para, parar, continuar)
│   ├── 📄 parser_saida.hpp - define PARSER_SRC_PARSER_SAIDA_HPP
│   └── 📄 parser_variaveis.hpp - define PARSER_SRC_PARSER_VARIAVEIS_HPP
├── 📄 ast.hpp - Definições dos nós da Árvore Sintática Abstrata (AST) da JPLang
├── 📄 import_processor.hpp - Processador de importações - carrega e processa arquivos .jp
├── 📄 jpc_compilador.hpp - Compilador JPLang -> C -> Executável
├── 📄 jpruntime.h - Header C puro para runtime JPLang com TCC
├── 📄 jpvm.hpp - Máquina Virtual da JPLang - execução de bytecode
├── 📄 jpvm_nativos.hpp - Sistema de carregamento e execução de funções nativas (DLL/SO)
├── 📄 lexer.hpp - Lexer principal - tokenização do código fonte JPLang
├── 📄 main.cpp - Ponto de entrada da JPLang
├── 📄 opcodes.hpp - Definições centrais de tokens, valores e instruções da VM
└── 📄 parser.hpp - Parser principal - análise sintática e construção da AST da JPLang
```

---
*Mapa gerado automaticamente pelo discovery.cpp*
