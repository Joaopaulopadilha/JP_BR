# windows
g++ -shared -o bibliotecas/svs/svs.jpd bibliotecas/svs/svs.cpp -O3 -lws2_32 -static


# linux
g++ -shared -fPIC -o bibliotecas/svs/libsvs.jpd bibliotecas/svs/svs.cpp -O3
