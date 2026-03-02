// codegen_nativo.hpp
// Carregamento de bibliotecas nativas C++ via JSON — unificado Windows/Linux via PlatformDefs
// Registra tipos, coleta .obj/.o/.jpd, libs e lib_paths extras para linkagem

// ======================================================================
// PROCESSAMENTO DE NATIVO/IMPORTAR
// ======================================================================
//
// Fluxo:
//   importar svs             → lê bibliotecas/svs/svs.json, campo "tipo" decide:
//                               "dinamico" → svs.jpd (Win) / libsvs.jpd (Linux)
//                               "estatico" → svs.obj (Win) / svs.o (Linux)
//   importar ola.jpd         → força dinâmico: bibliotecas/ola/ola.json + ola.jpd
//   importar "path/svs"      → lê path/svs.json, campo "tipo" decide
//   importar "path/ola.jpd"  → força dinâmico: path/ola.json + path/ola.jpd
//
// Campo "tipo" no JSON:
//   "dinamico" → <nome>.jpd (Windows) / lib<nome>.jpd (Linux)
//   "estatico" → <nome>.obj (Windows) / <nome>.o (Linux)
//   ausente    → assume estático (compatibilidade com JSONs antigos)
//
// ======================================================================

void emit_nativo(const NativoStmt& node) {
    std::string lib_dir = node.lib_path;

    // Verificar se tem extensão .jpd (força dinâmico)
    bool has_jpd_ext = (lib_dir.size() > 4 &&
                        lib_dir.substr(lib_dir.size() - 4) == ".jpd");

    // Extrair nome base e diretório
    std::string lib_name;

    if (has_jpd_ext) {
        // Remover extensão .jpd
        std::string without_ext = lib_dir.substr(0, lib_dir.size() - 4);
        size_t last_sep = without_ext.find_last_of("/\\");
        if (last_sep != std::string::npos) {
            lib_name = without_ext.substr(last_sep + 1);
            lib_dir = without_ext.substr(0, last_sep);
        } else {
            lib_name = without_ext;
            lib_dir = "bibliotecas/" + lib_name;
        }
    } else {
        // Sem extensão: extrair nome da última parte do path
        size_t last_sep = lib_dir.find_last_of("/\\");
        if (last_sep != std::string::npos) {
            lib_name = lib_dir.substr(last_sep + 1);
        } else {
            lib_name = lib_dir;
            lib_dir = "bibliotecas/" + lib_name;
        }
    }

    // Montar caminho do JSON
    std::string json_path = lib_dir + "/" + lib_name + ".json";

    // Tentar relativo ao base_dir se fornecido
    if (!base_dir_.empty()) {
        std::string try_json = base_dir_ + "/" + json_path;
        std::ifstream test(try_json);
        if (test.is_open()) {
            json_path = try_json;
            lib_dir = base_dir_ + "/" + lib_dir;
        }
    }

    // Ler JSON
    std::ifstream json_file(json_path);
    if (!json_file.is_open()) {
        std::cerr << "Aviso: Não encontrou '" << json_path << "'" << std::endl;
        for (auto& fname : node.functions) {
            if (!emitter_.has_symbol(fname)) {
                emitter_.add_extern_symbol(fname);
            }
        }
        return;
    }

    std::string json_content((std::istreambuf_iterator<char>(json_file)),
                              std::istreambuf_iterator<char>());
    json_file.close();

    // Determinar tipo de linkagem
    bool is_dynamic;
    if (has_jpd_ext) {
        is_dynamic = true;
    } else {
        is_dynamic = parse_lib_json_tipo(json_content);
    }

    // Montar caminho do binário (plataforma decide extensão e prefixo)
    std::string bin_path;
    if (is_dynamic) {
        if constexpr (PlatformDefs::is_windows) {
            bin_path = lib_dir + "/" + lib_name + ".jpd";
        } else {
            bin_path = lib_dir + "/lib" + lib_name + ".jpd";
        }
    } else {
        if constexpr (PlatformDefs::is_windows) {
            bin_path = lib_dir + "/" + lib_name + ".obj";
        } else {
            bin_path = lib_dir + "/" + lib_name + ".o";
        }
    }

    // Registrar para linkagem
    if (is_dynamic) {
        extra_dll_paths_.push_back(bin_path);

        // Linux: registrar diretório para busca em runtime (-rpath)
        if constexpr (!PlatformDefs::is_windows) {
            std::string dir = bin_path;
            size_t last_sep = dir.find_last_of("/\\");
            if (last_sep != std::string::npos) {
                dir = dir.substr(0, last_sep);
            } else {
                dir = ".";
            }
            bool found = false;
            for (auto& p : extra_lib_paths_) {
                if (p == dir) { found = true; break; }
            }
            if (!found) {
                extra_lib_paths_.push_back(dir);
            }
        }
    } else {
        extra_obj_paths_.push_back(bin_path);
    }

    // Parsear funções e libs do JSON
    parse_lib_json(json_content, lib_dir);
}

// ======================================================================
// PARSER JSON: campo "tipo" → retorna true se "dinamico"
// ======================================================================

bool parse_lib_json_tipo(const std::string& json) {
    size_t tipo_key = json.find("\"tipo\"");
    if (tipo_key == std::string::npos) return false; // ausente = estático

    std::string tipo_val = extract_json_string(json, tipo_key + 6);
    return (tipo_val == "dinamico" || tipo_val == "dinâmico" ||
            tipo_val == "dll" || tipo_val == "shared");
}

// ======================================================================
// PARSER JSON SIMPLES (sem dependências)
// ======================================================================

void parse_lib_json(const std::string& json, const std::string& lib_dir = "") {
    size_t pos = 0;
    while (pos < json.size()) {
        size_t nome_key = json.find("\"nome\"", pos);
        if (nome_key == std::string::npos) break;

        std::string func_name = extract_json_string(json, nome_key + 6);
        if (func_name.empty()) break;

        size_t ret_key = json.find("\"retorno\"", nome_key);
        if (ret_key == std::string::npos) break;

        std::string ret_type_str = extract_json_string(json, ret_key + 9);

        RuntimeType ret_type = parse_type_name(ret_type_str);

        if (!emitter_.has_symbol(func_name)) {
            emitter_.add_extern_symbol(func_name);
        }

        func_return_types_[func_name] = ret_type;

        pos = ret_key + 9;
    }

    // Procura campo "libs": ["gdiplus", "X11", ...]
    parse_lib_json_libs(json);

    // Windows: procura campo "lib_paths": ["bibliotecas/opencv_jp/lib", ...]
    if constexpr (PlatformDefs::is_windows) {
        parse_lib_json_lib_paths(json, lib_dir);
    } else {
        (void)lib_dir;
    }
}

// ======================================================================
// PARSER: campo "libs" → extra_libs_
// ======================================================================

void parse_lib_json_libs(const std::string& json) {
    size_t libs_key = json.find("\"libs\"");
    if (libs_key == std::string::npos) return;

    size_t bracket = json.find('[', libs_key + 5);
    if (bracket == std::string::npos) return;

    size_t pos = bracket + 1;
    while (pos < json.size()) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
               json[pos] == '\n' || json[pos] == '\r' || json[pos] == ',')) {
            pos++;
        }

        if (pos >= json.size() || json[pos] == ']') break;

        if (json[pos] == '"') {
            pos++;
            std::string lib_name;
            while (pos < json.size() && json[pos] != '"') {
                lib_name += json[pos];
                pos++;
            }
            if (pos < json.size()) pos++;

            if (!lib_name.empty()) {
                bool found = false;
                for (auto& l : extra_libs_) {
                    if (l == lib_name) { found = true; break; }
                }
                if (!found) {
                    extra_libs_.push_back(lib_name);
                }
            }
        } else {
            pos++;
        }
    }
}

// ======================================================================
// PARSER: campo "lib_paths" → extra_lib_paths_ (Windows)
// ======================================================================

void parse_lib_json_lib_paths(const std::string& json, const std::string& lib_dir) {
    size_t key = json.find("\"lib_paths\"");
    if (key == std::string::npos) return;

    size_t bracket = json.find('[', key + 10);
    if (bracket == std::string::npos) return;

    size_t pos = bracket + 1;
    while (pos < json.size()) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
               json[pos] == '\n' || json[pos] == '\r' || json[pos] == ',')) {
            pos++;
        }

        if (pos >= json.size() || json[pos] == ']') break;

        if (json[pos] == '"') {
            pos++;
            std::string path;
            while (pos < json.size() && json[pos] != '"') {
                path += json[pos];
                pos++;
            }
            if (pos < json.size()) pos++;

            if (!path.empty()) {
                bool found = false;
                for (auto& p : extra_lib_paths_) {
                    if (p == path) { found = true; break; }
                }
                if (!found) {
                    extra_lib_paths_.push_back(path);
                }
            }
        } else {
            pos++;
        }
    }

    (void)lib_dir;
}

// ======================================================================
// HELPERS JSON
// ======================================================================

std::string extract_json_string(const std::string& json, size_t pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' ||
           json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
        }
        result += json[pos];
        pos++;
    }
    return result;
}

RuntimeType parse_type_name(const std::string& name) {
    if (name == "texto")    return RuntimeType::String;
    if (name == "inteiro")  return RuntimeType::Int;
    if (name == "float")    return RuntimeType::Float;
    if (name == "bool")     return RuntimeType::Bool;
    return RuntimeType::Unknown;
}
