g++ -shared -o bibliotecas/overlay/overlay.jpd bibliotecas/overlay/overlay.cpp -lgdi32 -lgdiplus -mwindows -O3 -static


g++ -c bibliotecas/overlay/overlay.cpp -o overlay.o -O3; ar rcs bibliotecas/overlay/liboverlay.a overlay.o