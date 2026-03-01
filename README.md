# JP_BR

![Badge Status](https://img.shields.io/badge/status-ativo-brightgreen)
![Badge License](https://img.shields.io/badge/license-MIT-blue)
![Badge Backend](https://img.shields.io/badge/backend-C-orange)

**JP Lang** é uma linguagem de programação compilada, talvez a unica com sintaxe flexivel, podendo programar em portugues, ingles, espanhol, ou qualquer outra linguagem compativel com UTF-8. 

O objetivo é quebrar a barreira do idioma no aprendizado de lógica de programação, sem sacrificar a performance. O compilador JP gera um objeto, linkavel com ld-linker, o mesmo de gcc e mingw, gerando executáveis nativos extremamente rápidos, e standalone, com distribuição de um unico executavel, salvo dependencias obrigatorias de dll, podendo linkar diretamente com objetos gerados em c++.

## 🚀 Destaques

*   **100% em Português:** Comandos como `se`, `enquanto`, `para`, `saida`.
*   **Performance Nativa:** gera .exe e .elf.
*   **Tipagem Híbrida:** Suporta inferência de tipos assim como python.
*   **Orientação a Objetos:** Suporte simples a classes e métodos.
*   **Terminal Colorido:** Funções nativas para saída de texto colorido.

## 📦 Instalação

Baixe a versão mais recente para Windows ou Linux.


1. Baixe o executável `jp.exe`.
2. Adicione ao seu PATH (opcional).
3. Execute no terminal.

## 🛠️ Como Usar

Crie um arquivo com a extensão `.jp` (ex: `ola.jp`) e compile:

```bash
jp.exe ola.jp
