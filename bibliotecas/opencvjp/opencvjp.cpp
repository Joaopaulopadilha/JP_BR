// opencvjp.cpp
// Wrapper OpenCV 4.5.5 para JPLang - Versão Modularizada
//
// Saída: opencvjp.jpd
// Compile: g++ -shared -o opencvjp.jpd opencvjp.cpp -I<opencv>/include -L<opencv>/lib -lopencv_core455 -lopencv_highgui455 -lopencv_imgcodecs455 -lopencv_imgproc455 -lopencv_videoio455 -lopencv_objdetect455 -O3

#include <windows.h>

// Módulos OpenCV JPLang
#include "src/tipos.hpp"
#include "src/estado.hpp"
#include "src/imagem.hpp"
#include "src/camera.hpp"
#include "src/janela.hpp"
#include "src/deteccao.hpp"

using namespace opencvjp;

// =============================================================================
// EXPORTS PARA JPLANG (Interpretador C++)
// =============================================================================
extern "C" {

    // =========================================================================
    // IMAGEM
    // =========================================================================
    __declspec(dllexport) ValorVariant cv_carregar(const std::vector<ValorVariant>& args) {
        return imagem::carregar(args);
    }

    __declspec(dllexport) ValorVariant cv_salvar(const std::vector<ValorVariant>& args) {
        return imagem::salvar(args);
    }

    __declspec(dllexport) ValorVariant cv_largura(const std::vector<ValorVariant>& args) {
        return imagem::largura(args);
    }

    __declspec(dllexport) ValorVariant cv_altura(const std::vector<ValorVariant>& args) {
        return imagem::altura(args);
    }

    __declspec(dllexport) ValorVariant cv_redimensionar(const std::vector<ValorVariant>& args) {
        return imagem::redimensionar(args);
    }

    __declspec(dllexport) ValorVariant cv_cinza(const std::vector<ValorVariant>& args) {
        return imagem::cinza(args);
    }

    __declspec(dllexport) ValorVariant cv_bordas(const std::vector<ValorVariant>& args) {
        return imagem::bordas(args);
    }

    __declspec(dllexport) ValorVariant cv_blur(const std::vector<ValorVariant>& args) {
        return imagem::blur(args);
    }

    __declspec(dllexport) ValorVariant cv_inverter_h(const std::vector<ValorVariant>& args) {
        return imagem::inverter_h(args);
    }

    __declspec(dllexport) ValorVariant cv_inverter_v(const std::vector<ValorVariant>& args) {
        return imagem::inverter_v(args);
    }

    __declspec(dllexport) ValorVariant cv_liberar(const std::vector<ValorVariant>& args) {
        return imagem::liberar(args);
    }

    // =========================================================================
    // CÂMERA
    // =========================================================================
    __declspec(dllexport) ValorVariant cv_camera(const std::vector<ValorVariant>& args) {
        return camera::abrir(args);
    }

    __declspec(dllexport) ValorVariant cv_obter(const std::vector<ValorVariant>& args) {
        return camera::obter(args);
    }

    __declspec(dllexport) ValorVariant cv_camera_abrir(const std::vector<ValorVariant>& args) {
        return camera::abrir_legado(args);
    }

    __declspec(dllexport) ValorVariant cv_camera_ler(const std::vector<ValorVariant>& args) {
        return camera::ler_legado(args);
    }

    // =========================================================================
    // JANELA
    // =========================================================================
    __declspec(dllexport) ValorVariant cv_exibir(const std::vector<ValorVariant>& args) {
        return janela::exibir(args);
    }

    __declspec(dllexport) ValorVariant cv_esperar(const std::vector<ValorVariant>& args) {
        return janela::esperar(args);
    }

    __declspec(dllexport) ValorVariant cv_tecla(const std::vector<ValorVariant>& args) {
        return janela::tecla(args);
    }

    __declspec(dllexport) ValorVariant cv_fechar(const std::vector<ValorVariant>& args) {
        return janela::fechar(args);
    }

    __declspec(dllexport) ValorVariant cv_fechar_todas(const std::vector<ValorVariant>& args) {
        return janela::fechar_todas(args);
    }

    // =========================================================================
    // DETECÇÃO
    // =========================================================================
    __declspec(dllexport) ValorVariant cv_modelo_haarcascade(const std::vector<ValorVariant>& args) {
        return deteccao::modelo_haarcascade(args);
    }

    __declspec(dllexport) ValorVariant cv_detectar(const std::vector<ValorVariant>& args) {
        return deteccao::detectar(args);
    }

    // =========================================================================
    // WRAPPERS TCC (para executáveis compilados)
    // =========================================================================
    
    // Imagem
    __declspec(dllexport) JPValor jp_cv_carregar(JPValor* args, int n) {
        return variant_para_jp(imagem::carregar(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_salvar(JPValor* args, int n) {
        return variant_para_jp(imagem::salvar(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_largura(JPValor* args, int n) {
        return variant_para_jp(imagem::largura(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_altura(JPValor* args, int n) {
        return variant_para_jp(imagem::altura(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_redimensionar(JPValor* args, int n) {
        return variant_para_jp(imagem::redimensionar(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_cinza(JPValor* args, int n) {
        return variant_para_jp(imagem::cinza(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_bordas(JPValor* args, int n) {
        return variant_para_jp(imagem::bordas(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_blur(JPValor* args, int n) {
        return variant_para_jp(imagem::blur(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_inverter_h(JPValor* args, int n) {
        return variant_para_jp(imagem::inverter_h(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_inverter_v(JPValor* args, int n) {
        return variant_para_jp(imagem::inverter_v(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_liberar(JPValor* args, int n) {
        return variant_para_jp(imagem::liberar(jp_array_para_vector(args, n)));
    }

    // Câmera
    __declspec(dllexport) JPValor jp_cv_camera(JPValor* args, int n) {
        return variant_para_jp(camera::abrir(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_obter(JPValor* args, int n) {
        return variant_para_jp(camera::obter(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_camera_abrir(JPValor* args, int n) {
        return variant_para_jp(camera::abrir_legado(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_camera_ler(JPValor* args, int n) {
        return variant_para_jp(camera::ler_legado(jp_array_para_vector(args, n)));
    }

    // Janela
    __declspec(dllexport) JPValor jp_cv_exibir(JPValor* args, int n) {
        return variant_para_jp(janela::exibir(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_esperar(JPValor* args, int n) {
        return variant_para_jp(janela::esperar(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_tecla(JPValor* args, int n) {
        return variant_para_jp(janela::tecla(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_fechar(JPValor* args, int n) {
        return variant_para_jp(janela::fechar(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_fechar_todas(JPValor* args, int n) {
        return variant_para_jp(janela::fechar_todas(jp_array_para_vector(args, n)));
    }

    // Detecção
    __declspec(dllexport) JPValor jp_cv_modelo_haarcascade(JPValor* args, int n) {
        return variant_para_jp(deteccao::modelo_haarcascade(jp_array_para_vector(args, n)));
    }
    __declspec(dllexport) JPValor jp_cv_detectar(JPValor* args, int n) {
        return variant_para_jp(deteccao::detectar(jp_array_para_vector(args, n)));
    }

} // extern "C"