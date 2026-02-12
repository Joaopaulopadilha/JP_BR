// janela.hpp
// Funções de janela e exibição

#ifndef OPENCVJP_JANELA_HPP
#define OPENCVJP_JANELA_HPP

#include "tipos.hpp"
#include "estado.hpp"
#include <algorithm>

namespace opencvjp {
namespace janela {

// cv.exibir(titulo, id) - Exibe imagem em janela
inline ValorVariant exibir(const std::vector<ValorVariant>& args) {
    if (args.size() < 2) return 0;
    std::string titulo = get_str(args[0]);
    int id = get_int(args[1]);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_imagens.count(id)) {
        cv::imshow(titulo, g_imagens[id]);
        
        // Registra a janela se ainda não estiver na lista
        if (std::find(g_janelas.begin(), g_janelas.end(), titulo) == g_janelas.end()) {
            g_janelas.push_back(titulo);
        }
        
        cv::waitKey(1);
        return 1;
    }
    return 0;
}

// cv.esperar(ms) - Espera tecla por ms milissegundos
inline ValorVariant esperar(const std::vector<ValorVariant>& args) {
    int ms = (args.size() > 0) ? get_int(args[0]) : 0;
    return cv::waitKey(ms);
}

// cv.tecla(codigo) - Verifica se tecla foi pressionada ou janela fechada
inline ValorVariant tecla(const std::vector<ValorVariant>& args) {
    int codigo = (args.size() > 0) ? get_int(args[0]) : -1;
    int teclaPress = cv::waitKey(1) & 0xFF;
    
    // Verifica se alguma janela foi fechada (clicou no X)
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& jan : g_janelas) {
        if (cv::getWindowProperty(jan, cv::WND_PROP_VISIBLE) < 1) {
            return true;
        }
    }
    
    if (codigo < 0) {
        return teclaPress;
    }
    return (teclaPress == codigo) ? true : false;
}

// cv.fechar() - Fecha tudo (janelas e libera recursos)
inline ValorVariant fechar(const std::vector<ValorVariant>& args) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    // Libera todas as câmeras
    for (auto& par : g_cameras) {
        par.second.release();
    }
    g_cameras.clear();
    
    // Libera todas as imagens
    g_imagens.clear();
    
    // Libera todos os classificadores
    g_haarcascades.clear();
    
    // Limpa lista de janelas
    g_janelas.clear();
    
    // Fecha todas as janelas
    cv::destroyAllWindows();
    
    return 1;
}

// cv.fechar_todas() - Fecha todas as janelas (legado)
inline ValorVariant fechar_todas(const std::vector<ValorVariant>& args) {
    cv::destroyAllWindows();
    return 1;
}

}} // namespace opencvjp::janela

#endif // OPENCVJP_JANELA_HPP
