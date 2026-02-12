// imagem.hpp
// Módulo de manipulação de imagens para biblioteca CVN (Portável Windows/Linux)

#ifndef CVN_IMAGEM_HPP
#define CVN_IMAGEM_HPP

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>

// Verificação de plataforma
#ifdef _WIN32
    #include <io.h>
    #define acesso _access
    #define F_OK 0
#else
    #include <unistd.h>
    #define acesso access
#endif

namespace cvn {

// Estrutura de imagem
struct Imagem {
    unsigned char* dados;
    int largura;
    int altura;
    int canais;
    bool valida;
    
    Imagem() : dados(nullptr), largura(0), altura(0), canais(0), valida(false) {}
    
    ~Imagem() {
        if (dados) {
            free(dados);
            dados = nullptr;
        }
    }
    
    // Impede cópia para evitar double free
    Imagem(const Imagem&) = delete;
    Imagem& operator=(const Imagem&) = delete;
    
    // Permite mover
    Imagem(Imagem&& other) noexcept {
        dados = other.dados;
        largura = other.largura;
        altura = other.altura;
        canais = other.canais;
        valida = other.valida;
        other.dados = nullptr;
        other.valida = false;
    }
    
    Imagem& operator=(Imagem&& other) noexcept {
        if (this != &other) {
            if (dados) free(dados);
            dados = other.dados;
            largura = other.largura;
            altura = other.altura;
            canais = other.canais;
            valida = other.valida;
            other.dados = nullptr;
            other.valida = false;
        }
        return *this;
    }
};

// Gerenciador de imagens (armazena por ID)
class GerenciadorImagens {
private:
    std::unordered_map<int, Imagem*> imagens;
    int proximo_id = 1;
    
public:
    static GerenciadorImagens& instancia() {
        static GerenciadorImagens inst;
        return inst;
    }
    
    // Verifica se arquivo existe
    bool existe(const std::string& caminho) {
        return acesso(caminho.c_str(), F_OK) == 0;
    }
    
    // Verifica se arquivo existe e é uma imagem válida
    bool existe_imagem(const std::string& caminho) {
        if (!existe(caminho)) return false;
        
        // Tenta abrir como imagem para verificar se é válida
        int largura, altura, canais;
        if (stbi_info(caminho.c_str(), &largura, &altura, &canais)) {
            return true;
        }
        return false;
    }
    
    // Obtém tamanho do arquivo em bytes
    long tamanho_arquivo(const std::string& caminho) {
        struct stat st;
        if (stat(caminho.c_str(), &st) == 0) {
            return (long)st.st_size;
        }
        return -1;
    }
    
    // Carrega imagem e retorna ID
    int carregar(const std::string& caminho) {
        Imagem* img = new Imagem();
        
        // Força 4 canais (RGBA) para facilitar renderização
        img->dados = stbi_load(caminho.c_str(), &img->largura, &img->altura, &img->canais, 4);
        
        if (img->dados) {
            img->canais = 4; // Sempre RGBA após conversão
            img->valida = true;
            int id = proximo_id++;
            imagens[id] = img;
            return id;
        }
        
        delete img;
        return -1;
    }
    
    // Cria imagem a partir de dados existentes
    int criar(unsigned char* dados, int largura, int altura, int canais) {
        if (!dados || largura <= 0 || altura <= 0) return -1;
        
        Imagem* img = new Imagem();
        img->largura = largura;
        img->altura = altura;
        img->canais = canais;
        img->dados = (unsigned char*)malloc(largura * altura * canais);
        
        if (img->dados) {
            memcpy(img->dados, dados, largura * altura * canais);
            img->valida = true;
            int id = proximo_id++;
            imagens[id] = img;
            return id;
        }
        
        delete img;
        return -1;
    }
    
    // Obtém imagem por ID
    Imagem* obter(int id) {
        auto it = imagens.find(id);
        if (it != imagens.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // Retorna tamanho da imagem como string "largura,altura"
    std::string tamanho(int id) {
        Imagem* img = obter(id);
        if (img && img->valida) {
            return std::to_string(img->largura) + "," + std::to_string(img->altura);
        }
        return "0,0";
    }
    
    // Redimensiona imagem e retorna novo ID
    int redimensionar(int id, int nova_largura, int nova_altura) {
        Imagem* img = obter(id);
        if (!img || !img->valida) return -1;
        if (nova_largura <= 0 || nova_altura <= 0) return -1;
        
        // Aloca buffer para imagem redimensionada
        int canais = img->canais;
        unsigned char* dados_novos = (unsigned char*)malloc(nova_largura * nova_altura * canais);
        if (!dados_novos) return -1;
        
        // Redimensiona usando stb_image_resize2
        stbir_resize_uint8_linear(
            img->dados, img->largura, img->altura, img->largura * canais,
            dados_novos, nova_largura, nova_altura, nova_largura * canais,
            STBIR_RGBA
        );
        
        // Cria nova imagem
        Imagem* nova_img = new Imagem();
        nova_img->dados = dados_novos;
        nova_img->largura = nova_largura;
        nova_img->altura = nova_altura;
        nova_img->canais = canais;
        nova_img->valida = true;
        
        int novo_id = proximo_id++;
        imagens[novo_id] = nova_img;
        return novo_id;
    }
    
    // Salva imagem em arquivo
    bool salvar(int id, const std::string& caminho) {
        Imagem* img = obter(id);
        if (!img || !img->valida) return false;
        
        // Detecta formato pela extensão
        std::string ext = caminho.substr(caminho.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        int resultado = 0;
        
        if (ext == "png") {
            resultado = stbi_write_png(caminho.c_str(), img->largura, img->altura, img->canais, img->dados, img->largura * img->canais);
        } else if (ext == "jpg" || ext == "jpeg") {
            resultado = stbi_write_jpg(caminho.c_str(), img->largura, img->altura, img->canais, img->dados, 90);
        } else if (ext == "bmp") {
            resultado = stbi_write_bmp(caminho.c_str(), img->largura, img->altura, img->canais, img->dados);
        } else if (ext == "tga") {
            resultado = stbi_write_tga(caminho.c_str(), img->largura, img->altura, img->canais, img->dados);
        } else {
            // Padrão: salva como PNG
            resultado = stbi_write_png(caminho.c_str(), img->largura, img->altura, img->canais, img->dados, img->largura * img->canais);
        }
        
        return resultado != 0;
    }
    
    // Libera imagem
    void liberar(int id) {
        auto it = imagens.find(id);
        if (it != imagens.end()) {
            delete it->second;
            imagens.erase(it);
        }
    }
    
    // Libera todas
    void liberar_todas() {
        for (auto& par : imagens) {
            delete par.second;
        }
        imagens.clear();
    }
    
    ~GerenciadorImagens() {
        liberar_todas();
    }
};

} // namespace cvn

#endif // CVN_IMAGEM_HPP