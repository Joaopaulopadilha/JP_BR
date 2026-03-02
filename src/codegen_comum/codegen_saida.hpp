// codegen_saida.hpp
// Emissão de saida/saidal — unificado Windows x64 e Linux x86-64 via PlatformDefs

// ======================================================================
// HELPERS DE CHAMADA PRINTF (abstraem calling convention)
// ======================================================================

// Chama printf via símbolo (funciona em ambas plataformas)
// Deve ser chamado APÓS os argumentos estarem nos registradores corretos.
// Em Linux (System V variádica), seta AL = 0 automaticamente quando
// não há argumentos float.
void emit_call_printf_no_float() {
    if constexpr (PlatformDefs::VARIADIC_NEEDS_AL) {
        emit_xor_reg_reg(reg::RAX, reg::RAX);  // AL = 0 (sem XMM args)
    }
    emit_call_extern("printf");
}

// Chama printf com 1 argumento float em XMM0.
// Em Linux: seta AL = 1. Em Windows: já está ok (sem AL).
void emit_call_printf_with_float() {
    if constexpr (PlatformDefs::VARIADIC_NEEDS_AL) {
        emit_mov_reg_imm32(reg::RAX, 1);  // AL = 1 (1 XMM arg)
    }
    emit_call_extern("printf");
}

// ======================================================================
// CORES ANSI — emite escape codes antes e depois do conteúdo
// ======================================================================

void emit_ansi_cor_start(Cor cor) {
    if (cor == Cor::Nenhuma) return;
    const char* code = nullptr;
    switch (cor) {
        case Cor::Vermelho: code = "\033[31m"; break;
        case Cor::Verde:    code = "\033[32m"; break;
        case Cor::Azul:     code = "\033[34m"; break;
        case Cor::Amarelo:  code = "\033[33m"; break;
        default: return;
    }
    emit_load_string(PlatformDefs::ARG1, "%s");
    emit_load_string(PlatformDefs::ARG2, code);
    emit_call_printf_no_float();
}

void emit_ansi_cor_reset(Cor cor) {
    if (cor == Cor::Nenhuma) return;
    emit_load_string(PlatformDefs::ARG1, "%s");
    emit_load_string(PlatformDefs::ARG2, "\033[0m");
    emit_call_printf_no_float();
}

// ======================================================================
// HELPER: imprime double em XMM0 via printf
//
// Windows x64 variádica: printf("%g", val)
//   RCX = formato, XMM1 = double, RDX = double como bits (mirror)
//
// System V variádica: printf("%g", val)
//   RDI = formato, XMM0 = double, AL = 1
//
// ======================================================================

void emit_printf_double(const std::string& fmt) {
    // XMM0 já contém o double
    if constexpr (PlatformDefs::VARIADIC_FLOAT_MIRROR_GPR) {
        // Windows: mover XMM0 → XMM1 (2º arg float) e XMM0 → RDX (mirror GPR)
        emit_movsd_xmm_xmm(xmm::XMM1, xmm::XMM0);
        emit_movq_gpr_xmm(PlatformDefs::ARG2, xmm::XMM0);
        emit_load_string(PlatformDefs::ARG1, fmt);
    } else {
        // Linux: XMM0 já está no lugar certo (1º float arg)
        emit_load_string(PlatformDefs::ARG1, fmt);
    }
    emit_call_printf_with_float();
}

// ======================================================================
// HELPER: imprime newline
// ======================================================================

void emit_printf_newline() {
    emit_load_string(PlatformDefs::ARG1, "\n");
    emit_call_printf_no_float();
}

// ======================================================================
// SAIDA (print)
// ======================================================================

void emit_saida(const SaidaStmt& node) {
    auto* expr_ptr = node.value.get();

    // Cor: emitir escape code antes
    emit_ansi_cor_start(node.cor);

    // Caso especial: lista
    if constexpr (PlatformDefs::is_windows) {
        // Windows: emit_saida_list retorna bool
        if (emit_saida_list(*expr_ptr)) {
            emit_ansi_cor_reset(node.cor);
            return;
        }
    } else {
        // Linux: verificar se é variável lista
        if (std::holds_alternative<VarExpr>(expr_ptr->node)) {
            auto& var = std::get<VarExpr>(expr_ptr->node);
            if (is_list_var(var.name)) {
                emit_saida_list(var.name, node.newline);
                emit_ansi_cor_reset(node.cor);
                return;
            }
        }
    }

    // Caso especial: StringInterp
    if (std::holds_alternative<StringInterp>(expr_ptr->node)) {
        auto& interp = std::get<StringInterp>(expr_ptr->node);
        emit_saida_interp(interp, node.newline);
        emit_ansi_cor_reset(node.cor);
        return;
    }

    // Caso especial: "string" + expr (concatenação com +)
    if (std::holds_alternative<BinOpExpr>(expr_ptr->node)) {
        auto& binop = std::get<BinOpExpr>(expr_ptr->node);
        if (binop.op == BinOp::Add) {
            RuntimeType lt = infer_expr_type(*binop.left);
            RuntimeType rt = infer_expr_type(*binop.right);
            if (lt == RuntimeType::String || rt == RuntimeType::String) {
                emit_saida_concat(*binop.left, *binop.right, node.newline);
                emit_ansi_cor_reset(node.cor);
                return;
            }
        }
    }

    // Caso especial: ConcatExpr (múltiplos argumentos separados por vírgula)
    if (std::holds_alternative<ConcatExpr>(expr_ptr->node)) {
        emit_saida_concat_expr(*expr_ptr, node.newline);
        emit_ansi_cor_reset(node.cor);
        return;
    }

    // Inferir tipo
    RuntimeType type = infer_expr_type(*node.value);

    switch (type) {
        case RuntimeType::String: {
            std::string fmt = node.newline ? "%s\n" : "%s";
            emit_load_string(PlatformDefs::ARG1, fmt);
            emit_expr(*node.value);
            emit_mov_reg_reg(PlatformDefs::ARG2, reg::RAX);
            emit_call_printf_no_float();
            break;
        }

        case RuntimeType::Float: {
            std::string fmt = node.newline ? "%g\n" : "%g";
            emit_expr(*node.value);
            emit_printf_double(fmt);
            break;
        }

        case RuntimeType::Bool: {
            emit_expr(*node.value);
            emit_test_reg_reg(reg::RAX, reg::RAX);
            size_t false_patch = emit_je_rel32();

            std::string fmt_s = node.newline ? "%s\n" : "%s";
            emit_load_string(PlatformDefs::ARG1, fmt_s);
            emit_load_string(PlatformDefs::ARG2, "verdadeiro");
            emit_call_printf_no_float();
            size_t end_patch = emit_jmp_rel32();

            patch_jump(false_patch);
            emit_load_string(PlatformDefs::ARG1, fmt_s);
            emit_load_string(PlatformDefs::ARG2, "falso");
            emit_call_printf_no_float();
            patch_jump(end_patch);
            break;
        }

        default: {
            std::string fmt = node.newline ? "%d\n" : "%d";
            emit_expr(*node.value);
            emit_mov_reg_reg(PlatformDefs::ARG2, reg::RAX);
            emit_load_string(PlatformDefs::ARG1, fmt);
            emit_call_printf_no_float();
            break;
        }
    }

    emit_ansi_cor_reset(node.cor);
}

// ======================================================================
// SAIDA com StringInterp: emite printf para cada parte
// Suporta variáveis simples, auto.attr e obj.attr
// ======================================================================

void emit_saida_interp(const StringInterp& interp, bool newline) {
    for (auto& part : interp.parts) {
        if (!part.is_var) {
            // Texto literal
            emit_load_string(PlatformDefs::ARG1, "%s");
            emit_load_string(PlatformDefs::ARG2, part.value);
            emit_call_printf_no_float();
        } else {
            if (part.expr) {
                // Expressão completa já parseada
                RuntimeType type = infer_expr_type(*part.expr);
                emit_expr(*part.expr);
                emit_saida_value(type);
            } else {
                // Variável por nome — pode conter '.' (ex: auto.marca, obj.attr)
                emit_saida_interp_var(part.value);
            }
        }
    }

    if (newline) {
        emit_printf_newline();
    }
}

// ======================================================================
// HELPER: emite valor de interpolação por nome (com suporte a ponto)
// Trata "auto.attr", "var.attr" e variáveis simples
// Unificado: combina lógica de Windows (emit_saida_interp_dotted)
// e Linux (emit_saida_interp_var) pegando o mais completo de cada
// ======================================================================

void emit_saida_interp_var(const std::string& name) {
    // Verificar se contém '.' — indicando acesso a atributo
    size_t dot_pos = name.find('.');
    if (dot_pos != std::string::npos) {
        std::string obj_name = name.substr(0, dot_pos);
        std::string attr_name = name.substr(dot_pos + 1);

        // Resolver tipo do atributo e carregar ponteiro do objeto
        RuntimeType attr_type = RuntimeType::Unknown;

        if (obj_name == "auto") {
            // Dentro de método: auto.attr
            if (current_class_) {
                attr_type = current_class_->get_attr_type(attr_name);
                int32_t attr_off = current_class_->find_attr_offset(attr_name);
                if (attr_off >= 0) {
                    // Carregar ponteiro auto
                    emit_mov_reg_rbp(reg::RAX, auto_local_offset_);
                    // Carregar atributo
                    if (attr_type == RuntimeType::Float) {
                        text_->emit_u8(0xF2);
                        text_->emit_u8(0x0F);
                        text_->emit_u8(0x10);
                        text_->emit_u8(0x80);
                        text_->emit_i32(attr_off);
                    } else {
                        emit_rex_w(reg::RAX, reg::RAX);
                        text_->emit_u8(0x8B);
                        text_->emit_u8(0x80);
                        text_->emit_i32(attr_off);
                    }
                    emit_saida_value(attr_type);
                    return;
                }
            }
        } else {
            // var.attr — variável que contém instância
            auto vit = var_instance_class_.find(obj_name);
            if (vit != var_instance_class_.end()) {
                auto cit = declared_classes_.find(vit->second);
                if (cit != declared_classes_.end()) {
                    attr_type = cit->second.get_attr_type(attr_name);
                    int32_t attr_off = cit->second.find_attr_offset(attr_name);
                    if (attr_off >= 0) {
                        int32_t var_off = find_local(obj_name);
                        emit_mov_reg_rbp(reg::RAX, var_off);
                        if (attr_type == RuntimeType::Float) {
                            text_->emit_u8(0xF2);
                            text_->emit_u8(0x0F);
                            text_->emit_u8(0x10);
                            text_->emit_u8(0x80);
                            text_->emit_i32(attr_off);
                        } else {
                            emit_rex_w(reg::RAX, reg::RAX);
                            text_->emit_u8(0x8B);
                            text_->emit_u8(0x80);
                            text_->emit_i32(attr_off);
                        }
                        emit_saida_value(attr_type);
                        return;
                    }
                }
            }

            // Suporte a encadeamento: obj.sub.attr (múltiplos pontos)
            // Construir expressão AttrGetExpr em tempo de compilação
            ExprPtr object_expr = std::make_unique<Expr>(VarExpr{obj_name, 0});
            size_t next_dot = attr_name.find('.');
            while (next_dot != std::string::npos) {
                std::string current_attr = attr_name.substr(0, next_dot);
                object_expr = std::make_unique<Expr>(AttrGetExpr{
                    std::move(object_expr), current_attr, 0
                });
                attr_name = attr_name.substr(next_dot + 1);
                next_dot = attr_name.find('.');
            }

            Expr attr_expr(AttrGetExpr{
                std::move(object_expr), attr_name, 0
            });

            RuntimeType type = infer_expr_type(attr_expr);
            emit_expr(attr_expr);
            emit_saida_value(type);
            return;
        }

        // Fallback: imprimir 0
        emit_mov_reg_imm32(reg::RAX, 0);
        emit_saida_value(RuntimeType::Int);
    } else {
        // Variável simples por nome
        auto it = var_types_.find(name);
        RuntimeType type = (it != var_types_.end()) ? it->second : RuntimeType::Unknown;
        int32_t offset = find_local(name);
        if (type == RuntimeType::Float) {
            emit_movsd_xmm_rbp(xmm::XMM0, offset);
        } else {
            emit_mov_reg_rbp(reg::RAX, offset);
        }
        emit_saida_value(type);
    }
}

// ======================================================================
// Helper: imprime valor com formato do tipo
// Int/String/Bool → valor em RAX
// Float           → valor em XMM0
// ======================================================================

void emit_saida_value(RuntimeType type) {
    switch (type) {
        case RuntimeType::String:
            emit_mov_reg_reg(PlatformDefs::ARG2, reg::RAX);
            emit_load_string(PlatformDefs::ARG1, "%s");
            emit_call_printf_no_float();
            break;

        case RuntimeType::Float:
            emit_printf_double("%g");
            break;

        case RuntimeType::Bool: {
            emit_test_reg_reg(reg::RAX, reg::RAX);
            size_t false_patch = emit_je_rel32();
            emit_load_string(PlatformDefs::ARG1, "%s");
            emit_load_string(PlatformDefs::ARG2, "verdadeiro");
            emit_call_printf_no_float();
            size_t end_patch = emit_jmp_rel32();
            patch_jump(false_patch);
            emit_load_string(PlatformDefs::ARG1, "%s");
            emit_load_string(PlatformDefs::ARG2, "falso");
            emit_call_printf_no_float();
            patch_jump(end_patch);
            break;
        }

        default:
            emit_mov_reg_reg(PlatformDefs::ARG2, reg::RAX);
            emit_load_string(PlatformDefs::ARG1, "%d");
            emit_call_printf_no_float();
            break;
    }
}

// ======================================================================
// SAIDA com concatenação (string + valor)
// ======================================================================

void emit_saida_concat(const Expr& left, const Expr& right, bool newline) {
    emit_saida_concat_part(left);
    emit_saida_concat_part(right);

    if (newline) {
        emit_printf_newline();
    }
}

void emit_saida_concat_part(const Expr& expr) {
    if (std::holds_alternative<BinOpExpr>(expr.node)) {
        auto& binop = std::get<BinOpExpr>(expr.node);
        if (binop.op == BinOp::Add) {
            RuntimeType lt = infer_expr_type(*binop.left);
            RuntimeType rt = infer_expr_type(*binop.right);
            if (lt == RuntimeType::String || rt == RuntimeType::String) {
                emit_saida_concat_part(*binop.left);
                emit_saida_concat_part(*binop.right);
                return;
            }
        }
    }

    // ConcatExpr: desdobrar recursivamente cada lado
    if (std::holds_alternative<ConcatExpr>(expr.node)) {
        auto& concat = std::get<ConcatExpr>(expr.node);
        emit_saida_concat_part(*concat.left);
        emit_saida_concat_part(*concat.right);
        return;
    }

    RuntimeType type = infer_expr_type(expr);
    emit_expr(expr);
    emit_saida_value(type);
}

// ======================================================================
// SAIDA com ConcatExpr (múltiplos argumentos via vírgula)
// ======================================================================

void emit_saida_concat_expr(const Expr& expr, bool newline) {
    emit_saida_concat_part(expr);

    if (newline) {
        emit_printf_newline();
    }
}
