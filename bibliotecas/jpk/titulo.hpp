// --- START OF FILE titulo.hpp ---

// titulo.hpp
// Lógica de barra de título customizada para o wrapper ImGui da JPLang

#ifndef TITULO_HPP
#define TITULO_HPP

#include <string>
#include <algorithm> // std::min
#include "imgui.h"

// === ESTADO GLOBAL DA BARRA DE TÍTULO ===
static bool g_barraTituloAtiva = false;
static std::string g_tituloTexto = "";
static int g_janelaLargura = 0;
static int g_janelaAltura = 0;

// Altura da barra de título
static const float BARRA_TITULO_ALTURA = 30.0f;

// Cor de fundo da barra (0-255)
static int g_barraTituloCorR = 50;
static int g_barraTituloCorG = 50;
static int g_barraTituloCorB = 50;

// Cor do texto do título (0-255)
static int g_barraTituloTextoCorR = 255;
static int g_barraTituloTextoCorG = 255;
static int g_barraTituloTextoCorB = 255;

// Cor dos botões (0-255)
static int g_barraTituloBotoesCorR = 80;
static int g_barraTituloBotoesCorG = 80;
static int g_barraTituloBotoesCorB = 80;

// Estado de arrastar
static bool g_arrastando = false;
static float g_arrastoDeltaX = 0;
static float g_arrastoDeltaY = 0;

// === FUNÇÕES AUXILIARES ===

// Ativa a barra de título customizada
static void ativarBarraTitulo(const std::string& titulo, int largura, int altura) {
    g_barraTituloAtiva = true;
    g_tituloTexto = titulo;
    g_janelaLargura = largura;
    g_janelaAltura = altura;
}

// Desativa a barra de título customizada
static void desativarBarraTitulo() {
    g_barraTituloAtiva = false;
}

// Verifica se a barra está ativa
static bool barraTituloAtiva() {
    return g_barraTituloAtiva;
}

// Define cor de fundo da barra
static void barraTituloCor(int r, int g, int b) {
    g_barraTituloCorR = r;
    g_barraTituloCorG = g;
    g_barraTituloCorB = b;
}

// Define cor do texto do título
static void barraTituloTextoCor(int r, int g, int b) {
    g_barraTituloTextoCorR = r;
    g_barraTituloTextoCorG = g;
    g_barraTituloTextoCorB = b;
}

// Define cor dos botões
static void barraTituloBotoesCor(int r, int g, int b) {
    g_barraTituloBotoesCorR = r;
    g_barraTituloBotoesCorG = g;
    g_barraTituloBotoesCorB = b;
}

// Retorna a altura da barra de título (para offset dos componentes)
static float getBarraTituloAltura() {
    return g_barraTituloAtiva ? BARRA_TITULO_ALTURA : 0.0f;
}

#ifdef _WIN32
#include <windows.h>

// Tamanho da borda para detecção de resize
static const int BORDA_RESIZE = 5;

// Remove a barra de título padrão do Windows mas mantém resize
static void removerBarraTituloPadrao(HWND hwnd) {
    // Remove caption e bordas, usa popup com resize manual
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_BORDER | WS_DLGFRAME);
    style |= WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLong(hwnd, GWL_STYLE, style);
    
    // Remove borda estendida
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    // Força atualização da janela
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

// Desenha a barra de título customizada (Windows)
static void desenharBarraTitulo(HWND hwnd) {
    if (!g_barraTituloAtiva) return;
    
    // Usa o Viewport principal para garantir posicionamento correto (0,0) relativo à área cliente
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float larguraJanela = viewport->Size.x;
    
    // Define a posição e tamanho da janela da barra de título
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(larguraJanela, BARRA_TITULO_ALTURA), ImGuiCond_Always);
    
    // Removemos padding vertical (y=0) para evitar deslocamento interno
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
        g_barraTituloCorR / 255.0f,
        g_barraTituloCorG / 255.0f,
        g_barraTituloCorB / 255.0f,
        1.0f
    ));
    
    ImGui::Begin("##BarraTitulo", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus); // Evita foco de teclado
    
    // Texto do título
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
        g_barraTituloTextoCorR / 255.0f,
        g_barraTituloTextoCorG / 255.0f,
        g_barraTituloTextoCorB / 255.0f,
        1.0f
    ));
    
    // Centraliza verticalmente o texto na barra
    ImGui::SetCursorPosY((BARRA_TITULO_ALTURA - ImGui::GetFontSize()) / 2);
    ImGui::Text("%s", g_tituloTexto.c_str());
    
    ImGui::PopStyleColor();
    
    // Botões à direita
    float botaoLargura = 45.0f;
    float botaoAltura = BARRA_TITULO_ALTURA;
    float posX = larguraJanela - (botaoLargura * 3);
    
    // Garante que os botões comecem no topo
    ImGui::SetCursorPos(ImVec2(posX, 0));
    
    // Estilo dos botões
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
        g_barraTituloBotoesCorR / 255.0f,
        g_barraTituloBotoesCorG / 255.0f,
        g_barraTituloBotoesCorB / 255.0f,
        1.0f
    ));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
        std::min(255, g_barraTituloBotoesCorR + 30) / 255.0f,
        std::min(255, g_barraTituloBotoesCorG + 30) / 255.0f,
        std::min(255, g_barraTituloBotoesCorB + 30) / 255.0f,
        1.0f
    ));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(
        std::min(255, g_barraTituloBotoesCorR + 50) / 255.0f,
        std::min(255, g_barraTituloBotoesCorG + 50) / 255.0f,
        std::min(255, g_barraTituloBotoesCorB + 50) / 255.0f,
        1.0f
    ));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    
    // Botão Minimizar
    if (ImGui::Button("-", ImVec2(botaoLargura, botaoAltura))) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
    
    ImGui::SameLine(0, 0);
    
    // Botão Maximizar/Restaurar
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &wp);
    bool maximizado = (wp.showCmd == SW_MAXIMIZE);
    
    if (ImGui::Button(maximizado ? "o" : "[]", ImVec2(botaoLargura, botaoAltura))) {
        if (maximizado) {
            ShowWindow(hwnd, SW_RESTORE);
        } else {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
    }
    
    ImGui::SameLine(0, 0);
    
    // Botão Fechar (vermelho no hover)
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::Button("X", ImVec2(botaoLargura, botaoAltura))) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    
    // Detecta arrasto na barra de título
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseNaBarra = (mousePos.y >= viewport->Pos.y && 
                         mousePos.y < viewport->Pos.y + BARRA_TITULO_ALTURA && 
                         mousePos.x < posX + viewport->Pos.x); // Ajuste relativo ao viewport
    
    if (mouseNaBarra && ImGui::IsMouseClicked(0)) {
        g_arrastando = true;
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        RECT rect;
        GetWindowRect(hwnd, &rect);
        g_arrastoDeltaX = (float)(cursorPos.x - rect.left);
        g_arrastoDeltaY = (float)(cursorPos.y - rect.top);
    }
    
    if (g_arrastando) {
        if (ImGui::IsMouseDown(0)) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            SetWindowPos(hwnd, NULL, 
                (int)(cursorPos.x - g_arrastoDeltaX),
                (int)(cursorPos.y - g_arrastoDeltaY),
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        } else {
            g_arrastando = false;
        }
    }
    
    // Duplo clique para maximizar/restaurar
    if (mouseNaBarra && ImGui::IsMouseDoubleClicked(0)) {
        if (maximizado) {
            ShowWindow(hwnd, SW_RESTORE);
        } else {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
    }
    
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

#else
// === IMPLEMENTAÇÃO LINUX (GLFW) ===
#include <GLFW/glfw3.h>

static double g_lastClickTime = 0;

// Remove decorações da janela no Linux
static void removerBarraTituloPadrao(GLFWwindow* window) {
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
}

// Desenha a barra de título customizada (Linux)
static void desenharBarraTitulo(GLFWwindow* window) {
    if (!g_barraTituloAtiva) return;
    
    // Usa Viewport para consistência
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float larguraJanela = viewport->Size.x;
    
    // Janela da barra de título
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(larguraJanela, BARRA_TITULO_ALTURA), ImGuiCond_Always);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
        g_barraTituloCorR / 255.0f,
        g_barraTituloCorG / 255.0f,
        g_barraTituloCorB / 255.0f,
        1.0f
    ));
    
    ImGui::Begin("##BarraTitulo", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus);
    
    // Texto do título
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
        g_barraTituloTextoCorR / 255.0f,
        g_barraTituloTextoCorG / 255.0f,
        g_barraTituloTextoCorB / 255.0f,
        1.0f
    ));
    
    ImGui::SetCursorPosY((BARRA_TITULO_ALTURA - ImGui::GetFontSize()) / 2);
    ImGui::Text("%s", g_tituloTexto.c_str());
    
    ImGui::PopStyleColor();
    
    // Botões à direita
    float botaoLargura = 45.0f;
    float botaoAltura = BARRA_TITULO_ALTURA;
    float posX = larguraJanela - (botaoLargura * 3);
    
    ImGui::SetCursorPos(ImVec2(posX, 0));
    
    // Estilo dos botões
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
        g_barraTituloBotoesCorR / 255.0f,
        g_barraTituloBotoesCorG / 255.0f,
        g_barraTituloBotoesCorB / 255.0f,
        1.0f
    ));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
        std::min(255, g_barraTituloBotoesCorR + 30) / 255.0f,
        std::min(255, g_barraTituloBotoesCorG + 30) / 255.0f,
        std::min(255, g_barraTituloBotoesCorB + 30) / 255.0f,
        1.0f
    ));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(
        std::min(255, g_barraTituloBotoesCorR + 50) / 255.0f,
        std::min(255, g_barraTituloBotoesCorG + 50) / 255.0f,
        std::min(255, g_barraTituloBotoesCorB + 50) / 255.0f,
        1.0f
    ));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    
    // Botão Minimizar
    if (ImGui::Button("-", ImVec2(botaoLargura, botaoAltura))) {
        glfwIconifyWindow(window);
    }
    
    ImGui::SameLine(0, 0);
    
    // Botão Maximizar/Restaurar
    int maximizado = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
    
    if (ImGui::Button(maximizado ? "o" : "[]", ImVec2(botaoLargura, botaoAltura))) {
        if (maximizado) {
            glfwRestoreWindow(window);
        } else {
            glfwMaximizeWindow(window);
        }
    }
    
    ImGui::SameLine(0, 0);
    
    // Botão Fechar (vermelho no hover)
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::Button("X", ImVec2(botaoLargura, botaoAltura))) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    
    // Detecta arrasto na barra de título
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseNaBarra = (mousePos.y >= viewport->Pos.y && 
                         mousePos.y < viewport->Pos.y + BARRA_TITULO_ALTURA && 
                         mousePos.x < posX + viewport->Pos.x);
    
    if (mouseNaBarra && ImGui::IsMouseClicked(0)) {
        g_arrastando = true;
        double cursorX, cursorY;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        g_arrastoDeltaX = (float)cursorX;
        g_arrastoDeltaY = (float)cursorY;
    }
    
    if (g_arrastando) {
        if (ImGui::IsMouseDown(0)) {
            double cursorX, cursorY;
            glfwGetCursorPos(window, &cursorX, &cursorY);
            
            int winX, winY;
            glfwGetWindowPos(window, &winX, &winY);
            
            glfwSetWindowPos(window,
                winX + (int)(cursorX - g_arrastoDeltaX),
                winY + (int)(cursorY - g_arrastoDeltaY));
        } else {
            g_arrastando = false;
        }
    }
    
    // Duplo clique para maximizar/restaurar
    if (mouseNaBarra && ImGui::IsMouseClicked(0)) {
        double currentTime = glfwGetTime();
        if (currentTime - g_lastClickTime < 0.3) {
            if (maximizado) {
                glfwRestoreWindow(window);
            } else {
                glfwMaximizeWindow(window);
            }
        }
        g_lastClickTime = currentTime;
    }
    
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

#endif // _WIN32 / Linux

#endif // TITULO_HPP