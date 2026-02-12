// miniaudio.cpp
// Biblioteca de Áudio para JPLang - Síntese com miniaudio

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <cmath>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// PLATAFORMA E MACROS
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// =============================================================================
// TIPOS JPLANG
// =============================================================================
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
    } valor;
} JPValor;

inline JPValor jp_bool(bool b) {
    JPValor v; v.tipo = JP_TIPO_BOOL; v.valor.booleano = b ? 1 : 0; return v;
}

inline JPValor jp_int(int64_t i) {
    JPValor v; v.tipo = JP_TIPO_INT; v.valor.inteiro = i; return v;
}

inline int64_t get_int(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return 0;
    if (args[idx].tipo == JP_TIPO_INT) return args[idx].valor.inteiro;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return (int64_t)args[idx].valor.decimal;
    return 0;
}

inline double get_double(JPValor* args, int idx, int numArgs) {
    if (idx >= numArgs) return 0.0;
    if (args[idx].tipo == JP_TIPO_DOUBLE) return args[idx].valor.decimal;
    if (args[idx].tipo == JP_TIPO_INT) return (double)args[idx].valor.inteiro;
    return 0.0;
}

// =============================================================================
// CONSTANTES DE ÁUDIO
// =============================================================================
#define SAMPLE_RATE 44100
#define CHANNELS 1

// =============================================================================
// ESTRUTURA DE SOM SINTETIZADO
// =============================================================================
struct Som {
    std::vector<float> samples;
    float frequencia;
    int duracao_ms;
};

// =============================================================================
// ESTADO GLOBAL
// =============================================================================
static ma_device g_device;
static bool g_device_iniciado = false;
static std::vector<Som*> g_sons;
static std::mutex g_mutex;

// Som sendo tocado atualmente
static float* g_play_buffer = nullptr;
static size_t g_play_length = 0;
static size_t g_play_position = 0;
static bool g_playing = false;
static bool g_looping = false;
static bool g_releasing = false;
static float g_release_volume = 1.0f;
static int64_t g_current_sound_id = -1;

#define RELEASE_SPEED 0.005f

// =============================================================================
// CALLBACK DE ÁUDIO
// =============================================================================
void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    (void)device;
    (void)input;
    float* out = (float*)output;
    
    std::lock_guard<std::mutex> lock(g_mutex);
    
    for (ma_uint32 i = 0; i < frameCount; i++) {
        if (g_playing && g_play_buffer && g_play_position < g_play_length) {
            float sample = g_play_buffer[g_play_position];
            
            // Aplica fade-out se estiver liberando
            if (g_releasing) {
                sample *= g_release_volume;
                g_release_volume -= RELEASE_SPEED;
                if (g_release_volume <= 0.0f) {
                    g_release_volume = 1.0f;
                    g_releasing = false;
                    g_playing = false;
                    g_looping = false;
                    g_current_sound_id = -1;
                }
            }
            
            out[i] = sample;
            g_play_position++;
            
            // Loop: volta ao início quando chega ao fim
            if (g_play_position >= g_play_length && g_looping && !g_releasing) {
                g_play_position = 0;
            }
        } else {
            out[i] = 0.0f;
            if (!g_looping) {
                g_playing = false;
                g_current_sound_id = -1;
            }
        }
    }
}

// =============================================================================
// INICIALIZAÇÃO DO DISPOSITIVO DE ÁUDIO
// =============================================================================
bool iniciar_audio() {
    if (g_device_iniciado) return true;
    
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = CHANNELS;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = audio_callback;
    
    if (ma_device_init(NULL, &config, &g_device) != MA_SUCCESS) {
        return false;
    }
    
    if (ma_device_start(&g_device) != MA_SUCCESS) {
        ma_device_uninit(&g_device);
        return false;
    }
    
    g_device_iniciado = true;
    return true;
}

// =============================================================================
// GERAR SOM DE PIANO (para tocar uma vez - com envelope ADSR)
// =============================================================================
Som* gerar_som_piano(float frequencia, int duracao_ms) {
    Som* som = new Som();
    som->frequencia = frequencia;
    som->duracao_ms = duracao_ms;
    
    int num_samples = (SAMPLE_RATE * duracao_ms) / 1000;
    som->samples.resize(num_samples);
    
    // Envelope ADSR
    int attack = SAMPLE_RATE * 10 / 1000;
    int decay = SAMPLE_RATE * 100 / 1000;
    int release = SAMPLE_RATE * 200 / 1000;
    float sustain_level = 0.6f;
    
    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        
        // Onda com harmônicos
        float wave = 0.0f;
        wave += 1.0f * sinf(2.0f * M_PI * frequencia * t);
        wave += 0.5f * sinf(2.0f * M_PI * frequencia * 2.0f * t);
        wave += 0.25f * sinf(2.0f * M_PI * frequencia * 3.0f * t);
        wave += 0.125f * sinf(2.0f * M_PI * frequencia * 4.0f * t);
        wave /= 1.875f;
        
        // Envelope ADSR
        float envelope = 0.0f;
        if (i < attack) {
            envelope = (float)i / attack;
        } else if (i < attack + decay) {
            float decay_pos = (float)(i - attack) / decay;
            envelope = 1.0f - (1.0f - sustain_level) * decay_pos;
        } else if (i < num_samples - release) {
            envelope = sustain_level;
        } else {
            float release_pos = (float)(i - (num_samples - release)) / release;
            envelope = sustain_level * (1.0f - release_pos);
        }
        
        som->samples[i] = wave * envelope * 0.3f;
    }
    
    return som;
}

// =============================================================================
// GERAR SOM CONTÍNUO (para loop - sem envelope, só onda pura)
// =============================================================================
Som* gerar_som_continuo(float frequencia) {
    Som* som = new Som();
    som->frequencia = frequencia;
    som->duracao_ms = 0;
    
    // Gera exatamente um período da onda para loop perfeito
    int samples_por_periodo = (int)(SAMPLE_RATE / frequencia);
    if (samples_por_periodo < 100) samples_por_periodo = 100;
    
    som->samples.resize(samples_por_periodo);
    
    for (int i = 0; i < samples_por_periodo; i++) {
        float t = (float)i / SAMPLE_RATE;
        
        // Onda com harmônicos
        float wave = 0.0f;
        wave += 1.0f * sinf(2.0f * M_PI * frequencia * t);
        wave += 0.5f * sinf(2.0f * M_PI * frequencia * 2.0f * t);
        wave += 0.25f * sinf(2.0f * M_PI * frequencia * 3.0f * t);
        wave += 0.125f * sinf(2.0f * M_PI * frequencia * 4.0f * t);
        wave /= 1.875f;
        
        som->samples[i] = wave * 0.3f;
    }
    
    return som;
}

// =============================================================================
// FUNÇÕES EXPORTADAS
// =============================================================================

// sint(frequencia, duracao_ms) -> id do som
JP_EXPORT JPValor jp_miniaudio_sint(JPValor* args, int numArgs) {
    if (!iniciar_audio()) {
        return jp_int(-1);
    }
    
    double freq = get_double(args, 0, numArgs);
    int duracao = (int)get_int(args, 1, numArgs);
    
    if (freq <= 0) freq = 440.0;
    if (duracao <= 0) duracao = 300;
    
    Som* som = gerar_som_piano((float)freq, duracao);
    g_sons.push_back(som);
    
    return jp_int(g_sons.size() - 1);
}

// sint_loop(frequencia) -> id do som (para usar com tocar_loop)
JP_EXPORT JPValor jp_miniaudio_sint_loop(JPValor* args, int numArgs) {
    if (!iniciar_audio()) {
        return jp_int(-1);
    }
    
    double freq = get_double(args, 0, numArgs);
    if (freq <= 0) freq = 440.0;
    
    Som* som = gerar_som_continuo((float)freq);
    g_sons.push_back(som);
    
    return jp_int(g_sons.size() - 1);
}

// tocar(id) -> bool
JP_EXPORT JPValor jp_miniaudio_tocar(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    
    if (id < 0 || id >= (int64_t)g_sons.size()) {
        return jp_bool(false);
    }
    
    Som* som = g_sons[id];
    if (!som || som->samples.empty()) {
        return jp_bool(false);
    }
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_play_buffer = som->samples.data();
        g_play_length = som->samples.size();
        g_play_position = 0;
        g_playing = true;
        g_looping = false;
        g_releasing = false;
        g_release_volume = 1.0f;
        g_current_sound_id = id;
    }
    
    return jp_bool(true);
}

// tocar_loop(id) -> bool
JP_EXPORT JPValor jp_miniaudio_tocar_loop(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    
    if (id < 0 || id >= (int64_t)g_sons.size()) {
        return jp_bool(false);
    }
    
    Som* som = g_sons[id];
    if (!som || som->samples.empty()) {
        return jp_bool(false);
    }
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        // Só inicia se não estiver tocando este som
        if (g_current_sound_id != id || !g_playing) {
            g_play_buffer = som->samples.data();
            g_play_length = som->samples.size();
            g_play_position = 0;
            g_playing = true;
            g_looping = true;
            g_releasing = false;
            g_release_volume = 1.0f;
            g_current_sound_id = id;
        }
    }
    
    return jp_bool(true);
}

// tocando() -> bool
JP_EXPORT JPValor jp_miniaudio_tocando(JPValor* args, int numArgs) {
    (void)args;
    (void)numArgs;
    return jp_bool(g_playing);
}

// parar(id) -> bool
JP_EXPORT JPValor jp_miniaudio_parar(JPValor* args, int numArgs) {
    int64_t id = get_int(args, 0, numArgs);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (g_current_sound_id == id && g_playing) {
        g_releasing = true;
        g_looping = false;
    }
    
    return jp_bool(true);
}

// parar_todos() -> bool
JP_EXPORT JPValor jp_miniaudio_parar_todos(JPValor* args, int numArgs) {
    (void)args;
    (void)numArgs;
    
    std::lock_guard<std::mutex> lock(g_mutex);
    g_playing = false;
    g_looping = false;
    g_releasing = false;
    g_release_volume = 1.0f;
    g_play_position = 0;
    g_current_sound_id = -1;
    
    return jp_bool(true);
}

// finalizar() -> bool
JP_EXPORT JPValor jp_miniaudio_finalizar(JPValor* args, int numArgs) {
    (void)args;
    (void)numArgs;
    
    if (g_device_iniciado) {
        ma_device_uninit(&g_device);
        g_device_iniciado = false;
    }
    
    for (Som* som : g_sons) {
        delete som;
    }
    g_sons.clear();
    
    return jp_bool(true);
}