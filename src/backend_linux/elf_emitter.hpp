// elf_emitter.hpp
// Emissor de arquivos objeto ELF x86-64 para Linux — gera .o compatível com GNU ld

#ifndef JPLANG_ELF_EMITTER_HPP
#define JPLANG_ELF_EMITTER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <stdexcept>

namespace jplang {

// ============================================================================
// CONSTANTES ELF64
// ============================================================================

// ELF Header
constexpr uint8_t  ELFMAG0 = 0x7F;
constexpr uint8_t  ELFMAG1 = 'E';
constexpr uint8_t  ELFMAG2 = 'L';
constexpr uint8_t  ELFMAG3 = 'F';
constexpr uint8_t  ELFCLASS64    = 2;
constexpr uint8_t  ELFDATA2LSB   = 1;
constexpr uint8_t  EV_CURRENT_ID = 1;
constexpr uint8_t  ELFOSABI_NONE = 0;
constexpr uint16_t ET_REL        = 1;       // Relocatable file
constexpr uint16_t EM_X86_64     = 62;

// Section Header Types
constexpr uint32_t SHT_NULL     = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB   = 2;
constexpr uint32_t SHT_STRTAB   = 3;
constexpr uint32_t SHT_RELA     = 4;
constexpr uint32_t SHT_NOBITS   = 8;

// Section Flags
constexpr uint64_t SHF_WRITE     = 0x01;
constexpr uint64_t SHF_ALLOC     = 0x02;
constexpr uint64_t SHF_EXECINSTR = 0x04;
constexpr uint64_t SHF_INFO_LINK = 0x40;

// Symbol Binding
constexpr uint8_t STB_LOCAL  = 0;
constexpr uint8_t STB_GLOBAL = 1;

// Symbol Type
constexpr uint8_t STT_NOTYPE  = 0;
constexpr uint8_t STT_FUNC    = 2;
constexpr uint8_t STT_SECTION = 3;
constexpr uint8_t STT_FILE    = 4;

// Symbol Visibility
constexpr uint8_t STV_DEFAULT = 0;

// Special Section Indices
constexpr uint16_t SHN_UNDEF = 0;
constexpr uint16_t SHN_ABS   = 0xFFF1;

// Relocation Types (x86-64)
constexpr uint32_t R_X86_64_PC32  = 2;     // S + A - P (RIP-relative)
constexpr uint32_t R_X86_64_PLT32 = 4;     // L + A - P (function call)

// Flag interna: IDs com esse bit são referências a seções
constexpr uint32_t ELF_SECTION_SYM_FLAG = 0x80000000;

// ============================================================================
// ESTRUTURAS ELF64 (layout binário exato)
// ============================================================================

#pragma pack(push, 1)

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};

#pragma pack(pop)

// Helper para construir st_info
inline uint8_t elf_st_info(uint8_t bind, uint8_t type) {
    return (bind << 4) | (type & 0x0F);
}

// Helper para construir r_info
inline uint64_t elf_r_info(uint32_t sym, uint32_t type) {
    return (static_cast<uint64_t>(sym) << 32) | type;
}

// ============================================================================
// SEÇÃO (reutiliza mesma struct Section com relocações ELF)
// ============================================================================

struct ElfRelocation {
    uint64_t offset;
    uint32_t symbol_id;     // ID interno (será remapeado em write())
    uint32_t type;           // R_X86_64_PC32, R_X86_64_PLT32, etc.
    int64_t  addend;
};

struct Section {
    std::string name;
    uint32_t    type;        // SHT_PROGBITS, etc.
    uint64_t    flags;       // SHF_ALLOC | SHF_EXECINSTR, etc.
    uint64_t    alignment;
    std::vector<uint8_t> data;
    std::vector<ElfRelocation> relocations;

    size_t size() const { return data.size(); }

    void emit(uint8_t byte) { data.push_back(byte); }
    void emit(const uint8_t* bytes, size_t count) {
        data.insert(data.end(), bytes, bytes + count);
    }
    void emit(const std::vector<uint8_t>& bytes) {
        data.insert(data.end(), bytes.begin(), bytes.end());
    }

    void emit_u8(uint8_t v)   { emit(v); }
    void emit_u16(uint16_t v) { emit(reinterpret_cast<uint8_t*>(&v), 2); }
    void emit_u32(uint32_t v) { emit(reinterpret_cast<uint8_t*>(&v), 4); }
    void emit_u64(uint64_t v) { emit(reinterpret_cast<uint8_t*>(&v), 8); }
    void emit_i8(int8_t v)    { emit(static_cast<uint8_t>(v)); }
    void emit_i32(int32_t v)  { emit(reinterpret_cast<uint8_t*>(&v), 4); }

    void emit_string(const std::string& s) {
        emit(reinterpret_cast<const uint8_t*>(s.c_str()), s.size() + 1);
    }

    void emit_zeros(size_t count) {
        data.insert(data.end(), count, 0);
    }

    void align(size_t al) {
        size_t remainder = data.size() % al;
        if (remainder != 0) emit_zeros(al - remainder);
    }

    void patch_u32(size_t offset, uint32_t v) {
        if (offset + 4 > data.size())
            throw std::runtime_error("patch_u32: offset fora dos limites");
        std::memcpy(&data[offset], &v, 4);
    }

    void patch_i32(size_t offset, int32_t v) {
        if (offset + 4 > data.size())
            throw std::runtime_error("patch_i32: offset fora dos limites");
        std::memcpy(&data[offset], &v, 4);
    }

    size_t pos() const { return data.size(); }

    void add_relocation(uint32_t offset, uint32_t symbol_id, uint32_t type,
                        int64_t addend = -4) {
        relocations.push_back({offset, symbol_id, type, addend});
    }
};

// ============================================================================
// SÍMBOLO (dados internos)
// ============================================================================

struct SymbolInfo {
    std::string name;
    uint64_t    value;
    uint16_t    section_index;   // 0xFFFF = externo/undefined
    uint8_t     bind;            // STB_LOCAL, STB_GLOBAL
    uint8_t     type;            // STT_NOTYPE, STT_FUNC, STT_SECTION
    uint64_t    size;
    static constexpr uint16_t UNDEF = 0xFFFF;
};

// ============================================================================
// ELF EMITTER
// ============================================================================

class ElfEmitter {
public:
    ElfEmitter() = default;

    // ------------------------------------------------------------------
    // Seções
    // ------------------------------------------------------------------

    Section& create_text_section() {
        return create_section(".text", SHT_PROGBITS,
            SHF_ALLOC | SHF_EXECINSTR, 16);
    }

    Section& create_data_section() {
        return create_section(".data", SHT_PROGBITS,
            SHF_ALLOC | SHF_WRITE, 8);
    }

    Section& create_rdata_section() {
        // Linux usa .rodata em vez de .rdata
        return create_section(".rodata", SHT_PROGBITS,
            SHF_ALLOC, 8);
    }

    Section& create_section(const std::string& name, uint32_t type,
                            uint64_t flags, uint64_t alignment) {
        Section sec;
        sec.name = name;
        sec.type = type;
        sec.flags = flags;
        sec.alignment = alignment;
        sections_.push_back(std::move(sec));
        return sections_.back();
    }

    Section& section(size_t index) { return sections_[index]; }
    size_t section_count() const { return sections_.size(); }

    // Retorna ID para relocações que referenciam uma seção
    uint32_t section_symbol(size_t section_index) {
        return ELF_SECTION_SYM_FLAG | static_cast<uint32_t>(section_index);
    }

    // ------------------------------------------------------------------
    // Símbolos
    // ------------------------------------------------------------------

    uint32_t add_global_symbol(const std::string& name, size_t section_index,
                               uint32_t offset, bool is_function = false) {
        SymbolInfo sym;
        sym.name = name;
        sym.value = offset;
        sym.section_index = static_cast<uint16_t>(section_index);
        sym.bind = STB_GLOBAL;
        sym.type = is_function ? STT_FUNC : STT_NOTYPE;
        sym.size = 0;
        return register_symbol(sym);
    }

    uint32_t add_extern_symbol(const std::string& name) {
        auto it = symbol_index_map_.find(name);
        if (it != symbol_index_map_.end()) return it->second;

        SymbolInfo sym;
        sym.name = name;
        sym.value = 0;
        sym.section_index = SymbolInfo::UNDEF;
        sym.bind = STB_GLOBAL;
        sym.type = STT_NOTYPE;
        sym.size = 0;
        return register_symbol(sym);
    }

    uint32_t add_static_symbol(const std::string& name, size_t section_index,
                                uint32_t offset) {
        SymbolInfo sym;
        sym.name = name;
        sym.value = offset;
        sym.section_index = static_cast<uint16_t>(section_index);
        sym.bind = STB_LOCAL;
        sym.type = STT_NOTYPE;
        sym.size = 0;
        return register_symbol(sym);
    }

    uint32_t symbol_index(const std::string& name) const {
        auto it = symbol_index_map_.find(name);
        if (it == symbol_index_map_.end())
            throw std::runtime_error("Simbolo nao encontrado: " + name);
        return it->second;
    }

    bool has_symbol(const std::string& name) const {
        return symbol_index_map_.count(name) > 0;
    }

    // ------------------------------------------------------------------
    // Serialização ELF64
    // ------------------------------------------------------------------

    bool write(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // ============================================================
        // 1) Construir tabelas de strings e símbolos
        // ============================================================

        // .strtab (nomes dos símbolos)
        std::vector<uint8_t> strtab;
        strtab.push_back(0);  // índice 0 = string vazia

        auto add_str = [&](const std::string& s) -> uint32_t {
            uint32_t off = static_cast<uint32_t>(strtab.size());
            strtab.insert(strtab.end(), s.begin(), s.end());
            strtab.push_back(0);
            return off;
        };

        // .shstrtab (nomes das seções)
        std::vector<uint8_t> shstrtab;
        shstrtab.push_back(0);

        auto add_shstr = [&](const std::string& s) -> uint32_t {
            uint32_t off = static_cast<uint32_t>(shstrtab.size());
            shstrtab.insert(shstrtab.end(), s.begin(), s.end());
            shstrtab.push_back(0);
            return off;
        };

        // ============================================================
        // 2) Construir lista de símbolos ELF
        // ============================================================

        std::vector<Elf64_Sym> elf_syms;
        std::unordered_map<uint32_t, uint32_t> user_remap;    // user_id → elf_sym_index
        std::unordered_map<uint32_t, uint32_t> section_remap;  // sec_idx → elf_sym_index

        // [0] Símbolo nulo obrigatório
        {
            Elf64_Sym null_sym;
            std::memset(&null_sym, 0, sizeof(null_sym));
            elf_syms.push_back(null_sym);
        }

        // Símbolos de seção (LOCAL) — um para cada seção de dados
        for (size_t i = 0; i < sections_.size(); i++) {
            Elf64_Sym sec_sym;
            std::memset(&sec_sym, 0, sizeof(sec_sym));
            sec_sym.st_name = 0;  // sem nome
            sec_sym.st_info = elf_st_info(STB_LOCAL, STT_SECTION);
            sec_sym.st_other = STV_DEFAULT;
            // section_index no ELF header: seção i do user → posição real calculada depois
            // Temporariamente guardamos i, será ajustado
            sec_sym.st_shndx = 0;  // placeholder
            sec_sym.st_value = 0;
            sec_sym.st_size = 0;
            section_remap[static_cast<uint32_t>(i)] = static_cast<uint32_t>(elf_syms.size());
            elf_syms.push_back(sec_sym);
        }

        uint32_t first_global = static_cast<uint32_t>(elf_syms.size());

        // Símbolos do usuário (GLOBAL)
        for (size_t i = 0; i < symbol_order_.size(); i++) {
            auto& info = symbols_[symbol_order_[i]];

            Elf64_Sym sym;
            std::memset(&sym, 0, sizeof(sym));
            sym.st_name = add_str(info.name);
            sym.st_info = elf_st_info(info.bind, info.type);
            sym.st_other = STV_DEFAULT;
            sym.st_shndx = info.section_index;  // placeholder, será ajustado
            sym.st_value = info.value;
            sym.st_size = info.size;

            user_remap[static_cast<uint32_t>(i)] = static_cast<uint32_t>(elf_syms.size());
            elf_syms.push_back(sym);
        }

        // ============================================================
        // 3) Calcular layout das seções no arquivo
        // ============================================================

        // Layout:
        //   ELF Header (64 bytes)
        //   Dados das seções do usuário (.text, .rodata, .data)
        //   .rela.text (se houver)
        //   .symtab
        //   .strtab
        //   .shstrtab
        //   Section Headers (no final)

        // Seções ELF finais:
        //   [0] NULL
        //   [1..N] seções do usuário (.text, .rodata, .data)
        //   [N+1..] .rela.xxx para cada seção com relocações
        //   [R+1]  .symtab
        //   [R+2]  .strtab
        //   [R+3]  .shstrtab

        uint32_t num_user_sections = static_cast<uint32_t>(sections_.size());

        // Contar seções .rela
        std::vector<uint32_t> rela_for_section;  // índice no user → índice da seção .rela
        uint32_t num_rela_sections = 0;
        for (size_t i = 0; i < sections_.size(); i++) {
            if (!sections_[i].relocations.empty()) {
                rela_for_section.push_back(static_cast<uint32_t>(i));
                num_rela_sections++;
            }
        }

        // Índices das seções ELF
        // [0] = NULL
        // [1..num_user] = seções do usuário
        // [num_user+1..num_user+num_rela] = .rela seções
        // [num_user+num_rela+1] = .symtab
        // [num_user+num_rela+2] = .strtab
        // [num_user+num_rela+3] = .shstrtab

        uint32_t symtab_shndx  = 1 + num_user_sections + num_rela_sections;
        uint32_t strtab_shndx  = symtab_shndx + 1;
        uint32_t shstrtab_shndx = strtab_shndx + 1;
        uint32_t total_sections = shstrtab_shndx + 1;

        // Agora corrigir st_shndx dos símbolos
        // Seções do user: user index i → ELF section index (i + 1)
        for (size_t i = 0; i < sections_.size(); i++) {
            uint32_t elf_sec_idx = static_cast<uint32_t>(i + 1);
            // Símbolo de seção
            elf_syms[section_remap[static_cast<uint32_t>(i)]].st_shndx = elf_sec_idx;
        }
        // Símbolos do user
        for (size_t i = 0; i < symbol_order_.size(); i++) {
            auto& info = symbols_[symbol_order_[i]];
            auto& sym = elf_syms[user_remap[static_cast<uint32_t>(i)]];
            if (info.section_index == SymbolInfo::UNDEF) {
                sym.st_shndx = SHN_UNDEF;
            } else {
                sym.st_shndx = static_cast<uint16_t>(info.section_index + 1);
            }
        }

        // ============================================================
        // 4) Construir dados de relocação ELF (.rela)
        // ============================================================

        struct RelaSection {
            uint32_t target_user_index;  // qual seção user é alvo
            std::vector<Elf64_Rela> entries;
        };
        std::vector<RelaSection> rela_sections;

        for (auto sec_idx : rela_for_section) {
            RelaSection rs;
            rs.target_user_index = sec_idx;

            for (auto& rel : sections_[sec_idx].relocations) {
                Elf64_Rela rela;
                rela.r_offset = rel.offset;
                rela.r_addend = rel.addend;

                // Remapear symbol_id
                uint32_t elf_sym_idx;
                if (rel.symbol_id & ELF_SECTION_SYM_FLAG) {
                    uint32_t si = rel.symbol_id & ~ELF_SECTION_SYM_FLAG;
                    elf_sym_idx = section_remap[si];
                } else {
                    elf_sym_idx = user_remap[rel.symbol_id];
                }

                rela.r_info = elf_r_info(elf_sym_idx, rel.type);
                rs.entries.push_back(rela);
            }
            rela_sections.push_back(std::move(rs));
        }

        // ============================================================
        // 5) Reservar nomes de seções no shstrtab
        // ============================================================

        std::vector<uint32_t> user_sec_name_offsets(num_user_sections);
        for (size_t i = 0; i < num_user_sections; i++) {
            user_sec_name_offsets[i] = add_shstr(sections_[i].name);
        }

        std::vector<uint32_t> rela_sec_name_offsets(rela_sections.size());
        for (size_t i = 0; i < rela_sections.size(); i++) {
            std::string rname = ".rela" + sections_[rela_sections[i].target_user_index].name;
            rela_sec_name_offsets[i] = add_shstr(rname);
        }

        uint32_t symtab_name_off  = add_shstr(".symtab");
        uint32_t strtab_name_off  = add_shstr(".strtab");
        uint32_t shstrtab_name_off = add_shstr(".shstrtab");

        // ============================================================
        // 6) Calcular offsets no arquivo
        // ============================================================

        uint64_t offset = 64;  // após ELF header

        // Dados das seções user
        std::vector<uint64_t> user_sec_offsets(num_user_sections);
        for (size_t i = 0; i < num_user_sections; i++) {
            // Alinhar
            uint64_t al = sections_[i].alignment;
            if (al > 1) {
                uint64_t rem = offset % al;
                if (rem != 0) offset += al - rem;
            }
            user_sec_offsets[i] = offset;
            offset += sections_[i].size();
        }

        // .rela seções
        std::vector<uint64_t> rela_sec_offsets(rela_sections.size());
        for (size_t i = 0; i < rela_sections.size(); i++) {
            uint64_t rem = offset % 8;
            if (rem != 0) offset += 8 - rem;
            rela_sec_offsets[i] = offset;
            offset += rela_sections[i].entries.size() * sizeof(Elf64_Rela);
        }

        // .symtab
        uint64_t rem = offset % 8;
        if (rem != 0) offset += 8 - rem;
        uint64_t symtab_offset = offset;
        uint64_t symtab_size = elf_syms.size() * sizeof(Elf64_Sym);
        offset += symtab_size;

        // .strtab
        uint64_t strtab_offset = offset;
        offset += strtab.size();

        // .shstrtab
        uint64_t shstrtab_offset = offset;
        offset += shstrtab.size();

        // Alinhar section headers a 8 bytes
        rem = offset % 8;
        if (rem != 0) offset += 8 - rem;
        uint64_t shdr_offset = offset;

        // ============================================================
        // 7) Escrever ELF Header
        // ============================================================

        Elf64_Ehdr ehdr;
        std::memset(&ehdr, 0, sizeof(ehdr));
        ehdr.e_ident[0] = ELFMAG0;
        ehdr.e_ident[1] = ELFMAG1;
        ehdr.e_ident[2] = ELFMAG2;
        ehdr.e_ident[3] = ELFMAG3;
        ehdr.e_ident[4] = ELFCLASS64;
        ehdr.e_ident[5] = ELFDATA2LSB;
        ehdr.e_ident[6] = EV_CURRENT_ID;
        ehdr.e_ident[7] = ELFOSABI_NONE;
        ehdr.e_type    = ET_REL;
        ehdr.e_machine = EM_X86_64;
        ehdr.e_version = 1;
        ehdr.e_entry   = 0;
        ehdr.e_phoff   = 0;
        ehdr.e_shoff   = shdr_offset;
        ehdr.e_flags   = 0;
        ehdr.e_ehsize  = sizeof(Elf64_Ehdr);
        ehdr.e_phentsize = 0;
        ehdr.e_phnum   = 0;
        ehdr.e_shentsize = sizeof(Elf64_Shdr);
        ehdr.e_shnum   = static_cast<uint16_t>(total_sections);
        ehdr.e_shstrndx = static_cast<uint16_t>(shstrtab_shndx);

        file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));

        // ============================================================
        // 8) Escrever dados das seções
        // ============================================================

        // Padding + dados user
        for (size_t i = 0; i < num_user_sections; i++) {
            // Padding até offset
            uint64_t cur = file.tellp();
            if (user_sec_offsets[i] > cur) {
                std::vector<uint8_t> pad(user_sec_offsets[i] - cur, 0);
                file.write(reinterpret_cast<const char*>(pad.data()), pad.size());
            }
            if (!sections_[i].data.empty()) {
                file.write(reinterpret_cast<const char*>(sections_[i].data.data()),
                           sections_[i].data.size());
            }
        }

        // .rela seções
        for (size_t i = 0; i < rela_sections.size(); i++) {
            uint64_t cur = file.tellp();
            if (rela_sec_offsets[i] > cur) {
                std::vector<uint8_t> pad(rela_sec_offsets[i] - cur, 0);
                file.write(reinterpret_cast<const char*>(pad.data()), pad.size());
            }
            for (auto& r : rela_sections[i].entries) {
                file.write(reinterpret_cast<const char*>(&r), sizeof(Elf64_Rela));
            }
        }

        // .symtab
        {
            uint64_t cur = file.tellp();
            if (symtab_offset > cur) {
                std::vector<uint8_t> pad(symtab_offset - cur, 0);
                file.write(reinterpret_cast<const char*>(pad.data()), pad.size());
            }
            for (auto& s : elf_syms) {
                file.write(reinterpret_cast<const char*>(&s), sizeof(Elf64_Sym));
            }
        }

        // .strtab
        file.write(reinterpret_cast<const char*>(strtab.data()), strtab.size());

        // .shstrtab
        file.write(reinterpret_cast<const char*>(shstrtab.data()), shstrtab.size());

        // ============================================================
        // 9) Escrever Section Headers
        // ============================================================

        {
            uint64_t cur = file.tellp();
            if (shdr_offset > cur) {
                std::vector<uint8_t> pad(shdr_offset - cur, 0);
                file.write(reinterpret_cast<const char*>(pad.data()), pad.size());
            }
        }

        // [0] NULL section header
        {
            Elf64_Shdr shdr;
            std::memset(&shdr, 0, sizeof(shdr));
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        // [1..N] User sections
        for (size_t i = 0; i < num_user_sections; i++) {
            Elf64_Shdr shdr;
            std::memset(&shdr, 0, sizeof(shdr));
            shdr.sh_name = user_sec_name_offsets[i];
            shdr.sh_type = sections_[i].type;
            shdr.sh_flags = sections_[i].flags;
            shdr.sh_addr = 0;
            shdr.sh_offset = user_sec_offsets[i];
            shdr.sh_size = sections_[i].size();
            shdr.sh_link = 0;
            shdr.sh_info = 0;
            shdr.sh_addralign = sections_[i].alignment;
            shdr.sh_entsize = 0;
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        // .rela sections
        for (size_t i = 0; i < rela_sections.size(); i++) {
            uint32_t target_elf_idx = rela_sections[i].target_user_index + 1;

            Elf64_Shdr shdr;
            std::memset(&shdr, 0, sizeof(shdr));
            shdr.sh_name = rela_sec_name_offsets[i];
            shdr.sh_type = SHT_RELA;
            shdr.sh_flags = SHF_INFO_LINK;
            shdr.sh_addr = 0;
            shdr.sh_offset = rela_sec_offsets[i];
            shdr.sh_size = rela_sections[i].entries.size() * sizeof(Elf64_Rela);
            shdr.sh_link = symtab_shndx;        // link para .symtab
            shdr.sh_info = target_elf_idx;       // seção alvo das relocações
            shdr.sh_addralign = 8;
            shdr.sh_entsize = sizeof(Elf64_Rela);
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        // .symtab
        {
            Elf64_Shdr shdr;
            std::memset(&shdr, 0, sizeof(shdr));
            shdr.sh_name = symtab_name_off;
            shdr.sh_type = SHT_SYMTAB;
            shdr.sh_flags = 0;
            shdr.sh_addr = 0;
            shdr.sh_offset = symtab_offset;
            shdr.sh_size = symtab_size;
            shdr.sh_link = strtab_shndx;         // link para .strtab
            shdr.sh_info = first_global;          // índice do primeiro GLOBAL
            shdr.sh_addralign = 8;
            shdr.sh_entsize = sizeof(Elf64_Sym);
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        // .strtab
        {
            Elf64_Shdr shdr;
            std::memset(&shdr, 0, sizeof(shdr));
            shdr.sh_name = strtab_name_off;
            shdr.sh_type = SHT_STRTAB;
            shdr.sh_flags = 0;
            shdr.sh_addr = 0;
            shdr.sh_offset = strtab_offset;
            shdr.sh_size = strtab.size();
            shdr.sh_link = 0;
            shdr.sh_info = 0;
            shdr.sh_addralign = 1;
            shdr.sh_entsize = 0;
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        // .shstrtab
        {
            Elf64_Shdr shdr;
            std::memset(&shdr, 0, sizeof(shdr));
            shdr.sh_name = shstrtab_name_off;
            shdr.sh_type = SHT_STRTAB;
            shdr.sh_flags = 0;
            shdr.sh_addr = 0;
            shdr.sh_offset = shstrtab_offset;
            shdr.sh_size = shstrtab.size();
            shdr.sh_link = 0;
            shdr.sh_info = 0;
            shdr.sh_addralign = 1;
            shdr.sh_entsize = 0;
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        file.close();
        return true;
    }

private:
    std::vector<Section> sections_;
    std::vector<SymbolInfo> symbols_;
    std::vector<size_t> symbol_order_;
    std::unordered_map<std::string, uint32_t> symbol_index_map_;

    uint32_t register_symbol(SymbolInfo& sym) {
        symbols_.push_back(sym);
        uint32_t index = static_cast<uint32_t>(symbol_order_.size());
        symbol_order_.push_back(symbols_.size() - 1);
        symbol_index_map_[sym.name] = index;
        return index;
    }
};

} // namespace jplang

#endif // JPLANG_ELF_EMITTER_HPP
