Manual da Biblioteca de Manipulação de Texto para JPLang
📚 Introdução
Esta biblioteca oferece ferramentas práticas para manipulação de texto na linguagem JPLang, com funções implementadas em C++ e expostas via ligação estática (extern "C"). Todas as operações seguem princípios de robustez, tratamento seguro de erros e consistência com o buffer de retorno interno.

📖 Índice
Visão Geral
Funções Disponíveis
Exemplo de Uso Completo
Considerações de Uso
Limitações e Restrições
🔍 Visão Geral
A biblioteca fornece operações essenciais para processamento de texto: conversão de caso, verificação de substrings, extração de partes de strings, divisão por delimitadores e manipulação de caracteres. Todas as funções retornam valores diretamente ou via buffer interno otimizado para desempenho.

🛠️ Funções Disponíveis
txt_upper
Converte texto para maiúsculo.

resultado = txt_upper(texto)
Retorno: Texto em maiúsculo

Parâmetro	Tipo	Descrição
texto	texto	String a ser convertida
Exemplo:

txt_upper("hello world")  // Retorno: "HELLO WORLD"
txt_lower
Converte texto para minúsculo.

resultado = txt_lower(texto)
Parâmetro	Tipo	Descrição
texto	texto	String a ser convertida
Exemplo:

txt_lower("HELLO WORLD")  // Retorno: "hello world"
txt_tamanho
Retorna o número de caracteres em um texto.

n = txt_tamanho(texto)
Parâmetro	Tipo	Descrição
texto	texto	String para análise
Exemplo:

txt_tamanho("jplang")  // Retorno: 6
txt_contem
Verifica se um substring está presente no texto.

bol = txt_contem(texto, busca)
Parâmetro	Tipo	Descrição
texto	texto	Texto principal
busca	texto	Substring a procurar
Retorno: 1 (verdadeiro) se encontrado, 0 caso contrário.

Exemplo:

txt_contem("programar em português", "português")  // Retorno: 1
txt_contem("programar em português", "inglês")     // Retorno: 0
txt_trim
Remove espaços, abas e novos linhas do início e fim de um texto.

resultado = txt_trim(texto)
Parâmetro	Tipo	Descrição
texto	texto	String a ser limpa
Exemplo:

txt_trim("   espaços   ")  // Retorno: "espaços"
txt_substituir
Substitui ocorrências de um substring por outro.

resultado = txt_substituir(texto, antigo, novo)
Parâmetro	Tipo	Descrição
texto	texto	Texto original
antigo	texto	Substring a substituir
novo	texto	Substring substituta
Exemplo:

txt_substituir("eu gosto de java", "java", "jplang")  // Retorno: "eu gosto de jplang"
txt_repetir
Repete uma string um número definido de vezes.

resultado = txt_repetir(texto, quantas_vezes)
Parâmetro	Tipo	Descrição
texto	texto	String a ser repetida
quantas_vezes	inteiro	Número de repetições
Exemplo:

txt_repetir("abc", 3)  // Retorno: "abcabcabc"
txt_inverter
Inverte a ordem dos caracteres em um texto.

resultado = txt_inverter(texto)
Parâmetro	Tipo	Descrição
texto	texto	Texto a ser invertido
Exemplo:

txt_inverter("jplang")  // Retorno: "gnalp"
txt_substr
Extrai uma substring de um texto.

resultado = txt_substr(texto, inicio, tamanho)
Parâmetro	Tipo	Descrição
texto	texto	Texto principal
inicio	inteiro	Posição inicial (0-based)
tamanho	inteiro	Número de caracteres a extrair
Exemplo:

txt_substr("abcdefghij", 2, 5)  // Retorno: "cdefg"
txt_comeca_com
Verifica se um texto começa com uma substring.

bol = txt_comeca_com(texto, prefixo)
Parâmetro	Tipo	Descrição
texto	texto	Texto a verificar
prefixo	texto	Prefixo esperado
Retorno: 1 (verdadeiro) ou 0 (falso).

Exemplo:

txt_comeca_com("jplang é top", "jplang")  // Retorno: 1
txt_comeca_com("jplang é top", "python")   // Retorno: 0
txt_termina_com
Verifica se um texto termina com uma substring.

bol = txt_termina_com(texto, sufixo)
Parâmetro	Tipo	Descrição
texto	texto	Texto a verificar
sufixo	texto	Sufixo esperado
Retorno: 1 (verdadeiro) ou 0 (falso).

Exemplo:

txt_termina_com("arquivo.jp", ".jp")  // Retorno: 1
txt_termina_com("arquivo.jp", ".py")   // Retorno: 0
txt_posicao
Retorna a posição de ocorrência de uma substring.

n = txt_posicao(texto, busca)
Parâmetro	Tipo	Descrição
texto	texto	Texto principal
busca	texto	Substring a localizar
Retorno: Posição (inteiro) ou -1 se não encontrada.

Exemplo:

txt_posicao("abcdefghij", "def")  // Retorno: 3
txt_posicao("abcdefghij", "xyz")   // Retorno: -1
txt_char
Retorna um caractere específico em uma posição.

c = txt_char(texto, posicao)
Parâmetro	Tipo	Descrição
texto	texto	Texto principal
posicao	inteiro	Índice do caractere (0-based)
Exemplo:

txt_char("jplang", 0)  // Retorno: "j"
txt_contar
Conta quantas vezes uma substring aparece em um texto.

n = txt_contar(texto, busca)
Parâmetro	Tipo	Descrição
texto	texto	Texto principal
busca	texto	Substring a contar
Retorno: Número de ocorrências (inteiro).

Exemplo:

txt_contar("banana", "an")  // Retorno: 2
txt_dividir
Divide um texto por delimitadores e retorna uma parte específica.

resultado = txt_dividir(texto, delimitador, indice)
Parâmetro	Tipo	Descrição
texto	texto	Texto a dividir
delimitador	texto	Caracter ou substring separadora
indice	inteiro	Índice da parte desejada (0-based)
Exemplo:

txt_dividir("um;dois;tres;quatro", ";", 2)  // Retorno: "tres"
txt_dividir_contar
Conta quantas partes um texto tem ao ser dividido por delimitador.

n = txt_dividir_contar(texto, delimitador)
Parâmetro	Tipo	Descrição
texto	texto	Texto a dividir
delimitador	texto	Delimitador usado
Retorno: Número de partes (inteiro).

Exemplo:

txt_dividir_contar("a;b;c;d;", ";")  // Retorno: 4
txt_inicial
Capitaliza a primeira letra de um texto.

resultado = txt_inicial(texto)
Parâmetro	Tipo	Descrição
texto	texto	Texto a capitalizar
Exemplo:

txt_inicial("jplang")        // Retorno: "Jplang"
txt_inicial("Java")          // Retorno: "Java" (sem alteração)
💻 Exemplo de Uso Completo
importar texto
saida_verde("=== Teste Biblioteca Texto ===")
saida("")

saidal_verde("txt_upper: ")
t = "hello world"
r = txt_upper(t)
saida(r)

# ... [outros exemplos]
⚠️ Considerações de Uso
Tratamento de erros: Todas as funções verificam se o parâmetro texto é nulo e retornam valores padrão (ex: string vazia ou 0).
Buffer interno: O retorno é gerenciado via pool de buffers de 8 posições com tamanho máximo de 4096 caracteres por buffer. Strings acima desse limite são truncadas automaticamente.
Delimitadores em texto: Em funções como txt_dividir, strings vazias para delimitador retornam o texto original sem divisão.
🚧 Limitações e Restrições
Tamanho máximo de string: Operações com textos acima de 4096 caracteres podem ser truncadas devido ao buffer interno.
Delimitadores vazios: Funções txt_dividir retornam o texto original se o delimitador for vazio.
Posições inválidas: Funções como txt_substr e txt_char retornam strings vazias ou caracteres padrão para posições fora dos limites.
