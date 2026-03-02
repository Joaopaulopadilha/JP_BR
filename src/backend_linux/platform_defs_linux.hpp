// platform_defs_linux.hpp
// Definições de plataforma Linux x86-64 (ELF) — calling convention, relocations, símbolos externos

#ifndef JPLANG_PLATFORM_DEFS_HPP
#define JPLANG_PLATFORM_DEFS_HPP

#include "elf_emitter.hpp"

namespace jplang {

// Registradores (precisam estar visíveis antes de PlatformDefs)
namespace reg {
    constexpr uint8_t RAX = 0;
    constexpr uint8_t RCX = 1;
    constexpr uint8_t RDX = 2;
    constexpr uint8_t RBX = 3;
    constexpr uint8_t RSP = 4;
    constexpr uint8_t RBP = 5;
    constexpr uint8_t RSI = 6;
    constexpr uint8_t RDI = 7;
    constexpr uint8_t R8  = 8;
    constexpr uint8_t R9  = 9;
    constexpr uint8_t R10 = 10;
    constexpr uint8_t R11 = 11;
    constexpr uint8_t R12 = 12;
    constexpr uint8_t R13 = 13;
    constexpr uint8_t R14 = 14;
    constexpr uint8_t R15 = 15;
}

namespace xmm {
    constexpr uint8_t XMM0 = 0;
    constexpr uint8_t XMM1 = 1;
    constexpr uint8_t XMM2 = 2;
    constexpr uint8_t XMM3 = 3;
    constexpr uint8_t XMM4 = 4;
    constexpr uint8_t XMM5 = 5;
}

// ============================================================================
// DEFINIÇÕES DA PLATAFORMA LINUX x86-64
// ============================================================================

struct PlatformDefs {

    // --- Identificação ---
    static constexpr bool is_windows = false;
    static constexpr bool is_linux   = true;

    // --- Tipo do emitter ---
    using Emitter = ElfEmitter;

    // --- Calling Convention (System V AMD64 ABI) ---
    //
    // Parâmetros inteiros: RDI, RSI, RDX, RCX, R8, R9, depois stack
    // Parâmetros float:    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
    //                      (contagem SEPARADA dos inteiros)
    // Retorno:             RAX (int), XMM0 (float)
    // Shadow space:        NÃO existe
    // Variádica (printf):  AL = número de XMM args usados
    //
    static constexpr uint8_t ARG1 = reg::RDI;
    static constexpr uint8_t ARG2 = reg::RSI;
    static constexpr uint8_t ARG3 = reg::RDX;
    static constexpr uint8_t ARG4 = reg::RCX;
    static constexpr uint8_t ARG5 = reg::R8;
    static constexpr uint8_t ARG6 = reg::R9;

    // Float args têm contagem separada (1º float sempre em XMM0)
    static constexpr uint8_t FLOAT_ARG1 = xmm::XMM0;
    static constexpr uint8_t FLOAT_ARG2 = xmm::XMM1;
    static constexpr uint8_t FLOAT_ARG3 = xmm::XMM2;
    static constexpr uint8_t FLOAT_ARG4 = xmm::XMM3;

    // Stack mínima no prólogo (sem shadow space, apenas alinhamento)
    static constexpr int32_t MIN_STACK = 16;

    // --- Variádica (printf) ---
    //
    // System V: funções variádicas exigem AL = número de XMM args.
    // Floats vão APENAS no XMM, não precisam ir no GPR.
    //
    static constexpr bool VARIADIC_NEEDS_AL        = true;
    static constexpr bool VARIADIC_FLOAT_MIRROR_GPR = false;

    // --- Tipos de relocation ---
    static constexpr uint32_t REL_CALL = R_X86_64_PLT32;
    static constexpr uint32_t REL_RIP  = R_X86_64_PC32;

    // --- Relocation: ELF usa addend (RELA) ---
    static constexpr bool HAS_RELOC_ADDEND = true;
    static constexpr int64_t DEFAULT_CALL_ADDEND = -4;
    static constexpr int64_t DEFAULT_RIP_ADDEND  = -4;

    // --- Símbolos externos específicos da plataforma ---
    static void add_platform_externs(Emitter& emitter) {
        emitter.add_extern_symbol("stdin");
        emitter.add_extern_symbol("strcmp");
    }

    // --- Símbolos externos comuns (libc) ---
    static void add_common_externs(Emitter& emitter) {
        emitter.add_extern_symbol("printf");
        emitter.add_extern_symbol("sprintf");
        emitter.add_extern_symbol("exit");
        emitter.add_extern_symbol("putchar");
        emitter.add_extern_symbol("malloc");
        emitter.add_extern_symbol("realloc");
        emitter.add_extern_symbol("memmove");
        emitter.add_extern_symbol("fgets");
        emitter.add_extern_symbol("strlen");
        emitter.add_extern_symbol("atoi");
        emitter.add_extern_symbol("atof");
    }

    // --- Seções extras na inicialização ---
    static void create_extra_sections(Emitter& emitter) {
        // Seção vazia que indica stack não-executável (evita warning do ld)
        emitter.create_section(".note.GNU-stack", SHT_PROGBITS, 0, 1);
    }
};

} // namespace jplang

#endif // JPLANG_PLATFORM_DEFS_HPP
