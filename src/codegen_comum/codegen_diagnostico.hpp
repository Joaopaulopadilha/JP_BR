// codegen_diagnostico.hpp
// Sistema de diagnostico para chamadas FFI (.jpd) — validacao de retorno,
// deteccao de crash e mensagens traduzidas via LangConfig.
//
// Incluido inline dentro da classe Codegen (mesmo padrao dos outros codegen_*.hpp).
//
// ESTRATEGIA:
//   - PRE-FFI: imprime no stderr qual funcao vai ser chamada e em que linha.
//     Se a DLL crashar, a ultima mensagem no stderr mostra onde foi.
//   - POS-FFI: valida retorno tipo "texto" (ponteiro NULL ou endereco invalido).
//   - HANDLER: registrado via SetUnhandledExceptionFilter (Win) / sigaction (Linux).
//     Imprime mensagem amigavel e chama exit(1).
//
// Mensagens vem do campo "diagnostico" do JSON de idioma via lang_config_.diagnostico

// ======================================================================
// ESTADO
// ======================================================================

bool diag_handler_emitted_ = false;
std::string diag_source_file_ = "";

// ======================================================================
// HELPERS DE MENSAGEM (tempo de compilacao)
// ======================================================================

std::string diag_msg(const std::string& chave, const std::string& fallback) const {
    auto it = lang_config_.diagnostico.find(chave);
    if (it != lang_config_.diagnostico.end()) return it->second;
    return fallback;
}

static std::string diag_replace(const std::string& msg,
                                const std::string& placeholder,
                                const std::string& valor) {
    std::string result = msg;
    std::string token = "{" + placeholder + "}";
    size_t pos = result.find(token);
    if (pos != std::string::npos) {
        result.replace(pos, token.size(), valor);
    }
    return result;
}

// ======================================================================
// HELPER: fprintf(stderr, msg_constante) — sem args, sem alloc_local
// ======================================================================

void emit_diag_fprintf(const std::string& msg) {
    if constexpr (PlatformDefs::is_windows) {
        // __acrt_iob_func(2) → stderr
        emit_mov_reg_imm32(reg::RCX, 2);
        emit_call_extern("__acrt_iob_func");
        // fprintf(stderr, msg)
        emit_mov_reg_reg(reg::RCX, reg::RAX);
        emit_load_string(reg::RDX, msg);
        emit_call_extern("fprintf");
    } else {
        if (!emitter_.has_symbol("stderr")) {
            emitter_.add_extern_symbol("stderr");
        }
        uint32_t stderr_sym = emitter_.symbol_index("stderr");
        emit_lea_rip_symbol(reg::RAX, stderr_sym);
        // mov rdi, [rax] — dereferenciar ponteiro stderr
        emit_rex_w(reg::RDI, reg::RAX);
        text_->emit_u8(0x8B); text_->emit_u8(0x38);
        emit_load_string(reg::RSI, msg);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
        emit_call_extern("fprintf");
    }
}

// ======================================================================
// HELPER: fflush(stderr) — garante que a mensagem aparece antes de crash
// ======================================================================

void emit_diag_fflush_stderr() {
    if constexpr (PlatformDefs::is_windows) {
        emit_mov_reg_imm32(reg::RCX, 2);
        emit_call_extern("__acrt_iob_func");
        emit_mov_reg_reg(reg::RCX, reg::RAX);
        emit_call_extern("fflush");
    } else {
        if (!emitter_.has_symbol("stderr")) {
            emitter_.add_extern_symbol("stderr");
        }
        uint32_t stderr_sym = emitter_.symbol_index("stderr");
        emit_lea_rip_symbol(reg::RAX, stderr_sym);
        emit_rex_w(reg::RDI, reg::RAX);
        text_->emit_u8(0x8B); text_->emit_u8(0x38);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
        emit_call_extern("fflush");
    }
}

// ======================================================================
// CRASH HANDLER — funcao separada com frame proprio
//
// Windows: LONG WINAPI handler(EXCEPTION_POINTERS*)
// Linux:   void handler(int signo)
//
// Imprime mensagem generica amigavel e sai.
// A info especifica (funcao, linha) ja foi impressa pelo pre_ffi
// ANTES do crash, entao aparece no stderr antes desta mensagem.
// ======================================================================

void emit_crash_handler_func() {
    if (diag_handler_emitted_) return;
    diag_handler_emitted_ = true;

    uint32_t handler_offset = static_cast<uint32_t>(text_->pos());
    emitter_.add_global_symbol("__jp_crash_handler", text_idx_,
                               handler_offset, true);

    // Frame proprio
    emit_push(reg::RBP);
    emit_mov_reg_reg(reg::RBP, reg::RSP);
    emit_sub_rsp_imm32(PlatformDefs::MIN_STACK);

    // Mensagem de crash
    std::string msg = "\n" +
        diag_msg("header", "[JP DIAGNOSTICO]") + " " +
        diag_msg("crash_acesso",
            "Violacao de acesso durante chamada a biblioteca externa") + "\n" +
        "  " +
        diag_msg("erro_na_dll",
            "O erro ocorreu DENTRO da biblioteca externa") + "\n" +
        "  " +
        diag_msg("erro_nao_jp",
            "Isto nao e um bug do seu codigo JP — a biblioteca nativa crashou") + "\n" +
        "  " +
        diag_msg("crash_dica",
            "Veja a ultima linha [JP FFI] acima para identificar a chamada que causou o erro") +
        "\n\n";

    emit_diag_fprintf(msg);

    // exit(1)
    emit_mov_reg_imm32(PlatformDefs::ARG1, 1);
    emit_call_extern("exit");

    // Epilogo (inalcancavel)
    emit_mov_reg_reg(reg::RSP, reg::RBP);
    emit_pop(reg::RBP);
    emit_ret();
}

// ======================================================================
// REGISTRO DO HANDLER — chamado uma vez no inicio do main
// ======================================================================

void emit_diag_register_handler() {
    // O handler sera emitido depois do main pelo compile().
    // Aqui apenas registramos para que o OS chame quando houver crash.

    // Garantir que o symbol existe (sera resolvido quando emitirmos o handler)
    if (!emitter_.has_symbol("__jp_crash_handler")) {
        emitter_.add_extern_symbol("__jp_crash_handler");
    }

    if constexpr (PlatformDefs::is_windows) {
        if (!emitter_.has_symbol("SetUnhandledExceptionFilter")) {
            emitter_.add_extern_symbol("SetUnhandledExceptionFilter");
        }
        // LEA RCX, [RIP + __jp_crash_handler]
        emit_lea_rip_symbol(reg::RCX,
            emitter_.symbol_index("__jp_crash_handler"));
        emit_call_extern("SetUnhandledExceptionFilter");
    } else {
        if (!emitter_.has_symbol("sigaction")) {
            emitter_.add_extern_symbol("sigaction");
        }

        // struct sigaction na stack (160 bytes)
        emit_sub_rsp_imm32(160);

        // Zerar struct
        emit_mov_reg_reg(reg::RDI, reg::RSP);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
        emit_mov_reg_imm32(reg::RCX, 20); // 20*8=160
        // REP STOSQ
        text_->emit_u8(0xF3);
        text_->emit_u8(0x48);
        text_->emit_u8(0xAB);

        // sa_handler = handler
        emit_lea_rip_symbol(reg::RAX,
            emitter_.symbol_index("__jp_crash_handler"));
        // MOV [RSP], RAX
        emit_rex_w(reg::RAX, reg::RSP);
        text_->emit_u8(0x89);
        text_->emit_u8(0x04);
        text_->emit_u8(0x24);

        // sigaction(SIGSEGV=11, &act, NULL)
        emit_mov_reg_imm32(reg::RDI, 11);
        emit_mov_reg_reg(reg::RSI, reg::RSP);
        emit_xor_reg_reg(reg::RDX, reg::RDX);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
        emit_call_extern("sigaction");

        // sigaction(SIGBUS=7, &act, NULL)
        emit_mov_reg_imm32(reg::RDI, 7);
        emit_mov_reg_reg(reg::RSI, reg::RSP);
        emit_xor_reg_reg(reg::RDX, reg::RDX);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
        emit_call_extern("sigaction");

        emit_add_rsp_imm32(160);
    }
}

// ======================================================================
// PRE-CHAMADA FFI
//
// Imprime no stderr: "[JP FFI] lowllhama_carregar() | Linha 10 | chat.jp"
// Seguido de fflush(stderr) para garantir que aparece antes de qualquer crash.
//
// A mensagem e resolvida em tempo de compilacao (strings constantes).
// Nenhum global, nenhum alloc_local — impossivel de falhar.
// ======================================================================

void emit_diag_pre_ffi(const std::string& func_name, int line,
                       const std::string& arquivo) {
    // Trace so aparece se "depurar"/"debug" foi usado no codigo fonte
    if (!debug_mode_) return;

    // Montar mensagem completa em tempo de compilacao
    std::string lbl = diag_msg("ffi_label", "[JP FFI]");
    std::string lbl_linha = diag_msg("label_linha", "Linha");

    std::string msg = lbl + " " + func_name + "() | "
                    + lbl_linha + " " + std::to_string(line)
                    + " | " + arquivo + "\n";

    emit_diag_fprintf(msg);
    emit_diag_fflush_stderr();
}

// ======================================================================
// POS-CHAMADA FFI
//
// Valida retorno. Se tipo texto: verifica NULL e endereco invalido.
// Se detecta problema, imprime diagnostico e substitui por string vazia.
// ======================================================================

void emit_diag_pos_ffi(const std::string& func_name, int line,
                       RuntimeType ret_type) {
    // Salvar retorno
    std::string ret_tmp = "__diag_ret_" + std::to_string(text_->pos());
    int32_t ret_off = alloc_local(ret_tmp);
    if (ret_type == RuntimeType::Float) {
        emit_movsd_rbp_xmm(ret_off, xmm::XMM0);
    } else {
        emit_mov_rbp_reg(ret_off, reg::RAX);
    }

    if (ret_type == RuntimeType::String) {
        // --- Verificar NULL ---
        emit_mov_reg_rbp(reg::RAX, ret_off);
        emit_test_reg_reg(reg::RAX, reg::RAX);
        size_t skip_null = emit_jne_rel32();

        // NULL!
        {
            std::string msg = "\n" +
                diag_msg("header", "[JP DIAGNOSTICO]") + " " +
                diag_replace(
                    diag_msg("retorno_nulo",
                        "Funcao '{funcao}' retornou ponteiro nulo (NULL)"),
                    "funcao", func_name) + "\n" +
                "  " + diag_replace(
                    diag_msg("esperado_tipo", "Esperado: {esperado}"),
                    "esperado", "texto") + "\n" +
                "  " + diag_replace(
                    diag_msg("obtido_valor", "Obtido: {obtido}"),
                    "obtido", "NULL (0x0)") + "\n" +
                "  " + diag_replace(
                    diag_replace(
                        diag_msg("linha_arquivo", "Linha {num} em {arquivo}"),
                        "num", std::to_string(line)),
                    "arquivo", diag_source_file_) + "\n\n";
            emit_diag_fprintf(msg);
        }

        // Substituir por string vazia
        emit_load_string(reg::RAX, "");
        emit_mov_rbp_reg(ret_off, reg::RAX);
        size_t skip_end = emit_jmp_rel32();

        patch_jump(skip_null);

        // --- Verificar endereco baixo (< 0x10000) ---
        emit_mov_reg_rbp(reg::RAX, ret_off);
        emit_cmp_reg_imm32(reg::RAX, 0x10000);
        size_t skip_low = emit_jge_rel32();

        {
            std::string msg = "\n" +
                diag_msg("header", "[JP DIAGNOSTICO]") + " " +
                diag_replace(
                    diag_replace(
                        diag_msg("retorno_invalido",
                            "Funcao '{funcao}' retornou valor invalido como {tipo}"),
                        "funcao", func_name),
                    "tipo", "texto") + "\n" +
                "  " + diag_msg("ponteiro_suspeito",
                    "Valor muito baixo para ser endereco de memoria") + "\n" +
                "  " + diag_replace(
                    diag_replace(
                        diag_msg("linha_arquivo", "Linha {num} em {arquivo}"),
                        "num", std::to_string(line)),
                    "arquivo", diag_source_file_) + "\n\n";
            emit_diag_fprintf(msg);
        }

        emit_load_string(reg::RAX, "");
        emit_mov_rbp_reg(ret_off, reg::RAX);

        patch_jump(skip_low);
        patch_jump(skip_end);
    }

    // Restaurar retorno
    if (ret_type == RuntimeType::Float) {
        emit_movsd_xmm_rbp(xmm::XMM0, ret_off);
    } else {
        emit_mov_reg_rbp(reg::RAX, ret_off);
    }
}

// ======================================================================
// QUERY HELPERS
// ======================================================================

bool is_extern_jpd_func(const std::string& func_name) const {
    return func_param_types_.count(func_name) > 0;
}

std::string diag_source_file() const {
    return diag_source_file_;
}

void set_diag_source_file(const std::string& path) {
    diag_source_file_ = path;
}