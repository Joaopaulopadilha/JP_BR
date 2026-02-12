
# JPD (DLL)
g++ -shared -o bibliotecas/jpgame/jpgame.jpd bibliotecas/jpgame/jpgame.cpp -ld3d11 -ld3dcompiler -O3 -static


# liblinux
g++ -std=c++17 -shared -fPIC -o bibliotecas/jpgame/libjpgame.jpd bibliotecas/jpgame/jpgame.cpp -I bibliotecas/jpgame -lX11 -lGL -lGLX -lpthread -O3 -static-libgcc -static-libstdc++

# dependencias linux 
sudo apt install libx11-dev libgl1-mesa-dev libglu1-mesa-dev