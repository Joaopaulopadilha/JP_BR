# windows
g++ -shared -o bibliotecas/miniaudio/miniaudio.jpd bibliotecas/miniaudio/miniaudio.cpp -static-libgcc -static-libstdc++ -O3

# linux
g++ -shared -fPIC -o bibliotecas/miniaudio/libminiaudio.jpd bibliotecas/miniaudio/miniaudio.cpp -lpthread -ldl -O3