// jpc_runtime.hpp
// Código C do runtime gerado pelo compilador JPLang

#ifndef JPC_RUNTIME_HPP
#define JPC_RUNTIME_HPP

#include <string>

namespace JPCRuntime {

// Helpers básicos
inline const char* HELPERS = R"(// === HELPERS ===
char* to_s(JPValor v) {
    return jp_to_string(v);
}

int is_true(JPValor v) {
    return jp_is_true(v);
}

JPValor soma(JPValor a, JPValor b) {
    if (a.tipo == JP_TIPO_INT && b.tipo == JP_TIPO_INT) {
        return jp_int(a.valor.inteiro + b.valor.inteiro);
    }
    if ((a.tipo == JP_TIPO_INT || a.tipo == JP_TIPO_DOUBLE) && 
        (b.tipo == JP_TIPO_INT || b.tipo == JP_TIPO_DOUBLE)) {
        return jp_double(jp_get_double(a) + jp_get_double(b));
    }
    char* sa = to_s(a);
    char* sb = to_s(b);
    size_t len = strlen(sa) + strlen(sb) + 1;
    char* result = (char*)malloc(len);
    strcpy(result, sa);
    strcat(result, sb);
    free(sa); free(sb);
    return jp_string_nocopy(result);
}

JPValor subtracao(JPValor a, JPValor b) {
    if (a.tipo == JP_TIPO_INT && b.tipo == JP_TIPO_INT)
        return jp_int(a.valor.inteiro - b.valor.inteiro);
    return jp_double(jp_get_double(a) - jp_get_double(b));
}

JPValor multiplicacao(JPValor a, JPValor b) {
    if (a.tipo == JP_TIPO_INT && b.tipo == JP_TIPO_INT)
        return jp_int(a.valor.inteiro * b.valor.inteiro);
    return jp_double(jp_get_double(a) * jp_get_double(b));
}

JPValor divisao(JPValor a, JPValor b) {
    double db = jp_get_double(b);
    if (db == 0) {
        printf("[ERRO] Divisao por zero!\n");
        return jp_int(0);
    }
    if (a.tipo == JP_TIPO_INT && b.tipo == JP_TIPO_INT)
        return jp_int(a.valor.inteiro / b.valor.inteiro);
    return jp_double(jp_get_double(a) / db);
}

JPValor modulo(JPValor a, JPValor b) {
    int64_t ib = jp_get_int(b);
    if (ib == 0) {
        printf("[ERRO] Modulo por zero!\n");
        return jp_int(0);
    }
    return jp_int(jp_get_int(a) % ib);
}

JPValor igual(JPValor a, JPValor b) {
    if (a.tipo == JP_TIPO_INT && b.tipo == JP_TIPO_INT)
        return jp_bool(a.valor.inteiro == b.valor.inteiro);
    if ((a.tipo == JP_TIPO_INT || a.tipo == JP_TIPO_DOUBLE) &&
        (b.tipo == JP_TIPO_INT || b.tipo == JP_TIPO_DOUBLE))
        return jp_bool(jp_get_double(a) == jp_get_double(b));
    char* sa = to_s(a);
    char* sb = to_s(b);
    int r = strcmp(sa, sb) == 0;
    free(sa); free(sb);
    return jp_bool(r);
}

JPValor diferente(JPValor a, JPValor b) {
    JPValor eq = igual(a, b);
    return jp_bool(!eq.valor.booleano);
}

JPValor maior(JPValor a, JPValor b) {
    return jp_bool(jp_get_double(a) > jp_get_double(b));
}

JPValor menor(JPValor a, JPValor b) {
    return jp_bool(jp_get_double(a) < jp_get_double(b));
}

JPValor maior_igual(JPValor a, JPValor b) {
    return jp_bool(jp_get_double(a) >= jp_get_double(b));
}

JPValor menor_igual(JPValor a, JPValor b) {
    return jp_bool(jp_get_double(a) <= jp_get_double(b));
}

JPValor logico_e(JPValor a, JPValor b) {
    return jp_bool(is_true(a) && is_true(b));
}

JPValor logico_ou(JPValor a, JPValor b) {
    return jp_bool(is_true(a) || is_true(b));
}

void imprimir(JPValor v) {
    char* s = to_s(v);
    printf("%s", s);
    fflush(stdout);
    free(s);
}

void imprimir_ln(JPValor v) {
    char* s = to_s(v);
    printf("%s\n", s);
    free(s);
}

JPValor ler_entrada(JPValor prompt) {
    char* p = to_s(prompt);
    printf("%s", p);
    fflush(stdout);
    free(p);
    
    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
        return jp_string(buffer);
    }
    return jp_string("");
}

JPValor converter_int(JPValor v) {
    return jp_int(jp_get_int(v));
}

JPValor converter_double(JPValor v) {
    return jp_double(jp_get_double(v));
}

JPValor converter_string(JPValor v) {
    char* s = to_s(v);
    return jp_string_nocopy(s);
}

JPValor converter_bool(JPValor v) {
    return jp_bool(is_true(v));
}

JPValor tipo_de(JPValor v) {
    switch (v.tipo) {
        case JP_TIPO_INT:    return jp_string("inteiro");
        case JP_TIPO_DOUBLE: return jp_string("decimal");
        case JP_TIPO_STRING: return jp_string("texto");
        case JP_TIPO_BOOL:   return jp_string("booleano");
        case JP_TIPO_LISTA:  return jp_string("lista");
        case JP_TIPO_OBJETO: return jp_string("objeto");
        case JP_TIPO_PONTEIRO: return jp_string("ponteiro");
        default:             return jp_string("nulo");
    }
}

)";

// Código de listas
inline const char* LISTAS = R"(// === LISTAS ===
#define MAX_LISTAS 1024
#define MAX_ELEMENTOS_LISTA 4096

typedef struct {
    JPValor* elementos;
    int tamanho;
    int capacidade;
    int id;
} JPLista;

JPLista listas[MAX_LISTAS];
int num_listas = 0;
int proximo_id_lista = -1;

int criar_lista() {
    if (num_listas >= MAX_LISTAS) {
        printf("[ERRO] Limite de listas excedido!\n");
        return 0;
    }
    JPLista* l = &listas[num_listas];
    l->elementos = (JPValor*)malloc(16 * sizeof(JPValor));
    l->tamanho = 0;
    l->capacidade = 16;
    l->id = proximo_id_lista--;
    num_listas++;
    return l->id;
}

JPLista* obter_lista(int id) {
    for (int i = 0; i < num_listas; i++) {
        if (listas[i].id == id) return &listas[i];
    }
    return NULL;
}

void lista_adicionar(JPLista* l, JPValor v) {
    if (!l) return;
    if (l->tamanho >= l->capacidade) {
        l->capacidade *= 2;
        l->elementos = (JPValor*)realloc(l->elementos, l->capacidade * sizeof(JPValor));
    }
    l->elementos[l->tamanho++] = jp_copiar(v);
}

JPValor lista_obter(JPLista* l, int idx) {
    if (!l || idx < 0 || idx >= l->tamanho) {
        printf("[ERRO] Indice fora dos limites: %d\n", idx);
        return jp_nulo();
    }
    return jp_copiar(l->elementos[idx]);
}

void lista_definir(JPLista* l, int idx, JPValor v) {
    if (!l) return;
    if (idx == l->tamanho) {
        lista_adicionar(l, v);
        return;
    }
    if (idx < 0 || idx > l->tamanho) {
        printf("[ERRO] Indice fora dos limites: %d\n", idx);
        return;
    }
    l->elementos[idx] = jp_copiar(v);
}

void lista_remover(JPLista* l, int idx) {
    if (!l || idx < 0 || idx >= l->tamanho) {
        printf("[ERRO] Indice fora dos limites: %d\n", idx);
        return;
    }
    for (int i = idx; i < l->tamanho - 1; i++) {
        l->elementos[i] = l->elementos[i + 1];
    }
    l->tamanho--;
}

int lista_tamanho(JPLista* l) {
    return l ? l->tamanho : 0;
}

char* valor_para_str(JPValor v);

void lista_exibir(JPLista* l) {
    if (!l) {
        printf("[]\n");
        return;
    }
    printf("[");
    for (int i = 0; i < l->tamanho; i++) {
        if (i > 0) printf(", ");
        JPValor v = l->elementos[i];
        if (v.tipo == JP_TIPO_STRING) {
            printf("\"%s\"", v.valor.texto ? v.valor.texto : "");
        } else {
            char* s = to_s(v);
            printf("%s", s);
            free(s);
        }
    }
    printf("]\n");
}

char* valor_para_str(JPValor v) {
    if (v.tipo == JP_TIPO_INT && v.valor.inteiro < 0) {
        JPLista* l = obter_lista((int)v.valor.inteiro);
        if (l) {
            size_t len = 2;
            for (int i = 0; i < l->tamanho; i++) {
                if (i > 0) len += 2;
                char* s = to_s(l->elementos[i]);
                len += strlen(s) + (l->elementos[i].tipo == JP_TIPO_STRING ? 2 : 0);
                free(s);
            }
            char* result = (char*)malloc(len + 1);
            char* p = result;
            *p++ = '[';
            for (int i = 0; i < l->tamanho; i++) {
                if (i > 0) { *p++ = ','; *p++ = ' '; }
                if (l->elementos[i].tipo == JP_TIPO_STRING) {
                    *p++ = '"';
                    char* s = to_s(l->elementos[i]);
                    strcpy(p, s);
                    p += strlen(s);
                    free(s);
                    *p++ = '"';
                } else {
                    char* s = to_s(l->elementos[i]);
                    strcpy(p, s);
                    p += strlen(s);
                    free(s);
                }
            }
            *p++ = ']';
            *p = '\0';
            return result;
        }
    }
    return to_s(v);
}

void imprimir_valor(JPValor v) {
    char* s = valor_para_str(v);
    printf("%s", s);
    fflush(stdout);
    free(s);
}

void imprimir_valor_ln(JPValor v) {
    char* s = valor_para_str(v);
    printf("%s\n", s);
    free(s);
}

)";

// Código de objetos
inline const char* OBJETOS = R"(// === OBJETOS ===
#define MAX_OBJETOS 1024
#define MAX_ATTRS_POR_OBJETO 64

typedef struct {
    char nome[256];
    JPValor valor;
} JPAtributo;

typedef struct {
    char classe[256];
    JPAtributo attrs[MAX_ATTRS_POR_OBJETO];
    int num_attrs;
    int id;
} JPObjeto;

JPObjeto objetos[MAX_OBJETOS];
int num_objetos = 0;
int proximo_id_objeto = -1000000;

int criar_objeto() {
    if (num_objetos >= MAX_OBJETOS) {
        printf("[ERRO] Limite de objetos excedido!\n");
        return 0;
    }
    JPObjeto* o = &objetos[num_objetos];
    o->num_attrs = 0;
    o->classe[0] = '\0';
    o->id = proximo_id_objeto--;
    num_objetos++;
    return o->id;
}

JPObjeto* obter_objeto(int id) {
    for (int i = 0; i < num_objetos; i++) {
        if (objetos[i].id == id) return &objetos[i];
    }
    return NULL;
}

void objeto_set_classe(JPObjeto* o, const char* classe) {
    if (!o) return;
    strncpy(o->classe, classe, 255);
    o->classe[255] = '\0';
}

void objeto_set_attr(JPObjeto* o, const char* nome, JPValor valor) {
    if (!o) return;
    
    // Procura atributo existente
    for (int i = 0; i < o->num_attrs; i++) {
        if (strcmp(o->attrs[i].nome, nome) == 0) {
            o->attrs[i].valor = jp_copiar(valor);
            return;
        }
    }
    
    // Adiciona novo atributo
    if (o->num_attrs < MAX_ATTRS_POR_OBJETO) {
        strncpy(o->attrs[o->num_attrs].nome, nome, 255);
        o->attrs[o->num_attrs].nome[255] = '\0';
        o->attrs[o->num_attrs].valor = jp_copiar(valor);
        o->num_attrs++;
    } else {
        printf("[ERRO] Limite de atributos excedido para objeto!\n");
    }
}

JPValor objeto_get_attr(JPObjeto* o, const char* nome) {
    if (!o) return jp_nulo();
    
    for (int i = 0; i < o->num_attrs; i++) {
        if (strcmp(o->attrs[i].nome, nome) == 0) {
            return jp_copiar(o->attrs[i].valor);
        }
    }
    
    return jp_nulo();
}

)";

} // namespace JPCRuntime

#endif // JPC_RUNTIME_HPP