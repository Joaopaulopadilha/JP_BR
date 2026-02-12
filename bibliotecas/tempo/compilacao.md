# Windows (MinGW)
g++ -shared -o bibliotecas/tempo/tempo.jpd bibliotecas/tempo/tempo.cpp -O3 -static


# Linux
g++ -shared -fPIC -o bibliotecas/tempo/libtempo.jpd bibliotecas/tempo/tempo.cpp -O3
