// codegen_listas.hpp
// Emissão de listas dinâmicas — unificado Windows/Linux via PlatformDefs

// ======================================================================
// LAYOUT DE MEMÓRIA (POR PLATAFORMA)
// ======================================================================
//
// Windows: tagged values (16 bytes/elemento)
//   Header (24 bytes alocado com malloc):
//     [+0]  capacidade (int64)
//     [+8]  tamanho    (int64)
//     [+16] dados      (ponteiro para array de elementos)
//   Elemento (16 bytes cada):
//     [+0]  tipo  (int64): 0=Int, 1=Float, 2=String, 3=Bool
//     [+8]  valor (int64)
//
// Linux: valores diretos (8 bytes/elemento)
//   Header (24 bytes alocado com malloc):
//     [+0]  dados      (ponteiro para array de elementos)
//     [+8]  tamanho    (int64)
//     [+16] capacidade (int64)
//   Elemento (8 bytes cada): valor direto
//
// ======================================================================

// Constantes de tipo (Windows — tagged values)
static constexpr int64_t JP_LIST_INT    = 0;
static constexpr int64_t JP_LIST_FLOAT  = 1;
static constexpr int64_t JP_LIST_STRING = 2;
static constexpr int64_t JP_LIST_BOOL   = 3;

// Constantes de layout Linux
static constexpr int32_t LIST_OFF_DATA = 0;
static constexpr int32_t LIST_OFF_COUNT = 8;
static constexpr int32_t LIST_OFF_CAP = 16;
static constexpr int32_t LIST_STRUCT_SIZE = 24;
static constexpr int32_t LIST_INITIAL_CAP = 8;

// ======================================================================
// HELPERS: detecta se variável é lista, tipo dos elementos
// ======================================================================

bool is_list_var(const std::string& name) const {
    return var_is_list_.count(name) > 0;
}

RuntimeType get_list_elem_type(const std::string& name) const {
    auto it = var_list_elem_type_.find(name);
    if (it != var_list_elem_type_.end()) return it->second;
    return RuntimeType::Unknown;
}

std::string get_list_instance_class(const std::string& name) const {
    auto it = var_list_instance_class_.find(name);
    if (it != var_list_instance_class_.end()) return it->second;
    return "";
}

RuntimeType infer_list_element_type(const ListLitExpr& node) {
    if (node.elements.empty()) return RuntimeType::Unknown;
    return infer_expr_type(*node.elements[0]);
}

std::string infer_list_element_class(const ListLitExpr& node) {
    if (node.elements.empty()) return "";
    for (auto& elem : node.elements) {
        if (!std::holds_alternative<MetodoChamadaExpr>(elem->node)) return "";
        auto& mc = std::get<MetodoChamadaExpr>(elem->node);
        if (!std::holds_alternative<VarExpr>(mc.object->node)) return "";
        auto& var = std::get<VarExpr>(mc.object->node);
        if (declared_classes_.find(var.name) == declared_classes_.end()) return "";
        return var.name;
    }
    return "";
}

// ======================================================================
// JL (jump if less) — usado no realloc check
// ======================================================================

size_t emit_jl_rel32() {
    return emit_jcc_rel32(CC_L);
}

// ======================================================================
// CRIAÇÃO DE LISTA LITERAL: [expr1, expr2, ...]
// Resultado em RAX = ponteiro para header da lista
// ======================================================================

void emit_list_literal(const ListLitExpr& node) {
    if constexpr (PlatformDefs::is_windows) {
        emit_list_literal_windows(node);
    } else {
        emit_list_literal_linux(node);
    }
}

// --- Windows: tagged values (16 bytes/elem) ---
void emit_list_literal_windows(const ListLitExpr& node) {
    size_t count = node.elements.size();
    size_t capacity = count < 8 ? 8 : count;

    // malloc(24) para o header
    emit_mov_reg_imm32(PlatformDefs::ARG1, 24);
    emit_call_symbol(emitter_.symbol_index("malloc"));

    std::string hdr = "__list_hdr_" + std::to_string(text_->pos());
    int32_t hdr_off = alloc_local(hdr);
    emit_mov_rbp_reg(hdr_off, reg::RAX);

    // header->capacidade = capacity
    emit_mov_reg_imm32(reg::RCX, static_cast<int32_t>(capacity));
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89); // mov [rax+0], rcx
    text_->emit_u8(0x08);

    // header->tamanho = count
    emit_mov_reg_imm32(reg::RCX, static_cast<int32_t>(count));
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89); // mov [rax+8], rcx
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);

    // malloc(capacity * 16) para dados
    emit_mov_reg_imm32(PlatformDefs::ARG1, static_cast<int32_t>(capacity * 16));
    emit_call_symbol(emitter_.symbol_index("malloc"));

    std::string dat = "__list_dat_" + std::to_string(text_->pos());
    int32_t dat_off = alloc_local(dat);
    emit_mov_rbp_reg(dat_off, reg::RAX);

    // header->dados = dados
    emit_mov_reg_rbp(reg::RCX, hdr_off);
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x41);
    text_->emit_u8(0x10);

    // Preencher cada elemento
    for (size_t i = 0; i < count; i++) {
        RuntimeType etype = infer_expr_type(*node.elements[i]);
        emit_expr(*node.elements[i]);

        std::string val_tmp = "__list_val_" + std::to_string(text_->pos());
        int32_t val_off = alloc_local(val_tmp);
        if (etype == RuntimeType::Float) {
            emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
        }
        emit_mov_rbp_reg(val_off, reg::RAX);

        // dados + i*16
        emit_mov_reg_rbp(reg::RCX, dat_off);
        if (i > 0) {
            emit_mov_reg_imm32(reg::RDX, static_cast<int32_t>(i * 16));
            emit_add_reg_reg(reg::RCX, reg::RDX);
        }

        // [rcx+0] = tipo
        int64_t type_tag;
        switch (etype) {
            case RuntimeType::Float:  type_tag = JP_LIST_FLOAT;  break;
            case RuntimeType::String: type_tag = JP_LIST_STRING; break;
            case RuntimeType::Bool:   type_tag = JP_LIST_BOOL;   break;
            default:                  type_tag = JP_LIST_INT;    break;
        }
        emit_mov_reg_imm32(reg::RAX, static_cast<int32_t>(type_tag));
        emit_rex_w(reg::RAX, reg::RCX);
        text_->emit_u8(0x89); // mov [rcx], rax
        text_->emit_u8(0x01);

        // [rcx+8] = valor
        emit_mov_reg_rbp(reg::RAX, val_off);
        emit_rex_w(reg::RAX, reg::RCX);
        text_->emit_u8(0x89); // mov [rcx+8], rax
        text_->emit_u8(0x41);
        text_->emit_u8(0x08);
    }

    emit_mov_reg_rbp(reg::RAX, hdr_off);
}

// --- Linux: valores diretos (8 bytes/elem) ---
void emit_list_literal_linux(const ListLitExpr& node) {
    size_t count = node.elements.size();
    int32_t cap = (count > 0) ? static_cast<int32_t>(count * 2) : LIST_INITIAL_CAP;

    RuntimeType elem_type = RuntimeType::Unknown;
    if (!node.elements.empty()) {
        elem_type = infer_expr_type(*node.elements[0]);
    }

    // malloc(24) para a struct da lista
    emit_mov_reg_imm32(reg::RDI, LIST_STRUCT_SIZE);
    emit_call_extern("malloc");

    std::string struct_tmp = "__list_struct_" + std::to_string(text_->pos());
    int32_t struct_off = alloc_local(struct_tmp);
    emit_mov_rbp_reg(struct_off, reg::RAX);

    // malloc(cap * 8) para o array de dados
    emit_mov_reg_imm32(reg::RDI, cap * 8);
    emit_call_extern("malloc");

    std::string data_tmp = "__list_data_" + std::to_string(text_->pos());
    int32_t data_off = alloc_local(data_tmp);
    emit_mov_rbp_reg(data_off, reg::RAX);

    // Inicializar struct: data, count, capacity
    emit_mov_reg_rbp(reg::RCX, struct_off);

    // struct->data = data_ptr
    emit_mov_reg_rbp(reg::RAX, data_off);
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x01); // MOV [RCX], RAX

    // struct->count = count
    emit_mov_reg_imm32(reg::RAX, static_cast<int32_t>(count));
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x41); // MOV [RCX+disp8], RAX
    text_->emit_i8(LIST_OFF_COUNT);

    // struct->capacity = cap
    emit_mov_reg_imm32(reg::RAX, cap);
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x41); // MOV [RCX+disp8], RAX
    text_->emit_i8(LIST_OFF_CAP);

    // Preencher elementos no array de dados
    for (size_t i = 0; i < count; i++) {
        emit_expr(*node.elements[i]);

        if (elem_type == RuntimeType::Float) {
            emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
        }

        std::string val_tmp = "__lelem_" + std::to_string(i) + "_" + std::to_string(text_->pos());
        int32_t val_off = alloc_local(val_tmp);
        emit_mov_rbp_reg(val_off, reg::RAX);

        emit_mov_reg_rbp(reg::RCX, data_off);
        emit_mov_reg_rbp(reg::RAX, val_off);

        // MOV [RCX + i*8], RAX
        int32_t elem_off = static_cast<int32_t>(i * 8);
        emit_rex_w(reg::RAX, reg::RCX);
        text_->emit_u8(0x89);
        if (elem_off == 0) {
            text_->emit_u8(0x01);
        } else if (elem_off < 128) {
            text_->emit_u8(0x41);
            text_->emit_i8(static_cast<int8_t>(elem_off));
        } else {
            text_->emit_u8(0x81);
            text_->emit_i32(elem_off);
        }
    }

    emit_mov_reg_rbp(reg::RAX, struct_off);
}

// ======================================================================
// ACESSO POR ÍNDICE DE LISTA: lista[expr]
// Chamado pelo codegen_expr.hpp quando is_list_var() == true
// ======================================================================

void emit_list_index_get(const IndexGetExpr& node, const std::string& list_name) {
    if constexpr (PlatformDefs::is_windows) {
        emit_list_index_get_windows(node);
    } else {
        emit_list_index_get_linux(node, list_name);
    }
}

// Windows: tagged values (16 bytes/elem), tipo em [elem+0], valor em [elem+8]
void emit_list_index_get_windows(const IndexGetExpr& node) {
    emit_expr(*node.object);
    std::string lst = "__idx_lst_" + std::to_string(text_->pos());
    int32_t lst_off = alloc_local(lst);
    emit_mov_rbp_reg(lst_off, reg::RAX);

    emit_expr(*node.index);
    std::string idx = "__idx_i_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    // dados = header->dados ([header+16])
    emit_mov_reg_rbp(reg::RCX, lst_off);
    emit_rex_w(reg::RCX, reg::RCX);
    text_->emit_u8(0x8B); // mov rcx, [rcx+16]
    text_->emit_u8(0x49);
    text_->emit_u8(0x10);

    // offset = indice * 16
    emit_mov_reg_rbp(reg::RAX, idx_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xC1); // shl rax, 4
    text_->emit_u8(0xE0);
    text_->emit_u8(0x04);

    emit_add_reg_reg(reg::RCX, reg::RAX);

    // Carregar valor: [rcx+8] → RAX
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x8B); // mov rax, [rcx+8]
    text_->emit_u8(0x41);
    text_->emit_u8(0x08);

    // Carregar tipo: [rcx+0] → RDX
    emit_rex_w(reg::RDX, reg::RCX);
    text_->emit_u8(0x8B); // mov rdx, [rcx]
    text_->emit_u8(0x11);

    // Se tipo == FLOAT, converter bits em RAX para double em XMM0
    emit_cmp_reg_imm32(reg::RDX, static_cast<int32_t>(JP_LIST_FLOAT));
    size_t not_float = emit_jne_rel32();
    emit_movq_xmm_gpr(xmm::XMM0, reg::RAX);
    patch_jump(not_float);
}

// Linux: valores diretos (8 bytes/elem)
void emit_list_index_get_linux(const IndexGetExpr& node, const std::string& list_name) {
    emit_expr(*node.object);
    std::string base_tmp = "__lidxget_base_" + std::to_string(text_->pos());
    int32_t base_off = alloc_local(base_tmp);
    emit_mov_rbp_reg(base_off, reg::RAX);

    emit_expr(*node.index);
    std::string idx_tmp = "__lidxget_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx_tmp);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    // Carregar struct ptr → RCX
    emit_mov_reg_rbp(reg::RCX, base_off);

    // Carregar data ptr: RCX = [RCX + 0] (struct->data)
    emit_rex_w(reg::RCX, reg::RCX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x09); // MOV RCX, [RCX]

    // Carregar índice → RAX
    emit_mov_reg_rbp(reg::RAX, idx_off);

    // MOV RAX, [RCX + RAX*8]
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x04); // SIB follows
    text_->emit_u8(0xC1); // scale=8(11), index=RAX(000), base=RCX(001)

    // Se elementos são float, converter bits de volta para XMM0
    RuntimeType etype = get_list_elem_type(list_name);
    if (etype == RuntimeType::Float) {
        emit_movq_xmm_gpr(xmm::XMM0, reg::RAX);
    }
}

// ======================================================================
// ATRIBUIÇÃO POR ÍNDICE: lista[expr] = valor
// ======================================================================

void emit_list_index_set(const IndexSetStmt& node) {
    if constexpr (PlatformDefs::is_windows) {
        emit_list_index_set_windows(node);
    } else {
        emit_list_index_set_linux(node);
    }
}

// Windows: tagged values
void emit_list_index_set_windows(const IndexSetStmt& node) {
    int32_t lst_off = find_local(node.name);

    emit_expr(*node.index);
    std::string idx = "__iset_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    RuntimeType vtype = infer_expr_type(*node.value);
    emit_expr(*node.value);

    std::string val = "__iset_val_" + std::to_string(text_->pos());
    int32_t val_off = alloc_local(val);
    if (vtype == RuntimeType::Float) {
        emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
    }
    emit_mov_rbp_reg(val_off, reg::RAX);

    // dados = header->dados
    emit_mov_reg_rbp(reg::RAX, lst_off);
    emit_rex_w(reg::RAX, reg::RAX);
    text_->emit_u8(0x8B); // mov rax, [rax+16]
    text_->emit_u8(0x40);
    text_->emit_u8(0x10);

    // offset = indice * 16
    emit_mov_reg_rbp(reg::RCX, idx_off);
    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xC1); // shl rcx, 4
    text_->emit_u8(0xE1);
    text_->emit_u8(0x04);

    emit_add_reg_reg(reg::RAX, reg::RCX);

    // [rax+0] = tipo
    int64_t tag;
    switch (vtype) {
        case RuntimeType::Float:  tag = JP_LIST_FLOAT;  break;
        case RuntimeType::String: tag = JP_LIST_STRING; break;
        case RuntimeType::Bool:   tag = JP_LIST_BOOL;   break;
        default:                  tag = JP_LIST_INT;    break;
    }
    emit_mov_reg_imm32(reg::RCX, static_cast<int32_t>(tag));
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89); // mov [rax], rcx
    text_->emit_u8(0x08);

    // [rax+8] = valor
    emit_mov_reg_rbp(reg::RCX, val_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89); // mov [rax+8], rcx
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);
}

// Linux: valores diretos
void emit_list_index_set_linux(const IndexSetStmt& node) {
    RuntimeType etype = get_list_elem_type(node.name);
    emit_expr(*node.value);
    if (etype == RuntimeType::Float) {
        emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
    }
    std::string val_tmp = "__lidxset_val_" + std::to_string(text_->pos());
    int32_t val_off = alloc_local(val_tmp);
    emit_mov_rbp_reg(val_off, reg::RAX);

    emit_expr(*node.index);
    std::string idx_tmp = "__lidxset_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx_tmp);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    int32_t list_off = find_local(node.name);
    emit_mov_reg_rbp(reg::RCX, list_off);

    // RCX = struct->data
    emit_rex_w(reg::RCX, reg::RCX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x09); // MOV RCX, [RCX]

    emit_mov_reg_rbp(reg::RAX, idx_off);

    // LEA RCX, [RCX + RAX*8]
    emit_rex_w(reg::RCX, reg::RCX);
    text_->emit_u8(0x8D);
    text_->emit_u8(0x0C); // SIB follows
    text_->emit_u8(0xC1); // scale=8, index=RAX, base=RCX

    emit_mov_reg_rbp(reg::RAX, val_off);
    // MOV [RCX], RAX
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x01);
}

// ======================================================================
// MÉTODOS DE LISTA — dispatch
// ======================================================================

// Assinatura Windows: recebe MetodoChamadaExpr
bool emit_list_method(const MetodoChamadaExpr& node) {
    if (!std::holds_alternative<VarExpr>(node.object->node)) return false;
    auto& var = std::get<VarExpr>(node.object->node);
    if (!is_list_var(var.name)) return false;

    if constexpr (PlatformDefs::is_windows) {
        if (node.method == "adicionar") {
            emit_list_adicionar_windows(var.name, node);
            return true;
        }
        if (node.method == "remover") {
            emit_list_remover_windows(var.name, node);
            return true;
        }
    } else {
        if (node.method == "adicionar") {
            if (!node.args.empty()) {
                emit_list_adicionar_linux(var.name, *node.args[0]);
            }
            return true;
        }
        if (node.method == "remover") {
            if (!node.args.empty()) {
                emit_list_remover_linux(var.name, *node.args[0]);
            }
            return true;
        }
    }

    if (node.method == "tamanho") {
        emit_list_tamanho(var.name);
        return true;
    }
    if (node.method == "exibir") {
        emit_list_exibir(var.name);
        return true;
    }

    return false;
}

// Assinatura Linux: recebe nome, método e args separados
bool emit_list_method(const std::string& var_name, const std::string& method,
                      const std::vector<std::unique_ptr<Expr>>& args) {
    if (method == "exibir") {
        emit_list_exibir(var_name);
        return true;
    }
    if (method == "adicionar") {
        if constexpr (PlatformDefs::is_windows) {
            // Não deveria chegar aqui no Windows, mas por segurança
            (void)args;
        } else {
            if (!args.empty()) {
                emit_list_adicionar_linux(var_name, *args[0]);
            }
        }
        return true;
    }
    if (method == "remover") {
        if constexpr (PlatformDefs::is_windows) {
            (void)args;
        } else {
            if (!args.empty()) {
                emit_list_remover_linux(var_name, *args[0]);
            }
        }
        return true;
    }
    if (method == "tamanho") {
        emit_list_tamanho(var_name);
        return true;
    }
    return false;
}

// ======================================================================
// .tamanho() → Int (em RAX)
// ======================================================================

void emit_list_tamanho(const std::string& name) {
    int32_t lst_off = find_local(name);
    emit_mov_reg_rbp(reg::RAX, lst_off);
    if constexpr (PlatformDefs::is_windows) {
        // Windows: tamanho em [rax+8]
        emit_rex_w(reg::RAX, reg::RAX);
        text_->emit_u8(0x8B);
        text_->emit_u8(0x40);
        text_->emit_u8(0x08);
    } else {
        // Linux: count em [rax+LIST_OFF_COUNT]
        emit_rex_w(reg::RAX, reg::RAX);
        text_->emit_u8(0x8B);
        text_->emit_u8(0x40); // MOV RAX, [RAX+disp8]
        text_->emit_i8(LIST_OFF_COUNT);
    }
}

// ======================================================================
// .adicionar(valor) — Windows (tagged values, 16 bytes/elem)
// ======================================================================

void emit_list_adicionar_windows(const std::string& name, const MetodoChamadaExpr& node) {
    if (node.args.empty()) return;

    int32_t lst_off = find_local(name);

    RuntimeType vtype = infer_expr_type(*node.args[0]);
    emit_expr(*node.args[0]);

    std::string val = "__ladd_val_" + std::to_string(text_->pos());
    int32_t val_off = alloc_local(val);
    if (vtype == RuntimeType::Float) {
        emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
    }
    emit_mov_rbp_reg(val_off, reg::RAX);

    // header
    emit_mov_reg_rbp(reg::RAX, lst_off);
    std::string hdr = "__ladd_hdr_" + std::to_string(text_->pos());
    int32_t hdr_off = alloc_local(hdr);
    emit_mov_rbp_reg(hdr_off, reg::RAX);

    // tamanho = header->tamanho ([rax+8])
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);
    std::string sz = "__ladd_sz_" + std::to_string(text_->pos());
    int32_t sz_off = alloc_local(sz);
    emit_mov_rbp_reg(sz_off, reg::RCX);

    // capacidade = header->capacidade ([rax+0])
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x10);

    // if (tamanho < capacidade) → skip realloc
    emit_cmp_reg_reg(reg::RCX, reg::RDX);
    size_t skip_realloc = emit_jl_rel32();

    // Realloc: nova_cap = capacidade * 2
    emit_rex_w(0, reg::RDX);
    text_->emit_u8(0xD1);
    text_->emit_u8(0xE2);

    std::string nc = "__ladd_nc_" + std::to_string(text_->pos());
    int32_t nc_off = alloc_local(nc);
    emit_mov_rbp_reg(nc_off, reg::RDX);

    // Atualizar header->capacidade
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_mov_reg_rbp(reg::RCX, nc_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x08);

    // realloc(dados, nova_cap * 16)
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_u8(0x10);

    emit_mov_reg_rbp(reg::RDX, nc_off);
    emit_rex_w(0, reg::RDX);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE2);
    text_->emit_u8(0x04);

    emit_call_symbol(emitter_.symbol_index("realloc"));

    // Atualizar header->dados
    emit_mov_reg_rbp(reg::RCX, hdr_off);
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x41);
    text_->emit_u8(0x10);

    patch_jump(skip_realloc);

    // Adicionar o elemento: dados[tamanho]
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_u8(0x10);

    // offset = tamanho * 16
    emit_mov_reg_rbp(reg::RAX, sz_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE0);
    text_->emit_u8(0x04);
    emit_add_reg_reg(reg::RCX, reg::RAX);

    // [rcx+0] = tipo
    int64_t tag;
    switch (vtype) {
        case RuntimeType::Float:  tag = JP_LIST_FLOAT;  break;
        case RuntimeType::String: tag = JP_LIST_STRING; break;
        case RuntimeType::Bool:   tag = JP_LIST_BOOL;   break;
        default:                  tag = JP_LIST_INT;    break;
    }
    emit_mov_reg_imm32(reg::RAX, static_cast<int32_t>(tag));
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x01);

    // [rcx+8] = valor
    emit_mov_reg_rbp(reg::RAX, val_off);
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x41);
    text_->emit_u8(0x08);

    // header->tamanho++
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_mov_reg_rbp(reg::RCX, sz_off);
    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC1);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);
}

// ======================================================================
// .adicionar(valor) — Linux (8 bytes/elem)
// ======================================================================

void emit_list_adicionar_linux(const std::string& var_name, const Expr& value_expr) {
    RuntimeType etype = get_list_elem_type(var_name);

    emit_expr(value_expr);
    if (etype == RuntimeType::Float) {
        emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
    }
    std::string val_tmp = "__ladd_val_" + std::to_string(text_->pos());
    int32_t val_off = alloc_local(val_tmp);
    emit_mov_rbp_reg(val_off, reg::RAX);

    int32_t list_off = find_local(var_name);
    emit_mov_reg_rbp(reg::RAX, list_off);
    std::string sp_tmp = "__ladd_sp_" + std::to_string(text_->pos());
    int32_t sp_off = alloc_local(sp_tmp);
    emit_mov_rbp_reg(sp_off, reg::RAX);

    // Carregar count e capacity
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);

    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x50);
    text_->emit_i8(LIST_OFF_CAP);

    // if (count < capacity) skip realloc
    emit_cmp_reg_reg(reg::RCX, reg::RDX);
    size_t skip_realloc = emit_jcc_rel32(CC_L);

    // === REALLOC ===
    emit_mov_reg_rbp(reg::RAX, sp_off);

    // RDI = data
    emit_rex_w(reg::RDI, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x38);

    // RSI = capacity * 16
    emit_rex_w(reg::RSI, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x70);
    text_->emit_i8(LIST_OFF_CAP);
    emit_rex_w(0, reg::RSI);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE6);
    text_->emit_u8(0x04);

    emit_call_extern("realloc");

    // Atualizar struct: data = RAX
    emit_mov_reg_rbp(reg::RCX, sp_off);
    emit_rex_w(reg::RAX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x01);

    // Atualizar capacity *= 2
    emit_rex_w(reg::RDX, reg::RCX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x51);
    text_->emit_i8(LIST_OFF_CAP);

    emit_rex_w(0, reg::RDX);
    text_->emit_u8(0xD1);
    text_->emit_u8(0xE2);

    emit_rex_w(reg::RDX, reg::RCX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x51);
    text_->emit_i8(LIST_OFF_CAP);

    // === END REALLOC ===
    patch_jump(skip_realloc);

    // data[count] = valor
    emit_mov_reg_rbp(reg::RAX, sp_off);

    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);

    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x10);

    emit_mov_reg_rbp(reg::RSI, val_off);

    // MOV [RDX + RCX*8], RSI
    emit_rex_w(reg::RSI, reg::RDX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x34);
    text_->emit_u8(0xCA);

    // count++
    emit_mov_reg_rbp(reg::RAX, sp_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);

    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC1);

    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);
}

// ======================================================================
// .remover(indice) — Windows (tagged values, memmove com stride 16)
// ======================================================================

void emit_list_remover_windows(const std::string& name, const MetodoChamadaExpr& node) {
    if (node.args.empty()) return;

    int32_t lst_off = find_local(name);

    emit_expr(*node.args[0]);
    std::string idx = "__lrem_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    // header
    emit_mov_reg_rbp(reg::RAX, lst_off);
    std::string hdr = "__lrem_hdr_" + std::to_string(text_->pos());
    int32_t hdr_off = alloc_local(hdr);
    emit_mov_rbp_reg(hdr_off, reg::RAX);

    // tamanho
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);
    std::string sz = "__lrem_sz_" + std::to_string(text_->pos());
    int32_t sz_off = alloc_local(sz);
    emit_mov_rbp_reg(sz_off, reg::RCX);

    // dados
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_rex_w(reg::RAX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x40);
    text_->emit_u8(0x10);
    std::string dat = "__lrem_dat_" + std::to_string(text_->pos());
    int32_t dat_off = alloc_local(dat);
    emit_mov_rbp_reg(dat_off, reg::RAX);

    // memmove(dst, src, bytes)
    // Windows: RCX=dst, RDX=src, R8=bytes

    // RCX (dst) = dados + indice * 16
    emit_mov_reg_rbp(reg::RAX, idx_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE0);
    text_->emit_u8(0x04);
    emit_mov_reg_rbp(reg::RCX, dat_off);
    emit_add_reg_reg(reg::RCX, reg::RAX);

    // RDX (src) = dst + 16
    emit_mov_reg_reg(reg::RDX, reg::RCX);
    emit_mov_reg_imm32(reg::RAX, 16);
    emit_add_reg_reg(reg::RDX, reg::RAX);

    // R8 (bytes) = (tamanho - indice - 1) * 16
    emit_mov_reg_rbp(reg::R8, sz_off);
    emit_mov_reg_rbp(reg::RAX, idx_off);
    emit_sub_reg_reg(reg::R8, reg::RAX);
    // dec r8
    emit_rex_w(0, reg::R8);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC8 | (reg::R8 & 7));
    // shl r8, 4
    emit_rex_w(0, reg::R8);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE0 | (reg::R8 & 7));
    text_->emit_u8(0x04);

    emit_call_symbol(emitter_.symbol_index("memmove"));

    // header->tamanho = tamanho - 1
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_mov_reg_rbp(reg::RCX, sz_off);
    // dec rcx
    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC9);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);
}

// ======================================================================
// .remover(indice) — Linux (8 bytes/elem, memmove com stride 8)
// ======================================================================

void emit_list_remover_linux(const std::string& var_name, const Expr& index_expr) {
    emit_expr(index_expr);
    std::string idx_tmp = "__lrem_idx_" + std::to_string(text_->pos());
    int32_t idx_off = alloc_local(idx_tmp);
    emit_mov_rbp_reg(idx_off, reg::RAX);

    int32_t list_off = find_local(var_name);
    emit_mov_reg_rbp(reg::RAX, list_off);
    std::string sp_tmp = "__lrem_sp_" + std::to_string(text_->pos());
    int32_t sp_off = alloc_local(sp_tmp);
    emit_mov_rbp_reg(sp_off, reg::RAX);

    // RCX = count
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);

    // RDX = data
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x10);

    // Salvar data
    std::string data_tmp = "__lrem_data_" + std::to_string(text_->pos());
    int32_t data_off = alloc_local(data_tmp);
    emit_mov_rbp_reg(data_off, reg::RDX);

    // memmove(dst=data+idx*8, src=data+(idx+1)*8, n=(count-idx-1)*8)
    // System V: RDI=dst, RSI=src, RDX=n

    // Calcular idx*8
    emit_mov_reg_rbp(reg::RAX, idx_off);
    emit_rex_w(reg::RAX, reg::RAX);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE0);
    text_->emit_u8(0x03); // SHL RAX, 3

    std::string ib_tmp = "__lrem_ib_" + std::to_string(text_->pos());
    int32_t ib_off = alloc_local(ib_tmp);
    emit_mov_rbp_reg(ib_off, reg::RAX);

    // Calcular n = (count - idx - 1) * 8
    emit_mov_reg_rbp(reg::RAX, sp_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT); // RCX = count
    emit_mov_reg_rbp(reg::RAX, idx_off); // RAX = idx
    emit_mov_reg_reg(reg::RDX, reg::RCX); // RDX = count
    emit_sub_reg_reg(reg::RDX, reg::RAX); // RDX = count - idx
    // DEC RDX
    emit_rex_w(0, reg::RDX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xCA);
    // SHL RDX, 3 → (count - idx - 1) * 8
    emit_rex_w(0, reg::RDX);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE2);
    text_->emit_u8(0x03);

    // RDI = data + idx*8
    emit_mov_reg_rbp(reg::RDI, data_off);
    emit_mov_reg_rbp(reg::RAX, ib_off);
    emit_add_reg_reg(reg::RDI, reg::RAX);

    // RSI = RDI + 8
    emit_mov_reg_reg(reg::RSI, reg::RDI);
    emit_rex_w(0, reg::RSI);
    text_->emit_u8(0x83);
    text_->emit_u8(0xC6);
    text_->emit_u8(0x08);

    emit_call_extern("memmove");

    // count--
    emit_mov_reg_rbp(reg::RAX, sp_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);
    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC9); // DEC RCX
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);
}

// ======================================================================
// .exibir() — imprime todos os elementos: [elem1, elem2, ...]
// ======================================================================

void emit_list_exibir(const std::string& name) {
    if constexpr (PlatformDefs::is_windows) {
        emit_list_exibir_windows(name);
    } else {
        emit_list_exibir_linux(name);
    }
}

// --- Windows: tagged values, switch por tipo em runtime ---
void emit_list_exibir_windows(const std::string& name) {
    int32_t lst_off = find_local(name);

    // Imprimir "["
    emit_load_string(reg::RCX, "%s");
    emit_load_string(reg::RDX, "[");
    emit_call_symbol(emitter_.symbol_index("printf"));

    // header
    emit_mov_reg_rbp(reg::RAX, lst_off);
    std::string hdr = "__lexb_hdr_" + std::to_string(text_->pos());
    int32_t hdr_off = alloc_local(hdr);
    emit_mov_rbp_reg(hdr_off, reg::RAX);

    // tamanho
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_u8(0x08);
    std::string sz = "__lexb_sz_" + std::to_string(text_->pos());
    int32_t sz_off = alloc_local(sz);
    emit_mov_rbp_reg(sz_off, reg::RCX);

    // dados
    emit_mov_reg_rbp(reg::RAX, hdr_off);
    emit_rex_w(reg::RAX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x40);
    text_->emit_u8(0x10);
    std::string dat = "__lexb_dat_" + std::to_string(text_->pos());
    int32_t dat_off = alloc_local(dat);
    emit_mov_rbp_reg(dat_off, reg::RAX);

    // i = 0
    std::string li = "__lexb_i_" + std::to_string(text_->pos());
    int32_t i_off = alloc_local(li);
    emit_mov_rbp_imm32(i_off, 0);

    size_t exb_top = text_->pos();

    // if (i >= tamanho) break
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_mov_reg_rbp(reg::RCX, sz_off);
    emit_cmp_reg_reg(reg::RAX, reg::RCX);
    size_t exb_exit = emit_jge_rel32();

    // if (i > 0) print ", "
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_cmp_reg_imm32(reg::RAX, 0);
    size_t skip_comma = emit_je_rel32();
    emit_load_string(reg::RCX, "%s");
    emit_load_string(reg::RDX, ", ");
    emit_call_symbol(emitter_.symbol_index("printf"));
    patch_jump(skip_comma);

    // elem = dados + i*16
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xC1);
    text_->emit_u8(0xE0);
    text_->emit_u8(0x04);
    emit_mov_reg_rbp(reg::RCX, dat_off);
    emit_add_reg_reg(reg::RAX, reg::RCX);

    std::string ep = "__lexb_ep_" + std::to_string(text_->pos());
    int32_t ep_off = alloc_local(ep);
    emit_mov_rbp_reg(ep_off, reg::RAX);

    // tipo = [rax+0], valor = [rax+8]
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x08);
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x50);
    text_->emit_u8(0x08);

    // if tipo == STRING (2)
    emit_cmp_reg_imm32(reg::RCX, static_cast<int32_t>(JP_LIST_STRING));
    size_t not_str = emit_jne_rel32();
    emit_load_string(reg::RCX, "%s");
    emit_call_symbol(emitter_.symbol_index("printf"));
    size_t exb_next = emit_jmp_rel32();
    patch_jump(not_str);

    // if tipo == FLOAT (1)
    emit_mov_reg_rbp(reg::RAX, ep_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x08);
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x50);
    text_->emit_u8(0x08);

    emit_cmp_reg_imm32(reg::RCX, static_cast<int32_t>(JP_LIST_FLOAT));
    size_t not_flt = emit_jne_rel32();
    emit_movq_xmm_gpr(xmm::XMM0, reg::RDX);
    // Inline: printf("%g", XMM0) — Windows calling convention
    emit_load_string(PlatformDefs::ARG1, "%g");
    if constexpr (PlatformDefs::VARIADIC_FLOAT_MIRROR_GPR) {
        emit_movq_gpr_xmm(PlatformDefs::ARG2, xmm::XMM0);
    }
    emit_call_symbol(emitter_.symbol_index("printf"));
    size_t exb_next2 = emit_jmp_rel32();
    patch_jump(not_flt);

    // if tipo == BOOL (3)
    emit_mov_reg_rbp(reg::RAX, ep_off);
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x08);
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x50);
    text_->emit_u8(0x08);

    emit_cmp_reg_imm32(reg::RCX, static_cast<int32_t>(JP_LIST_BOOL));
    size_t not_bool = emit_jne_rel32();
    emit_test_reg_reg(reg::RDX, reg::RDX);
    size_t is_false = emit_je_rel32();
    emit_load_string(reg::RCX, "%s");
    emit_load_string(reg::RDX, "verdadeiro");
    emit_call_symbol(emitter_.symbol_index("printf"));
    size_t exb_next3 = emit_jmp_rel32();
    patch_jump(is_false);
    emit_load_string(reg::RCX, "%s");
    emit_load_string(reg::RDX, "falso");
    emit_call_symbol(emitter_.symbol_index("printf"));
    size_t exb_next4 = emit_jmp_rel32();
    patch_jump(not_bool);

    // Default: INT
    emit_mov_reg_rbp(reg::RAX, ep_off);
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x50);
    text_->emit_u8(0x08);
    emit_load_string(reg::RCX, "%d");
    emit_call_symbol(emitter_.symbol_index("printf"));

    patch_jump(exb_next);
    patch_jump(exb_next2);
    patch_jump(exb_next3);
    patch_jump(exb_next4);

    // i++
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC0);
    emit_mov_rbp_reg(i_off, reg::RAX);

    // jmp exb_top
    text_->emit_u8(0xE9);
    int32_t back_off = static_cast<int32_t>(exb_top) -
                       static_cast<int32_t>(text_->pos() + 4);
    text_->emit_i32(back_off);

    patch_jump(exb_exit);

    // Imprimir "]\n"
    emit_load_string(reg::RCX, "%s");
    emit_load_string(reg::RDX, "]\n");
    emit_call_symbol(emitter_.symbol_index("printf"));
}

// --- Linux: tipo estático, 8 bytes/elem ---
void emit_list_exibir_linux(const std::string& var_name) {
    RuntimeType etype = get_list_elem_type(var_name);

    int32_t list_off = find_local(var_name);

    // Imprimir "["
    emit_load_string(reg::RDI, "[");
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_call_extern("printf");

    // Carregar struct
    emit_mov_reg_rbp(reg::RAX, list_off);
    std::string sp_tmp = "__lexib_sp_" + std::to_string(text_->pos());
    int32_t sp_off = alloc_local(sp_tmp);
    emit_mov_rbp_reg(sp_off, reg::RAX);

    // Carregar count
    emit_rex_w(reg::RCX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x48);
    text_->emit_i8(LIST_OFF_COUNT);
    std::string cnt_tmp = "__lexib_cnt_" + std::to_string(text_->pos());
    int32_t cnt_off = alloc_local(cnt_tmp);
    emit_mov_rbp_reg(cnt_off, reg::RCX);

    // Carregar data
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x10);
    std::string data_tmp = "__lexib_data_" + std::to_string(text_->pos());
    int32_t data_off = alloc_local(data_tmp);
    emit_mov_rbp_reg(data_off, reg::RDX);

    // Loop: i = 0
    std::string i_tmp = "__lexib_i_" + std::to_string(text_->pos());
    int32_t i_off = alloc_local(i_tmp);
    emit_mov_reg_imm32(reg::RAX, 0);
    emit_mov_rbp_reg(i_off, reg::RAX);

    size_t loop_top = text_->pos();

    // if (i >= count) break
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_mov_reg_rbp(reg::RCX, cnt_off);
    emit_cmp_reg_reg(reg::RAX, reg::RCX);
    size_t exit_patch = emit_jge_rel32();

    // if (i > 0) print ", "
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_cmp_reg_imm32(reg::RAX, 0);
    size_t skip_comma = emit_je_rel32();
    emit_load_string(reg::RDI, ", ");
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_call_extern("printf");
    patch_jump(skip_comma);

    // Carregar data[i]
    emit_mov_reg_rbp(reg::RDX, data_off);
    emit_mov_reg_rbp(reg::RAX, i_off);
    // RAX = [RDX + RAX*8]
    emit_rex_w(reg::RAX, reg::RDX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x04);
    text_->emit_u8(0xC2); // SIB: scale=8(11), index=RAX(000), base=RDX(010)

    // Imprimir segundo tipo
    switch (etype) {
        case RuntimeType::String:
            emit_mov_reg_reg(reg::RSI, reg::RAX);
            emit_load_string(reg::RDI, "%s");
            emit_xor_reg_reg(reg::RAX, reg::RAX);
            emit_call_extern("printf");
            break;
        case RuntimeType::Float:
            emit_movq_xmm_gpr(xmm::XMM0, reg::RAX);
            emit_load_string(reg::RDI, "%g");
            emit_mov_reg_imm32(reg::RAX, 1);
            emit_call_extern("printf");
            break;
        default:
            emit_mov_reg_reg(reg::RSI, reg::RAX);
            emit_load_string(reg::RDI, "%d");
            emit_xor_reg_reg(reg::RAX, reg::RAX);
            emit_call_extern("printf");
            break;
    }

    // i++
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC0);
    emit_mov_rbp_reg(i_off, reg::RAX);

    // Jump back
    text_->emit_u8(0xE9);
    int32_t back_offset = static_cast<int32_t>(loop_top) -
                          static_cast<int32_t>(text_->pos() + 4);
    text_->emit_i32(back_offset);

    patch_jump(exit_patch);

    // Imprimir "]\n"
    emit_load_string(reg::RDI, "]\n");
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_call_extern("printf");
}

// ======================================================================
// SAIDA de lista: saida(var) quando var é lista → chama exibir
// ======================================================================

bool emit_saida_list(const Expr& expr) {
    if (!std::holds_alternative<VarExpr>(expr.node)) return false;
    auto& var = std::get<VarExpr>(expr.node);
    if (!is_list_var(var.name)) return false;
    emit_list_exibir(var.name);
    return true;
}

void emit_saida_list(const std::string& var_name, bool newline) {
    emit_list_exibir(var_name);
    (void)newline; // exibir já imprime \n
}