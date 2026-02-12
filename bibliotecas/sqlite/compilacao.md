# windows
compilar a sqlite para .o primeiro
gcc -c -o bibliotecas/sqlite/sqlite3.o bibliotecas/sqlite/src/sqlite3.c -I bibliotecas/sqlite/src -O3

linkar a biblioteca
g++ -shared -o bibliotecas/sqlite/sqlite.jpd bibliotecas/sqlite/sqlite.cpp bibliotecas/sqlite/sqlite3.o -I bibliotecas/sqlite -O3 -static


# linux
compilar a sqlite para .o primeiro
gcc -c -fPIC -o bibliotecas/sqlite/sqlite3.o bibliotecas/sqlite/src/sqlite3.c -I bibliotecas/sqlite/src -O3

linkar a biblioteca
g++ -shared -fPIC -o bibliotecas/sqlite/libsqlite.jpd bibliotecas/sqlite/sqlite.cpp bibliotecas/sqlite/sqlite3.o -I bibliotecas/sqlite -lpthread -ldl -O3
