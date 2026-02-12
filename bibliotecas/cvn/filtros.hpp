// filtros.hpp
// Módulo de filtros de imagem para biblioteca CVN (Portável Windows/Linux)

#ifndef CVN_FILTROS_HPP
#define CVN_FILTROS_HPP

#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace cvn {

class Filtros {
public:
    // Converte para escala de cinza
    static void cinza(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados || canais < 3) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            unsigned char r = dados[idx + 0];
            unsigned char g = dados[idx + 1];
            unsigned char b = dados[idx + 2];
            
            // Fórmula de luminância
            unsigned char cinza = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
            
            dados[idx + 0] = cinza;
            dados[idx + 1] = cinza;
            dados[idx + 2] = cinza;
        }
    }
    
    // Inverte cores
    static void inverter(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            dados[idx + 0] = 255 - dados[idx + 0];
            dados[idx + 1] = 255 - dados[idx + 1];
            dados[idx + 2] = 255 - dados[idx + 2];
        }
    }
    
    // Ajusta brilho (-255 a 255)
    static void brilho(unsigned char* dados, int largura, int altura, int canais, int valor) {
        if (!dados) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            for (int c = 0; c < 3; c++) {
                int novo = dados[idx + c] + valor;
                dados[idx + c] = (unsigned char)std::clamp(novo, 0, 255);
            }
        }
    }
    
    // Ajusta contraste (0.0 a 3.0, 1.0 = normal)
    static void contraste(unsigned char* dados, int largura, int altura, int canais, float valor) {
        if (!dados) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            for (int c = 0; c < 3; c++) {
                float novo = ((dados[idx + c] / 255.0f - 0.5f) * valor + 0.5f) * 255.0f;
                dados[idx + c] = (unsigned char)std::clamp((int)novo, 0, 255);
            }
        }
    }
    
    // Threshold/Limiar binário (0-255)
    static void limiar(unsigned char* dados, int largura, int altura, int canais, int valor) {
        if (!dados || canais < 3) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            unsigned char r = dados[idx + 0];
            unsigned char g = dados[idx + 1];
            unsigned char b = dados[idx + 2];
            
            unsigned char cinza = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
            unsigned char resultado = (cinza >= valor) ? 255 : 0;
            
            dados[idx + 0] = resultado;
            dados[idx + 1] = resultado;
            dados[idx + 2] = resultado;
        }
    }
    
    // Blur simples (box blur)
    static void blur(unsigned char* dados, int largura, int altura, int canais, int raio) {
        if (!dados || raio <= 0) return;
        
        int tamanho = largura * altura * canais;
        unsigned char* temp = (unsigned char*)malloc(tamanho);
        if (!temp) return;
        memcpy(temp, dados, tamanho);
        
        for (int y = 0; y < altura; y++) {
            for (int x = 0; x < largura; x++) {
                int soma_r = 0, soma_g = 0, soma_b = 0;
                int count = 0;
                
                for (int dy = -raio; dy <= raio; dy++) {
                    for (int dx = -raio; dx <= raio; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < largura && ny >= 0 && ny < altura) {
                            int idx = (ny * largura + nx) * canais;
                            soma_r += temp[idx + 0];
                            soma_g += temp[idx + 1];
                            soma_b += temp[idx + 2];
                            count++;
                        }
                    }
                }
                
                int idx = (y * largura + x) * canais;
                dados[idx + 0] = soma_r / count;
                dados[idx + 1] = soma_g / count;
                dados[idx + 2] = soma_b / count;
            }
        }
        
        free(temp);
    }
    
    // Detecção de bordas (Sobel)
    static void bordas(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados || canais < 3) return;
        
        int tamanho = largura * altura * canais;
        unsigned char* temp = (unsigned char*)malloc(tamanho);
        if (!temp) return;
        
        // Primeiro converte para cinza
        memcpy(temp, dados, tamanho);
        cinza(temp, largura, altura, canais);
        
        // Kernels Sobel
        int gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
        int gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
        
        for (int y = 1; y < altura - 1; y++) {
            for (int x = 1; x < largura - 1; x++) {
                int soma_x = 0, soma_y = 0;
                
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int idx = ((y + dy) * largura + (x + dx)) * canais;
                        int pixel = temp[idx];
                        soma_x += pixel * gx[dy + 1][dx + 1];
                        soma_y += pixel * gy[dy + 1][dx + 1];
                    }
                }
                
                int magnitude = (int)sqrt((double)(soma_x * soma_x + soma_y * soma_y));
                magnitude = std::clamp(magnitude, 0, 255);
                
                int idx = (y * largura + x) * canais;
                dados[idx + 0] = magnitude;
                dados[idx + 1] = magnitude;
                dados[idx + 2] = magnitude;
            }
        }
        
        free(temp);
    }
    
    // Efeito sépia
    static void sepia(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados || canais < 3) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            int r = dados[idx + 0];
            int g = dados[idx + 1];
            int b = dados[idx + 2];
            
            int novo_r = (int)(0.393f * r + 0.769f * g + 0.189f * b);
            int novo_g = (int)(0.349f * r + 0.686f * g + 0.168f * b);
            int novo_b = (int)(0.272f * r + 0.534f * g + 0.131f * b);
            
            dados[idx + 0] = (unsigned char)std::clamp(novo_r, 0, 255);
            dados[idx + 1] = (unsigned char)std::clamp(novo_g, 0, 255);
            dados[idx + 2] = (unsigned char)std::clamp(novo_b, 0, 255);
        }
    }
    
    // Ajusta saturação (0.0 = cinza, 1.0 = normal, 2.0 = super saturado)
    static void saturacao(unsigned char* dados, int largura, int altura, int canais, float valor) {
        if (!dados || canais < 3) return;
        
        int total = largura * altura;
        for (int i = 0; i < total; i++) {
            int idx = i * canais;
            float r = dados[idx + 0] / 255.0f;
            float g = dados[idx + 1] / 255.0f;
            float b = dados[idx + 2] / 255.0f;
            
            float cinza = 0.299f * r + 0.587f * g + 0.114f * b;
            
            r = cinza + (r - cinza) * valor;
            g = cinza + (g - cinza) * valor;
            b = cinza + (b - cinza) * valor;
            
            dados[idx + 0] = (unsigned char)std::clamp((int)(r * 255), 0, 255);
            dados[idx + 1] = (unsigned char)std::clamp((int)(g * 255), 0, 255);
            dados[idx + 2] = (unsigned char)std::clamp((int)(b * 255), 0, 255);
        }
    }
    
    // Flip horizontal
    static void flip_h(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados) return;
        
        for (int y = 0; y < altura; y++) {
            for (int x = 0; x < largura / 2; x++) {
                int idx1 = (y * largura + x) * canais;
                int idx2 = (y * largura + (largura - 1 - x)) * canais;
                
                for (int c = 0; c < canais; c++) {
                    std::swap(dados[idx1 + c], dados[idx2 + c]);
                }
            }
        }
    }
    
    // Flip vertical
    static void flip_v(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados) return;
        
        int linha_bytes = largura * canais;
        unsigned char* temp = (unsigned char*)malloc(linha_bytes);
        if (!temp) return;
        
        for (int y = 0; y < altura / 2; y++) {
            int idx1 = y * linha_bytes;
            int idx2 = (altura - 1 - y) * linha_bytes;
            
            memcpy(temp, &dados[idx1], linha_bytes);
            memcpy(&dados[idx1], &dados[idx2], linha_bytes);
            memcpy(&dados[idx2], temp, linha_bytes);
        }
        
        free(temp);
    }
    
    // Rotacionar 90, 180 ou 270 graus
    static unsigned char* rotacionar(unsigned char* dados, int& largura, int& altura, int canais, int graus) {
        if (!dados) return nullptr;
        
        // Normaliza para 0, 90, 180, 270
        graus = ((graus % 360) + 360) % 360;
        
        if (graus == 0) {
            int tamanho = largura * altura * canais;
            unsigned char* copia = (unsigned char*)malloc(tamanho);
            if (copia) memcpy(copia, dados, tamanho);
            return copia;
        }
        
        int nova_largura, nova_altura;
        if (graus == 90 || graus == 270) {
            nova_largura = altura;
            nova_altura = largura;
        } else {
            nova_largura = largura;
            nova_altura = altura;
        }
        
        unsigned char* novo = (unsigned char*)malloc(nova_largura * nova_altura * canais);
        if (!novo) return nullptr;
        
        for (int y = 0; y < altura; y++) {
            for (int x = 0; x < largura; x++) {
                int nx, ny;
                
                if (graus == 90) {
                    nx = altura - 1 - y;
                    ny = x;
                } else if (graus == 180) {
                    nx = largura - 1 - x;
                    ny = altura - 1 - y;
                } else { // 270
                    nx = y;
                    ny = largura - 1 - x;
                }
                
                int idx_origem = (y * largura + x) * canais;
                int idx_destino = (ny * nova_largura + nx) * canais;
                
                for (int c = 0; c < canais; c++) {
                    novo[idx_destino + c] = dados[idx_origem + c];
                }
            }
        }
        
        largura = nova_largura;
        altura = nova_altura;
        return novo;
    }
};

} // namespace cvn

#endif // CVN_FILTROS_HPP