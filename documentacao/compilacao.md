interpretador
# windows
g++ -std=c++17 -Wall -Wextra -static -I. -o jp.exe src/main.cpp

# linux
g++ -std=c++17 -Wall -Wextra -static -I. -o jp src/main.cpp





#instalador
# windows
gcc documentacao\instalador_c.c -o instalar_jp.exe

# linux
gcc documentacao/instalador_c.c -o instalador_jp_linux