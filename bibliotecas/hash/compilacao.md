# Windows
g++ -shared -o bibliotecas/hash/hash.jpd bibliotecas/hash/hash.cpp -O3 -static

# Linux
g++ -shared -fPIC -o bibliotecas/hash/libhash.jpd bibliotecas/hash/hash.cpp -O3 -static-libgcc -static-libstdc++