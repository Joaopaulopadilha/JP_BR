// sqlite.cpp
// Biblioteca SQLite para JPLang (Portável Windows/Linux)

#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <cstdint>

extern "C" {
    #include "src/sqlite3.h"
}

// Plataforma
#ifdef _WIN32
    #define EXPORTAR extern "C" __declspec(dllexport)
#else
    #define EXPORTAR extern "C" __attribute__((visibility("default")))
#endif

// === TIPOS ABI-COMPATÍVEIS (C puro) ===
typedef enum {
    JP_TIPO_NULO = 0,
    JP_TIPO_INT = 1,
    JP_TIPO_DOUBLE = 2,
    JP_TIPO_STRING = 3,
    JP_TIPO_BOOL = 4,
    JP_TIPO_OBJETO = 5,
    JP_TIPO_LISTA = 6,
    JP_TIPO_PONTEIRO = 7
} JPTipo;

typedef struct {
    JPTipo tipo;
    union {
        int64_t inteiro;
        double decimal;
        char* texto;
        int booleano;
        void* objeto;
        void* lista;
        void* ponteiro;
    } valor;
} JPValor;

// === HELPERS PARA JPValor ===
static int jp_get_int(const JPValor& v) {
    switch (v.tipo) {
        case JP_TIPO_INT: return (int)v.valor.inteiro;
        case JP_TIPO_DOUBLE: return (int)v.valor.decimal;
        default: return 0;
    }
}

static double jp_get_double(const JPValor& v) {
    switch (v.tipo) {
        case JP_TIPO_DOUBLE: return v.valor.decimal;
        case JP_TIPO_INT: return (double)v.valor.inteiro;
        default: return 0.0;
    }
}

static std::string jp_get_str(const JPValor& v) {
    if (v.tipo == JP_TIPO_STRING && v.valor.texto) return std::string(v.valor.texto);
    if (v.tipo == JP_TIPO_INT) return std::to_string(v.valor.inteiro);
    if (v.tipo == JP_TIPO_DOUBLE) return std::to_string(v.valor.decimal);
    if (v.tipo == JP_TIPO_BOOL) return v.valor.booleano ? "verdadeiro" : "falso";
    return "";
}

// === HELPERS PARA CRIAR JPValor ===
static JPValor jp_int(int64_t i) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_INT;
    v.valor.inteiro = i;
    return v;
}

static JPValor jp_double(double d) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_DOUBLE;
    v.valor.decimal = d;
    return v;
}

static JPValor jp_bool(bool b) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_BOOL;
    v.valor.booleano = b ? 1 : 0;
    return v;
}

static JPValor jp_string(const std::string& s) {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_STRING;
    v.valor.texto = (char*)malloc(s.size() + 1);
    if (v.valor.texto) memcpy(v.valor.texto, s.c_str(), s.size() + 1);
    return v;
}

static JPValor jp_nulo() {
    JPValor v; memset(&v, 0, sizeof(v));
    v.tipo = JP_TIPO_NULO;
    return v;
}

// === GERENCIADOR DE CONEXÕES ===
class GerenciadorSQLite {
private:
    std::unordered_map<int, sqlite3*> conexoes;
    std::unordered_map<int, sqlite3_stmt*> statements;
    int proximo_db_id = 1;
    int proximo_stmt_id = 1;
    std::string ultimo_erro;

public:
    static GerenciadorSQLite& instancia() {
        static GerenciadorSQLite inst;
        return inst;
    }

    // Abre banco de dados
    int abrir(const std::string& caminho) {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(caminho.c_str(), &db);
        
        if (rc != SQLITE_OK) {
            ultimo_erro = sqlite3_errmsg(db);
            sqlite3_close(db);
            return -1;
        }
        
        int id = proximo_db_id++;
        conexoes[id] = db;
        return id;
    }

    // Fecha banco de dados
    bool fechar(int db_id) {
        auto it = conexoes.find(db_id);
        if (it == conexoes.end()) return false;
        
        sqlite3_close(it->second);
        conexoes.erase(it);
        return true;
    }

    // Executa SQL sem retorno (CREATE, INSERT, UPDATE, DELETE)
    bool executar(int db_id, const std::string& sql) {
        auto it = conexoes.find(db_id);
        if (it == conexoes.end()) {
            ultimo_erro = "Banco de dados não encontrado";
            return false;
        }
        
        char* erro = nullptr;
        int rc = sqlite3_exec(it->second, sql.c_str(), nullptr, nullptr, &erro);
        
        if (rc != SQLITE_OK) {
            if (erro) {
                ultimo_erro = erro;
                sqlite3_free(erro);
            }
            return false;
        }
        
        return true;
    }

    // Consulta SQL com retorno (SELECT)
    // Retorna resultado como string no formato: "col1,col2,col3\nval1,val2,val3\n..."
    std::string consultar(int db_id, const std::string& sql) {
        auto it = conexoes.find(db_id);
        if (it == conexoes.end()) {
            ultimo_erro = "Banco de dados não encontrado";
            return "";
        }
        
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(it->second, sql.c_str(), -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            ultimo_erro = sqlite3_errmsg(it->second);
            return "";
        }
        
        std::string resultado;
        int num_colunas = sqlite3_column_count(stmt);
        
        // Cabeçalho com nomes das colunas
        for (int i = 0; i < num_colunas; i++) {
            if (i > 0) resultado += ",";
            const char* nome = sqlite3_column_name(stmt, i);
            resultado += nome ? nome : "";
        }
        resultado += "\n";
        
        // Dados
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            for (int i = 0; i < num_colunas; i++) {
                if (i > 0) resultado += ",";
                
                int tipo = sqlite3_column_type(stmt, i);
                switch (tipo) {
                    case SQLITE_INTEGER:
                        resultado += std::to_string(sqlite3_column_int64(stmt, i));
                        break;
                    case SQLITE_FLOAT:
                        resultado += std::to_string(sqlite3_column_double(stmt, i));
                        break;
                    case SQLITE_TEXT: {
                        const char* texto = (const char*)sqlite3_column_text(stmt, i);
                        resultado += texto ? texto : "";
                        break;
                    }
                    case SQLITE_NULL:
                        resultado += "NULO";
                        break;
                    default:
                        resultado += "";
                        break;
                }
            }
            resultado += "\n";
        }
        
        sqlite3_finalize(stmt);
        return resultado;
    }

    // Retorna última linha inserida
    int64_t ultimo_id(int db_id) {
        auto it = conexoes.find(db_id);
        if (it == conexoes.end()) return -1;
        return sqlite3_last_insert_rowid(it->second);
    }

    // Retorna linhas afetadas
    int linhas_afetadas(int db_id) {
        auto it = conexoes.find(db_id);
        if (it == conexoes.end()) return -1;
        return sqlite3_changes(it->second);
    }

    // Prepara statement
    int preparar(int db_id, const std::string& sql) {
        auto it = conexoes.find(db_id);
        if (it == conexoes.end()) {
            ultimo_erro = "Banco de dados não encontrado";
            return -1;
        }
        
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(it->second, sql.c_str(), -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            ultimo_erro = sqlite3_errmsg(it->second);
            return -1;
        }
        
        int id = proximo_stmt_id++;
        statements[id] = stmt;
        return id;
    }

    // Vincula inteiro ao statement
    bool vincular_int(int stmt_id, int indice, int64_t valor) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return false;
        return sqlite3_bind_int64(it->second, indice, valor) == SQLITE_OK;
    }

    // Vincula double ao statement
    bool vincular_double(int stmt_id, int indice, double valor) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return false;
        return sqlite3_bind_double(it->second, indice, valor) == SQLITE_OK;
    }

    // Vincula texto ao statement
    bool vincular_texto(int stmt_id, int indice, const std::string& valor) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return false;
        return sqlite3_bind_text(it->second, indice, valor.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }

    // Vincula nulo ao statement
    bool vincular_nulo(int stmt_id, int indice) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return false;
        return sqlite3_bind_null(it->second, indice) == SQLITE_OK;
    }

    // Executa passo do statement
    // Retorna: 0 = DONE, 1 = ROW, -1 = ERRO
    int passo(int stmt_id) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return -1;
        
        int rc = sqlite3_step(it->second);
        if (rc == SQLITE_DONE) return 0;
        if (rc == SQLITE_ROW) return 1;
        return -1;
    }

    // Obtém coluna como inteiro
    int64_t coluna_int(int stmt_id, int indice) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return 0;
        return sqlite3_column_int64(it->second, indice);
    }

    // Obtém coluna como double
    double coluna_double(int stmt_id, int indice) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return 0.0;
        return sqlite3_column_double(it->second, indice);
    }

    // Obtém coluna como texto
    std::string coluna_texto(int stmt_id, int indice) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return "";
        const char* texto = (const char*)sqlite3_column_text(it->second, indice);
        return texto ? texto : "";
    }

    // Obtém tipo da coluna
    int coluna_tipo(int stmt_id, int indice) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return -1;
        return sqlite3_column_type(it->second, indice);
    }

    // Obtém número de colunas
    int num_colunas(int stmt_id) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return 0;
        return sqlite3_column_count(it->second);
    }

    // Obtém nome da coluna
    std::string coluna_nome(int stmt_id, int indice) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return "";
        const char* nome = sqlite3_column_name(it->second, indice);
        return nome ? nome : "";
    }

    // Reseta statement para reutilizar
    bool resetar(int stmt_id) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return false;
        return sqlite3_reset(it->second) == SQLITE_OK;
    }

    // Finaliza statement
    bool finalizar(int stmt_id) {
        auto it = statements.find(stmt_id);
        if (it == statements.end()) return false;
        
        sqlite3_finalize(it->second);
        statements.erase(it);
        return true;
    }

    // Retorna último erro
    std::string erro() {
        return ultimo_erro;
    }

    // Inicia transação
    bool iniciar_transacao(int db_id) {
        return executar(db_id, "BEGIN TRANSACTION");
    }

    // Confirma transação
    bool confirmar(int db_id) {
        return executar(db_id, "COMMIT");
    }

    // Reverte transação
    bool reverter(int db_id) {
        return executar(db_id, "ROLLBACK");
    }

    // Verifica se tabela existe
    bool tabela_existe(int db_id, const std::string& nome) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + nome + "'";
        std::string resultado = consultar(db_id, sql);
        // Se tem mais que só o cabeçalho, a tabela existe
        return resultado.find('\n') != resultado.rfind('\n');
    }

    // Lista tabelas
    std::string listar_tabelas(int db_id) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
        std::string resultado = consultar(db_id, sql);
        
        // Remove cabeçalho e formata como lista separada por vírgulas
        std::string tabelas;
        size_t pos = resultado.find('\n');
        if (pos != std::string::npos) {
            resultado = resultado.substr(pos + 1);
            while ((pos = resultado.find('\n')) != std::string::npos) {
                std::string tabela = resultado.substr(0, pos);
                if (!tabela.empty()) {
                    if (!tabelas.empty()) tabelas += ",";
                    tabelas += tabela;
                }
                resultado = resultado.substr(pos + 1);
            }
        }
        return tabelas;
    }

    // Destrutor
    ~GerenciadorSQLite() {
        for (auto& par : statements) {
            sqlite3_finalize(par.second);
        }
        statements.clear();
        
        for (auto& par : conexoes) {
            sqlite3_close(par.second);
        }
        conexoes.clear();
    }
};

// =============================================================================
// === EXPORTS COM INTERFACE JPValor ===
// =============================================================================

// === CONEXÃO ===

EXPORTAR JPValor jp_sqlite_abrir(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    std::string caminho = jp_get_str(args[0]);
    return jp_int(GerenciadorSQLite::instancia().abrir(caminho));
}

EXPORTAR JPValor jp_sqlite_fechar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int db_id = jp_get_int(args[0]);
    return jp_bool(GerenciadorSQLite::instancia().fechar(db_id));
}

// === EXECUÇÃO SIMPLES ===

EXPORTAR JPValor jp_sqlite_executar(JPValor* args, int n) {
    if (n < 2) return jp_bool(false);
    int db_id = jp_get_int(args[0]);
    std::string sql = jp_get_str(args[1]);
    return jp_bool(GerenciadorSQLite::instancia().executar(db_id, sql));
}

EXPORTAR JPValor jp_sqlite_consultar(JPValor* args, int n) {
    if (n < 2) return jp_string("");
    int db_id = jp_get_int(args[0]);
    std::string sql = jp_get_str(args[1]);
    return jp_string(GerenciadorSQLite::instancia().consultar(db_id, sql));
}

EXPORTAR JPValor jp_sqlite_ultimo_id(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int db_id = jp_get_int(args[0]);
    return jp_int(GerenciadorSQLite::instancia().ultimo_id(db_id));
}

EXPORTAR JPValor jp_sqlite_linhas_afetadas(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int db_id = jp_get_int(args[0]);
    return jp_int(GerenciadorSQLite::instancia().linhas_afetadas(db_id));
}

EXPORTAR JPValor jp_sqlite_erro(JPValor* args, int n) {
    return jp_string(GerenciadorSQLite::instancia().erro());
}

// === PREPARED STATEMENTS ===

EXPORTAR JPValor jp_sqlite_preparar(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int db_id = jp_get_int(args[0]);
    std::string sql = jp_get_str(args[1]);
    return jp_int(GerenciadorSQLite::instancia().preparar(db_id, sql));
}

EXPORTAR JPValor jp_sqlite_vincular(JPValor* args, int n) {
    if (n < 3) return jp_bool(false);
    int stmt_id = jp_get_int(args[0]);
    int indice = jp_get_int(args[1]);
    
    // Detecta tipo do valor
    switch (args[2].tipo) {
        case JP_TIPO_INT:
            return jp_bool(GerenciadorSQLite::instancia().vincular_int(stmt_id, indice, args[2].valor.inteiro));
        case JP_TIPO_DOUBLE:
            return jp_bool(GerenciadorSQLite::instancia().vincular_double(stmt_id, indice, args[2].valor.decimal));
        case JP_TIPO_STRING:
            return jp_bool(GerenciadorSQLite::instancia().vincular_texto(stmt_id, indice, jp_get_str(args[2])));
        case JP_TIPO_NULO:
            return jp_bool(GerenciadorSQLite::instancia().vincular_nulo(stmt_id, indice));
        default:
            return jp_bool(GerenciadorSQLite::instancia().vincular_texto(stmt_id, indice, jp_get_str(args[2])));
    }
}

EXPORTAR JPValor jp_sqlite_passo(JPValor* args, int n) {
    if (n < 1) return jp_int(-1);
    int stmt_id = jp_get_int(args[0]);
    return jp_int(GerenciadorSQLite::instancia().passo(stmt_id));
}

EXPORTAR JPValor jp_sqlite_coluna_int(JPValor* args, int n) {
    if (n < 2) return jp_int(0);
    int stmt_id = jp_get_int(args[0]);
    int indice = jp_get_int(args[1]);
    return jp_int(GerenciadorSQLite::instancia().coluna_int(stmt_id, indice));
}

EXPORTAR JPValor jp_sqlite_coluna_double(JPValor* args, int n) {
    if (n < 2) return jp_double(0.0);
    int stmt_id = jp_get_int(args[0]);
    int indice = jp_get_int(args[1]);
    return jp_double(GerenciadorSQLite::instancia().coluna_double(stmt_id, indice));
}

EXPORTAR JPValor jp_sqlite_coluna_texto(JPValor* args, int n) {
    if (n < 2) return jp_string("");
    int stmt_id = jp_get_int(args[0]);
    int indice = jp_get_int(args[1]);
    return jp_string(GerenciadorSQLite::instancia().coluna_texto(stmt_id, indice));
}

EXPORTAR JPValor jp_sqlite_coluna_tipo(JPValor* args, int n) {
    if (n < 2) return jp_int(-1);
    int stmt_id = jp_get_int(args[0]);
    int indice = jp_get_int(args[1]);
    return jp_int(GerenciadorSQLite::instancia().coluna_tipo(stmt_id, indice));
}

EXPORTAR JPValor jp_sqlite_num_colunas(JPValor* args, int n) {
    if (n < 1) return jp_int(0);
    int stmt_id = jp_get_int(args[0]);
    return jp_int(GerenciadorSQLite::instancia().num_colunas(stmt_id));
}

EXPORTAR JPValor jp_sqlite_coluna_nome(JPValor* args, int n) {
    if (n < 2) return jp_string("");
    int stmt_id = jp_get_int(args[0]);
    int indice = jp_get_int(args[1]);
    return jp_string(GerenciadorSQLite::instancia().coluna_nome(stmt_id, indice));
}

EXPORTAR JPValor jp_sqlite_resetar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int stmt_id = jp_get_int(args[0]);
    return jp_bool(GerenciadorSQLite::instancia().resetar(stmt_id));
}

EXPORTAR JPValor jp_sqlite_finalizar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int stmt_id = jp_get_int(args[0]);
    return jp_bool(GerenciadorSQLite::instancia().finalizar(stmt_id));
}

// === TRANSAÇÕES ===

EXPORTAR JPValor jp_sqlite_iniciar_transacao(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int db_id = jp_get_int(args[0]);
    return jp_bool(GerenciadorSQLite::instancia().iniciar_transacao(db_id));
}

EXPORTAR JPValor jp_sqlite_confirmar(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int db_id = jp_get_int(args[0]);
    return jp_bool(GerenciadorSQLite::instancia().confirmar(db_id));
}

EXPORTAR JPValor jp_sqlite_reverter(JPValor* args, int n) {
    if (n < 1) return jp_bool(false);
    int db_id = jp_get_int(args[0]);
    return jp_bool(GerenciadorSQLite::instancia().reverter(db_id));
}

// === UTILITÁRIOS ===

EXPORTAR JPValor jp_sqlite_tabela_existe(JPValor* args, int n) {
    if (n < 2) return jp_bool(false);
    int db_id = jp_get_int(args[0]);
    std::string nome = jp_get_str(args[1]);
    return jp_bool(GerenciadorSQLite::instancia().tabela_existe(db_id, nome));
}

EXPORTAR JPValor jp_sqlite_listar_tabelas(JPValor* args, int n) {
    if (n < 1) return jp_string("");
    int db_id = jp_get_int(args[0]);
    return jp_string(GerenciadorSQLite::instancia().listar_tabelas(db_id));
}

EXPORTAR JPValor jp_sqlite_versao(JPValor* args, int n) {
    return jp_string(sqlite3_libversion());
}