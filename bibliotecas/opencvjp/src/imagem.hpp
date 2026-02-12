// imagem.hpp
// Funções de manipulação de imagem

#ifndef OPENCVJP_IMAGEM_HPP
#define OPENCVJP_IMAGEM_HPP

#include "tipos.hpp"
#include "estado.hpp"

namespace opencvjp {
namespace imagem {

// cv.carregar(caminho) - Carrega imagem do disco
inline ValorVariant carregar(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    std::string caminho = get_str(args[0]);
    
    cv::Mat img = cv::imread(caminho);
    if (img.empty()) return 0;
    
    return registrar(g_imagens, img);
}

// cv.salvar(id, caminho) - Salva imagem no disco
inline ValorVariant salvar(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0;
    int id = get_int(args[0]);
    std::string path = get_str(args[1]);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_imagens.count(id)) {
        return (int)cv::imwrite(path, g_imagens[id]);
    }
    return 0;
}

// cv.largura(id) - Retorna largura da imagem
inline ValorVariant largura(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_imagens.count(id)) return g_imagens[id].cols;
    return 0;
}

// cv.altura(id) - Retorna altura da imagem
inline ValorVariant altura(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_imagens.count(id)) return g_imagens[id].rows;
    return 0;
}

// cv.redimensionar(id, w, h) - Redimensiona imagem
inline ValorVariant redimensionar(const std::vector<ValorVariant>& args) {
    if (args.size() < 3) return 0;
    int id = get_int(args[0]);
    int w = get_int(args[1]);
    int h = get_int(args[2]);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_imagens.count(id)) return 0;

    cv::Mat nova;
    cv::resize(g_imagens[id], nova, cv::Size(w, h));
    
    int novoId = g_nextId++;
    g_imagens[novoId] = nova;
    return novoId;
}

// cv.cinza(id) - Converte para escala de cinza
inline ValorVariant cinza(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_imagens.count(id)) return 0;

    cv::Mat nova;
    if (g_imagens[id].channels() == 3)
        cv::cvtColor(g_imagens[id], nova, cv::COLOR_BGR2GRAY);
    else
        nova = g_imagens[id].clone();

    int novoId = g_nextId++;
    g_imagens[novoId] = nova;
    return novoId;
}

// cv.bordas(id) - Detecta bordas (Canny)
inline ValorVariant bordas(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_imagens.count(id)) return 0;

    cv::Mat nova;
    cv::Canny(g_imagens[id], nova, 100, 200);

    int novoId = g_nextId++;
    g_imagens[novoId] = nova;
    return novoId;
}

// cv.blur(id, k) - Aplica desfoque gaussiano
inline ValorVariant blur(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0;
    int id = get_int(args[0]);
    int k = get_int(args[1]);
    if (k % 2 == 0) k++;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_imagens.count(id)) return 0;

    cv::Mat nova;
    cv::GaussianBlur(g_imagens[id], nova, cv::Size(k, k), 0);

    int novoId = g_nextId++;
    g_imagens[novoId] = nova;
    return novoId;
}

// cv.inverter_h(id) - Inverte horizontalmente
inline ValorVariant inverter_h(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_imagens.count(id)) return 0;

    cv::Mat nova;
    cv::flip(g_imagens[id], nova, 1);

    int novoId = g_nextId++;
    g_imagens[novoId] = nova;
    return novoId;
}

// cv.inverter_v(id) - Inverte verticalmente
inline ValorVariant inverter_v(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_imagens.count(id)) return 0;

    cv::Mat nova;
    cv::flip(g_imagens[id], nova, 0);

    int novoId = g_nextId++;
    g_imagens[novoId] = nova;
    return novoId;
}

// cv.liberar(id) - Libera memória de imagem ou câmera
inline ValorVariant liberar(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_imagens.erase(id)) return 1;
    if (g_cameras.erase(id)) return 1;
    return 0;
}

}} // namespace opencvjp::imagem

#endif // OPENCVJP_IMAGEM_HPP
