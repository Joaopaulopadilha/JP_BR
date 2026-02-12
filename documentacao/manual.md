# JPLang - Manual da Linguagem

**JPLang** é uma linguagem de programação em português brasileiro, projetada para ser simples e acessível.

---

## Índice

1. [Saída de Dados](#1-saída-de-dados)
2. [Variáveis e Tipos](#2-variáveis-e-tipos)
3. [Entrada de Dados](#3-entrada-de-dados)
4. [Condicionais](#4-condicionais)
5. [Laços de Repetição](#5-laços-de-repetição)
6. [Funções](#6-funções)
7. [Classes](#7-classes)
8. [Importação de Bibliotecas](#8-importação-de-bibliotecas)
9. [Bibliotecas Disponíveis](#9-bibliotecas-disponíveis)

---

## 1. Saída de Dados

### Com quebra de linha automática

```jplang
saida("Olá, mundo!")
saida_amarelo("Texto amarelo")
saida_azul("Texto azul")
saida_vermelho("Texto vermelho")
saida_verde("Texto verde")
```

### Sem quebra de linha

```jplang
saidal("Texto na mesma linha, ")
saidal_amarelo("amarelo, ")
saidal_azul("azul, ")
saidal_vermelho("vermelho, ")
saidal_verde("verde")
```

### Interpolação de variáveis

```jplang
nome = "João"
idade = 25
saida("Olá {nome}, você tem {idade} anos!")
```

---

## 2. Variáveis e Tipos

### Tipagem automática

O JPLang detecta o tipo automaticamente:

```jplang
nome = "João"           # texto
idade = 20              # inteiro
altura = 1.88           # decimal
ativo = verdadeiro      # booleano
```

### Tipagem explícita

Você também pode declarar o tipo:

```jplang
texto produto = "feijão"
int tamanho = 20
dec valor = 1.30
bool aprovado = falso
```

### Tipos disponíveis

| Tipo | Descrição | Exemplo |
|------|-----------|---------|
| `texto` | Cadeia de caracteres | `"Olá"` |
| `int` | Número inteiro | `42` |
| `dec` | Número decimal | `3.14` |
| `bool` | Booleano | `verdadeiro` / `falso` |

---

## 3. Entrada de Dados

### Leitura do teclado

```jplang
nome = entrada("Digite seu nome: ")
saida("Olá, {nome}!")
```

### Conversão de tipos

```jplang
texto_idade = entrada("Digite sua idade: ")
idade = int(texto_idade)
saida("No próximo ano você terá {idade + 1} anos")
```

---

## 4. Condicionais

### Estrutura básica

```jplang
se condição:
    # código se verdadeiro
senao:
    # código se falso
```

### Exemplo com se/senao

```jplang
idade = 17

se idade >= 18:
    saida("Maior de idade")
    saida("Pode dirigir")
senao:
    saida("Menor de idade")
    saida_vermelho("Não pode dirigir")
```

### Múltiplas condições com ou_se

```jplang
x = 20
y = 20

se x > y:
    saida("x é maior que y")
ou_se x < y:
    saida("x é menor que y")
senao:
    saida_verde("x é igual a y")
```

### Comparação de textos

```jplang
senha = "joao"

se senha == "joao":
    saida("Acesso liberado")
senao:
    saida("Não permitido")
```

### Comparação de booleanos

```jplang
ativo = verdadeiro

se ativo == verdadeiro:
    saida_amarelo("Sistema ativo")
senao:
    saida("Sistema inativo")
```

---

## 5. Laços de Repetição

### enquanto

Repete enquanto a condição for verdadeira:

```jplang
x = 0
enquanto x < 10:
    saida("Valor de x: {x}")
    x = x + 1
```

### repetir

Repete um número fixo de vezes:

```jplang
t = 0
repetir(5):
    saida_azul("Iteração {t}")
    t = t + 1
```

### para (intervalo)

Itera sobre um intervalo de valores:

```jplang
# intervalo(inicio, fim, passo)
para i em intervalo(0, 10, 2):
    saida("Valor: {i}")
```

**Saída:** 0, 2, 4, 6, 8

### Controle de fluxo

```jplang
# parar - sai do laço
enquanto verdadeiro:
    saida("Executando...")
    parar

# continuar - pula para próxima iteração
para i em intervalo(0, 10):
    se i == 5:
        continuar
    saida(i)
```

---

## 6. Funções

### Função simples

```jplang
funcao bom_dia():
    saida("Bom dia!")

bom_dia()
```

### Função com parâmetros

```jplang
funcao saudar(nome):
    saida("Olá {nome}!")

saudar("João")
```

### Função com retorno

```jplang
funcao somar(a, b):
    res = a + b
    retorna res

x = somar(10, 20)
saida("Total: {x}")
```

### Exemplo: Cálculo de área

```jplang
funcao calcular_area(largura, altura):
    area = largura * altura
    saida("Calculando área...")
    retorna area

resultado = calcular_area(3, 4)
saida("Área = {resultado}")
```

### Exemplo: Fatorial

```jplang
funcao fatorial(n):
    r = 1    
    para i em intervalo(1, n + 1):
        r = r * i
    retorna r

f = fatorial(5)
saida("Fatorial de 5 = {f}")
```

---

## 7. Classes

### Definição de classe

```jplang
classe Carro:
    funcao criar(marca, modelo):
        auto.marca = marca
        auto.modelo = modelo
        retorna auto
    
    funcao buzinar():
        saida_verde("Beep beep! Sou um {auto.marca} {auto.modelo}")
    
    funcao info():
        saida("Carro: {auto.marca} {auto.modelo}")
```

### Uso da classe

```jplang
meu_carro = Carro.criar("Toyota", "Corolla")
meu_carro.info()
meu_carro.buzinar()
```

**Saída:**
```
Carro: Toyota Corolla
Beep beep! Sou um Toyota Corolla
```

### A palavra-chave `auto`

Dentro de uma classe, `auto` se refere à instância atual (equivalente ao `self` em Python ou `this` em outras linguagens).

---

## 8. Importação de Bibliotecas

### Sintaxe

```jplang
importar nome_da_biblioteca
```

### Exemplo

```jplang
importar tempo
importar teclado

tempo.dormir(1000)  # pausa por 1 segundo
teclado.pressionar("enter")
```

---

## 9. Bibliotecas Disponíveis

### tempo

Funções relacionadas a tempo e datas.

```jplang
importar tempo

tempo.dormir(1000)              # pausa em milissegundos
hora = tempo.hora()             # hora atual (0-23)
minuto = tempo.minuto()         # minuto atual (0-59)
segundo = tempo.segundo()       # segundo atual (0-59)
dia = tempo.dia()               # dia do mês (1-31)
mes = tempo.mes()               # mês atual (1-12)
ano = tempo.ano()               # ano atual
semana = tempo.dia_semana()     # dia da semana (0=dom, 6=sab)
data = tempo.data_formatada()   # "DD/MM/AAAA"
horario = tempo.hora_formatada() # "HH:MM:SS"
```

### teclado

Simulação e captura de teclas.

```jplang
importar teclado

# Pressionar uma tecla
teclado.pressionar("enter")
teclado.pressionar("a")

# Digitar uma frase
teclado.pressionar("Olá mundo")

# Digitar com velocidade (ms entre teclas)
teclado.digitar("Texto digitado devagar", 80)

# Escutar teclas pressionadas
tecla = teclado.escutar()
se tecla != "":
    saida("Tecla: {tecla}")
```

### os

Funções do sistema operacional.

```jplang
importar os

os.limpar_terminal()            # limpa a tela
os.dormir(1000)                 # pausa em ms
os.executar("dir")              # executa comando do sistema
os.sair(0)                      # encerra o programa

# Variáveis de ambiente
valor = os.getenv("PATH")
os.setenv("MINHA_VAR", "valor")

# Sistema de arquivos
dir = os.diretorio_atual()
os.mudar_diretorio("C:/pasta")
existe = os.existe("arquivo.txt")
eh_pasta = os.eh_diretorio("pasta")
os.criar_diretorio("nova_pasta")
os.remover_arquivo("arquivo.txt")
os.remover_diretorio("pasta")
os.renomear("antigo.txt", "novo.txt")
os.copiar_arquivo("origem.txt", "destino.txt")

# Informações do sistema
usuario = os.usuario()
pc = os.computador()
ts = os.timestamp()
tick = os.tick()
os.beep(440, 500)               # frequência, duração
```

### imgui

Interface gráfica com Dear ImGui.

```jplang
importar imgui

# Criar janela
janela = imgui.janela("Minha App", 800, 600)
imgui.fundo(janela, 240, 240, 240)  # cor de fundo RGB

# Criar botão
btn = imgui.botao(janela, "Clique", 100, 50, 200, 40)
imgui.botao_cor_fundo(btn, 0, 120, 215)
imgui.botao_cor_fonte(btn, 255, 255, 255)

# Criar etiqueta (texto)
lbl = imgui.etiqueta(janela, "Olá!", 100, 100)
imgui.etiqueta_cor(lbl, 0, 0, 0)
imgui.etiqueta_texto(lbl, "Novo texto")

# Criar campo de entrada
campo = imgui.input(janela, "Digite aqui", 100, 150, 200, 30)
imgui.input_cor_fundo(campo, 255, 255, 255)
imgui.input_cor_fonte(campo, 0, 0, 0)
valor = imgui.input_valor(campo)
imgui.input_definir(campo, "texto inicial")

# Loop principal
enquanto imgui.ativa(janela):
    se imgui.clicado(btn):
        saida("Botão clicado!")
    
    tempo.dormir(10)

imgui.fechar(janela)
```

---

## Comentários

```jplang
# Comentário de linha única

// Também funciona assim

# Comentários ajudam a documentar o código
x = 10  # pode ser no final da linha
```

---

## Operadores

### Aritméticos

| Operador | Descrição |
|----------|-----------|
| `+` | Adição |
| `-` | Subtração |
| `*` | Multiplicação |
| `/` | Divisão |

### Comparação

| Operador | Descrição |
|----------|-----------|
| `==` | Igual |
| `!=` | Diferente |
| `>` | Maior que |
| `<` | Menor que |
| `>=` | Maior ou igual |
| `<=` | Menor ou igual |

---

## Exemplo Completo: Calculadora

```jplang
importar imgui
importar tempo

# Criar janela
janela = imgui.janela("Calculadora", 300, 400)
imgui.fundo(janela, 45, 45, 45)

# Display
display = imgui.etiqueta(janela, "0", 20, 20)
imgui.etiqueta_cor(display, 255, 255, 255)

# Variável para armazenar o valor
valor_atual = ""

# Botões numéricos
btn_1 = imgui.botao(janela, "1", 20, 80, 60, 50)
btn_2 = imgui.botao(janela, "2", 90, 80, 60, 50)
btn_3 = imgui.botao(janela, "3", 160, 80, 60, 50)

btn_4 = imgui.botao(janela, "4", 20, 140, 60, 50)
btn_5 = imgui.botao(janela, "5", 90, 140, 60, 50)
btn_6 = imgui.botao(janela, "6", 160, 140, 60, 50)

btn_7 = imgui.botao(janela, "7", 20, 200, 60, 50)
btn_8 = imgui.botao(janela, "8", 90, 200, 60, 50)
btn_9 = imgui.botao(janela, "9", 160, 200, 60, 50)

btn_0 = imgui.botao(janela, "0", 90, 260, 60, 50)
btn_limpar = imgui.botao(janela, "C", 20, 260, 60, 50)

# Loop principal
enquanto imgui.ativa(janela):
    se imgui.clicado(btn_1):
        valor_atual = valor_atual + "1"
        imgui.etiqueta_texto(display, valor_atual)
    
    se imgui.clicado(btn_2):
        valor_atual = valor_atual + "2"
        imgui.etiqueta_texto(display, valor_atual)
    
    # ... outros botões ...
    
    se imgui.clicado(btn_limpar):
        valor_atual = ""
        imgui.etiqueta_texto(display, "0")
    
    tempo.dormir(10)

imgui.fechar(janela)
```

---

## Compilação

### Executar (interpretador)

```bash
jp programa.jp
```

### Compilar para executável

```bash
jp build programa.jp
```

### Compilar sem console (modo janela)

```bash
jp build -w programa.jp
```

---

**JPLang** - Programação em Português 🇧🇷