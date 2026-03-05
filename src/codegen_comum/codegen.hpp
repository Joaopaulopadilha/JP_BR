// codegen.hpp
// Gerador de código x86-64 para JPLang — classe unificada Windows/Linux via PlatformDefs

#ifndef JPLANG_CODEGEN_HPP
#define JPLANG_CODEGEN_HPP

// ============================================================================
// SELEÇÃO DE PLATAFORMA — inclui o platform_defs correto
// ============================================================================

#ifdef _WIN32
    #include "../backend_windows/platform_defs_windows.hpp"
#else
    #include "../backend_linux/platform_defs_linux.hpp"
#endif

#include "../frontend/ast.hpp"
#include "../frontend/lexer.hpp"  // LangConfig

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

namespace jplang {

// ============================================================================
// INFORMAÇÃO DE VARIÁVEL LOCAL
// ============================================================================

struct LocalVar {
    std::string name;
    int32_t rbp_offset;
};

// ============================================================================
// TIPO EM TEMPO DE COMPILAÇÃO (para type tracking simples)
// ============================================================================

enum class RuntimeType {
    Int,
    Float,
    String,
    Bool,
    Null,
    Unknown
};

// ============================================================================
// INFORMAÇÃO DE FUNÇÃO
// ============================================================================

struct FuncInfo {
    std::string name;
    uint32_t symbol_index = 0;
    std::vector<std::string> params;
    std::vector<RuntimeType> param_types;
};

// ============================================================================
// CONTEXTO DE LOOP (para parar/continuar)
// ============================================================================

struct LoopContext {
    std::vector<size_t> break_patches;
    std::vector<size_t> continue_patches;
    size_t loop_start;
};

// ============================================================================
// CODEGEN — classe unificada
// ============================================================================

class Codegen {
public:
    Codegen() : text_(nullptr), rdata_(nullptr), data_(nullptr),
                text_idx_(0), rdata_idx_(0), data_idx_(0),
                local_offset_(0), stack_reserved_(0) {}

    // ======================================================================
    // COMPILE — entrada principal
    // ======================================================================

    bool compile(const Program& program, const std::string& output_path,
                 const std::string& base_dir = "",
                 const LangConfig& lang_config = LangConfig{}) {
        base_dir_ = base_dir;
        lang_config_ = lang_config;
        extra_obj_paths_.clear();
        extra_libs_.clear();
        extra_lib_paths_.clear();
        extra_dll_paths_.clear();

        // Criar seções
        emitter_.create_text_section();   // [0]
        emitter_.create_rdata_section();  // [1]
        emitter_.create_data_section();   // [2]

        // Seções extras da plataforma (ex: .note.GNU-stack no Linux)
        PlatformDefs::create_extra_sections(emitter_);

        text_idx_  = 0;
        rdata_idx_ = 1;
        data_idx_  = 2;

        text_  = &emitter_.section(text_idx_);
        rdata_ = &emitter_.section(rdata_idx_);
        data_  = &emitter_.section(data_idx_);

        // Símbolos externos comuns (printf, malloc, etc.)
        PlatformDefs::add_common_externs(emitter_);

        // Símbolos externos específicos da plataforma
        PlatformDefs::add_platform_externs(emitter_);

        init_native_funcs();

        // Pré-processamento: registrar funções, classes e nativas
        for (auto& stmt : program.statements) {
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, FuncaoStmt>) {
                    declared_funcs_[node.name] = {};
                }
                else if constexpr (std::is_same_v<T, ClasseStmt>) {
                    preregister_class(node);
                }
                else if constexpr (std::is_same_v<T, NativoStmt>) {
                    emit_nativo(node);
                }
            }, stmt->node);
        }

        // Resolver tipos de atributos de classes a partir dos call sites
        if constexpr (PlatformDefs::is_windows) {
            resolve_class_attr_types(program);
        } else {
            preanalyze_constructor_calls(program);
        }

        // Pré-analisar tipos de retorno das funções do usuário
        // (precisa rodar antes de emit_main para que infer_expr_type funcione)
        preanalyze_func_return_types(program);

        // Gerar main
        emit_main_function(program);

        // Gerar funções e métodos de classe
        for (auto& stmt : program.statements) {
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, FuncaoStmt>) {
                    emit_function(node);
                }
                else if constexpr (std::is_same_v<T, ClasseStmt>) {
                    emit_class_methods(node);
                }
            }, stmt->node);
        }

        return emitter_.write(output_path);
    }

    // ======================================================================
    // DIRETÓRIO DO EXECUTÁVEL (para resolver bibliotecas/)
    // ======================================================================

    void set_exe_dir(const std::string& dir) { exe_dir_ = dir; }

    // ======================================================================
    // ACESSORES PARA LINKAGEM
    // ======================================================================

    const std::vector<std::string>& extra_obj_paths() const {
        return extra_obj_paths_;
    }

    const std::vector<std::string>& extra_libs() const {
        return extra_libs_;
    }

    const std::vector<std::string>& extra_lib_paths() const {
        return extra_lib_paths_;
    }

    const std::vector<std::string>& extra_dll_paths() const {
        return extra_dll_paths_;
    }

private:
    // ======================================================================
    // MEMBERS
    // ======================================================================

    typename PlatformDefs::Emitter emitter_;
    Section* text_;
    Section* rdata_;
    Section* data_;
    size_t text_idx_;
    size_t rdata_idx_;
    size_t data_idx_;

    std::vector<LocalVar> locals_;
    int32_t local_offset_;
    int32_t stack_reserved_;

    std::unordered_map<std::string, FuncInfo> declared_funcs_;
    std::unordered_map<std::string, uint32_t> string_offsets_;
    std::vector<LoopContext> loop_stack_;
    std::unordered_map<std::string, RuntimeType> var_types_;
    std::unordered_map<std::string, RuntimeType> func_return_types_;
    std::vector<std::string> extra_obj_paths_;
    std::vector<std::string> extra_libs_;
    std::vector<std::string> extra_lib_paths_;
    std::vector<std::string> extra_dll_paths_;
    std::string base_dir_;
    std::string exe_dir_;
    LangConfig lang_config_;
    std::unordered_map<std::string, std::string> var_instance_class_;

    // Members de listas (usados por codegen_atribuicao e codegen_listas)
    std::unordered_set<std::string> var_is_list_;
    std::unordered_map<std::string, RuntimeType> var_list_elem_type_;
    std::unordered_map<std::string, std::string> var_list_instance_class_;

    // Members de classes — definidos em codegen_classe.hpp (incluído inline abaixo)
    // ClassInfo, declared_classes_, current_class_, auto_local_offset_
    // são declarados no #include "codegen_classe.hpp"

    // ======================================================================
    // HELPERS DE EMISSÃO x86-64 (instruções puras, sem relocation)
    // ======================================================================

    void emit_rex_w(uint8_t reg = 0, uint8_t rm = 0) {
        uint8_t rex = 0x48;
        if (reg >= 8) rex |= 0x04;
        if (rm >= 8)  rex |= 0x01;
        text_->emit_u8(rex);
    }

    void emit_mov_reg_imm64(uint8_t reg, uint64_t imm) {
        emit_rex_w(0, reg);
        text_->emit_u8(0xB8 + (reg & 7));
        text_->emit_u64(imm);
    }

    void emit_mov_reg_imm32(uint8_t reg, int32_t imm) {
        emit_rex_w(0, reg);
        text_->emit_u8(0xC7);
        text_->emit_u8(0xC0 | (reg & 7));
        text_->emit_i32(imm);
    }

    void emit_mov_reg_reg(uint8_t dst, uint8_t src) {
        emit_rex_w(src, dst);
        text_->emit_u8(0x89);
        text_->emit_u8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    void emit_mov_reg_rbp(uint8_t reg, int32_t offset) {
        emit_rex_w(reg, reg::RBP);
        text_->emit_u8(0x8B);
        text_->emit_u8(0x85 | ((reg & 7) << 3));
        text_->emit_i32(offset);
    }

    void emit_mov_rbp_reg(int32_t offset, uint8_t reg) {
        emit_rex_w(reg, reg::RBP);
        text_->emit_u8(0x89);
        text_->emit_u8(0x85 | ((reg & 7) << 3));
        text_->emit_i32(offset);
    }

    void emit_mov_rbp_imm32(int32_t offset, int32_t imm) {
        emit_rex_w(0, reg::RBP);
        text_->emit_u8(0xC7);
        text_->emit_u8(0x85);
        text_->emit_i32(offset);
        text_->emit_i32(imm);
    }

    void emit_push(uint8_t reg) {
        if (reg >= 8) text_->emit_u8(0x41);
        text_->emit_u8(0x50 + (reg & 7));
    }

    void emit_pop(uint8_t reg) {
        if (reg >= 8) text_->emit_u8(0x41);
        text_->emit_u8(0x58 + (reg & 7));
    }

    void emit_add_reg_reg(uint8_t dst, uint8_t src) {
        emit_rex_w(src, dst);
        text_->emit_u8(0x01);
        text_->emit_u8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    void emit_sub_reg_reg(uint8_t dst, uint8_t src) {
        emit_rex_w(src, dst);
        text_->emit_u8(0x29);
        text_->emit_u8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    void emit_imul_reg_reg(uint8_t dst, uint8_t src) {
        emit_rex_w(dst, src);
        text_->emit_u8(0x0F);
        text_->emit_u8(0xAF);
        text_->emit_u8(0xC0 | ((dst & 7) << 3) | (src & 7));
    }

    void emit_idiv_reg(uint8_t reg) {
        emit_rex_w(0, reg);
        text_->emit_u8(0xF7);
        text_->emit_u8(0xF8 | (reg & 7));
    }

    void emit_cqo() {
        text_->emit_u8(0x48);
        text_->emit_u8(0x99);
    }

    // SHL reg, imm8
    void emit_shl_reg_imm(uint8_t reg, uint8_t imm) {
        emit_rex_w(0, reg);
        text_->emit_u8(0xC1);
        text_->emit_u8(0xE0 | (reg & 7));
        text_->emit_u8(imm);
    }

    // SHR reg, imm8
    void emit_shr_reg_imm(uint8_t reg, uint8_t imm) {
        emit_rex_w(0, reg);
        text_->emit_u8(0xC1);
        text_->emit_u8(0xE8 | (reg & 7));
        text_->emit_u8(imm);
    }

    // AND reg, imm32
    void emit_and_reg_imm32(uint8_t reg, int32_t imm) {
        emit_rex_w(0, reg);
        text_->emit_u8(0x81);
        text_->emit_u8(0xE0 | (reg & 7));
        text_->emit_i32(imm);
    }

    void emit_sub_rsp_imm32(int32_t imm) {
        emit_rex_w(0, reg::RSP);
        text_->emit_u8(0x81);
        text_->emit_u8(0xEC);
        text_->emit_i32(imm);
    }

    void emit_add_rsp_imm32(int32_t imm) {
        emit_rex_w(0, reg::RSP);
        text_->emit_u8(0x81);
        text_->emit_u8(0xC4);
        text_->emit_i32(imm);
    }

    void emit_cmp_reg_reg(uint8_t left, uint8_t right) {
        emit_rex_w(right, left);
        text_->emit_u8(0x39);
        text_->emit_u8(0xC0 | ((right & 7) << 3) | (left & 7));
    }

    void emit_cmp_reg_imm32(uint8_t reg, int32_t imm) {
        emit_rex_w(0, reg);
        text_->emit_u8(0x81);
        text_->emit_u8(0xF8 | (reg & 7));
        text_->emit_i32(imm);
    }

    void emit_setcc(uint8_t cc, uint8_t reg) {
        if (reg >= 8) text_->emit_u8(0x41);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x90 + cc);
        text_->emit_u8(0xC0 | (reg & 7));
    }

    void emit_movzx_reg64_reg8(uint8_t dst, uint8_t src) {
        emit_rex_w(dst, src);
        text_->emit_u8(0x0F);
        text_->emit_u8(0xB6);
        text_->emit_u8(0xC0 | ((dst & 7) << 3) | (src & 7));
    }

    // Condition codes (inteiro)
    static constexpr uint8_t CC_E  = 0x04;
    static constexpr uint8_t CC_NE = 0x05;
    static constexpr uint8_t CC_G  = 0x0F;
    static constexpr uint8_t CC_L  = 0x0C;
    static constexpr uint8_t CC_GE = 0x0D;
    static constexpr uint8_t CC_LE = 0x0E;

    uint8_t cmpop_to_cc(CmpOp op) {
        switch (op) {
            case CmpOp::Eq: return CC_E;
            case CmpOp::Ne: return CC_NE;
            case CmpOp::Gt: return CC_G;
            case CmpOp::Lt: return CC_L;
            case CmpOp::Ge: return CC_GE;
            case CmpOp::Le: return CC_LE;
        }
        return CC_E;
    }

    // ======================================================================
    // JUMPS
    // ======================================================================

    size_t emit_jmp_rel32() {
        text_->emit_u8(0xE9);
        size_t p = text_->pos();
        text_->emit_i32(0);
        return p;
    }

    size_t emit_jcc_rel32(uint8_t cc) {
        text_->emit_u8(0x0F);
        text_->emit_u8(0x80 + cc);
        size_t p = text_->pos();
        text_->emit_i32(0);
        return p;
    }

    size_t emit_je_rel32()  { return emit_jcc_rel32(CC_E); }
    size_t emit_jne_rel32() { return emit_jcc_rel32(CC_NE); }
    size_t emit_jle_rel32() { return emit_jcc_rel32(CC_LE); }
    size_t emit_jge_rel32() { return emit_jcc_rel32(CC_GE); }

    void patch_jump(size_t patch_offset) {
        int32_t target = static_cast<int32_t>(text_->pos());
        int32_t source = static_cast<int32_t>(patch_offset + 4);
        text_->patch_i32(patch_offset, target - source);
    }

    // ======================================================================
    // CALL E LEA — dependem da plataforma (relocation types)
    // ======================================================================

    // Helper: adiciona relocation de forma portável (COFF=3 args, ELF=4 args)
    void emit_relocation(uint32_t offset, uint32_t sym_id, uint32_t type, int64_t addend = 0) {
        #ifdef _WIN32
        // COFF: sem addend
        (void)addend;
        text_->add_relocation(offset, sym_id, static_cast<uint16_t>(type));
        #else
        // ELF: com addend
        text_->add_relocation(offset, sym_id, type, addend);
        #endif
    }

    void emit_call_symbol(uint32_t sym_index) {
        text_->emit_u8(0xE8);
        uint32_t reloc_pos = static_cast<uint32_t>(text_->pos());
        text_->emit_i32(0);
        emit_relocation(reloc_pos, sym_index,
                        PlatformDefs::REL_CALL,
                        PlatformDefs::DEFAULT_CALL_ADDEND);
    }

    // Helper: chama função externa por nome (registra símbolo se necessário)
    void emit_call_extern(const std::string& name) {
        if (!emitter_.has_symbol(name)) {
            emitter_.add_extern_symbol(name);
        }
        emit_call_symbol(emitter_.symbol_index(name));
    }

    // LEA reg, [RIP + section:offset]
    void emit_lea_rip_reloc(uint8_t reg, size_t section_idx, uint32_t offset_in_section) {
        emit_rex_w(reg, 0);
        text_->emit_u8(0x8D);
        text_->emit_u8(0x05 | ((reg & 7) << 3));
        uint32_t reloc_pos = static_cast<uint32_t>(text_->pos());
        #ifdef _WIN32
        // COFF: offset vai no imm32, relocation sem addend
        text_->emit_i32(static_cast<int32_t>(offset_in_section));
        emit_relocation(reloc_pos,
                        emitter_.section_symbol(section_idx),
                        PlatformDefs::REL_RIP, 0);
        #else
        // ELF: placeholder 0, addend codifica o offset
        text_->emit_i32(0);
        emit_relocation(reloc_pos,
                        emitter_.section_symbol(section_idx),
                        PlatformDefs::REL_RIP,
                        static_cast<int64_t>(offset_in_section) + PlatformDefs::DEFAULT_RIP_ADDEND);
        #endif
    }

    // LEA reg, [RIP + symbol]
    void emit_lea_rip_symbol(uint8_t reg, uint32_t sym_index) {
        emit_rex_w(reg, 0);
        text_->emit_u8(0x8D);
        text_->emit_u8(0x05 | ((reg & 7) << 3));
        uint32_t reloc_pos = static_cast<uint32_t>(text_->pos());
        text_->emit_i32(0);
        emit_relocation(reloc_pos, sym_index,
                        PlatformDefs::REL_RIP,
                        PlatformDefs::DEFAULT_RIP_ADDEND);
    }

    void emit_xor_reg_reg(uint8_t reg1, uint8_t reg2) {
        emit_rex_w(reg1, reg2);
        text_->emit_u8(0x31);
        text_->emit_u8(0xC0 | ((reg1 & 7) << 3) | (reg2 & 7));
    }

    void emit_test_reg_reg(uint8_t reg1, uint8_t reg2) {
        emit_rex_w(reg1, reg2);
        text_->emit_u8(0x85);
        text_->emit_u8(0xC0 | ((reg1 & 7) << 3) | (reg2 & 7));
    }

    void emit_ret() { text_->emit_u8(0xC3); }
    void emit_nop() { text_->emit_u8(0x90); }

    // ======================================================================
    // HELPERS SSE2 — OPERAÇÕES COM DOUBLE (XMM0-XMM5)
    // ======================================================================

    uint32_t add_double_constant(double val) {
        uint64_t bits;
        std::memcpy(&bits, &val, 8);
        std::string key = "__dbl_" + std::to_string(bits);

        auto it = string_offsets_.find(key);
        if (it != string_offsets_.end()) return it->second;

        rdata_->align(8);
        uint32_t offset = static_cast<uint32_t>(rdata_->pos());
        rdata_->emit(reinterpret_cast<const uint8_t*>(&val), 8);
        string_offsets_[key] = offset;
        return offset;
    }

    // MOVSD xmm, [RIP+disp32] — carrega double via RIP-relative
    void emit_movsd_xmm_rip(uint8_t xmm_dst, size_t section_idx, uint32_t offset_in_section) {
        text_->emit_u8(0xF2);
        if (xmm_dst >= 8) {
            text_->emit_u8(0x44);
        }
        text_->emit_u8(0x0F);
        text_->emit_u8(0x10);
        text_->emit_u8(0x05 | ((xmm_dst & 7) << 3));
        uint32_t reloc_pos = static_cast<uint32_t>(text_->pos());
        #ifdef _WIN32
        text_->emit_i32(static_cast<int32_t>(offset_in_section));
        emit_relocation(reloc_pos,
                        emitter_.section_symbol(section_idx),
                        PlatformDefs::REL_RIP, 0);
        #else
        text_->emit_i32(0);
        emit_relocation(reloc_pos,
                        emitter_.section_symbol(section_idx),
                        PlatformDefs::REL_RIP,
                        static_cast<int64_t>(offset_in_section) + PlatformDefs::DEFAULT_RIP_ADDEND);
        #endif
    }

    void emit_load_double_imm(uint8_t xmm_dst, double val) {
        uint32_t offset = add_double_constant(val);
        emit_movsd_xmm_rip(xmm_dst, rdata_idx_, offset);
    }

    // MOVSD xmm, [RBP+offset]
    void emit_movsd_xmm_rbp(uint8_t xmm_dst, int32_t rbp_offset) {
        text_->emit_u8(0xF2);
        if (xmm_dst >= 8) {
            text_->emit_u8(0x44);
        }
        text_->emit_u8(0x0F);
        text_->emit_u8(0x10);
        text_->emit_u8(0x85 | ((xmm_dst & 7) << 3));
        text_->emit_i32(rbp_offset);
    }

    // MOVSD [RBP+offset], xmm
    void emit_movsd_rbp_xmm(int32_t rbp_offset, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        if (xmm_src >= 8) {
            text_->emit_u8(0x44);
        }
        text_->emit_u8(0x0F);
        text_->emit_u8(0x11);
        text_->emit_u8(0x85 | ((xmm_src & 7) << 3));
        text_->emit_i32(rbp_offset);
    }

    // MOVSD xmm_dst, xmm_src
    void emit_movsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0;
        if (xmm_dst >= 8) rex |= 0x44;
        if (xmm_src >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x10);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // ADDSD xmm_dst, xmm_src
    void emit_addsd(uint8_t xmm_dst, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0;
        if (xmm_dst >= 8) rex |= 0x44;
        if (xmm_src >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x58);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // SUBSD xmm_dst, xmm_src
    void emit_subsd(uint8_t xmm_dst, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0;
        if (xmm_dst >= 8) rex |= 0x44;
        if (xmm_src >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x5C);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // MULSD xmm_dst, xmm_src
    void emit_mulsd(uint8_t xmm_dst, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0;
        if (xmm_dst >= 8) rex |= 0x44;
        if (xmm_src >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x59);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // DIVSD xmm_dst, xmm_src
    void emit_divsd(uint8_t xmm_dst, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0;
        if (xmm_dst >= 8) rex |= 0x44;
        if (xmm_src >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x5E);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // UCOMISD xmm1, xmm2 — comparação double (seta EFLAGS)
    void emit_ucomisd(uint8_t xmm1, uint8_t xmm2) {
        text_->emit_u8(0x66);
        uint8_t rex = 0;
        if (xmm1 >= 8) rex |= 0x44;
        if (xmm2 >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x2E);
        text_->emit_u8(0xC0 | ((xmm1 & 7) << 3) | (xmm2 & 7));
    }

    // CVTSI2SD xmm, reg64
    void emit_cvtsi2sd(uint8_t xmm_dst, uint8_t gpr_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0x48;
        if (xmm_dst >= 8) rex |= 0x04;
        if (gpr_src >= 8)  rex |= 0x01;
        text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x2A);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (gpr_src & 7));
    }

    // CVTTSD2SI reg64, xmm
    void emit_cvttsd2si(uint8_t gpr_dst, uint8_t xmm_src) {
        text_->emit_u8(0xF2);
        uint8_t rex = 0x48;
        if (gpr_dst >= 8) rex |= 0x04;
        if (xmm_src >= 8) rex |= 0x01;
        text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x2C);
        text_->emit_u8(0xC0 | ((gpr_dst & 7) << 3) | (xmm_src & 7));
    }

    // MOVQ GPR, XMM
    void emit_movq_gpr_xmm(uint8_t gpr_dst, uint8_t xmm_src) {
        text_->emit_u8(0x66);
        uint8_t rex = 0x48;
        if (xmm_src >= 8) rex |= 0x04;
        if (gpr_dst >= 8)  rex |= 0x01;
        text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x7E);
        text_->emit_u8(0xC0 | ((xmm_src & 7) << 3) | (gpr_dst & 7));
    }

    // MOVQ XMM, GPR
    void emit_movq_xmm_gpr(uint8_t xmm_dst, uint8_t gpr_src) {
        text_->emit_u8(0x66);
        uint8_t rex = 0x48;
        if (xmm_dst >= 8) rex |= 0x04;
        if (gpr_src >= 8)  rex |= 0x01;
        text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x6E);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (gpr_src & 7));
    }

    // XORPD xmm, xmm — zerar XMM
    void emit_xorpd(uint8_t xmm_dst, uint8_t xmm_src) {
        text_->emit_u8(0x66);
        uint8_t rex = 0;
        if (xmm_dst >= 8) rex |= 0x44;
        if (xmm_src >= 8) rex |= 0x41;
        if (rex) text_->emit_u8(rex);
        text_->emit_u8(0x0F);
        text_->emit_u8(0x57);
        text_->emit_u8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // Condition codes para float (após ucomisd)
    static constexpr uint8_t CC_A  = 0x07;
    static constexpr uint8_t CC_AE = 0x03;
    static constexpr uint8_t CC_B  = 0x02;
    static constexpr uint8_t CC_BE = 0x06;

    uint8_t cmpop_to_float_cc(CmpOp op) {
        switch (op) {
            case CmpOp::Eq: return CC_E;
            case CmpOp::Ne: return CC_NE;
            case CmpOp::Gt: return CC_A;
            case CmpOp::Lt: return CC_B;
            case CmpOp::Ge: return CC_AE;
            case CmpOp::Le: return CC_BE;
        }
        return CC_E;
    }

    // ======================================================================
    // STRINGS
    // ======================================================================

    uint32_t add_string(const std::string& str) {
        auto it = string_offsets_.find(str);
        if (it != string_offsets_.end()) return it->second;
        uint32_t offset = static_cast<uint32_t>(rdata_->pos());
        rdata_->emit_string(str);
        string_offsets_[str] = offset;
        return offset;
    }

    void emit_load_string(uint8_t reg, const std::string& str) {
        uint32_t offset = add_string(str);
        emit_lea_rip_reloc(reg, rdata_idx_, offset);
    }

    // ======================================================================
    // VARIÁVEIS LOCAIS
    // ======================================================================

    int32_t alloc_local(const std::string& name) {
        for (auto& lv : locals_) {
            if (lv.name == name) return lv.rbp_offset;
        }
        local_offset_ -= 8;
        locals_.push_back({name, local_offset_});
        return local_offset_;
    }

    int32_t find_local(const std::string& name) {
        for (auto& lv : locals_) {
            if (lv.name == name) return lv.rbp_offset;
        }
        return alloc_local(name);
    }

    // ======================================================================
    // PRÓLOGO / EPÍLOGO — usa PlatformDefs::MIN_STACK
    // ======================================================================

    void emit_prologue(int32_t local_bytes) {
        if (local_bytes % 16 != 0)
            local_bytes = (local_bytes + 15) & ~15;
        if (local_bytes < PlatformDefs::MIN_STACK)
            local_bytes = PlatformDefs::MIN_STACK;
        stack_reserved_ = local_bytes;
        emit_push(reg::RBP);
        emit_mov_reg_reg(reg::RBP, reg::RSP);
        emit_sub_rsp_imm32(local_bytes);
    }

    void emit_epilogue() {
        emit_mov_reg_reg(reg::RSP, reg::RBP);
        emit_pop(reg::RBP);
        emit_ret();
    }

    // ======================================================================
    // TYPE INFERENCE
    // ======================================================================

    RuntimeType infer_expr_type(const Expr& expr) {
        return std::visit([&](const auto& node) -> RuntimeType {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, NumberLit>)         return RuntimeType::Int;
            else if constexpr (std::is_same_v<T, FloatLit>)     return RuntimeType::Float;
            else if constexpr (std::is_same_v<T, StringLit>)    return RuntimeType::String;
            else if constexpr (std::is_same_v<T, StringInterp>) return RuntimeType::String;
            else if constexpr (std::is_same_v<T, BoolLit>)      return RuntimeType::Bool;
            else if constexpr (std::is_same_v<T, NullLit>)      return RuntimeType::Null;
            else if constexpr (std::is_same_v<T, VarExpr>) {
                auto it = var_types_.find(node.name);
                return (it != var_types_.end()) ? it->second : RuntimeType::Unknown;
            }
            else if constexpr (std::is_same_v<T, BinOpExpr>) {
                auto lt = infer_expr_type(*node.left);
                auto rt = infer_expr_type(*node.right);
                if (lt == RuntimeType::Float || rt == RuntimeType::Float)
                    return RuntimeType::Float;
                if (node.op == BinOp::Div)
                    return RuntimeType::Float;
                if (node.op == BinOp::Add &&
                    (lt == RuntimeType::String || rt == RuntimeType::String))
                    return RuntimeType::String;
                return RuntimeType::Int;
            }
            else if constexpr (std::is_same_v<T, CmpOpExpr>)    return RuntimeType::Bool;
            else if constexpr (std::is_same_v<T, LogicOpExpr>)  return RuntimeType::Bool;
            else if constexpr (std::is_same_v<T, ConcatExpr>)   return RuntimeType::String;
            else if constexpr (std::is_same_v<T, ChamadaExpr>) {
                auto it = func_return_types_.find(node.name);
                if (it != func_return_types_.end()) return it->second;
                return get_native_return_type(node.name);
            }
            else if constexpr (std::is_same_v<T, AttrGetExpr>) {
                return infer_attr_type(node);
            }
            else if constexpr (std::is_same_v<T, MetodoChamadaExpr>) {
                if (std::holds_alternative<VarExpr>(node.object->node)) {
                    auto& var = std::get<VarExpr>(node.object->node);
                    if (is_list_var(var.name) && node.method == "tamanho")
                        return RuntimeType::Int;
                }
                return infer_metodo_type(node);
            }
            else if constexpr (std::is_same_v<T, AutoExpr>) {
                return RuntimeType::Unknown;
            }
            else if constexpr (std::is_same_v<T, ListLitExpr>) {
                return RuntimeType::Unknown;
            }
            else if constexpr (std::is_same_v<T, IndexGetExpr>) {
                if (std::holds_alternative<VarExpr>(node.object->node)) {
                    auto& var = std::get<VarExpr>(node.object->node);
                    if (is_list_var(var.name)) {
                        return get_list_elem_type(var.name);
                    }
                }
                return RuntimeType::Unknown;
            }
            else return RuntimeType::Unknown;
        }, expr.node);
    }

    bool is_string_expr(const Expr* expr) {
        return infer_expr_type(*expr) == RuntimeType::String;
    }

    // ======================================================================
    // DISPATCH DE STATEMENTS
    // ======================================================================

    void emit_stmt(const Stmt& stmt) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, AssignStmt>)        emit_assign(node);
            else if constexpr (std::is_same_v<T, AttrSetStmt>)  emit_attr_set(node);
            else if constexpr (std::is_same_v<T, SaidaStmt>)    emit_saida(node);
            else if constexpr (std::is_same_v<T, IfStmt>)       emit_if(node);
            else if constexpr (std::is_same_v<T, EnquantoStmt>) emit_enquanto(node);
            else if constexpr (std::is_same_v<T, RepetirStmt>)  emit_repetir(node);
            else if constexpr (std::is_same_v<T, ParaStmt>)     emit_para(node);
            else if constexpr (std::is_same_v<T, PararStmt>)    emit_parar();
            else if constexpr (std::is_same_v<T, ContinuarStmt>) emit_continuar();
            else if constexpr (std::is_same_v<T, RetornaStmt>)  emit_retorna(node);
            else if constexpr (std::is_same_v<T, IndexSetStmt>) {
                if (is_list_var(node.name)) emit_list_index_set(node);
                else emit_index_set(node);
            }
            else if constexpr (std::is_same_v<T, ExprStmt>)     emit_expr(*node.expr);
        }, stmt.node);
    }

    // (todos os stubs foram migrados para sub-headers)

    // ======================================================================
    // SUB-HEADERS (incluídos inline dentro da classe)
    // Agora vêm de codegen_comum/ (unificados)
    // Descomentar conforme forem sendo migrados
    // ======================================================================

    // --- Já unificado ---
    // codegen_classe.hpp: define ClassInfo, declared_classes_, etc.
    // codegen_listas.hpp: define emit_list_*, is_list_var, emit_saida_list
    // codegen_saida.hpp: usa emit_saida_list do codegen_listas
    #include "codegen_classe.hpp"
    // codegen_nativo.hpp: carregamento de bibliotecas nativas via JSON
    #include "codegen_nativo.hpp"
    #include "codegen_funcoes_nativas.hpp"
    #include "codegen_listas.hpp"
    #include "codegen_saida.hpp"
    #include "codegen_expr.hpp"
    #include "codegen_atribuicao.hpp"
    #include "codegen_controle.hpp"
    #include "codegen_funcao.hpp"
};

} // namespace jplang

#endif // JPLANG_CODEGEN_HPP