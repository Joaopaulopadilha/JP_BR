// deteccao.hpp
// Funções de detecção de objetos (Haar Cascade, YOLO futuro)

#ifndef OPENCVJP_DETECCAO_HPP
#define OPENCVJP_DETECCAO_HPP

#include "tipos.hpp"
#include "estado.hpp"

namespace opencvjp {
namespace deteccao {

// cv.modelo_haarcascade(caminho) - Carrega classificador Haar Cascade
inline ValorVariant modelo_haarcascade(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    std::string caminho = get_str(args[0]);
    
    cv::CascadeClassifier cascade;
    if (!cascade.load(caminho)) return 0;
    
    std::lock_guard<std::mutex> lock(g_mutex);
    int id = g_nextId++;
    g_haarcascades[id] = cascade;
    return id;
}

// cv.detectar(modelo, frame) - Detecta objetos e desenha retângulos
inline ValorVariant detectar(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0;
    int modeloId = get_int(args[0]);
    int frameId = get_int(args[1]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (!g_haarcascades.count(modeloId)) return 0;
    if (!g_imagens.count(frameId)) return 0;
    
    cv::Mat& frame = g_imagens[frameId];
    cv::CascadeClassifier& cascade = g_haarcascades[modeloId];
    
    // Converte para cinza para detecção
    cv::Mat cinza;
    if (frame.channels() == 3)
        cv::cvtColor(frame, cinza, cv::COLOR_BGR2GRAY);
    else
        cinza = frame;
    
    // Equaliza histograma para melhorar detecção
    cv::equalizeHist(cinza, cinza);
    
    // Detecta objetos
    std::vector<cv::Rect> objetos;
    cascade.detectMultiScale(cinza, objetos, 1.1, 3, 0, cv::Size(30, 30));
    
    // Desenha retângulos (verde, 1px)
    for (const auto& obj : objetos) {
        cv::rectangle(frame, obj, cv::Scalar(0, 255, 0), 1);
    }
    
    return (int)objetos.size();
}

// ============================================================================
// FUTURO: Funções para YOLO
// ============================================================================

// cv.modelo_yolo(cfg, weights) - Carrega modelo YOLO (futuro)
// cv.modelo_onnx(caminho) - Carrega modelo ONNX (futuro)

}} // namespace opencvjp::deteccao

#endif // OPENCVJP_DETECCAO_HPP
