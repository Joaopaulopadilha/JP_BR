jpd

g++ -shared -o bibliotecas/imgui/imgui.jpd bibliotecas/imgui/imgui.cpp bibliotecas/imgui/src/imgui.cpp bibliotecas/imgui/src/imgui_draw.cpp bibliotecas/imgui/src/imgui_tables.cpp bibliotecas/imgui/src/imgui_widgets.cpp bibliotecas/imgui/src/backends/imgui_impl_win32.cpp bibliotecas/imgui/src/backends/imgui_impl_dx11.cpp -I bibliotecas/imgui/src -ld3d11 -ld3dcompiler -ldwmapi -lgdi32 -O3 -static-libgcc -static-libstdc++

# linux
g++ -shared -fPIC -std=c++17 -o bibliotecas/imgui/libimgui.jpd bibliotecas/imgui/imgui.cpp bibliotecas/imgui/src/imgui.cpp bibliotecas/imgui/src/imgui_draw.cpp bibliotecas/imgui/src/imgui_tables.cpp bibliotecas/imgui/src/imgui_widgets.cpp bibliotecas/imgui/src/backends/imgui_impl_glfw.cpp bibliotecas/imgui/src/backends/imgui_impl_opengl3.cpp -Ibibliotecas/imgui/src -lglfw -lGL -ldl -lpthread -O3 -static-libgcc -static-libstdc++