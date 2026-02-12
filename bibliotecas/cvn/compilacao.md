# windows
g++ -shared -o bibliotecas/cvn/cvn.jpd bibliotecas/cvn/cvn.cpp -I bibliotecas/cvn -lgdi32 -lole32 -loleaut32 -lstrmiids -luuid -O3 -static


# linux
g++ -std=c++17 -shared -fPIC -o bibliotecas/cvn/libcvn.jpd bibliotecas/cvn/cvn.cpp -I bibliotecas/cvn -lX11 -lXrandr -lpthread -O3 -static-libgcc -static-libstdc++