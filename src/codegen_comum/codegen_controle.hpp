// codegen_controle.hpp
// Emissão de estruturas de controle — unificado Windows/Linux (código idêntico)
// if/ou_se/senao, enquanto, repetir, para, parar, continuar

// ======================================================================
// IF / OU_SE / SENAO
// ======================================================================

void emit_if(const IfStmt& node) {
    std::vector<size_t> end_patches;

    for (size_t i = 0; i < node.branches.size(); i++) {
        auto& branch = node.branches[i];

        if (branch.condition) {
            emit_expr(*branch.condition);
            emit_test_reg_reg(reg::RAX, reg::RAX);
            size_t skip_patch = emit_je_rel32();

            for (auto& stmt : branch.body) {
                emit_stmt(*stmt);
            }

            size_t end_patch = emit_jmp_rel32();
            end_patches.push_back(end_patch);
            patch_jump(skip_patch);
        } else {
            // senao
            for (auto& stmt : branch.body) {
                emit_stmt(*stmt);
            }
        }
    }

    for (auto& ep : end_patches) {
        patch_jump(ep);
    }
}

// ======================================================================
// ENQUANTO (while)
// ======================================================================

void emit_enquanto(const EnquantoStmt& node) {
    LoopContext ctx;
    ctx.loop_start = text_->pos();
    loop_stack_.push_back(ctx);

    size_t loop_top = text_->pos();

    emit_expr(*node.condition);
    emit_test_reg_reg(reg::RAX, reg::RAX);
    size_t exit_patch = emit_je_rel32();

    for (auto& stmt : node.body) {
        emit_stmt(*stmt);
    }

    // Jump de volta pro topo
    text_->emit_u8(0xE9);
    int32_t back_offset = static_cast<int32_t>(loop_top) -
                          static_cast<int32_t>(text_->pos() + 4);
    text_->emit_i32(back_offset);

    patch_jump(exit_patch);

    auto& lc = loop_stack_.back();
    for (auto& bp : lc.break_patches) patch_jump(bp);
    for (auto& cp : lc.continue_patches) {
        int32_t rel = static_cast<int32_t>(loop_top) -
                      static_cast<int32_t>(cp + 4);
        text_->patch_i32(cp, rel);
    }
    loop_stack_.pop_back();
}

// ======================================================================
// REPETIR (repeat N times)
// ======================================================================

void emit_repetir(const RepetirStmt& node) {
    emit_expr(*node.count);

    std::string counter_name = "__repetir_" + std::to_string(text_->pos());
    int32_t counter_off = alloc_local(counter_name);
    emit_mov_rbp_reg(counter_off, reg::RAX);

    LoopContext ctx;
    ctx.loop_start = text_->pos();
    loop_stack_.push_back(ctx);

    size_t loop_top = text_->pos();

    emit_mov_reg_rbp(reg::RAX, counter_off);
    emit_cmp_reg_imm32(reg::RAX, 0);
    size_t exit_patch = emit_jle_rel32();

    for (auto& stmt : node.body) {
        emit_stmt(*stmt);
    }

    // Decrementar
    emit_mov_reg_rbp(reg::RAX, counter_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xFF); // dec rax
    text_->emit_u8(0xC8);
    emit_mov_rbp_reg(counter_off, reg::RAX);

    // Jump back
    text_->emit_u8(0xE9);
    int32_t back_offset = static_cast<int32_t>(loop_top) -
                          static_cast<int32_t>(text_->pos() + 4);
    text_->emit_i32(back_offset);

    patch_jump(exit_patch);

    auto& lc = loop_stack_.back();
    for (auto& bp : lc.break_patches) patch_jump(bp);
    for (auto& cp : lc.continue_patches) {
        int32_t rel = static_cast<int32_t>(loop_top) -
                      static_cast<int32_t>(cp + 4);
        text_->patch_i32(cp, rel);
    }
    loop_stack_.pop_back();
}

// ======================================================================
// PARA (for var em intervalo(start, end, step))
// ======================================================================

void emit_para(const ParaStmt& node) {
    emit_expr(*node.start);
    int32_t var_off = alloc_local(node.var);
    emit_mov_rbp_reg(var_off, reg::RAX);

    // Registrar tipo da variável do loop como Int
    var_types_[node.var] = RuntimeType::Int;

    emit_expr(*node.end);
    std::string end_name = "__para_end_" + std::to_string(text_->pos());
    int32_t end_off = alloc_local(end_name);
    emit_mov_rbp_reg(end_off, reg::RAX);

    std::string step_name = "__para_step_" + std::to_string(text_->pos());
    int32_t step_off;
    if (node.step) {
        emit_expr(*node.step);
        step_off = alloc_local(step_name);
        emit_mov_rbp_reg(step_off, reg::RAX);
    } else {
        step_off = alloc_local(step_name);
        emit_mov_rbp_imm32(step_off, 1);
    }

    LoopContext ctx;
    ctx.loop_start = text_->pos();
    loop_stack_.push_back(ctx);

    size_t loop_top = text_->pos();

    emit_mov_reg_rbp(reg::RAX, var_off);
    emit_mov_reg_rbp(reg::RCX, end_off);
    emit_cmp_reg_reg(reg::RAX, reg::RCX);
    size_t exit_patch = emit_jge_rel32();

    for (auto& stmt : node.body) {
        emit_stmt(*stmt);
    }

    // Incrementar: var += step
    emit_mov_reg_rbp(reg::RAX, var_off);
    emit_mov_reg_rbp(reg::RCX, step_off);
    emit_add_reg_reg(reg::RAX, reg::RCX);
    emit_mov_rbp_reg(var_off, reg::RAX);

    // Jump back
    text_->emit_u8(0xE9);
    int32_t back_offset = static_cast<int32_t>(loop_top) -
                          static_cast<int32_t>(text_->pos() + 4);
    text_->emit_i32(back_offset);

    patch_jump(exit_patch);

    auto& lc = loop_stack_.back();
    for (auto& bp : lc.break_patches) patch_jump(bp);
    for (auto& cp : lc.continue_patches) {
        int32_t rel = static_cast<int32_t>(loop_top) -
                      static_cast<int32_t>(cp + 4);
        text_->patch_i32(cp, rel);
    }
    loop_stack_.pop_back();
}

// ======================================================================
// PARAR / CONTINUAR (break / continue)
// ======================================================================

void emit_parar() {
    if (loop_stack_.empty()) return;
    size_t patch = emit_jmp_rel32();
    loop_stack_.back().break_patches.push_back(patch);
}

void emit_continuar() {
    if (loop_stack_.empty()) return;
    size_t patch = emit_jmp_rel32();
    loop_stack_.back().continue_patches.push_back(patch);
}
