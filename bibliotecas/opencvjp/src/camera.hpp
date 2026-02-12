// camera.hpp
// Funções de câmera

#ifndef OPENCVJP_CAMERA_HPP
#define OPENCVJP_CAMERA_HPP

#include "tipos.hpp"
#include "estado.hpp"

namespace opencvjp {
namespace camera {

// cv.camera(indice) - Abre câmera (simplificado)
inline ValorVariant abrir(const std::vector<ValorVariant>& args) {
    int idx = (args.size() > 0) ? get_int(args[0]) : 0;
    cv::VideoCapture cap(idx);
    
    if (!cap.isOpened()) return 0;
    
    return registrar(g_cameras, cap);
}

// cv.obter(cam) - Obtém frame da câmera
inline ValorVariant obter(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_cameras.count(id)) return 0;

    cv::Mat frame;
    if (g_cameras[id].read(frame)) {
        int imgId = g_nextId++;
        g_imagens[imgId] = frame;
        return imgId;
    }
    return 0;
}

// cv.camera_abrir(indice) - Abre câmera (legado)
inline ValorVariant abrir_legado(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int idx = get_int(args[0]);
    cv::VideoCapture cap(idx);
    
    if (!cap.isOpened()) return 0;
    
    return registrar(g_cameras, cap);
}

// cv.camera_ler(cam) - Lê frame da câmera (legado)
inline ValorVariant ler_legado(const std::vector<ValorVariant>& args) {
    if (args.empty()) return 0;
    int id = get_int(args[0]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_cameras.count(id)) return 0;

    cv::Mat frame;
    if (g_cameras[id].read(frame)) {
        int imgId = g_nextId++;
        g_imagens[imgId] = frame;
        return imgId;
    }
    return 0;
}

}} // namespace opencvjp::camera

#endif // OPENCVJP_CAMERA_HPP
