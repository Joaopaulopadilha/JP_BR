@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo      COMPILADOR UNIVERSAL JPLAMA
echo ==========================================

REM 1. Define a Raiz
set BASE=bibliotecas\jpllama\src\llama.cpp-master

REM 2. Define Includes (O segredo para achar os headers)
set INC=-I %BASE%\include -I %BASE%\ggml\include -I %BASE%\ggml\src -I %BASE%\ggml\src\ggml-cpu

REM 3. Define Flags (Corrige o erro de versao e habilita otimizacoes)
REM Adicionamos -DGGML_VERSION para corrigir o erro que voce teve
set C_FLAGS=-O3 -DGGML_USE_OPENMP -D_XOPEN_SOURCE=600 -DGGML_VERSION="\"jplang\"" -DGGML_COMMIT="\"0\"" -DGGML_SCHED_MAX_COPIES=4
set CPP_FLAGS=%C_FLAGS% -std=c++17

echo.
echo [1/5] Compilando Core GGML (Matematica Base)
REM Compila todos os .c da pasta ggml/src
for %%f in (%BASE%\ggml\src\*.c) do (
    echo .. %%~nxf
    gcc -c "%%f" -o "%%~nf.o" %INC% %C_FLAGS%
)

REM Compila os .cpp essenciais do ggml (backend, threading, gguf)
REM Nao usamos *.cpp aqui para evitar arquivos de teste ou CUDA se tiver
g++ -c %BASE%\ggml\src\ggml-backend.cpp -o ggml-backend-cpp.o %INC% %CPP_FLAGS%
g++ -c %BASE%\ggml\src\ggml-alloc.c -o ggml-alloc.o %INC% %C_FLAGS%
g++ -c %BASE%\ggml\src\gguf.cpp -o gguf.o %INC% %CPP_FLAGS%
g++ -c %BASE%\ggml\src\ggml-opt.cpp -o ggml-opt.o %INC% %CPP_FLAGS%
g++ -c %BASE%\ggml\src\ggml-threading.cpp -o ggml-threading.o %INC% %CPP_FLAGS%

echo.
echo [2/5] Compilando Backend CPU (O Motor)
REM Compila tudo que estiver em ggml-cpu
for %%f in (%BASE%\ggml\src\ggml-cpu\*.c) do (
    echo .. %%~nxf
    gcc -c "%%f" -o "%%~nf.o" %INC% %C_FLAGS%
)
for %%f in (%BASE%\ggml\src\ggml-cpu\*.cpp) do (
    echo .. %%~nxf
    g++ -c "%%f" -o "%%~nf.o" %INC% %CPP_FLAGS%
)

echo.
echo [3/5] Compilando Llama (A Logica de IA)
REM Aqui esta a magica: Compilar TUDO que esta em src/*.cpp
REM Isso resolve os erros de llama_mmap, llama_context, etc.
for %%f in (%BASE%\src\*.cpp) do (
    echo .. %%~nxf
    g++ -c "%%f" -o "%%~nf.o" %INC% %CPP_FLAGS%
)

echo.
echo [4/5] Compilando Adaptador JPLang
g++ -c bibliotecas\jpllama\jpllama.cpp -o jpllama.o %INC% %CPP_FLAGS%

echo.
echo [5/5] Linkando DLL final (JPLAMA.JPD)
REM Junta todos os .o gerados
g++ -shared -o bibliotecas\jpllama\jpllama.jpd *.o -static-libgcc -static-libstdc++ -lpthread -O3

echo.
echo === Limpeza ===
del *.o

echo.
echo ==========================================
echo    CONCLUIDO! Tente rodar o teste.
echo ==========================================
pause