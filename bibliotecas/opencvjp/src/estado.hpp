// estado.hpp
// Estado global compartilhado entre módulos

#ifndef OPENCVJP_ESTADO_HPP
#define OPENCVJP_ESTADO_HPP

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <opencv2/opencv.hpp>

namespace opencvjp {

// =============================================================================
// ESTADO GLOBAL (Registro de Objetos)
// =============================================================================
inline std::map<int, cv::Mat> g_imagens;
inline std::map<int, cv::VideoCapture> g_cameras;
inline std::map<int, cv::CascadeClassifier> g_haarcascades;
inline std::vector<std::string> g_janelas;
inline int g_nextId = 1;
inline std::mutex g_mutex;

// =============================================================================
// FUNÇÃO DE REGISTRO
// =============================================================================
template<typename T>
inline int registrar(std::map<int, T>& mapa, T obj) {
    std::lock_guard<std::mutex> lock(g_mutex);
    int id = g_nextId++;
    mapa[id] = obj;
    return id;
}

} // namespace opencvjp

#endif // OPENCVJP_ESTADO_HPP
