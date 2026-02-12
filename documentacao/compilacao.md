interpretador
# Windows
g++ -std=c++17 -static -DJP_MAIN_EXE -o jp src/main.cpp src/recurso.o -lstdc++fs

# Linux
g++ -o jp src/main.cpp -std=c++17 -ldl







#instalador
# windows
gcc documentacao\instalador_c.c -o instalar_jp.exe

# linux
gcc install.c -o instalar_jp