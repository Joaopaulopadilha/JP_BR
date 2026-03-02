// platform_defs_windows.hpp
// Definições de plataforma Windows x64 (COFF/PE) — calling convention, relocations, símbolos externos

#ifndef JPLANG_PLATFORM_DEFS_HPP
#define JPLANG_PLATFORM_DEFS_HPP

#include "coff_emitter.hpp"

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
// DEFINIÇÕES DA PLATAFORMA WINDOWS x64
// ============================================================================

struct PlatformDefs {

    // --- Identificação ---
    static constexpr bool is_windows = true;
    static constexpr bool is_linux   = false;

    // --- Tipo do emitter ---
    using Emitter = CoffEmitter;

    // --- Calling Convention (Windows x64) ---
    //
    // Parâmetros inteiros: RCX, RDX, R8, R9, depois stack
    // Parâmetros float:    XMM0, XMM1, XMM2, XMM3
    // Retorno:             RAX (int), XMM0 (float)
    // Shadow space:        32 bytes obrigatórios
    // Variádica (printf):  float deve ir TANTO no XMM quanto no GPR correspondente
    //
    static constexpr uint8_t ARG1 = reg::RCX;
    static constexpr uint8_t ARG2 = reg::RDX;
    static constexpr uint8_t ARG3 = reg::R8;
    static constexpr uint8_t ARG4 = reg::R9;
    // Windows só tem 4 regs de arg — dummy pra compatibilidade com codegen unificado
    static constexpr uint8_t ARG5 = 0;
    static constexpr uint8_t ARG6 = 0;

    // Float args ocupam a MESMA posição que int args (posicional, não separado)
    // Ex: 2º argumento float vai em XMM1 (não XMM0)
    static constexpr uint8_t FLOAT_ARG1 = xmm::XMM0;
    static constexpr uint8_t FLOAT_ARG2 = xmm::XMM1;
    static constexpr uint8_t FLOAT_ARG3 = xmm::XMM2;
    static constexpr uint8_t FLOAT_ARG4 = xmm::XMM3;

    // Stack mínima no prólogo (shadow space = 32 bytes)
    static constexpr int32_t MIN_STACK = 32;

    // --- Variádica (printf) ---
    //
    // Windows x64: funções variádicas exigem que floats sejam passados
    // TANTO no XMM quanto no GPR correspondente à posição.
    // Não precisa setar AL.
    //
    static constexpr bool VARIADIC_NEEDS_AL        = false;
    static constexpr bool VARIADIC_FLOAT_MIRROR_GPR = true;

    // --- Tipos de relocation ---
    static constexpr uint16_t REL_CALL   = IMAGE_REL_AMD64_REL32;
    static constexpr uint16_t REL_RIP    = IMAGE_REL_AMD64_REL32;
    static constexpr uint16_t REL_ADDR64 = IMAGE_REL_AMD64_ADDR64;

    // --- Relocation: COFF não usa addend (diferente de ELF) ---
    static constexpr bool HAS_RELOC_ADDEND = false;
    // Valores dummy — nunca usados (o if constexpr garante), mas precisam existir pra compilar
    static constexpr int64_t DEFAULT_CALL_ADDEND = 0;
    static constexpr int64_t DEFAULT_RIP_ADDEND  = 0;

    // --- Símbolos externos específicos da plataforma ---
    static void add_platform_externs(Emitter& emitter) {
        emitter.add_extern_symbol("SetConsoleOutputCP");
        emitter.add_extern_symbol("GetStdHandle");
        emitter.add_extern_symbol("SetConsoleMode");
        emitter.add_extern_symbol("__acrt_iob_func");
        emitter.add_extern_symbol("__main");
        emitter.add_extern_symbol("__wgetmainargs");
        emitter.add_extern_symbol("WideCharToMultiByte");
    }

    // --- Símbolos externos comuns (libc/CRT) ---
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
    static void create_extra_sections(Emitter& /*emitter*/) {
        // Windows não precisa de seções extras (sem .note.GNU-stack)
    }
};

} // namespace jplang

#endif // JPLANG_PLATFORM_DEFS_HPP