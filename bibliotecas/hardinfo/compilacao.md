g++ -shared -o bibliotecas/hardinfo/hardinfo.jpd bibliotecas/hardinfo/hardinfo.cpp -O3 -lpdh -lpsapi

g++ -c bibliotecas/hardinfo/hardinfo.cpp -o hardinfo.o -O3; ar rcs bibliotecas/hardinfo/libhardinfo.a hardinfo.o