#include "llama.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <algorithm>

// === ESTRUTURAS DO JPLANG ===
#ifdef _WIN32
    #define JP_EXPORT __declspec(dllexport)
#else
    #define JP_EXPORT __attribute__((visibility("default")))
#endif

typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        void* ponteiro;
    } valor;
} JPValor;

// Helpers
JPValor jp_ret_nulo() { JPValor v; v.tipo = JP_TIPO_NULO; v.valor.inteiro = 0; return v; }
JPValor jp_ret_ptr(void* p) { JPValor v; v.tipo = JP_TIPO_PONTEIRO; v.valor.ponteiro = p; return v; }
JPValor jp_ret_string(const char* s) {
    JPValor v; v.tipo = JP_TIPO_STRING;
    if (s) {
        v.valor.texto = (char*)malloc(strlen(s) + 1);
        strcpy(v.valor.texto, s);
    } else { v.valor.texto = NULL; }
    return v;
}

// === ESTADO (Atualizado com Vocab) ===
struct EstadoIA {
    llama_model* model = nullptr;
    const llama_vocab* vocab = nullptr; // Novo ponteiro necessario
    llama_context* ctx = nullptr;
    llama_batch batch;
    int n_ctx;
    int n_past = 0; 
};

// Funcao auxiliar interna para adicionar ao batch manualmente
// (Substitui a llama_batch_add que foi removida da lib)
void batch_add(llama_batch& batch, llama_token id, int pos, std::vector<int32_t>& seq_ids, bool logits) {
    batch.token[batch.n_tokens] = id;
    batch.pos[batch.n_tokens] = pos;
    batch.n_seq_id[batch.n_tokens] = seq_ids.size();
    for (size_t i = 0; i < seq_ids.size(); ++i) {
        batch.seq_id[batch.n_tokens][i] = seq_ids[i];
    }
    batch.logits[batch.n_tokens] = logits;
    batch.n_tokens++;
}

extern "C" {
    JP_EXPORT JPValor jp_llama_init(JPValor* args, int n) {
        llama_backend_init();
        return jp_ret_nulo();
    }

    JP_EXPORT JPValor jp_llama_carregar(JPValor* args, int n) {
        if (n < 1 || args[0].tipo != JP_TIPO_STRING) return jp_ret_nulo();
        
        auto mparams = llama_model_default_params();
        mparams.n_gpu_layers = 0; 

        // 1. Carrega Modelo
        llama_model* model = llama_model_load_from_file(args[0].valor.texto, mparams);
        if (!model) return jp_ret_nulo();

        // 2. Pega o Vocabulario (Novo passo obrigatorio)
        const llama_vocab* vocab = llama_model_get_vocab(model);

        auto cparams = llama_context_default_params();
        cparams.n_ctx = 2048; 

        // 3. Cria contexto (Usando nova API se disponivel, senao fallback)
        // Nota: Se sua versao do header ainda tiver a deprecated, vai avisar mas funcionar.
        // Se removeu, use llama_init_from_model
        llama_context* ctx = llama_new_context_with_model(model, cparams);
        if (!ctx) {
            llama_model_free(model);
            return jp_ret_nulo();
        }

        EstadoIA* estado = new EstadoIA();
        estado->model = model;
        estado->vocab = vocab; // Salva o vocab
        estado->ctx = ctx;
        estado->n_ctx = llama_n_ctx(ctx);
        estado->batch = llama_batch_init(2048, 0, 1); 
        estado->n_past = 0;

        return jp_ret_ptr(estado);
    }

    JP_EXPORT JPValor jp_llama_prompt(JPValor* args, int n) {
        if (n < 2 || args[0].tipo != JP_TIPO_PONTEIRO || args[1].tipo != JP_TIPO_STRING) 
            return jp_ret_nulo();

        EstadoIA* st = (EstadoIA*)args[0].valor.ponteiro;
        const char* text = args[1].valor.texto;

        // Tokeniza usando o VOCAB (Correcao do erro de conversao)
        std::vector<llama_token> tokens;
        tokens.resize(st->n_ctx);
        // Negativo no tamanho indica que queremos contar
        int n_tokens = llama_tokenize(st->vocab, text, strlen(text), tokens.data(), tokens.size(), true, false);
        if (n_tokens < 0) {
            tokens.resize(-n_tokens);
            n_tokens = llama_tokenize(st->vocab, text, strlen(text), tokens.data(), tokens.size(), true, false);
        } else {
            tokens.resize(n_tokens);
        }

        // Limpa batch manualmente
        st->batch.n_tokens = 0;

        // Adiciona tokens ao batch
        std::vector<int32_t> seq_id = {0};
        for (int i = 0; i < n_tokens; i++) {
            batch_add(st->batch, tokens[i], st->n_past, seq_id, false);
            st->n_past++;
        }
        
        // O ultimo token precisa gerar logits
        if (st->batch.n_tokens > 0) {
            st->batch.logits[st->batch.n_tokens - 1] = true;
        }

        if (llama_decode(st->ctx, st->batch) != 0) {
            return jp_ret_string("ERRO_DECODE");
        }

        return jp_ret_nulo();
    }

    JP_EXPORT JPValor jp_llama_gerar_token(JPValor* args, int n) {
        if (n < 1 || args[0].tipo != JP_TIPO_PONTEIRO) return jp_ret_string("");
        
        EstadoIA* st = (EstadoIA*)args[0].valor.ponteiro;

        // Sampling
        auto* logits = llama_get_logits_ith(st->ctx, st->batch.n_tokens - 1);
        
        // Correcao: Usa funcao do vocabulario
        int n_vocab = llama_vocab_n_tokens(st->vocab);
        
        llama_token token_id = 0;
        float max_prob = -1e9;

        for (int i = 0; i < n_vocab; i++) {
            if (logits[i] > max_prob) {
                max_prob = logits[i];
                token_id = i;
            }
        }

        // Correcao: Verifica EOS no vocabulario
        if (token_id == llama_vocab_eos(st->vocab)) {
            return jp_ret_string("[EOS]");
        }

        // Correcao: Token to Piece com vocab
        char buf[256];
        int n_chars = llama_token_to_piece(st->vocab, token_id, buf, sizeof(buf), 0, true);
        if (n_chars < 0) {
             // Buffer pequeno, ignorar por seguranca ou tratar depois
             buf[0] = '\0'; 
        } else {
             buf[n_chars] = '\0';
        }

        // Prepara proxima rodada
        st->batch.n_tokens = 0;
        std::vector<int32_t> seq_id = {0};
        batch_add(st->batch, token_id, st->n_past, seq_id, true);
        st->n_past++;

        if (llama_decode(st->ctx, st->batch) != 0) {
            return jp_ret_string("");
        }

        return jp_ret_string(buf);
    }

    JP_EXPORT JPValor jp_llama_liberar(JPValor* args, int n) {
        if (n > 0 && args[0].tipo == JP_TIPO_PONTEIRO) {
            EstadoIA* st = (EstadoIA*)args[0].valor.ponteiro;
            llama_batch_free(st->batch);
            llama_free(st->ctx);
            llama_model_free(st->model);
            delete st;
        }
        return jp_ret_nulo();
    }
    
    JP_EXPORT JPValor jp_llama_versao(JPValor* args, int n) {
        return jp_ret_string(llama_print_system_info());
    }
}