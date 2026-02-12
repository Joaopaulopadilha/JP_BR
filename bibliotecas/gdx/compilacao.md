# windows
g++ -shared -o bibliotecas/gdx/gdx.jpd bibliotecas/gdx/gdx.cpp -lgdi32 -luser32 -lopengl32 -static-libgcc -static-libstdc++ -O3
# linux
g++ -shared -fPIC -o bibliotecas/gdx/libgdx.jpd bibliotecas/gdx/gdx.cpp -lX11 -lGL -O3 -static-libgcc -static-libstdc++