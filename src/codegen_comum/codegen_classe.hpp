// codegen_classe.hpp
// Emissão de classes — unificado Windows/Linux via PlatformDefs
// Registro, instanciação via malloc, atributos, métodos e auto

// ======================================================================
// ESTRUTURAS DE CLASSE EM TEMPO DE COMPILAÇÃO
// ======================================================================

struct ClassAttrInfo {
    std::string name;
    int32_t offset;
    RuntimeType type;
};

struct ClassInfo {
    std::string name;
    std::vector<ClassAttrInfo> attrs;
    std::vector<std::string> method_names;
    int32_t instance_size;

    int32_t find_attr_offset(const std::string& attr_name) const {
        for (auto& a : attrs) {
            if (a.name == attr_name) return a.offset;
        }
        return -1;
    }

    int32_t register_attr(const std::string& attr_name, RuntimeType type = RuntimeType::Unknown) {
        for (auto& a : attrs) {
            if (a.name == attr_name) return a.offset;
        }
        int32_t off = instance_size;
        attrs.push_back({attr_name, off, type});
        instance_size += 8;
        return off;
    }

    RuntimeType get_attr_type(const std::string& attr_name) const {
        for (auto& a : attrs) {
            if (a.name == attr_name) return a.type;
        }
        return RuntimeType::Unknown;
    }
};

// ======================================================================
// DADOS DE CLASSE (membros da classe Codegen)
// ======================================================================

std::unordered_map<std::string, ClassInfo> declared_classes_;
ClassInfo* current_class_ = nullptr;
int32_t auto_local_offset_ = 0;

// Mapeamento atributo → parâmetro (Windows: inferência avançada)
struct AttrParamMapping {
    std::string class_name;
    std::string attr_name;
    int param_index;
};
std::unordered_map<std::string, AttrParamMapping> class_attr_param_map_;

// ======================================================================
// INFERÊNCIA DE TIPOS PARA CLASSES
// ======================================================================

// Windows: resolve tipos a partir de literais e mapeamento param→attr
// Linux: usa preanalyze_constructor_calls (abaixo)

#ifdef _WIN32
void resolve_class_attr_types(const Program& program) {
    for (auto& stmt : program.statements) {
        resolve_class_attr_in_stmt(*stmt);
    }
}

void preanalyze_constructor_calls(const Program&) {
    // No-op no Windows — usa resolve_class_attr_types
}

void resolve_class_attr_in_stmt(const Stmt& stmt) {
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, AssignStmt>) {
            resolve_class_attr_in_assign(s);
        }
        else if constexpr (std::is_same_v<T, IfStmt>) {
            for (auto& br : s.branches)
                for (auto& st : br.body) resolve_class_attr_in_stmt(*st);
        }
        else if constexpr (std::is_same_v<T, EnquantoStmt>) {
            for (auto& st : s.body) resolve_class_attr_in_stmt(*st);
        }
        else if constexpr (std::is_same_v<T, ParaStmt>) {
            for (auto& st : s.body) resolve_class_attr_in_stmt(*st);
        }
        else if constexpr (std::is_same_v<T, ExprStmt>) {
            resolve_class_attr_in_expr(*s.expr);
        }
    }, stmt.node);
}

void resolve_class_attr_in_assign(const AssignStmt& node) {
    if (std::holds_alternative<MetodoChamadaExpr>(node.value->node)) {
        auto& mc = std::get<MetodoChamadaExpr>(node.value->node);
        resolve_class_attr_from_method_call(mc);
    }
    else if (std::holds_alternative<ListLitExpr>(node.value->node)) {
        auto& list = std::get<ListLitExpr>(node.value->node);
        for (auto& elem : list.elements) {
            if (std::holds_alternative<MetodoChamadaExpr>(elem->node)) {
                auto& mc = std::get<MetodoChamadaExpr>(elem->node);
                resolve_class_attr_from_method_call(mc);
            }
        }
    }
}

void resolve_class_attr_in_expr(const Expr& expr) {
    if (std::holds_alternative<MetodoChamadaExpr>(expr.node)) {
        auto& mc = std::get<MetodoChamadaExpr>(expr.node);
        resolve_class_attr_from_method_call(mc);
    }
}

void resolve_class_attr_from_method_call(const MetodoChamadaExpr& mc) {
    if (!std::holds_alternative<VarExpr>(mc.object->node)) return;
    auto& var = std::get<VarExpr>(mc.object->node);
    if (declared_classes_.find(var.name) == declared_classes_.end()) return;

    std::string class_name = var.name;
    for (size_t i = 0; i < mc.args.size(); i++) {
        RuntimeType arg_type = infer_literal_type(*mc.args[i]);
        if (arg_type == RuntimeType::Unknown) continue;
        for (auto& [key, mapping] : class_attr_param_map_) {
            if (mapping.class_name == class_name &&
                mapping.param_index == static_cast<int>(i)) {
                auto cit = declared_classes_.find(class_name);
                if (cit != declared_classes_.end()) {
                    for (auto& attr : cit->second.attrs) {
                        if (attr.name == mapping.attr_name &&
                            attr.type == RuntimeType::Unknown) {
                            attr.type = arg_type;
                        }
                    }
                }
            }
        }
    }
}

RuntimeType infer_literal_type(const Expr& expr) {
    return std::visit([&](const auto& node) -> RuntimeType {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, NumberLit>)     return RuntimeType::Int;
        else if constexpr (std::is_same_v<T, FloatLit>)     return RuntimeType::Float;
        else if constexpr (std::is_same_v<T, StringLit>)    return RuntimeType::String;
        else if constexpr (std::is_same_v<T, StringInterp>) return RuntimeType::String;
        else if constexpr (std::is_same_v<T, BoolLit>)      return RuntimeType::Bool;
        else return RuntimeType::Unknown;
    }, expr.node);
}

#else
// Linux: pré-análise de call sites de construtores

void resolve_class_attr_types(const Program&) {
    // No-op no Linux — usa preanalyze_constructor_calls
}

void preanalyze_constructor_calls(const Program& program) {
    for (auto& stmt : program.statements) {
        preanalyze_stmt_for_constructors(*stmt);
    }
}

void preanalyze_stmt_for_constructors(const Stmt& stmt) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, AssignStmt>) {
            preanalyze_expr_for_constructors(*node.value);
        }
        else if constexpr (std::is_same_v<T, ExprStmt>) {
            preanalyze_expr_for_constructors(*node.expr);
        }
        else if constexpr (std::is_same_v<T, IfStmt>) {
            for (auto& br : node.branches)
                for (auto& s : br.body)
                    preanalyze_stmt_for_constructors(*s);
        }
        else if constexpr (std::is_same_v<T, EnquantoStmt>) {
            for (auto& s : node.body)
                preanalyze_stmt_for_constructors(*s);
        }
        else if constexpr (std::is_same_v<T, ParaStmt>) {
            for (auto& s : node.body)
                preanalyze_stmt_for_constructors(*s);
        }
    }, stmt.node);
}

void preanalyze_expr_for_constructors(const Expr& expr) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, MetodoChamadaExpr>) {
            if (std::holds_alternative<VarExpr>(node.object->node)) {
                auto& var = std::get<VarExpr>(node.object->node);
                auto cit = declared_classes_.find(var.name);
                if (cit != declared_classes_.end()) {
                    propagate_constructor_types(cit->second, node.method, node.args);
                }
            }
            for (auto& arg : node.args) {
                preanalyze_expr_for_constructors(*arg);
            }
        }
        else if constexpr (std::is_same_v<T, ListLitExpr>) {
            for (auto& elem : node.elements) {
                preanalyze_expr_for_constructors(*elem);
            }
        }
    }, expr.node);
}

void propagate_constructor_types(ClassInfo& cls, const std::string& method_name,
                                  const std::vector<std::unique_ptr<Expr>>& args) {
    std::string method_sym = cls.name + "__" + method_name;
    auto fit = declared_funcs_.find(method_sym);
    if (fit == declared_funcs_.end()) return;

    std::vector<RuntimeType> arg_types;
    for (auto& arg : args) {
        arg_types.push_back(infer_expr_type(*arg));
    }

    if (fit->second.param_types.size() < arg_types.size() + 1) {
        fit->second.param_types.resize(arg_types.size() + 1, RuntimeType::Unknown);
    }
    for (size_t i = 0; i < arg_types.size(); i++) {
        if (fit->second.param_types[i + 1] == RuntimeType::Unknown) {
            fit->second.param_types[i + 1] = arg_types[i];
        }
    }

    if (fit->second.params.size() == args.size()) {
        for (size_t i = 0; i < args.size(); i++) {
            std::string param_name = fit->second.params[i];
            for (auto& attr : cls.attrs) {
                if (attr.name == param_name && attr.type == RuntimeType::Unknown) {
                    attr.type = arg_types[i];
                }
            }
        }
    }
}

#endif

// ======================================================================
// PRÉ-REGISTRO DE CLASSES
// ======================================================================

void preregister_class(const ClasseStmt& node) {
    ClassInfo cls;
    cls.name = node.name;
    cls.instance_size = 0;

    for (auto& stmt : node.body) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, FuncaoStmt>) {
                std::string method_sym = node.name + "__" + s.name;
                cls.method_names.push_back(s.name);
                #ifdef _WIN32
                declared_funcs_[method_sym] = {};
                scan_auto_attrs_typed(s.body, s.params, cls);
                #else
                FuncInfo fi;
                fi.name = method_sym;
                fi.params = s.params;
                declared_funcs_[method_sym] = fi;
                scan_auto_attrs(s.body, cls);
                #endif
            }
        }, stmt->node);
    }

    declared_classes_[node.name] = cls;
}

// Scan simples (Linux) — registra atributos sem inferir tipo
void scan_auto_attrs(const StmtList& stmts, ClassInfo& cls) {
    for (auto& stmt : stmts) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, AttrSetStmt>) {
                if (std::holds_alternative<AutoExpr>(s.object->node)) {
                    cls.register_attr(s.attr, RuntimeType::Unknown);
                }
            }
            else if constexpr (std::is_same_v<T, IfStmt>) {
                for (auto& br : s.branches)
                    scan_auto_attrs(br.body, cls);
            }
            else if constexpr (std::is_same_v<T, EnquantoStmt>) {
                scan_auto_attrs(s.body, cls);
            }
            else if constexpr (std::is_same_v<T, RepetirStmt>) {
                scan_auto_attrs(s.body, cls);
            }
            else if constexpr (std::is_same_v<T, ParaStmt>) {
                scan_auto_attrs(s.body, cls);
            }
        }, stmt->node);
    }
}

#ifdef _WIN32
// Scan com inferência (Windows) — infere tipos de literais e mapeia params
void scan_auto_attrs_typed(const StmtList& stmts,
                           const std::vector<std::string>& params,
                           ClassInfo& cls) {
    for (auto& stmt : stmts) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, AttrSetStmt>) {
                if (std::holds_alternative<AutoExpr>(s.object->node)) {
                    RuntimeType type = RuntimeType::Unknown;
                    if (std::holds_alternative<StringLit>(s.value->node) ||
                        std::holds_alternative<StringInterp>(s.value->node)) {
                        type = RuntimeType::String;
                    }
                    else if (std::holds_alternative<NumberLit>(s.value->node)) {
                        type = RuntimeType::Int;
                    }
                    else if (std::holds_alternative<FloatLit>(s.value->node)) {
                        type = RuntimeType::Float;
                    }
                    else if (std::holds_alternative<BoolLit>(s.value->node)) {
                        type = RuntimeType::Bool;
                    }
                    else if (std::holds_alternative<VarExpr>(s.value->node)) {
                        auto& var = std::get<VarExpr>(s.value->node);
                        for (size_t i = 0; i < params.size(); i++) {
                            if (params[i] == var.name) {
                                cls.register_attr(s.attr, RuntimeType::Unknown);
                                class_attr_param_map_[cls.name + "." + s.attr] =
                                    {cls.name, s.attr, static_cast<int>(i)};
                                return;
                            }
                        }
                    }
                    cls.register_attr(s.attr, type);
                }
            }
            else if constexpr (std::is_same_v<T, IfStmt>) {
                for (auto& br : s.branches)
                    scan_auto_attrs_typed(br.body, params, cls);
            }
            else if constexpr (std::is_same_v<T, EnquantoStmt>) {
                scan_auto_attrs_typed(s.body, params, cls);
            }
            else if constexpr (std::is_same_v<T, RepetirStmt>) {
                scan_auto_attrs_typed(s.body, params, cls);
            }
            else if constexpr (std::is_same_v<T, ParaStmt>) {
                scan_auto_attrs_typed(s.body, params, cls);
            }
        }, stmt->node);
    }
}
#endif

// ======================================================================
// EMISSÃO DE MÉTODOS DA CLASSE
// ======================================================================

void emit_class_methods(const ClasseStmt& node) {
    auto it = declared_classes_.find(node.name);
    if (it == declared_classes_.end()) return;

    ClassInfo& cls = it->second;

    for (auto& stmt : node.body) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, FuncaoStmt>) {
                emit_method(cls, s);
            }
        }, stmt->node);
    }
}

// ======================================================================
// EMISSÃO DE UM MÉTODO — via PlatformDefs
// ARG1 = auto (ponteiro instância), ARG2+ = args explícitos
// ======================================================================

void emit_method(ClassInfo& cls, const FuncaoStmt& func) {
    std::string method_sym = cls.name + "__" + func.name;

    uint32_t func_offset = static_cast<uint32_t>(text_->pos());
    uint32_t func_sym_idx = emitter_.add_global_symbol(method_sym, text_idx_,
                                                        func_offset, true);

    FuncInfo finfo;
    finfo.name = method_sym;
    finfo.symbol_index = func_sym_idx;
    finfo.params = func.params;

    {
        auto prev = declared_funcs_.find(method_sym);
        if (prev != declared_funcs_.end()) {
            finfo.param_types = prev->second.param_types;
        }
    }
    declared_funcs_[method_sym] = finfo;

    locals_.clear();
    local_offset_ = 0;
    var_types_.clear();

    constexpr size_t MAX_REG_PARAMS = PlatformDefs::is_windows ? 4 : 6;
    const uint8_t param_regs[] = {
        PlatformDefs::ARG1, PlatformDefs::ARG2,
        PlatformDefs::ARG3, PlatformDefs::ARG4,
        PlatformDefs::ARG5, PlatformDefs::ARG6,
    };

    int32_t estimated_locals = count_locals(func.body) +
                               static_cast<int32_t>(func.params.size()) + 20;
    int32_t local_bytes = estimated_locals * 8 + PlatformDefs::MIN_STACK;
    emit_prologue(local_bytes);

    // Salvar ponteiro "auto" (ARG1) como variável local
    int32_t auto_off = alloc_local("__auto__");
    emit_mov_rbp_reg(auto_off, PlatformDefs::ARG1);
    auto_local_offset_ = auto_off;

    // Salvar parâmetros explícitos (deslocados +1 por causa do "auto")
    for (size_t i = 0; i < func.params.size(); i++) {
        int32_t offset = alloc_local(func.params[i]);
        if (i + 1 < MAX_REG_PARAMS) {
            emit_mov_rbp_reg(offset, param_regs[i + 1]);
        } else {
            constexpr int32_t STACK_ARGS_BASE = PlatformDefs::is_windows ? 48 : 16;
            int32_t src_offset = STACK_ARGS_BASE +
                                 static_cast<int32_t>((i + 1 - MAX_REG_PARAMS) * 8);
            emit_mov_reg_rbp(reg::RAX, src_offset);
            emit_mov_rbp_reg(offset, reg::RAX);
        }
    }

    // Registrar tipos dos parâmetros
    for (size_t i = 0; i < func.params.size(); i++) {
        if (i + 1 < finfo.param_types.size()) {
            var_types_[func.params[i]] = finfo.param_types[i + 1];
        }
    }

    current_class_ = &cls;

    for (auto& s : func.body) {
        emit_stmt(*s);
    }

    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_epilogue();

    current_class_ = nullptr;
}

// ======================================================================
// ATRIBUIÇÃO DE ATRIBUTO: auto.attr = valor
// ======================================================================

void emit_attr_set(const AttrSetStmt& node) {
    if (!current_class_) return;
    if (!std::holds_alternative<AutoExpr>(node.object->node)) return;

    int32_t attr_off = current_class_->find_attr_offset(node.attr);
    if (attr_off < 0) {
        attr_off = current_class_->register_attr(node.attr);
    }

    RuntimeType type = infer_expr_type(*node.value);
    for (auto& a : current_class_->attrs) {
        if (a.name == node.attr) {
            a.type = type;
            break;
        }
    }

    emit_expr(*node.value);

    if (type == RuntimeType::Float) {
        std::string tmp = "__attrset_tmp_" + std::to_string(text_->pos());
        int32_t tmp_off = alloc_local(tmp);
        emit_movsd_rbp_xmm(tmp_off, xmm::XMM0);

        emit_mov_reg_rbp(reg::RAX, auto_local_offset_);
        emit_movsd_xmm_rbp(xmm::XMM0, tmp_off);
        // MOVSD [RAX + disp32], XMM0
        text_->emit_u8(0xF2);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x11);
        text_->emit_u8(0x80); // ModR/M: mod=10, reg=XMM0, rm=RAX
        text_->emit_i32(attr_off);
    } else {
        emit_push(reg::RAX);
        emit_mov_reg_rbp(reg::RCX, auto_local_offset_);
        emit_pop(reg::RAX);
        // MOV [RCX + attr_off], RAX
        emit_rex_w(reg::RAX, reg::RCX);
        text_->emit_u8(0x89);
        text_->emit_u8(0x81); // ModR/M: mod=10, reg=RAX, rm=RCX
        text_->emit_i32(attr_off);
    }
}

// ======================================================================
// LEITURA DE ATRIBUTO: auto.attr ou var.attr
// ======================================================================

void emit_attr_get(const AttrGetExpr& node) {
    emit_expr(*node.object);

    RuntimeType attr_type = RuntimeType::Unknown;
    int32_t attr_off = resolve_attr_offset(node, attr_type);

    if (attr_off < 0) {
        emit_mov_reg_imm32(reg::RAX, 0);
        return;
    }

    if (attr_type == RuntimeType::Float) {
        // MOVSD XMM0, [RAX + disp32]
        text_->emit_u8(0xF2);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x10);
        text_->emit_u8(0x80);
        text_->emit_i32(attr_off);
    } else {
        // MOV RAX, [RAX + disp32]
        emit_rex_w(reg::RAX, reg::RAX);
        text_->emit_u8(0x8B);
        text_->emit_u8(0x80);
        text_->emit_i32(attr_off);
    }
}

// ======================================================================
// HELPER: resolve offset de atributo
// ======================================================================

int32_t resolve_attr_offset(const AttrGetExpr& node, RuntimeType& out_type) {
    if (std::holds_alternative<AutoExpr>(node.object->node)) {
        if (current_class_) {
            int32_t off = current_class_->find_attr_offset(node.attr);
            out_type = current_class_->get_attr_type(node.attr);
            return off;
        }
        return -1;
    }

    std::string class_name = resolve_object_class(*node.object);
    if (!class_name.empty()) {
        auto cit = declared_classes_.find(class_name);
        if (cit != declared_classes_.end()) {
            int32_t off = cit->second.find_attr_offset(node.attr);
            out_type = cit->second.get_attr_type(node.attr);
            return off;
        }
    }

    return -1;
}

// ======================================================================
// AUTO EXPRESSION
// ======================================================================

void emit_auto_expr() {
    emit_mov_reg_rbp(reg::RAX, auto_local_offset_);
}

// ======================================================================
// HELPER: resolve nome da classe para expressão objeto
// ======================================================================

std::string resolve_object_class(const Expr& object_expr) {
    if (std::holds_alternative<VarExpr>(object_expr.node)) {
        auto& var = std::get<VarExpr>(object_expr.node);
        auto vit = var_instance_class_.find(var.name);
        if (vit != var_instance_class_.end()) {
            return vit->second;
        }
    }
    else if (std::holds_alternative<AutoExpr>(object_expr.node)) {
        if (current_class_) {
            return current_class_->name;
        }
    }
    else if (std::holds_alternative<IndexGetExpr>(object_expr.node)) {
        auto& idx = std::get<IndexGetExpr>(object_expr.node);
        if (std::holds_alternative<VarExpr>(idx.object->node)) {
            auto& var = std::get<VarExpr>(idx.object->node);
            auto lit = var_list_instance_class_.find(var.name);
            if (lit != var_list_instance_class_.end()) {
                return lit->second;
            }
        }
    }
    return "";
}

// ======================================================================
// CHAMADA DE MÉTODO: obj.metodo(args) — via PlatformDefs
// ======================================================================

void emit_metodo_chamada(const MetodoChamadaExpr& node) {
    std::string class_name = resolve_object_class(*node.object);

    if (class_name.empty()) {
        emit_mov_reg_imm32(reg::RAX, 0);
        return;
    }

    std::string method_sym = class_name + "__" + node.method;

    constexpr size_t MAX_REG_ARGS = PlatformDefs::is_windows ? 4 : 6;
    const uint8_t call_regs[] = {
        PlatformDefs::ARG1, PlatformDefs::ARG2,
        PlatformDefs::ARG3, PlatformDefs::ARG4,
        PlatformDefs::ARG5, PlatformDefs::ARG6,
    };

    // Avaliar args e salvar em temporários
    std::vector<int32_t> arg_offsets;
    std::vector<RuntimeType> arg_types;

    for (size_t i = 0; i < node.args.size(); i++) {
        RuntimeType type = infer_expr_type(*node.args[i]);
        arg_types.push_back(type);

        emit_expr(*node.args[i]);
        std::string tmp = "__marg_" + std::to_string(i) + "_" +
                          std::to_string(text_->pos());
        int32_t off = alloc_local(tmp);
        if (type == RuntimeType::Float) {
            emit_movsd_rbp_xmm(off, xmm::XMM0);
        } else {
            emit_mov_rbp_reg(off, reg::RAX);
        }
        arg_offsets.push_back(off);

        auto fit = declared_funcs_.find(method_sym);
        if (fit != declared_funcs_.end()) {
            size_t slot = i + 1;
            if (fit->second.param_types.size() <= slot) {
                fit->second.param_types.resize(slot + 1, RuntimeType::Unknown);
            }
            if (fit->second.param_types[slot] == RuntimeType::Unknown) {
                fit->second.param_types[slot] = type;
            }
        }
    }

    // Avaliar objeto → ponteiro da instância → salvar
    emit_expr(*node.object);
    std::string auto_tmp = "__mobj_" + std::to_string(text_->pos());
    int32_t auto_tmp_off = alloc_local(auto_tmp);
    emit_mov_rbp_reg(auto_tmp_off, reg::RAX);

    // Carregar ARG1 = ponteiro instância
    emit_mov_reg_rbp(PlatformDefs::ARG1, auto_tmp_off);

    // Carregar args explícitos nos registradores (deslocados +1)
    for (size_t i = 0; i < node.args.size(); i++) {
        size_t reg_slot = i + 1;
        if (reg_slot < MAX_REG_ARGS) {
            if (arg_types[i] == RuntimeType::Float) {
                const uint8_t arg_xmms[] = {
                    PlatformDefs::FLOAT_ARG1, PlatformDefs::FLOAT_ARG2,
                    PlatformDefs::FLOAT_ARG3, PlatformDefs::FLOAT_ARG4,
                };
                if (reg_slot < 4) {
                    emit_movsd_xmm_rbp(arg_xmms[reg_slot], arg_offsets[i]);
                }
                if constexpr (PlatformDefs::VARIADIC_FLOAT_MIRROR_GPR) {
                    emit_movq_gpr_xmm(call_regs[reg_slot], arg_xmms[reg_slot]);
                }
            } else {
                emit_mov_reg_rbp(call_regs[reg_slot], arg_offsets[i]);
            }
        } else {
            // Stack args
            if (arg_types[i] == RuntimeType::Float) {
                emit_movsd_xmm_rbp(xmm::XMM0, arg_offsets[i]);
                emit_movq_gpr_xmm(reg::RAX, xmm::XMM0);
            } else {
                emit_mov_reg_rbp(reg::RAX, arg_offsets[i]);
            }
            int32_t stack_off;
            if constexpr (PlatformDefs::is_windows) {
                stack_off = 32 + static_cast<int32_t>(reg_slot * 8);
            } else {
                stack_off = static_cast<int32_t>((reg_slot - MAX_REG_ARGS) * 8);
            }
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

    // System V: AL = 0 para variádica
    if constexpr (PlatformDefs::VARIADIC_NEEDS_AL) {
        emit_xor_reg_reg(reg::RAX, reg::RAX);
    }

    uint32_t sym_idx;
    if (emitter_.has_symbol(method_sym)) {
        sym_idx = emitter_.symbol_index(method_sym);
    } else {
        sym_idx = emitter_.add_extern_symbol(method_sym);
    }

    emit_call_symbol(sym_idx);
}

// ======================================================================
// CHAMADA ESTÁTICA: Classe.metodo(args) — construtores
// Aloca instância com malloc, chama método com instância como auto
// ======================================================================

void emit_static_method_call(const std::string& class_name,
                              const std::string& method_name,
                              const MetodoChamadaExpr& node) {
    auto cit = declared_classes_.find(class_name);
    if (cit == declared_classes_.end()) {
        emit_mov_reg_imm32(reg::RAX, 0);
        return;
    }

    ClassInfo& cls = cit->second;
    std::string method_sym = class_name + "__" + method_name;

    // malloc(instance_size)
    int32_t alloc_size = cls.instance_size;
    if (alloc_size < 8) alloc_size = 8;
    emit_mov_reg_imm32(PlatformDefs::ARG1, alloc_size);
    emit_call_symbol(emitter_.symbol_index("malloc"));

    // RAX = ponteiro da instância
    std::string inst_tmp = "__inst_" + std::to_string(text_->pos());
    int32_t inst_off = alloc_local(inst_tmp);
    emit_mov_rbp_reg(inst_off, reg::RAX);

    constexpr size_t MAX_REG_ARGS = PlatformDefs::is_windows ? 4 : 6;
    const uint8_t call_regs[] = {
        PlatformDefs::ARG1, PlatformDefs::ARG2,
        PlatformDefs::ARG3, PlatformDefs::ARG4,
        PlatformDefs::ARG5, PlatformDefs::ARG6,
    };

    // Avaliar args
    std::vector<int32_t> arg_offsets;
    std::vector<RuntimeType> arg_types;

    for (size_t i = 0; i < node.args.size(); i++) {
        RuntimeType type = infer_expr_type(*node.args[i]);
        arg_types.push_back(type);

        emit_expr(*node.args[i]);
        std::string tmp = "__sarg_" + std::to_string(i) + "_" +
                          std::to_string(text_->pos());
        int32_t off = alloc_local(tmp);
        if (type == RuntimeType::Float) {
            emit_movsd_rbp_xmm(off, xmm::XMM0);
        } else {
            emit_mov_rbp_reg(off, reg::RAX);
        }
        arg_offsets.push_back(off);

        auto fit = declared_funcs_.find(method_sym);
        if (fit != declared_funcs_.end()) {
            size_t slot = i + 1;
            if (fit->second.param_types.size() <= slot) {
                fit->second.param_types.resize(slot + 1, RuntimeType::Unknown);
            }
            if (fit->second.param_types[slot] == RuntimeType::Unknown) {
                fit->second.param_types[slot] = type;
            }
        }
    }

    // Carregar ARG1 = ponteiro instância
    emit_mov_reg_rbp(PlatformDefs::ARG1, inst_off);

    // Carregar args (deslocados +1)
    for (size_t i = 0; i < node.args.size(); i++) {
        size_t reg_slot = i + 1;
        if (reg_slot < MAX_REG_ARGS) {
            if (arg_types[i] == RuntimeType::Float) {
                const uint8_t arg_xmms[] = {
                    PlatformDefs::FLOAT_ARG1, PlatformDefs::FLOAT_ARG2,
                    PlatformDefs::FLOAT_ARG3, PlatformDefs::FLOAT_ARG4,
                };
                if (reg_slot < 4) {
                    emit_movsd_xmm_rbp(arg_xmms[reg_slot], arg_offsets[i]);
                }
            } else {
                emit_mov_reg_rbp(call_regs[reg_slot], arg_offsets[i]);
            }
        } else {
            emit_mov_reg_rbp(reg::RAX, arg_offsets[i]);
            int32_t stack_off;
            if constexpr (PlatformDefs::is_windows) {
                stack_off = 32 + static_cast<int32_t>(reg_slot * 8);
            } else {
                stack_off = static_cast<int32_t>((reg_slot - MAX_REG_ARGS) * 8);
            }
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

    // System V: AL = 0
    if constexpr (PlatformDefs::VARIADIC_NEEDS_AL) {
        emit_xor_reg_reg(reg::RAX, reg::RAX);
    }

    uint32_t sym_idx;
    if (emitter_.has_symbol(method_sym)) {
        sym_idx = emitter_.symbol_index(method_sym);
    } else {
        sym_idx = emitter_.add_extern_symbol(method_sym);
    }
    emit_call_symbol(sym_idx);

    // Se RAX == 0 (método sem retorno), usar ponteiro salvo
    emit_test_reg_reg(reg::RAX, reg::RAX);
    size_t skip = emit_jne_rel32();
    emit_mov_reg_rbp(reg::RAX, inst_off);
    patch_jump(skip);
}

// ======================================================================
// INFERÊNCIA DE TIPO PARA EXPRESSÕES DE CLASSE
// ======================================================================

RuntimeType infer_attr_type(const AttrGetExpr& node) {
    RuntimeType type = RuntimeType::Unknown;
    resolve_attr_offset(node, type);
    return type;
}

RuntimeType infer_metodo_type(const MetodoChamadaExpr& node) {
    (void)node;
    return RuntimeType::Unknown;
}

// ======================================================================
// HELPER: detectar se AssignStmt é atribuição com construtor
// ======================================================================

bool is_constructor_call(const AssignStmt& node,
                          std::string& out_class,
                          std::string& out_method) {
    if (!std::holds_alternative<MetodoChamadaExpr>(node.value->node))
        return false;

    auto& mc = std::get<MetodoChamadaExpr>(node.value->node);

    if (!std::holds_alternative<VarExpr>(mc.object->node))
        return false;

    auto& var = std::get<VarExpr>(mc.object->node);
    if (declared_classes_.find(var.name) == declared_classes_.end())
        return false;

    out_class = var.name;
    out_method = mc.method;
    return true;
}

// Helper para obter classe de instâncias em listas
std::string get_list_instance_class(const std::string& var_name) {
    auto it = var_list_instance_class_.find(var_name);
    if (it != var_list_instance_class_.end()) return it->second;
    return "";
}
