// codegen_atribuicao.hpp
// Emissão de atribuição de variáveis — unificado Windows/Linux
// Type tracking, suporte a double (XMM0), listas, instâncias de classe

// ======================================================================
// ASSIGN
// ======================================================================

void emit_assign(const AssignStmt& node) {
    // Detectar chamada de construtor: var = Classe.metodo(args)
    std::string class_name, method_name;
    if (is_constructor_call(node, class_name, method_name)) {
        if constexpr (PlatformDefs::is_windows) {
            // Windows: emitir chamada estática e armazenar ponteiro
            auto& mc = std::get<MetodoChamadaExpr>(node.value->node);
            emit_static_method_call(class_name, method_name, mc);
            int32_t offset = find_local(node.name);
            emit_mov_rbp_reg(offset, reg::RAX);
            var_types_[node.name] = RuntimeType::Unknown;
            var_instance_class_[node.name] = class_name;
            return;
        } else {
            // Linux: registrar classe e continuar fluxo normal
            var_instance_class_[node.name] = class_name;
        }
    }

    // Inferir tipo
    RuntimeType type = infer_expr_type(*node.value);
    var_types_[node.name] = type;

    // Detectar lista literal: var = [...]
    if (std::holds_alternative<ListLitExpr>(node.value->node)) {
        auto& list_node = std::get<ListLitExpr>(node.value->node);

        if constexpr (PlatformDefs::is_windows) {
            emit_expr(*node.value);
            int32_t offset = find_local(node.name);
            emit_mov_rbp_reg(offset, reg::RAX);
            var_types_[node.name] = RuntimeType::Unknown;
            var_is_list_.insert(node.name);
            var_list_elem_type_[node.name] = infer_list_element_type(list_node);
            std::string elem_class = infer_list_element_class(list_node);
            if (!elem_class.empty()) {
                var_list_instance_class_[node.name] = elem_class;
            }
            return;
        } else {
            var_is_list_.insert(node.name);
            if (!list_node.elements.empty()) {
                var_list_elem_type_[node.name] = infer_expr_type(*list_node.elements[0]);
                // Detectar se elementos são chamadas de construtor
                auto& first = list_node.elements[0];
                if (std::holds_alternative<MetodoChamadaExpr>(first->node)) {
                    auto& mc = std::get<MetodoChamadaExpr>(first->node);
                    if (std::holds_alternative<VarExpr>(mc.object->node)) {
                        auto& var = std::get<VarExpr>(mc.object->node);
                        if (declared_classes_.find(var.name) != declared_classes_.end()) {
                            var_list_instance_class_[node.name] = var.name;
                        }
                    }
                }
            } else {
                var_list_elem_type_[node.name] = RuntimeType::Unknown;
            }
            // Linux: continua pro emit_expr abaixo
        }
    }

    // Detectar acesso a lista: var = lista[indice]  (só Windows tem early return)
    if constexpr (PlatformDefs::is_windows) {
        if (std::holds_alternative<IndexGetExpr>(node.value->node)) {
            auto& idx_node = std::get<IndexGetExpr>(node.value->node);
            RuntimeType elem_type = RuntimeType::Unknown;
            if (std::holds_alternative<VarExpr>(idx_node.object->node)) {
                auto& src_var = std::get<VarExpr>(idx_node.object->node);
                elem_type = get_list_elem_type(src_var.name);
            }
            emit_expr(*node.value);
            int32_t offset = find_local(node.name);
            if (elem_type == RuntimeType::Float) {
                emit_movsd_rbp_xmm(offset, xmm::XMM0);
            } else {
                emit_mov_rbp_reg(offset, reg::RAX);
            }
            var_types_[node.name] = elem_type;
            return;
        }
    }

    // Caso geral: avaliar expressão e salvar
    emit_expr(*node.value);
    int32_t offset = find_local(node.name);

    if (type == RuntimeType::Float) {
        emit_movsd_rbp_xmm(offset, xmm::XMM0);
    } else {
        emit_mov_rbp_reg(offset, reg::RAX);
    }
}

// ======================================================================
// INDEX SET: lista[indice] = valor (para arrays simples)
// Para listas struct, usa emit_list_index_set de codegen_listas.hpp
// ======================================================================

void emit_index_set(const IndexSetStmt& node) {
    // Avaliar o valor → salvar temporariamente
    emit_expr(*node.value);
    std::string val_tmp = "__idxset_val_" + std::to_string(text_->pos());
    int32_t val_off = alloc_local(val_tmp);
    emit_mov_rbp_reg(val_off, reg::RAX);

    // Avaliar o índice → salvar temporariamente
    emit_expr(*node.index);
    std::string idx_tmp = "__idxset_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx_tmp);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    // Carregar ponteiro base da lista
    int32_t base_off = find_local(node.name);
    emit_mov_reg_rbp(reg::RCX, base_off);

    // Carregar índice → RAX
    emit_mov_reg_rbp(reg::RAX, idx_off);

    // Calcular endereço: RCX + RAX * 8
    // LEA RCX, [RCX + RAX*8]
    emit_rex_w(reg::RCX, reg::RCX);
    text_->emit_u8(0x8D);
    text_->emit_u8(0x0C); // ModR/M: RCX, SIB follows
    text_->emit_u8(0xC1); // SIB: scale=8(11), index=RAX(000), base=RCX(001)

    // Carregar valor e armazenar em [RCX]
    emit_mov_reg_rbp(reg::RAX, val_off);
    // MOV [RCX], RAX
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x01); // ModR/M: mod=00, reg=RAX, rm=RCX
}
