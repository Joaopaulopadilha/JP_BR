// codegen_funcoes_nativas.hpp
// Registro de funções nativas (built-in) do JPLang — unificado Windows/Linux via PlatformDefs
// Metadados, consulta, validação e emissão x86-64

// ======================================================================
// ESTRUTURA DE INFORMAÇÃO DE FUNÇÃO NATIVA
// ======================================================================

struct NativeFuncParam {
    std::string name;
    RuntimeType type;
};

struct NativeFuncInfo {
    std::string name;
    std::vector<NativeFuncParam> params;
    RuntimeType return_type;
    int min_args;
    int max_args; // -1 = sem limite
};

// ======================================================================
// REGISTRO DE FUNÇÕES NATIVAS
// ======================================================================

std::unordered_map<std::string, NativeFuncInfo> native_funcs_;

void init_native_funcs() {
    native_funcs_["entrada"] = {
        "entrada", { {"prompt", RuntimeType::String} },
        RuntimeType::String, 0, 1
    };
    native_funcs_["inteiro"] = {
        "inteiro", { {"valor", RuntimeType::Unknown} },
        RuntimeType::Int, 1, 1
    };
    native_funcs_["decimal"] = {
        "decimal", { {"valor", RuntimeType::Unknown} },
        RuntimeType::Float, 1, 1
    };
    native_funcs_["tipo"] = {
        "tipo", { {"valor", RuntimeType::Unknown} },
        RuntimeType::String, 1, 1
    };
    native_funcs_["texto"] = {
        "texto", { {"valor", RuntimeType::Unknown} },
        RuntimeType::String, 1, 1
    };
    native_funcs_["booleano"] = {
        "booleano", { {"valor", RuntimeType::Unknown} },
        RuntimeType::Bool, 1, 1
    };
    native_funcs_["args"] = {
        "args", { {"indice", RuntimeType::Int} },
        RuntimeType::String, 1, 1
    };
    native_funcs_["num_args"] = {
        "num_args", {},
        RuntimeType::Int, 0, 0
    };
}

// ======================================================================
// CONSULTA
// ======================================================================

bool is_native_func(const std::string& name) const {
    return native_funcs_.count(name) > 0;
}

const NativeFuncInfo* get_native_func(const std::string& name) const {
    auto it = native_funcs_.find(name);
    if (it != native_funcs_.end()) return &it->second;
    return nullptr;
}

RuntimeType get_native_return_type(const std::string& name) const {
    auto it = native_funcs_.find(name);
    if (it != native_funcs_.end()) return it->second.return_type;
    return RuntimeType::Unknown;
}

// ======================================================================
// VALIDAÇÃO
// ======================================================================

void validate_native_call(const std::string& name,
                          const std::vector<std::unique_ptr<Expr>>& args) {
    auto* info = get_native_func(name);
    if (!info) return;

    int num = static_cast<int>(args.size());

    if (num < info->min_args) {
        std::cerr << "Aviso: '" << name << "' espera no mínimo "
                  << info->min_args << " argumento(s), recebeu "
                  << num << std::endl;
    }
    if (info->max_args >= 0 && num > info->max_args) {
        std::cerr << "Aviso: '" << name << "' espera no máximo "
                  << info->max_args << " argumento(s), recebeu "
                  << num << std::endl;
    }

    int check_count = std::min(num, static_cast<int>(info->params.size()));
    for (int i = 0; i < check_count; i++) {
        RuntimeType expected = info->params[i].type;
        if (expected == RuntimeType::Unknown) continue;

        RuntimeType actual = infer_expr_type(*args[i]);
        if (actual == RuntimeType::Unknown) continue;

        if (actual != expected) {
            std::cerr << "Aviso: '" << name << "' argumento "
                      << (i + 1) << " ('" << info->params[i].name
                      << "') espera " << runtime_type_name(expected)
                      << ", recebeu " << runtime_type_name(actual)
                      << std::endl;
        }
    }
}

// ======================================================================
// HELPERS
// ======================================================================

static const char* runtime_type_name(RuntimeType type) {
    switch (type) {
        case RuntimeType::Int:     return "inteiro";
        case RuntimeType::Float:   return "decimal";
        case RuntimeType::String:  return "texto";
        case RuntimeType::Bool:    return "booleano";
        case RuntimeType::Unknown: return "desconhecido";
    }
    return "desconhecido";
}

std::string translate_type_name(const std::string& internal_name) const {
    auto it = lang_config_.tipos.find(internal_name);
    if (it != lang_config_.tipos.end()) return it->second;
    return internal_name;
}

// ======================================================================
// DISPATCH
// ======================================================================

bool emit_native_call(const ChamadaExpr& node) {
    if (!is_native_func(node.name)) return false;

    if (node.name == "entrada")    { emit_native_entrada(node);   return true; }
    if (node.name == "inteiro")    { emit_native_int(node);       return true; }
    if (node.name == "decimal")    { emit_native_dec(node);       return true; }
    if (node.name == "tipo")       { emit_native_tipo(node);      return true; }
    if (node.name == "texto")      { emit_native_texto(node);     return true; }
    if (node.name == "booleano")   { emit_native_booleano(node);  return true; }
    if (node.name == "args")       { emit_native_args(node);      return true; }
    if (node.name == "num_args")   { emit_native_num_args(node);  return true; }

    return false;
}

// ======================================================================
// ENTRADA(prompt) → String
//
// Windows: printf(RCX=fmt, RDX=str), malloc(RCX), __acrt_iob_func(0)→stdin,
//          fgets(RCX=buf, RDX=size, R8=stdin), strlen(RCX)
// Linux:   printf(RDI=fmt, RSI=str, AL=0), malloc(RDI), stdin=global symbol,
//          fgets(RDI=buf, RSI=size, RDX=stdin), strlen(RDI)
// ======================================================================

void emit_native_entrada(const ChamadaExpr& node) {
    // Se tem argumento (prompt), imprimir
    if (!node.args.empty()) {
        emit_expr(*node.args[0]);
        if constexpr (PlatformDefs::is_windows) {
            emit_mov_reg_reg(reg::RDX, reg::RAX);
            emit_load_string(reg::RCX, "%s");
        } else {
            emit_mov_reg_reg(reg::RSI, reg::RAX);
            emit_load_string(reg::RDI, "%s");
            emit_xor_reg_reg(reg::RAX, reg::RAX);
        }
        emit_call_symbol(emitter_.symbol_index("printf"));
    }

    // malloc(1024) → RAX = ponteiro do buffer
    emit_mov_reg_imm32(PlatformDefs::ARG1, 1024);
    emit_call_symbol(emitter_.symbol_index("malloc"));

    std::string buf_tmp = "__entrada_buf_" + std::to_string(text_->pos());
    int32_t buf_off = alloc_local(buf_tmp);
    emit_mov_rbp_reg(buf_off, reg::RAX);

    // fgets(buf, 1024, stdin)
    if constexpr (PlatformDefs::is_windows) {
        // stdin = __acrt_iob_func(0)
        emit_xor_reg_reg(reg::RCX, reg::RCX);
        emit_call_symbol(emitter_.symbol_index("__acrt_iob_func"));
        // RAX = FILE* stdin
        emit_mov_reg_reg(reg::R8, reg::RAX);           // R8 = stdin (3º arg)
        emit_mov_reg_imm32(reg::RDX, 1024);            // RDX = tamanho (2º arg)
        emit_mov_reg_rbp(reg::RCX, buf_off);           // RCX = buf (1º arg)
    } else {
        // stdin no Linux: símbolo global, dereferenciar
        uint32_t stdin_sym = emitter_.symbol_index("stdin");
        emit_lea_rip_symbol(reg::RAX, stdin_sym);
        // RAX = &stdin, dereferenciar: mov rdx, [rax]
        emit_rex_w(reg::RDX, reg::RAX);
        text_->emit_u8(0x8B);
        text_->emit_u8(0x10); // mov rdx, [rax]

        emit_mov_reg_imm32(reg::RSI, 1024);            // RSI = tamanho
        emit_mov_reg_rbp(reg::RDI, buf_off);           // RDI = buf
        emit_xor_reg_reg(reg::RAX, reg::RAX);
    }
    emit_call_symbol(emitter_.symbol_index("fgets"));

    // strlen(buf) para remover \n
    if constexpr (PlatformDefs::is_windows) {
        emit_mov_reg_rbp(reg::RCX, buf_off);
    } else {
        emit_mov_reg_rbp(reg::RDI, buf_off);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
    }
    emit_call_symbol(emitter_.symbol_index("strlen"));
    // RAX = comprimento

    // if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0'
    emit_test_reg_reg(reg::RAX, reg::RAX);
    size_t skip_strip = emit_je_rel32();

    emit_mov_reg_reg(reg::RCX, reg::RAX);          // RCX = len
    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xFF); // dec rcx
    text_->emit_u8(0xC9);

    emit_mov_reg_rbp(reg::RDX, buf_off);           // RDX = buf
    emit_add_reg_reg(reg::RDX, reg::RCX);          // RDX = buf + len - 1

    // cmp byte [rdx], 0x0A ('\n')
    text_->emit_u8(0x80);
    text_->emit_u8(0x3A);
    text_->emit_u8(0x0A);

    size_t skip_no_newline = emit_jne_rel32();

    // mov byte [rdx], 0x00
    text_->emit_u8(0xC6);
    text_->emit_u8(0x02);
    text_->emit_u8(0x00);

    patch_jump(skip_no_newline);
    patch_jump(skip_strip);

    // Resultado: RAX = ponteiro do buffer
    emit_mov_reg_rbp(reg::RAX, buf_off);
}

// ======================================================================
// INTEIRO(valor) → Int
// ======================================================================

void emit_native_int(const ChamadaExpr& node) {
    if (node.args.empty()) {
        emit_mov_reg_imm32(reg::RAX, 0);
        return;
    }

    RuntimeType type = infer_expr_type(*node.args[0]);
    emit_expr(*node.args[0]);

    switch (type) {
        case RuntimeType::String:
            // RAX = ponteiro string → atoi(RAX)
            if constexpr (PlatformDefs::is_windows) {
                emit_mov_reg_reg(reg::RCX, reg::RAX);
            } else {
                emit_mov_reg_reg(reg::RDI, reg::RAX);
                emit_xor_reg_reg(reg::RAX, reg::RAX);
            }
            emit_call_symbol(emitter_.symbol_index("atoi"));
            // movsxd rax, eax (sign-extend)
            text_->emit_u8(0x48);
            text_->emit_u8(0x63);
            text_->emit_u8(0xC0);
            break;

        case RuntimeType::Float:
            emit_cvttsd2si(reg::RAX, xmm::XMM0);
            break;

        case RuntimeType::Bool:
        case RuntimeType::Int:
        case RuntimeType::Unknown:
        default:
            break;
    }
}

// ======================================================================
// DECIMAL(valor) → Float (double em XMM0)
// ======================================================================

void emit_native_dec(const ChamadaExpr& node) {
    if (node.args.empty()) {
        emit_xorpd(xmm::XMM0, xmm::XMM0);
        return;
    }

    RuntimeType type = infer_expr_type(*node.args[0]);
    emit_expr(*node.args[0]);

    switch (type) {
        case RuntimeType::String:
            // RAX = ponteiro string → atof(RAX)
            if constexpr (PlatformDefs::is_windows) {
                emit_mov_reg_reg(reg::RCX, reg::RAX);
            } else {
                emit_mov_reg_reg(reg::RDI, reg::RAX);
                emit_xor_reg_reg(reg::RAX, reg::RAX);
            }
            emit_call_symbol(emitter_.symbol_index("atof"));
            break;

        case RuntimeType::Int:
        case RuntimeType::Bool:
            emit_cvtsi2sd(xmm::XMM0, reg::RAX);
            break;

        case RuntimeType::Float:
        case RuntimeType::Unknown:
        default:
            if (type == RuntimeType::Unknown) {
                emit_cvtsi2sd(xmm::XMM0, reg::RAX);
            }
            break;
    }
}

// ======================================================================
// TIPO(valor) → String (resolvido em tempo de compilação)
// ======================================================================

void emit_native_tipo(const ChamadaExpr& node) {
    if (node.args.empty()) {
        std::string unknown = translate_type_name("desconhecido");
        emit_load_string(reg::RAX, unknown);
        return;
    }

    RuntimeType type = infer_expr_type(*node.args[0]);

    std::string type_str;
    switch (type) {
        case RuntimeType::Int:     type_str = translate_type_name("inteiro");      break;
        case RuntimeType::Float:   type_str = translate_type_name("decimal");      break;
        case RuntimeType::String:  type_str = translate_type_name("texto");        break;
        case RuntimeType::Bool:    type_str = translate_type_name("booleano");     break;
        case RuntimeType::Unknown: type_str = translate_type_name("desconhecido"); break;
        default:                   type_str = translate_type_name("desconhecido"); break;
    }

    emit_load_string(reg::RAX, type_str);
}

// ======================================================================
// TEXTO(valor) → String
//
// Windows: sprintf(RCX=buf, RDX=fmt, R8=val) / XMM2+R8 para float
// Linux:   sprintf(RDI=buf, RSI=fmt, RDX=val, AL=0) / XMM0+AL=1 para float
// ======================================================================

void emit_native_texto(const ChamadaExpr& node) {
    if (node.args.empty()) {
        emit_load_string(reg::RAX, "");
        return;
    }

    RuntimeType type = infer_expr_type(*node.args[0]);
    emit_expr(*node.args[0]);

    switch (type) {
        case RuntimeType::String:
            break;

        case RuntimeType::Int: {
            std::string tmp = "__texto_tmp_" + std::to_string(text_->pos());
            int32_t tmp_off = alloc_local(tmp);
            emit_mov_rbp_reg(tmp_off, reg::RAX);

            emit_mov_reg_imm32(PlatformDefs::ARG1, 64);
            emit_call_symbol(emitter_.symbol_index("malloc"));

            std::string buf_tmp = "__texto_buf_" + std::to_string(text_->pos());
            int32_t buf_off = alloc_local(buf_tmp);
            emit_mov_rbp_reg(buf_off, reg::RAX);

            // sprintf(buf, "%lld", valor)
            if constexpr (PlatformDefs::is_windows) {
                emit_mov_reg_rbp(reg::R8, tmp_off);
                emit_load_string(reg::RDX, "%lld");
                emit_mov_reg_rbp(reg::RCX, buf_off);
            } else {
                emit_mov_reg_rbp(reg::RDX, tmp_off);
                emit_load_string(reg::RSI, "%lld");
                emit_mov_reg_rbp(reg::RDI, buf_off);
                emit_xor_reg_reg(reg::RAX, reg::RAX);
            }
            emit_call_symbol(emitter_.symbol_index("sprintf"));

            emit_mov_reg_rbp(reg::RAX, buf_off);
            break;
        }

        case RuntimeType::Float: {
            std::string tmp = "__texto_ftmp_" + std::to_string(text_->pos());
            int32_t tmp_off = alloc_local(tmp);
            emit_movsd_rbp_xmm(tmp_off, xmm::XMM0);

            emit_mov_reg_imm32(PlatformDefs::ARG1, 64);
            emit_call_symbol(emitter_.symbol_index("malloc"));

            std::string buf_tmp = "__texto_fbuf_" + std::to_string(text_->pos());
            int32_t buf_off = alloc_local(buf_tmp);
            emit_mov_rbp_reg(buf_off, reg::RAX);

            // sprintf(buf, "%g", valor)
            if constexpr (PlatformDefs::is_windows) {
                emit_movsd_xmm_rbp(xmm::XMM2, tmp_off);
                emit_mov_reg_rbp(reg::R8, tmp_off);
                emit_load_string(reg::RDX, "%g");
                emit_mov_reg_rbp(reg::RCX, buf_off);
            } else {
                emit_movsd_xmm_rbp(xmm::XMM0, tmp_off);
                emit_load_string(reg::RSI, "%g");
                emit_mov_reg_rbp(reg::RDI, buf_off);
                emit_mov_reg_imm32(reg::RAX, 1); // AL = 1 (1 XMM arg)
            }
            emit_call_symbol(emitter_.symbol_index("sprintf"));

            emit_mov_reg_rbp(reg::RAX, buf_off);
            break;
        }

        case RuntimeType::Bool:
            emit_test_reg_reg(reg::RAX, reg::RAX);
            {
                size_t false_jump = emit_je_rel32();
                emit_load_string(reg::RAX, lang_config_.bool_true);
                size_t end_jump = emit_jmp_rel32();
                patch_jump(false_jump);
                emit_load_string(reg::RAX, lang_config_.bool_false);
                patch_jump(end_jump);
            }
            break;

        case RuntimeType::Unknown:
        default: {
            std::string tmp = "__texto_utmp_" + std::to_string(text_->pos());
            int32_t tmp_off = alloc_local(tmp);
            emit_mov_rbp_reg(tmp_off, reg::RAX);

            emit_mov_reg_imm32(PlatformDefs::ARG1, 64);
            emit_call_symbol(emitter_.symbol_index("malloc"));

            std::string buf_tmp = "__texto_ubuf_" + std::to_string(text_->pos());
            int32_t buf_off = alloc_local(buf_tmp);
            emit_mov_rbp_reg(buf_off, reg::RAX);

            if constexpr (PlatformDefs::is_windows) {
                emit_mov_reg_rbp(reg::R8, tmp_off);
                emit_load_string(reg::RDX, "%lld");
                emit_mov_reg_rbp(reg::RCX, buf_off);
            } else {
                emit_mov_reg_rbp(reg::RDX, tmp_off);
                emit_load_string(reg::RSI, "%lld");
                emit_mov_reg_rbp(reg::RDI, buf_off);
                emit_xor_reg_reg(reg::RAX, reg::RAX);
            }
            emit_call_symbol(emitter_.symbol_index("sprintf"));

            emit_mov_reg_rbp(reg::RAX, buf_off);
            break;
        }
    }
}

// ======================================================================
// BOOLEANO(valor) → Bool (idêntico nas duas plataformas)
// ======================================================================

void emit_native_booleano(const ChamadaExpr& node) {
    if (node.args.empty()) {
        emit_mov_reg_imm32(reg::RAX, 0);
        return;
    }

    RuntimeType type = infer_expr_type(*node.args[0]);
    emit_expr(*node.args[0]);

    switch (type) {
        case RuntimeType::Bool:
            break;

        case RuntimeType::Int:
            emit_test_reg_reg(reg::RAX, reg::RAX);
            emit_setcc(CC_NE, reg::RAX);
            emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
            break;

        case RuntimeType::Float:
            emit_xorpd(xmm::XMM1, xmm::XMM1);
            emit_ucomisd(xmm::XMM0, xmm::XMM1);
            emit_setcc(CC_NE, reg::RAX);
            emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
            break;

        case RuntimeType::String:
            // cmp byte [rax], 0
            text_->emit_u8(0x80);
            text_->emit_u8(0x38);
            text_->emit_u8(0x00);
            emit_setcc(CC_NE, reg::RAX);
            emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
            break;

        case RuntimeType::Unknown:
        default:
            emit_test_reg_reg(reg::RAX, reg::RAX);
            emit_setcc(CC_NE, reg::RAX);
            emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
            break;
    }
}

// ======================================================================
// ARGS(indice) → String (idêntico nas duas plataformas)
// ======================================================================

void emit_native_args(const ChamadaExpr& node) {
    if (node.args.empty()) {
        emit_load_string(reg::RAX, "");
        return;
    }

    emit_expr(*node.args[0]);

    std::string idx_tmp = "__args_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx_tmp);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    int32_t argc_off = find_local("__jp_argc");
    emit_mov_reg_rbp(reg::RCX, argc_off);

    emit_cmp_reg_reg(reg::RAX, reg::RCX);
    size_t out_of_bounds = emit_jge_rel32();

    int32_t argv_off = find_local("__jp_argv");
    emit_mov_reg_rbp(reg::RAX, argv_off);

    emit_mov_reg_rbp(reg::RCX, idx_off);
    // mov rax, [rax + rcx*8]
    emit_rex_w(reg::RAX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x04);   // SIB follows
    text_->emit_u8(0xC8);   // scale=8(11), index=RCX(001), base=RAX(000)

    size_t end_patch = emit_jmp_rel32();

    patch_jump(out_of_bounds);
    emit_load_string(reg::RAX, "");

    patch_jump(end_patch);
}

// ======================================================================
// NUM_ARGS() → Int (idêntico nas duas plataformas)
// ======================================================================

void emit_native_num_args(const ChamadaExpr& node) {
    (void)node;
    int32_t argc_off = find_local("__jp_argc");
    emit_mov_reg_rbp(reg::RAX, argc_off);
}
