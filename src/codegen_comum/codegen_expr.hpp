// codegen_expr.hpp
// Emissão de expressões — unificado Windows x64 e Linux x86-64 via PlatformDefs
// Suporte a double via SSE2 — resultado int/string/bool em RAX, float em XMM0

// ======================================================================
// EMISSÃO DE EXPRESSÕES
// Convenção de resultado:
//   Int, String, Bool, Unknown → RAX
//   Float                      → XMM0
// ======================================================================

void emit_expr(const Expr& expr) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, NumberLit>) {
            emit_mov_reg_imm32(reg::RAX, node.value);
        }
        else if constexpr (std::is_same_v<T, FloatLit>) {
            emit_load_double_imm(xmm::XMM0, node.value);
        }
        else if constexpr (std::is_same_v<T, StringLit>) {
            emit_load_string(reg::RAX, node.value);
        }
        else if constexpr (std::is_same_v<T, StringInterp>) {
            emit_string_interp(node);
        }
        else if constexpr (std::is_same_v<T, BoolLit>) {
            emit_mov_reg_imm32(reg::RAX, node.value ? 1 : 0);
        }
        else if constexpr (std::is_same_v<T, NullLit>) {
            emit_mov_reg_imm32(reg::RAX, 0);
        }
        else if constexpr (std::is_same_v<T, VarExpr>) {
            RuntimeType type = RuntimeType::Unknown;
            auto it = var_types_.find(node.name);
            if (it != var_types_.end()) type = it->second;

            int32_t offset = find_local(node.name);
            if (type == RuntimeType::Float) {
                emit_movsd_xmm_rbp(xmm::XMM0, offset);
            } else {
                emit_mov_reg_rbp(reg::RAX, offset);
            }
        }
        else if constexpr (std::is_same_v<T, BinOpExpr>) {
            emit_binop(node);
        }
        else if constexpr (std::is_same_v<T, CmpOpExpr>) {
            emit_cmpop(node);
        }
        else if constexpr (std::is_same_v<T, LogicOpExpr>) {
            emit_logicop(node);
        }
        else if constexpr (std::is_same_v<T, ChamadaExpr>) {
            emit_chamada(node);
        }
        else if constexpr (std::is_same_v<T, ConcatExpr>) {
            // Concatenação de strings: left .. right
            // Emitir como BinOpExpr(Add) com strings
            emit_expr(*node.left);
            std::string cl_tmp = "__concat_l_" + std::to_string(text_->pos());
            int32_t cl_off = alloc_local(cl_tmp);
            emit_mov_rbp_reg(cl_off, reg::RAX);

            emit_expr(*node.right);
            std::string cr_tmp = "__concat_r_" + std::to_string(text_->pos());
            int32_t cr_off = alloc_local(cr_tmp);
            emit_mov_rbp_reg(cr_off, reg::RAX);

            // strlen(left)
            emit_mov_reg_rbp(PlatformDefs::ARG1, cl_off);
            emit_call_extern("strlen");
            std::string clen1 = "__concat_len1_" + std::to_string(text_->pos());
            int32_t clen1_off = alloc_local(clen1);
            emit_mov_rbp_reg(clen1_off, reg::RAX);

            // strlen(right)
            emit_mov_reg_rbp(PlatformDefs::ARG1, cr_off);
            emit_call_extern("strlen");
            std::string clen2 = "__concat_len2_" + std::to_string(text_->pos());
            int32_t clen2_off = alloc_local(clen2);
            emit_mov_rbp_reg(clen2_off, reg::RAX);

            // malloc(len1 + len2 + 1)
            emit_mov_reg_rbp(reg::RAX, clen1_off);
            emit_mov_reg_rbp(reg::RCX, clen2_off);
            emit_add_reg_reg(reg::RAX, reg::RCX);
            emit_rex_w(0, reg::RAX);
            text_->emit_u8(0x83);
            text_->emit_u8(0xC0);
            text_->emit_u8(0x01);
            emit_mov_reg_reg(PlatformDefs::ARG1, reg::RAX);
            emit_call_extern("malloc");
            std::string cbuf = "__concat_buf_" + std::to_string(text_->pos());
            int32_t cbuf_off = alloc_local(cbuf);
            emit_mov_rbp_reg(cbuf_off, reg::RAX);

            // strcpy + strcat
            emit_mov_reg_rbp(PlatformDefs::ARG1, cbuf_off);
            emit_mov_reg_rbp(PlatformDefs::ARG2, cl_off);
            emit_call_extern("strcpy");
            emit_mov_reg_rbp(PlatformDefs::ARG1, cbuf_off);
            emit_mov_reg_rbp(PlatformDefs::ARG2, cr_off);
            emit_call_extern("strcat");

            emit_mov_reg_rbp(reg::RAX, cbuf_off);
        }
        else if constexpr (std::is_same_v<T, MetodoChamadaExpr>) {
            // Verificar se é chamada estática de construtor: Classe.metodo(args)
            if (std::holds_alternative<VarExpr>(node.object->node)) {
                auto& var = std::get<VarExpr>(node.object->node);
                if (declared_classes_.find(var.name) != declared_classes_.end()) {
                    emit_static_method_call(var.name, node.method, node);
                    return;
                }
                // Verificar se é método de lista
                if (is_list_var(var.name)) {
                    if constexpr (PlatformDefs::is_windows) {
                        if (!emit_list_method(node)) {
                            emit_metodo_chamada(node);
                        }
                    } else {
                        emit_list_method(var.name, node.method, node.args);
                    }
                    return;
                }
            }
            // Chamada de método de instância: obj.metodo(args)
            emit_metodo_chamada(node);
        }
        else if constexpr (std::is_same_v<T, AttrGetExpr>) {
            emit_attr_get(node);
        }
        else if constexpr (std::is_same_v<T, AutoExpr>) {
            emit_auto_expr();
        }
        else if constexpr (std::is_same_v<T, ListLitExpr>) {
            emit_list_literal(node);
        }
        else if constexpr (std::is_same_v<T, IndexGetExpr>) {
            if (std::holds_alternative<VarExpr>(node.object->node)) {
                auto& var = std::get<VarExpr>(node.object->node);
                if (is_list_var(var.name)) {
                    emit_list_index_get(node, var.name);
                    return;
                }
            }
            emit_index_get(node);
        }
        else {
            emit_mov_reg_imm32(reg::RAX, 0);
        }
    }, expr.node);
}

// ======================================================================
// HELPER: garante que o valor de uma expressão esteja em XMM0
// Se a expressão é Int, converte RAX → XMM0
// Se já é Float, não faz nada (emit_expr já deixou em XMM0)
// ======================================================================

void emit_expr_as_float(const Expr& expr) {
    RuntimeType type = infer_expr_type(expr);
    emit_expr(expr);
    if (type != RuntimeType::Float) {
        emit_cvtsi2sd(xmm::XMM0, reg::RAX);
    }
}

// ======================================================================
// INDEX GET — implementação base para arrays simples (não-lista)
// Para listas struct, usa emit_list_index_get de codegen_listas.hpp
// ======================================================================

void emit_index_get(const IndexGetExpr& node) {
    // Avaliar o objeto (ponteiro base) → salvar
    emit_expr(*node.object);
    std::string base_tmp = "__idxget_base_" + std::to_string(text_->pos());
    int32_t base_off = alloc_local(base_tmp);
    emit_mov_rbp_reg(base_off, reg::RAX);

    // Avaliar o índice → RAX
    emit_expr(*node.index);

    // Carregar ponteiro base → RCX
    emit_mov_reg_rbp(reg::RCX, base_off);

    // MOV RAX, [RCX + RAX*8]
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x04); // ModR/M: RAX, SIB follows
    text_->emit_u8(0xC1); // SIB: scale=8(11), index=RAX(000), base=RCX(001)
}

// ======================================================================
// OPERAÇÕES BINÁRIAS (com suporte a float)
// ======================================================================

void emit_binop(const BinOpExpr& node) {
    RuntimeType lt = infer_expr_type(*node.left);
    RuntimeType rt = infer_expr_type(*node.right);
    bool is_float = (lt == RuntimeType::Float || rt == RuntimeType::Float);
    bool is_string = (lt == RuntimeType::String || rt == RuntimeType::String);

    // Concatenação de strings: "abc" + "def" ou var_str + var_str
    if (node.op == BinOp::Add && is_string) {
        emit_binop_strcat(node);
        return;
    }

    // Divisão sempre usa float (como Python 3)
    if (node.op == BinOp::Div) {
        is_float = true;
    }

    if (is_float) {
        emit_binop_float(node, lt, rt);
    } else {
        emit_binop_int(node);
    }
}

void emit_binop_int(const BinOpExpr& node) {
    emit_expr(*node.left);
    emit_push(reg::RAX);

    emit_expr(*node.right);
    emit_mov_reg_reg(reg::RCX, reg::RAX);

    emit_pop(reg::RAX);

    switch (node.op) {
        case BinOp::Add:
            emit_add_reg_reg(reg::RAX, reg::RCX);
            break;
        case BinOp::Sub:
            emit_sub_reg_reg(reg::RAX, reg::RCX);
            break;
        case BinOp::Mul:
            emit_imul_reg_reg(reg::RAX, reg::RCX);
            break;
        case BinOp::Div:
            emit_cqo();
            emit_idiv_reg(reg::RCX);
            break;
        case BinOp::Mod:
            emit_cqo();
            emit_idiv_reg(reg::RCX);
            emit_mov_reg_reg(reg::RAX, reg::RDX);
            break;
    }
}

// ======================================================================
// CONCATENAÇÃO DE STRINGS: a + b → malloc(strlen(a)+strlen(b)+1),
//                                   strcpy, strcat
// Resultado: ponteiro para nova string em RAX
// ======================================================================

// Converte valor em RAX (int/bool) para string via sprintf
// Resultado: ponteiro para string em RAX
void emit_int_to_string_inplace(int32_t val_off, int32_t& out_off) {
    // Aloca buffer de 32 bytes para o número convertido
    std::string buf_name = "__itoa_buf_" + std::to_string(text_->pos());
    int32_t buf_off = alloc_local(buf_name);

    // malloc(32)
    emit_mov_reg_imm32(PlatformDefs::ARG1, 32);
    emit_call_extern("malloc");
    emit_mov_rbp_reg(buf_off, reg::RAX);

    // sprintf(buf, "%lld", valor)
    emit_mov_reg_rbp(PlatformDefs::ARG1, buf_off);
    emit_load_string(PlatformDefs::ARG2, "%lld");
    emit_mov_reg_rbp(PlatformDefs::ARG3, val_off);
    emit_call_extern("sprintf");

    out_off = buf_off;
}

// Converte valor float em XMM0 para string via sprintf
void emit_float_to_string_inplace(int32_t val_off, int32_t& out_off) {
    std::string buf_name = "__ftoa_buf_" + std::to_string(text_->pos());
    int32_t buf_off = alloc_local(buf_name);

    // malloc(64)
    emit_mov_reg_imm32(PlatformDefs::ARG1, 64);
    emit_call_extern("malloc");
    emit_mov_rbp_reg(buf_off, reg::RAX);

    // sprintf(buf, "%g", valor)
    emit_mov_reg_rbp(PlatformDefs::ARG1, buf_off);
    emit_load_string(PlatformDefs::ARG2, "%g");

    if constexpr (PlatformDefs::is_windows) {
        // Windows: float vai no XMM2 para sprintf, mas também precisa ir no GPR (variádica)
        emit_movsd_xmm_rbp(xmm::XMM2, val_off);
        emit_movq_gpr_xmm(PlatformDefs::ARG3, xmm::XMM2);
    } else {
        // Linux System V: floats vão nos XMM regs
        emit_movsd_xmm_rbp(xmm::XMM0, val_off);
    }
    emit_call_extern("sprintf");

    out_off = buf_off;
}

void emit_binop_strcat(const BinOpExpr& node) {
    RuntimeType lt = infer_expr_type(*node.left);
    RuntimeType rt = infer_expr_type(*node.right);

    // 1. Avaliar left → salvar em temp
    emit_expr(*node.left);
    std::string left_tmp = "__strcat_l_" + std::to_string(text_->pos());
    int32_t left_off = alloc_local(left_tmp);
    if (lt == RuntimeType::Float) {
        emit_movsd_rbp_xmm(left_off, xmm::XMM0);
    } else {
        emit_mov_rbp_reg(left_off, reg::RAX);
    }

    // 2. Avaliar right → salvar em temp
    emit_expr(*node.right);
    std::string right_tmp = "__strcat_r_" + std::to_string(text_->pos());
    int32_t right_off = alloc_local(right_tmp);
    if (rt == RuntimeType::Float) {
        emit_movsd_rbp_xmm(right_off, xmm::XMM0);
    } else {
        emit_mov_rbp_reg(right_off, reg::RAX);
    }

    // 3. Converter lado esquerdo pra string se necessário
    if (lt != RuntimeType::String) {
        int32_t conv_off;
        if (lt == RuntimeType::Float) {
            emit_float_to_string_inplace(left_off, conv_off);
        } else {
            emit_int_to_string_inplace(left_off, conv_off);
        }
        left_off = conv_off;
    }

    // 4. Converter lado direito pra string se necessário
    if (rt != RuntimeType::String) {
        int32_t conv_off;
        if (rt == RuntimeType::Float) {
            emit_float_to_string_inplace(right_off, conv_off);
        } else {
            emit_int_to_string_inplace(right_off, conv_off);
        }
        right_off = conv_off;
    }

    // 5. strlen(left) → salvar
    emit_mov_reg_rbp(PlatformDefs::ARG1, left_off);
    emit_call_extern("strlen");
    std::string len1_tmp = "__strcat_len1_" + std::to_string(text_->pos());
    int32_t len1_off = alloc_local(len1_tmp);
    emit_mov_rbp_reg(len1_off, reg::RAX);

    // 6. strlen(right) → salvar
    emit_mov_reg_rbp(PlatformDefs::ARG1, right_off);
    emit_call_extern("strlen");
    std::string len2_tmp = "__strcat_len2_" + std::to_string(text_->pos());
    int32_t len2_off = alloc_local(len2_tmp);
    emit_mov_rbp_reg(len2_off, reg::RAX);

    // 7. malloc(len1 + len2 + 1)
    emit_mov_reg_rbp(reg::RAX, len1_off);
    emit_mov_reg_rbp(reg::RCX, len2_off);
    emit_add_reg_reg(reg::RAX, reg::RCX);
    // ADD RAX, 1
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0x83);
    text_->emit_u8(0xC0);
    text_->emit_u8(0x01);
    emit_mov_reg_reg(PlatformDefs::ARG1, reg::RAX);
    emit_call_extern("malloc");
    std::string buf_tmp = "__strcat_buf_" + std::to_string(text_->pos());
    int32_t buf_off = alloc_local(buf_tmp);
    emit_mov_rbp_reg(buf_off, reg::RAX);

    // 8. strcpy(buf, left)
    emit_mov_reg_rbp(PlatformDefs::ARG1, buf_off);
    emit_mov_reg_rbp(PlatformDefs::ARG2, left_off);
    emit_call_extern("strcpy");

    // 9. strcat(buf, right)
    emit_mov_reg_rbp(PlatformDefs::ARG1, buf_off);
    emit_mov_reg_rbp(PlatformDefs::ARG2, right_off);
    emit_call_extern("strcat");

    // 10. Resultado em RAX = buf
    emit_mov_reg_rbp(reg::RAX, buf_off);
}

void emit_binop_float(const BinOpExpr& node, RuntimeType lt, RuntimeType rt) {
    (void)lt; (void)rt;

    // Avaliar left → salvar na stack (seguro pra operações aninhadas)
    emit_expr_as_float(*node.left);
    std::string left_tmp = "__binop_fl_" + std::to_string(text_->pos());
    int32_t left_off = alloc_local(left_tmp);
    emit_movsd_rbp_xmm(left_off, xmm::XMM0);

    // Avaliar right → XMM0 (pode usar XMM1 internamente sem problema)
    emit_expr_as_float(*node.right);

    // Recuperar left da stack → XMM1
    emit_movsd_xmm_rbp(xmm::XMM1, left_off);

    // XMM1 = left, XMM0 = right

    switch (node.op) {
        case BinOp::Add:
            emit_addsd(xmm::XMM1, xmm::XMM0);
            emit_movsd_xmm_xmm(xmm::XMM0, xmm::XMM1);
            break;
        case BinOp::Sub:
            emit_subsd(xmm::XMM1, xmm::XMM0);
            emit_movsd_xmm_xmm(xmm::XMM0, xmm::XMM1);
            break;
        case BinOp::Mul:
            emit_mulsd(xmm::XMM1, xmm::XMM0);
            emit_movsd_xmm_xmm(xmm::XMM0, xmm::XMM1);
            break;
        case BinOp::Div:
            emit_divsd(xmm::XMM1, xmm::XMM0);
            emit_movsd_xmm_xmm(xmm::XMM0, xmm::XMM1);
            break;
        case BinOp::Mod:
            // fmod: a - trunc(a/b) * b
            emit_movsd_xmm_xmm(xmm::XMM2, xmm::XMM1); // XMM2 = a
            emit_movsd_xmm_xmm(xmm::XMM3, xmm::XMM0); // XMM3 = b
            emit_divsd(xmm::XMM1, xmm::XMM0);           // XMM1 = a / b
            emit_cvttsd2si(reg::RAX, xmm::XMM1);         // RAX = trunc(a/b)
            emit_cvtsi2sd(xmm::XMM1, reg::RAX);          // XMM1 = (double)trunc
            emit_mulsd(xmm::XMM1, xmm::XMM3);           // XMM1 = trunc(a/b) * b
            emit_subsd(xmm::XMM2, xmm::XMM1);           // XMM2 = a - trunc*b
            emit_movsd_xmm_xmm(xmm::XMM0, xmm::XMM2);  // XMM0 = resultado
            break;
    }
}

// ======================================================================
// OPERAÇÕES DE COMPARAÇÃO (com suporte a float e string)
// ======================================================================

void emit_cmpop(const CmpOpExpr& node) {
    RuntimeType lt = infer_expr_type(*node.left);
    RuntimeType rt = infer_expr_type(*node.right);
    bool is_null = (lt == RuntimeType::Null || rt == RuntimeType::Null);
    bool is_float = (lt == RuntimeType::Float || rt == RuntimeType::Float);
    bool is_string = (lt == RuntimeType::String || rt == RuntimeType::String);

    if (is_null) {
        // Comparação com nulo: sempre usa inteiro (nulo = 0)
        emit_cmpop_int(node);
    } else if (is_string) {
        emit_cmpop_string(node);
    } else if (is_float) {
        emit_cmpop_float(node);
    } else {
        emit_cmpop_int(node);
    }
}

// ======================================================================
// COMPARAÇÃO DE STRINGS (via strcmp)
//
// strcmp(a, b) retorna: 0 se iguais, <0 se a < b, >0 se a > b
// Usa PlatformDefs::ARG1/ARG2 para passing convention
// ======================================================================

void emit_cmpop_string(const CmpOpExpr& node) {
    // Avaliar left → salvar em temporário
    emit_expr(*node.left);
    std::string tmp = "__strcmp_left_" + std::to_string(text_->pos());
    int32_t tmp_off = alloc_local(tmp);
    emit_mov_rbp_reg(tmp_off, reg::RAX);

    // Avaliar right → ARG2
    emit_expr(*node.right);
    emit_mov_reg_reg(PlatformDefs::ARG2, reg::RAX);

    // Carregar left → ARG1
    emit_mov_reg_rbp(PlatformDefs::ARG1, tmp_off);

    // Chamar strcmp
    emit_call_extern("strcmp");

    // RAX = resultado do strcmp — comparar com 0
    emit_cmp_reg_imm32(reg::RAX, 0);

    uint8_t cc = cmpop_to_cc(node.op);
    emit_setcc(cc, reg::RAX);
    emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
}

void emit_cmpop_int(const CmpOpExpr& node) {
    emit_expr(*node.left);
    emit_push(reg::RAX);
    emit_expr(*node.right);
    emit_mov_reg_reg(reg::RCX, reg::RAX);
    emit_pop(reg::RAX);

    emit_cmp_reg_reg(reg::RAX, reg::RCX);

    uint8_t cc = cmpop_to_cc(node.op);
    emit_setcc(cc, reg::RAX);
    emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
}

void emit_cmpop_float(const CmpOpExpr& node) {
    emit_expr_as_float(*node.left);
    emit_movsd_xmm_xmm(xmm::XMM1, xmm::XMM0);

    emit_expr_as_float(*node.right);

    // UCOMISD XMM1, XMM0
    emit_ucomisd(xmm::XMM1, xmm::XMM0);

    uint8_t cc = cmpop_to_float_cc(node.op);
    emit_setcc(cc, reg::RAX);
    emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
}

// ======================================================================
// OPERAÇÕES LÓGICAS (short-circuit)
// ======================================================================

void emit_logicop(const LogicOpExpr& node) {
    if (node.op == LogicOp::And) {
        emit_expr(*node.left);
        if (infer_expr_type(*node.left) == RuntimeType::Float) {
            emit_cvttsd2si(reg::RAX, xmm::XMM0);
        }
        emit_test_reg_reg(reg::RAX, reg::RAX);
        size_t skip_patch = emit_je_rel32();

        emit_expr(*node.right);
        if (infer_expr_type(*node.right) == RuntimeType::Float) {
            emit_cvttsd2si(reg::RAX, xmm::XMM0);
        }
        emit_test_reg_reg(reg::RAX, reg::RAX);
        emit_setcc(CC_NE, reg::RAX);
        emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
        size_t end_patch = emit_jmp_rel32();

        patch_jump(skip_patch);
        emit_xor_reg_reg(reg::RAX, reg::RAX);
        patch_jump(end_patch);
    }
    else { // Or
        emit_expr(*node.left);
        if (infer_expr_type(*node.left) == RuntimeType::Float) {
            emit_cvttsd2si(reg::RAX, xmm::XMM0);
        }
        emit_test_reg_reg(reg::RAX, reg::RAX);
        size_t skip_patch = emit_jne_rel32();

        emit_expr(*node.right);
        if (infer_expr_type(*node.right) == RuntimeType::Float) {
            emit_cvttsd2si(reg::RAX, xmm::XMM0);
        }
        emit_test_reg_reg(reg::RAX, reg::RAX);
        emit_setcc(CC_NE, reg::RAX);
        emit_movzx_reg64_reg8(reg::RAX, reg::RAX);
        size_t end_patch = emit_jmp_rel32();

        patch_jump(skip_patch);
        emit_mov_reg_imm32(reg::RAX, 1);
        patch_jump(end_patch);
    }
}

// ======================================================================
// CHAMADA DE FUNÇÃO — unificado via PlatformDefs
//
// Windows x64: RCX, RDX, R8, R9 (4 regs), shadow space 32 bytes
// System V:    RDI, RSI, RDX, RCX, R8, R9 (6 regs), sem shadow space, AL=0
//
// ======================================================================

void emit_chamada(const ChamadaExpr& node) {
    // Verificar se é função nativa (built-in) — emitir inline
    if constexpr (PlatformDefs::is_windows) {
        if (is_native_func(node.name)) {
            validate_native_call(node.name, node.args);
            if (emit_native_call(node)) return;
        }
    } else {
        if (emit_native_call(node)) return;
    }

    // Registradores de argumento da plataforma
    constexpr size_t MAX_REG_ARGS = PlatformDefs::is_windows ? 4 : 6;

    const uint8_t arg_regs[] = {
        PlatformDefs::ARG1, PlatformDefs::ARG2,
        PlatformDefs::ARG3, PlatformDefs::ARG4,
        // ARG5 e ARG6 só existem no Linux (System V tem 6 regs)
        PlatformDefs::is_linux ? PlatformDefs::ARG5 : uint8_t(0),
        PlatformDefs::is_linux ? PlatformDefs::ARG6 : uint8_t(0),
    };

    const uint8_t arg_xmms[] = {
        PlatformDefs::FLOAT_ARG1, PlatformDefs::FLOAT_ARG2,
        PlatformDefs::FLOAT_ARG3, PlatformDefs::FLOAT_ARG4,
    };

    // Avaliar args e salvar em temporários
    std::vector<int32_t> arg_offsets;
    std::vector<RuntimeType> arg_types;

    // Buscar tipos de parâmetros declarados no JSON (funções externas)
    std::vector<RuntimeType>* extern_param_types = nullptr;
    {
        auto ept = func_param_types_.find(node.name);
        if (ept != func_param_types_.end()) {
            extern_param_types = &ept->second;
        }
    }

    for (size_t i = 0; i < node.args.size(); i++) {
        RuntimeType type = infer_expr_type(*node.args[i]);

        // Registrar tipos dos argumentos na FuncInfo
        auto fit = declared_funcs_.find(node.name);
        if (fit != declared_funcs_.end()) {
            if (fit->second.param_types.size() < node.args.size()) {
                fit->second.param_types.resize(node.args.size(), RuntimeType::Unknown);
            }
            if (fit->second.param_types[i] == RuntimeType::Unknown) {
                fit->second.param_types[i] = type;
            }
        }

        // Verificar se o JSON espera decimal nesta posição
        bool needs_float_conv = false;
        if (extern_param_types && i < extern_param_types->size()) {
            if ((*extern_param_types)[i] == RuntimeType::Float && type != RuntimeType::Float) {
                needs_float_conv = true;
            }
        }

        if (needs_float_conv) {
            // Emitir expressão como float: se é int, converte RAX→XMM0
            emit_expr(*node.args[i]);
            if (type != RuntimeType::Float) {
                emit_cvtsi2sd(xmm::XMM0, reg::RAX);
            }
            type = RuntimeType::Float;
        } else {
            emit_expr(*node.args[i]);
        }

        arg_types.push_back(type);

        std::string temp = "__arg_" + std::to_string(i) + "_" +
                           std::to_string(text_->pos());
        int32_t off = alloc_local(temp);

        if (type == RuntimeType::Float) {
            emit_movsd_rbp_xmm(off, xmm::XMM0);
        } else {
            emit_mov_rbp_reg(off, reg::RAX);
        }
        arg_offsets.push_back(off);
    }

    // Carregar args nos registradores
    for (size_t i = 0; i < node.args.size(); i++) {
        if (i < MAX_REG_ARGS) {
            if (arg_types[i] == RuntimeType::Float) {
                emit_movsd_xmm_rbp(arg_xmms[i], arg_offsets[i]);
                // Windows variádica: float deve ir TAMBÉM no GPR
                if constexpr (PlatformDefs::VARIADIC_FLOAT_MIRROR_GPR) {
                    emit_movq_gpr_xmm(arg_regs[i], arg_xmms[i]);
                }
            } else {
                emit_mov_reg_rbp(arg_regs[i], arg_offsets[i]);
            }
        } else {
            // Args extras na stack
            if (arg_types[i] == RuntimeType::Float) {
                emit_movsd_xmm_rbp(xmm::XMM0, arg_offsets[i]);
                emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
            } else {
                emit_mov_reg_rbp(reg::RAX, arg_offsets[i]);
            }

            int32_t stack_off;
            if constexpr (PlatformDefs::is_windows) {
                // Windows: shadow space (32) + posição extra
                stack_off = 32 + static_cast<int32_t>((i - MAX_REG_ARGS) * 8);
            } else {
                // Linux: sem shadow space
                stack_off = static_cast<int32_t>((i - MAX_REG_ARGS) * 8);
            }

            // mov [rsp + stack_off], rax
            emit_rex_w(reg::RAX, reg::RSP);
            text_->emit_u8(0x89);
            if (stack_off == 0) {
                text_->emit_u8(0x04);
                text_->emit_u8(0x24);
            } else if (stack_off < 128) {
                text_->emit_u8(0x44);
                text_->emit_u8(0x24);
                text_->emit_i8(static_cast<int8_t>(stack_off));
            } else {
                text_->emit_u8(0x84);
                text_->emit_u8(0x24);
                text_->emit_i32(stack_off);
            }
        }
    }

    // System V variádica: AL = 0
    if constexpr (PlatformDefs::VARIADIC_NEEDS_AL) {
        emit_xor_reg_reg(reg::RAX, reg::RAX);
    }

    // Resolver símbolo e chamar
    uint32_t sym_idx;
    if (emitter_.has_symbol(node.name)) {
        sym_idx = emitter_.symbol_index(node.name);
    } else {
        sym_idx = emitter_.add_extern_symbol(node.name);
    }

    emit_call_symbol(sym_idx);

    // Se a função externa retorna decimal, o valor está em RAX como bits de double.
    // Mover pra XMM0 pra que o resto do codegen trate como float.
    auto ret_it = func_return_types_.find(node.name);
    if (ret_it != func_return_types_.end() && ret_it->second == RuntimeType::Float) {
        // MOVQ XMM0, RAX (mover bits raw, sem converter)
        emit_movq_xmm_gpr(xmm::XMM0, reg::RAX);
    }
}

// ======================================================================
// STRING INTERPOLATION (como expressão — placeholder)
// ======================================================================

void emit_string_interp(const StringInterp& node) {
    if (!node.parts.empty() && !node.parts[0].is_var) {
        emit_load_string(reg::RAX, node.parts[0].value);
    } else {
        emit_mov_reg_imm32(reg::RAX, 0);
    }
}