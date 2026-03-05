g++ -std=c++17 -static -DJP_MAIN_EXE -o jp src/main.cpp -lstdc++fs


g++ -o jpc jpc/jpc.cpp -static -O3

g++ -shared -o bibliotecas/paravoz.jpd bibliotecas/paravoz.cpp -static -O3 -lole32 -loleaut32 -luuid -lsapi 


PS C:\Users\jpp\Desktop\jp_vm> g++ -shared -o bibliotecas/teclado.jpd bibliotecas/teclado.cpp -O3
PS C:\Users\jpp\Desktop\jp_vm> g++ -c bibliotecas/teclado.cpp -o teclado.o -O3
PS C:\Users\jpp\Desktop\jp_vm> ar rcs bibliotecas/libteclado.a teclado.o