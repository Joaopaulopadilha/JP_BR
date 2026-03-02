// codegen_funcao.hpp
// Emissão de funções (main, usuário), contagem de locais e retorno — unificado Windows/Linux

// ======================================================================
// PRÉ-ANÁLISE DE TIPOS DE RETORNO DE FUNÇÕES DO USUÁRIO
//
// Estratégia em 3 etapas:
//   1. Coletar call sites no programa inteiro → inferir tipos dos argumentos
//      e propagar para os parâmetros de cada função
//   2. Simular as atribuições dentro de cada função para rastrear
//      tipos de variáveis locais
//   3. Analisar os `retorna` para inferir o tipo de retorno
//
// Isso é necessário porque emit_main roda ANTES de emit_function,
// então infer_expr_type(ChamadaExpr) precisa saber o tipo de retorno.
// ======================================================================

// Helper: coleta call sites recursivamente em expressões
void collect_call_sites_expr(const Expr& expr,
        std::unordered_map<std::string, std::vector<RuntimeType>>& param_types_map) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ChamadaExpr>) {
            auto fit = declared_funcs_.find(node.name);
            if (fit != declared_funcs_.end()) {
                for (size_t i = 0; i < node.args.size(); i++) {
                    RuntimeType at = infer_expr_type(*node.args[i]);
                    auto& vec = param_types_map[node.name];
                    if (vec.size() <= i) vec.resize(i + 1, RuntimeType::Unknown);
                    if (at != RuntimeType::Unknown) vec[i] = at;
                }
            }
            // Recursar nos argumentos
            for (auto& arg : node.args) {
                collect_call_sites_expr(*arg, param_types_map);
            }
        }
        else if constexpr (std::is_same_v<T, BinOpExpr>) {
            collect_call_sites_expr(*node.left, param_types_map);
            collect_call_sites_expr(*node.right, param_types_map);
        }
        else if constexpr (std::is_same_v<T, CmpOpExpr>) {
            collect_call_sites_expr(*node.left, param_types_map);
            collect_call_sites_expr(*node.right, param_types_map);
        }
        else if constexpr (std::is_same_v<T, LogicOpExpr>) {
            collect_call_sites_expr(*node.left, param_types_map);
            collect_call_sites_expr(*node.right, param_types_map);
        }
        else if constexpr (std::is_same_v<T, ConcatExpr>) {
            collect_call_sites_expr(*node.left, param_types_map);
            collect_call_sites_expr(*node.right, param_types_map);
        }
        else if constexpr (std::is_same_v<T, MetodoChamadaExpr>) {
            collect_call_sites_expr(*node.object, param_types_map);
            for (auto& arg : node.args) {
                collect_call_sites_expr(*arg, param_types_map);
            }
        }
        else if constexpr (std::is_same_v<T, IndexGetExpr>) {
            collect_call_sites_expr(*node.object, param_types_map);
            collect_call_sites_expr(*node.index, param_types_map);
        }
    }, expr.node);
}

// Helper: coleta call sites recursivamente em statements
void collect_call_sites_stmts(const StmtList& stmts,
        std::unordered_map<std::string, std::vector<RuntimeType>>& param_types_map) {
    for (auto& stmt : stmts) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, AssignStmt>) {
                collect_call_sites_expr(*node.value, param_types_map);
            }
            else if constexpr (std::is_same_v<T, ExprStmt>) {
                collect_call_sites_expr(*node.expr, param_types_map);
            }
            else if constexpr (std::is_same_v<T, SaidaStmt>) {
                if (node.value) collect_call_sites_expr(*node.value, param_types_map);
            }
            else if constexpr (std::is_same_v<T, RetornaStmt>) {
                if (node.value) collect_call_sites_expr(*node.value, param_types_map);
            }
            else if constexpr (std::is_same_v<T, IfStmt>) {
                for (auto& br : node.branches) {
                    if (br.condition) collect_call_sites_expr(*br.condition, param_types_map);
                    collect_call_sites_stmts(br.body, param_types_map);
                }
            }
            else if constexpr (std::is_same_v<T, EnquantoStmt>) {
                collect_call_sites_expr(*node.condition, param_types_map);
                collect_call_sites_stmts(node.body, param_types_map);
            }
            else if constexpr (std::is_same_v<T, ParaStmt>) {
                collect_call_sites_stmts(node.body, param_types_map);
            }
            else if constexpr (std::is_same_v<T, RepetirStmt>) {
                collect_call_sites_stmts(node.body, param_types_map);
            }
            else if constexpr (std::is_same_v<T, IndexSetStmt>) {
                collect_call_sites_expr(*node.index, param_types_map);
                collect_call_sites_expr(*node.value, param_types_map);
            }
            else if constexpr (std::is_same_v<T, AttrSetStmt>) {
                collect_call_sites_expr(*node.value, param_types_map);
            }
        }, stmt->node);
    }
}

// Helper: infere tipo de retorno percorrendo statements com var_types_ preenchido
RuntimeType infer_return_type_from_stmts(const StmtList& stmts) {
    RuntimeType found = RuntimeType::Unknown;
    for (auto& stmt : stmts) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, AssignStmt>) {
                // Rastrear tipo da variável para uso em retorna
                RuntimeType vt = infer_expr_type(*node.value);
                if (vt != RuntimeType::Unknown) {
                    var_types_[node.name] = vt;
                }
            }
            else if constexpr (std::is_same_v<T, RetornaStmt>) {
                if (node.value) {
                    RuntimeType rt = infer_expr_type(*node.value);
                    if (rt != RuntimeType::Unknown) {
                        found = rt;
                    }
                }
            }
            else if constexpr (std::is_same_v<T, IfStmt>) {
                for (auto& br : node.branches) {
                    RuntimeType rt = infer_return_type_from_stmts(br.body);
                    if (rt != RuntimeType::Unknown) {
                        found = rt;
                    }
                }
            }
            else if constexpr (std::is_same_v<T, EnquantoStmt>) {
                RuntimeType rt = infer_return_type_from_stmts(node.body);
                if (rt != RuntimeType::Unknown) {
                    found = rt;
                }
            }
            else if constexpr (std::is_same_v<T, ParaStmt>) {
                RuntimeType rt = infer_return_type_from_stmts(node.body);
                if (rt != RuntimeType::Unknown) {
                    found = rt;
                }
            }
            else if constexpr (std::is_same_v<T, RepetirStmt>) {
                RuntimeType rt = infer_return_type_from_stmts(node.body);
                if (rt != RuntimeType::Unknown) {
                    found = rt;
                }
            }
        }, stmt->node);
    }
    return found;
}

void preanalyze_func_return_types(const Program& program) {
    // Etapa 1: Coletar tipos dos argumentos de todos os call sites
    // (no main e dentro das próprias funções)
    std::unordered_map<std::string, std::vector<RuntimeType>> param_types_map;

    // Call sites no corpo do programa (main)
    collect_call_sites_stmts(program.statements, param_types_map);

    // Call sites dentro das funções
    for (auto& stmt : program.statements) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, FuncaoStmt>) {
                collect_call_sites_stmts(node.body, param_types_map);
            }
        }, stmt->node);
    }

    // Etapa 2+3: Para cada função, setar var_types_ dos parâmetros
    // com os tipos coletados dos call sites, simular atribuições,
    // e inferir tipo de retorno.
    // Duas passadas para resolver dependências entre funções.
    for (int pass = 0; pass < 2; pass++) {
        for (auto& stmt : program.statements) {
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, FuncaoStmt>) {
                    var_types_.clear();

                    // Propagar tipos dos argumentos dos call sites
                    // para os parâmetros da função
                    auto pit = param_types_map.find(node.name);
                    if (pit != param_types_map.end()) {
                        for (size_t i = 0; i < node.params.size() &&
                             i < pit->second.size(); i++) {
                            if (pit->second[i] != RuntimeType::Unknown) {
                                var_types_[node.params[i]] = pit->second[i];
                            }
                        }
                    }

                    RuntimeType rt = infer_return_type_from_stmts(node.body);
                    if (rt != RuntimeType::Unknown) {
                        func_return_types_[node.name] = rt;
                    }
                }
            }, stmt->node);
        }
    }

    // Limpar var_types_ — será preenchido corretamente durante emissão
    var_types_.clear();
}

// ======================================================================
// EMISSÃO DA FUNÇÃO MAIN
// ======================================================================

void emit_main_function(const Program& program) {
    uint32_t main_offset = static_cast<uint32_t>(text_->pos());
    uint32_t main_sym = emitter_.add_global_symbol("main", text_idx_,
                                                     main_offset, true);
    (void)main_sym;

    locals_.clear();
    local_offset_ = 0;
    var_types_.clear();

    int32_t estimated_locals = count_locals(program.statements) + 32;
    int32_t local_bytes = estimated_locals * 8 + PlatformDefs::MIN_STACK;
    emit_prologue(local_bytes);

#ifdef _WIN32
    // ---- WINDOWS: inicialização do CRT e captura de args Unicode ----

    // Inicializar CRT do MinGW (constructors)
    emit_call_symbol(emitter_.symbol_index("__main"));

    // Capturar argumentos via __wgetmainargs (Unicode/UTF-16)
    //   int __wgetmainargs(int* argc, wchar_t*** argv, wchar_t*** env, int doWild, void* si)
    int32_t wargc_off = alloc_local("__jp_wargc");
    int32_t wargv_off = alloc_local("__jp_wargv");
    std::string wenv_tmp = "__jp_wenv_" + std::to_string(text_->pos());
    int32_t wenv_off = alloc_local(wenv_tmp);
    std::string si_tmp = "__jp_si_" + std::to_string(text_->pos());
    int32_t si_off = alloc_local(si_tmp);
    emit_mov_rbp_imm32(si_off, 0);

    // RCX = &wargc
    emit_rex_w(reg::RCX, reg::RBP);
    text_->emit_u8(0x8D);
    text_->emit_u8(0x8D);
    text_->emit_i32(wargc_off);

    // RDX = &wargv
    emit_rex_w(reg::RDX, reg::RBP);
    text_->emit_u8(0x8D);
    text_->emit_u8(0x95);
    text_->emit_i32(wargv_off);

    // R8 = &wenv
    emit_rex_w(reg::R8, reg::RBP);
    text_->emit_u8(0x8D);
    text_->emit_u8(0x85 | ((reg::R8 & 7) << 3));
    text_->emit_i32(wenv_off);

    // R9 = 0
    emit_mov_reg_imm32(reg::R9, 0);

    // 5º arg: &startinfo → [rsp+32]
    emit_rex_w(reg::RAX, reg::RBP);
    text_->emit_u8(0x8D);
    text_->emit_u8(0x85);
    text_->emit_i32(si_off);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89);
    text_->emit_u8(0x44);
    text_->emit_u8(0x24);
    text_->emit_u8(0x20);

    emit_call_symbol(emitter_.symbol_index("__wgetmainargs"));

    // Sign-extend argc (int32 → int64)
    emit_mov_reg_rbp(reg::RAX, wargc_off);
    text_->emit_u8(0x48);
    text_->emit_u8(0x63);
    text_->emit_u8(0xC0);
    int32_t argc_off = alloc_local("__jp_argc");
    emit_mov_rbp_reg(argc_off, reg::RAX);

    // Alocar array de char* para argv UTF-8: malloc(argc * 8)
    emit_mov_reg_reg(reg::RCX, reg::RAX);
    emit_rex_w(0, reg::RCX);
    text_->emit_u8(0xC1); // shl rcx, 3
    text_->emit_u8(0xE1);
    text_->emit_u8(0x03);
    emit_call_symbol(emitter_.symbol_index("malloc"));
    int32_t argv_off = alloc_local("__jp_argv");
    emit_mov_rbp_reg(argv_off, reg::RAX);

    // Loop de conversão: para cada i, converter wargv[i] de UTF-16 → UTF-8
    std::string li = "__jp_argi_" + std::to_string(text_->pos());
    int32_t i_off = alloc_local(li);
    emit_mov_rbp_imm32(i_off, 0);

    size_t conv_loop_top = text_->pos();

    // if (i >= argc) sair
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_mov_reg_rbp(reg::RCX, argc_off);
    emit_cmp_reg_reg(reg::RAX, reg::RCX);
    size_t conv_loop_exit = emit_jge_rel32();

    // wstr = wargv[i]
    emit_mov_reg_rbp(reg::RAX, wargv_off);
    emit_mov_reg_rbp(reg::RCX, i_off);
    emit_rex_w(reg::RAX, reg::RAX);
    text_->emit_u8(0x8B);
    text_->emit_u8(0x04);
    text_->emit_u8(0xC8);
    std::string ws = "__jp_wstr_" + std::to_string(text_->pos());
    int32_t wstr_off = alloc_local(ws);
    emit_mov_rbp_reg(wstr_off, reg::RAX);

    // WideCharToMultiByte(65001, 0, wstr, -1, NULL, 0, NULL, NULL) → tamanho
    emit_mov_reg_imm32(reg::RCX, 65001);
    emit_mov_reg_imm32(reg::RDX, 0);
    emit_mov_reg_rbp(reg::R8, wstr_off);
    emit_mov_reg_imm32(reg::R9, -1);
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x20);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x28);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x30);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x38);
    emit_call_symbol(emitter_.symbol_index("WideCharToMultiByte"));

    // RAX = tamanho, salvar e malloc
    std::string sz = "__jp_sz_" + std::to_string(text_->pos());
    int32_t sz_off = alloc_local(sz);
    emit_mov_rbp_reg(sz_off, reg::RAX);
    emit_mov_reg_reg(reg::RCX, reg::RAX);
    emit_call_symbol(emitter_.symbol_index("malloc"));
    std::string bf = "__jp_buf_" + std::to_string(text_->pos());
    int32_t buf_off = alloc_local(bf);
    emit_mov_rbp_reg(buf_off, reg::RAX);

    // WideCharToMultiByte(65001, 0, wstr, -1, buf, tamanho, NULL, NULL)
    emit_mov_reg_imm32(reg::RCX, 65001);
    emit_mov_reg_imm32(reg::RDX, 0);
    emit_mov_reg_rbp(reg::R8, wstr_off);
    emit_mov_reg_imm32(reg::R9, -1);
    emit_mov_reg_rbp(reg::RAX, buf_off);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x20);
    emit_mov_reg_rbp(reg::RAX, sz_off);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x28);
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x30);
    emit_rex_w(reg::RAX, reg::RSP);
    text_->emit_u8(0x89); text_->emit_u8(0x44); text_->emit_u8(0x24); text_->emit_u8(0x38);
    emit_call_symbol(emitter_.symbol_index("WideCharToMultiByte"));

    // argv[i] = buf
    emit_mov_reg_rbp(reg::RAX, argv_off);
    emit_mov_reg_rbp(reg::RCX, i_off);
    emit_mov_reg_rbp(reg::RDX, buf_off);
    emit_rex_w(reg::RDX, reg::RAX);
    text_->emit_u8(0x89);
    text_->emit_u8(0x14);
    text_->emit_u8(0xC8);

    // i++
    emit_mov_reg_rbp(reg::RAX, i_off);
    emit_rex_w(0, reg::RAX);
    text_->emit_u8(0xFF);
    text_->emit_u8(0xC0);
    emit_mov_rbp_reg(i_off, reg::RAX);

    // jmp conv_loop_top
    text_->emit_u8(0xE9);
    int32_t back_off = static_cast<int32_t>(conv_loop_top) -
                       static_cast<int32_t>(text_->pos() + 4);
    text_->emit_i32(back_off);

    patch_jump(conv_loop_exit);

    // Ativar UTF-8 no console: SetConsoleOutputCP(65001)
    emit_mov_reg_imm32(reg::RCX, 65001);
    emit_call_symbol(emitter_.symbol_index("SetConsoleOutputCP"));

    // Habilitar ANSI escape codes: SetConsoleMode
    emit_mov_reg_imm32(reg::RCX, -11); // STD_OUTPUT_HANDLE
    emit_call_symbol(emitter_.symbol_index("GetStdHandle"));
    emit_mov_reg_reg(reg::RCX, reg::RAX);
    emit_mov_reg_imm32(reg::RDX, 0x0005);
    emit_call_symbol(emitter_.symbol_index("SetConsoleMode"));

#else
    // ---- LINUX: args vêm direto nos registradores ----

    // System V: main(int argc, char** argv)
    //   RDI = argc, RSI = argv
    int32_t argc_off = alloc_local("__jp_argc");
    int32_t argv_off = alloc_local("__jp_argv");
    emit_mov_rbp_reg(argc_off, reg::RDI);
    emit_mov_rbp_reg(argv_off, reg::RSI);

#endif

    // Emitir statements (pular declarações de função/classe/nativo)
    for (auto& stmt : program.statements) {
        bool is_func_or_class = std::visit([](const auto& node) -> bool {
            using T = std::decay_t<decltype(node)>;
            return std::is_same_v<T, FuncaoStmt> ||
                   std::is_same_v<T, ClasseStmt> ||
                   std::is_same_v<T, NativoStmt>;
        }, stmt->node);

        if (!is_func_or_class) {
            emit_stmt(*stmt);
        }
    }

    // Saída
#ifdef _WIN32
    emit_xor_reg_reg(reg::RCX, reg::RCX);
    emit_call_symbol(emitter_.symbol_index("exit"));
#endif
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_epilogue();
}

// ======================================================================
// EMISSÃO DE FUNÇÃO DO USUÁRIO — unificado via PlatformDefs
// ======================================================================

void emit_function(const FuncaoStmt& func) {
    uint32_t func_offset = static_cast<uint32_t>(text_->pos());
    uint32_t func_sym = emitter_.add_global_symbol(func.name, text_idx_,
                                                    func_offset, true);

    FuncInfo info;
    info.name = func.name;
    info.symbol_index = func_sym;
    info.params = func.params;

    // Preservar param_types coletados dos call sites durante emit_main
    auto prev = declared_funcs_.find(func.name);
    if (prev != declared_funcs_.end()) {
        info.param_types = prev->second.param_types;
    }
    declared_funcs_[func.name] = info;

    locals_.clear();
    local_offset_ = 0;
    var_types_.clear();

    // Registradores de parâmetros da plataforma
    constexpr size_t MAX_REG_PARAMS = PlatformDefs::is_windows ? 4 : 6;
    const uint8_t param_regs[] = {
        PlatformDefs::ARG1, PlatformDefs::ARG2,
        PlatformDefs::ARG3, PlatformDefs::ARG4,
        PlatformDefs::ARG5, PlatformDefs::ARG6,
    };

    int32_t estimated_locals = count_locals(func.body) +
                               static_cast<int32_t>(func.params.size()) + 16;
    int32_t local_bytes = estimated_locals * 8 + PlatformDefs::MIN_STACK;
    emit_prologue(local_bytes);

    // Salvar parâmetros dos registradores em variáveis locais
    for (size_t i = 0; i < func.params.size() && i < MAX_REG_PARAMS; i++) {
        int32_t offset = alloc_local(func.params[i]);
        emit_mov_rbp_reg(offset, param_regs[i]);
    }

    // Parâmetros extras vêm da stack
    // Windows: [RBP+48] = 5º arg (shadow 32 + ret 8 + rbp 8)
    // Linux:   [RBP+16] = 7º arg (ret 8 + rbp 8)
    constexpr int32_t STACK_ARGS_BASE = PlatformDefs::is_windows ? 48 : 16;
    for (size_t i = MAX_REG_PARAMS; i < func.params.size(); i++) {
        int32_t src_offset = STACK_ARGS_BASE +
                             static_cast<int32_t>((i - MAX_REG_PARAMS) * 8);
        int32_t dst = alloc_local(func.params[i]);
        emit_mov_reg_rbp(reg::RAX, src_offset);
        emit_mov_rbp_reg(dst, reg::RAX);
    }

    // Registrar tipos dos parâmetros a partir dos call sites
    auto fit = declared_funcs_.find(func.name);
    if (fit != declared_funcs_.end()) {
        for (size_t i = 0; i < func.params.size(); i++) {
            if (i < fit->second.param_types.size()) {
                var_types_[func.params[i]] = fit->second.param_types[i];
            }
        }
    }

    for (auto& stmt : func.body) {
        emit_stmt(*stmt);
    }

    // Retorno padrão: 0
    emit_xor_reg_reg(reg::RAX, reg::RAX);
    emit_epilogue();
}

// ======================================================================
// CONTAGEM DE VARIÁVEIS LOCAIS (para pré-alocação de stack frame)
//
// IMPORTANTE: toda função emit_* que chama alloc_local() para criar
// temporários precisa ser contabilizada aqui. Caso contrário o frame
// fica subdimensionado e temporários invadem zona abaixo do RSP.
// ======================================================================

int32_t count_locals(const StmtList& stmts) {
    int32_t count = 0;
    for (auto& stmt : stmts) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, AssignStmt>) {
                count++;
                count += count_expr_temps(*node.value);
            }
            else if constexpr (std::is_same_v<T, ExprStmt>) {
                count += count_expr_temps(*node.expr);
            }
            else if constexpr (std::is_same_v<T, SaidaStmt>) {
                if (node.value) {
                    count += count_expr_temps(*node.value);
                    count += 6;
                }
            }
            else if constexpr (std::is_same_v<T, IndexSetStmt>) {
                count += 2;
                count += count_expr_temps(*node.index);
                count += count_expr_temps(*node.value);
            }
            else if constexpr (std::is_same_v<T, ParaStmt>) {
                count += 3;
                count += count_locals(node.body);
            }
            else if constexpr (std::is_same_v<T, IfStmt>) {
                for (auto& br : node.branches)
                    count += count_locals(br.body);
            }
            else if constexpr (std::is_same_v<T, EnquantoStmt>) {
                count += count_locals(node.body);
            }
            else if constexpr (std::is_same_v<T, RepetirStmt>) {
                count++;
                count += count_locals(node.body);
            }
            else if constexpr (std::is_same_v<T, AttrSetStmt>) {
                count += 2;
            }
        }, stmt->node);
    }
    return count;
}

// ======================================================================
// Conta temporários que uma expressão vai alocar
// ======================================================================

int32_t count_expr_temps(const Expr& expr) {
    return std::visit([&](const auto& node) -> int32_t {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ChamadaExpr>) {
            int32_t base = static_cast<int32_t>(node.args.size()) + 1;
            base += 2;
            return base;
        }
        else if constexpr (std::is_same_v<T, MetodoChamadaExpr>) {
            return static_cast<int32_t>(node.args.size()) + 8;
        }
        else if constexpr (std::is_same_v<T, ListLitExpr>) {
            return 2 + static_cast<int32_t>(node.elements.size());
        }
        else if constexpr (std::is_same_v<T, IndexGetExpr>) {
            return 2 + count_expr_temps(*node.object) + count_expr_temps(*node.index);
        }
        else if constexpr (std::is_same_v<T, BinOpExpr>) {
            int32_t base = count_expr_temps(*node.left) + count_expr_temps(*node.right);
            // String concat (a + b) usa 5 temporários extras: left, right, len1, len2, buf
            base += 5;
            return base;
        }
        else if constexpr (std::is_same_v<T, CmpOpExpr>) {
            return count_expr_temps(*node.left) + count_expr_temps(*node.right);
        }
        else if constexpr (std::is_same_v<T, ConcatExpr>) {
            int32_t base = count_expr_temps(*node.left) + count_expr_temps(*node.right);
            base += 6; // left, right, len1, len2, buf + margem
            return base;
        }
        else {
            return 0;
        }
    }, expr.node);
}

// ======================================================================
// RETORNA
// ======================================================================

void emit_retorna(const RetornaStmt& node) {
    if (node.value) {
        emit_expr(*node.value);
    } else {
        emit_xor_reg_reg(reg::RAX, reg::RAX);
    }
    emit_epilogue();
}