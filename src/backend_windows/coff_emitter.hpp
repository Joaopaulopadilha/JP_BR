// coff_emitter.hpp
// Emissor de arquivos objeto COFF x86-64 para Windows — gera .obj compatível com MinGW e MSVC

#ifndef JPLANG_COFF_EMITTER_HPP
#define JPLANG_COFF_EMITTER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <stdexcept>

namespace jplang {

// ============================================================================
// CONSTANTES COFF (PE/COFF Specification)
// ============================================================================

constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;

constexpr uint32_t IMAGE_SCN_CNT_CODE              = 0x00000020;
constexpr uint32_t IMAGE_SCN_CNT_INITIALIZED_DATA  = 0x00000040;
constexpr uint32_t IMAGE_SCN_ALIGN_1BYTES           = 0x00100000;
constexpr uint32_t IMAGE_SCN_ALIGN_4BYTES           = 0x00300000;
constexpr uint32_t IMAGE_SCN_ALIGN_8BYTES           = 0x00400000;
constexpr uint32_t IMAGE_SCN_ALIGN_16BYTES          = 0x00500000;
constexpr uint32_t IMAGE_SCN_MEM_EXECUTE            = 0x20000000;
constexpr uint32_t IMAGE_SCN_MEM_READ               = 0x40000000;
constexpr uint32_t IMAGE_SCN_MEM_WRITE              = 0x80000000;

constexpr uint8_t IMAGE_SYM_CLASS_EXTERNAL = 2;
constexpr uint8_t IMAGE_SYM_CLASS_STATIC   = 3;
constexpr uint8_t IMAGE_SYM_CLASS_LABEL    = 6;

constexpr int16_t IMAGE_SYM_UNDEFINED = 0;
constexpr int16_t IMAGE_SYM_ABSOLUTE  = -1;

constexpr uint16_t IMAGE_SYM_TYPE_NULL     = 0x0000;
constexpr uint16_t IMAGE_SYM_DTYPE_FUNCTION = 0x0020;

constexpr uint16_t IMAGE_REL_AMD64_ADDR64  = 0x0001;
constexpr uint16_t IMAGE_REL_AMD64_ADDR32  = 0x0002;
constexpr uint16_t IMAGE_REL_AMD64_ADDR32NB = 0x0003;
constexpr uint16_t IMAGE_REL_AMD64_REL32   = 0x0004;
constexpr uint16_t IMAGE_REL_AMD64_REL32_1 = 0x0005;
constexpr uint16_t IMAGE_REL_AMD64_REL32_2 = 0x0006;
constexpr uint16_t IMAGE_REL_AMD64_REL32_3 = 0x0007;
constexpr uint16_t IMAGE_REL_AMD64_REL32_4 = 0x0008;

// Flag interna: IDs com esse bit são referências a seções, não a user symbols
constexpr uint32_t SECTION_SYM_FLAG = 0x80000000;

// ============================================================================
// ESTRUTURAS COFF (layout binário exato)
// ============================================================================

#pragma pack(push, 1)

struct CoffHeader {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
};

struct CoffSectionHeader {
    char     name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_line_numbers;
    uint16_t number_of_relocations;
    uint16_t number_of_line_numbers;
    uint32_t characteristics;
};

struct CoffRelocation {
    uint32_t virtual_address;
    uint32_t symbol_table_index;
    uint16_t type;
};

struct CoffSymbol {
    union {
        char short_name[8];
        struct {
            uint32_t zeroes;
            uint32_t offset;
        } long_name;
    } name;
    uint32_t value;
    int16_t  section_number;
    uint16_t type;
    uint8_t  storage_class;
    uint8_t  number_of_aux_symbols;
};

#pragma pack(pop)

// ============================================================================
// SEÇÃO
// ============================================================================

struct Section {
    std::string name;
    uint32_t    characteristics;
    std::vector<uint8_t> data;
    std::vector<CoffRelocation> relocations;
    uint32_t    final_symbol_index; // preenchido em write()

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

    void align(size_t alignment) {
        size_t remainder = data.size() % alignment;
        if (remainder != 0) emit_zeros(alignment - remainder);
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

    void add_relocation(uint32_t offset, uint32_t symbol_index, uint16_t type) {
        CoffRelocation reloc;
        reloc.virtual_address = offset;
        reloc.symbol_table_index = symbol_index;
        reloc.type = type;
        relocations.push_back(reloc);
    }
};

// ============================================================================
// SÍMBOLO (dados internos)
// ============================================================================

struct SymbolInfo {
    std::string name;
    uint32_t    value;
    int16_t     section_number;
    uint16_t    type;
    uint8_t     storage_class;
};

// ============================================================================
// COFF EMITTER
// ============================================================================

class CoffEmitter {
public:
    CoffEmitter() = default;

    // ------------------------------------------------------------------
    // Seções
    // ------------------------------------------------------------------

    Section& create_text_section() {
        return create_section(".text",
            IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES |
            IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
    }

    Section& create_data_section() {
        return create_section(".data",
            IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_8BYTES |
            IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE);
    }

    Section& create_rdata_section() {
        return create_section(".rdata",
            IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_8BYTES |
            IMAGE_SCN_MEM_READ);
    }

    Section& create_section(const std::string& name, uint32_t characteristics) {
        Section sec;
        sec.name = name;
        sec.characteristics = characteristics;
        sec.final_symbol_index = 0;
        sections_.push_back(std::move(sec));
        return sections_.back();
    }

    Section& section(size_t index) { return sections_[index]; }
    size_t section_count() const { return sections_.size(); }

    // Retorna ID para usar em relocações que referenciam uma seção
    // O bit SECTION_SYM_FLAG distingue de user symbols
    uint32_t section_symbol(size_t section_index) {
        return SECTION_SYM_FLAG | static_cast<uint32_t>(section_index);
    }

    // ------------------------------------------------------------------
    // Símbolos
    // ------------------------------------------------------------------

    uint32_t add_global_symbol(const std::string& name, size_t section_index,
                               uint32_t offset, bool is_function = false) {
        SymbolInfo sym;
        sym.name = name;
        sym.value = offset;
        sym.section_number = static_cast<int16_t>(section_index + 1);
        sym.type = is_function ? IMAGE_SYM_DTYPE_FUNCTION : IMAGE_SYM_TYPE_NULL;
        sym.storage_class = IMAGE_SYM_CLASS_EXTERNAL;
        return register_symbol(sym);
    }

    uint32_t add_extern_symbol(const std::string& name) {
        auto it = symbol_index_map_.find(name);
        if (it != symbol_index_map_.end()) return it->second;

        SymbolInfo sym;
        sym.name = name;
        sym.value = 0;
        sym.section_number = IMAGE_SYM_UNDEFINED;
        sym.type = IMAGE_SYM_DTYPE_FUNCTION;
        sym.storage_class = IMAGE_SYM_CLASS_EXTERNAL;
        return register_symbol(sym);
    }

    uint32_t add_static_symbol(const std::string& name, size_t section_index,
                                uint32_t offset) {
        SymbolInfo sym;
        sym.name = name;
        sym.value = offset;
        sym.section_number = static_cast<int16_t>(section_index + 1);
        sym.type = IMAGE_SYM_TYPE_NULL;
        sym.storage_class = IMAGE_SYM_CLASS_STATIC;
        return register_symbol(sym);
    }

    uint32_t symbol_index(const std::string& name) const {
        auto it = symbol_index_map_.find(name);
        if (it == symbol_index_map_.end())
            throw std::runtime_error("Símbolo não encontrado: " + name);
        return it->second;
    }

    bool has_symbol(const std::string& name) const {
        return symbol_index_map_.count(name) > 0;
    }

    // ------------------------------------------------------------------
    // Serialização
    // ------------------------------------------------------------------

    bool write(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        std::vector<CoffSymbol> final_symbols;
        std::vector<uint8_t> string_table;
        uint32_t strtab_size = 4;

        auto add_str = [&](const std::string& name) -> uint32_t {
            uint32_t offset = strtab_size;
            string_table.insert(string_table.end(), name.begin(), name.end());
            string_table.push_back(0);
            strtab_size += static_cast<uint32_t>(name.size() + 1);
            return offset;
        };

        auto set_name = [&](CoffSymbol& s, const std::string& name) {
            std::memset(&s, 0, sizeof(CoffSymbol));
            if (name.size() <= 8) {
                std::memcpy(s.name.short_name, name.c_str(), name.size());
            } else {
                s.name.long_name.zeroes = 0;
                s.name.long_name.offset = add_str(name);
            }
        };

        // 1) Section symbols (cada seção = 1 symbol + 1 aux = 2 entradas)
        for (size_t i = 0; i < sections_.size(); i++) {
            auto& sec = sections_[i];
            sec.final_symbol_index = static_cast<uint32_t>(final_symbols.size());

            CoffSymbol sym;
            set_name(sym, sec.name);
            sym.value = 0;
            sym.section_number = static_cast<int16_t>(i + 1);
            sym.type = IMAGE_SYM_TYPE_NULL;
            sym.storage_class = IMAGE_SYM_CLASS_STATIC;
            sym.number_of_aux_symbols = 1;
            final_symbols.push_back(sym);

            CoffSymbol aux;
            std::memset(&aux, 0, sizeof(CoffSymbol));
            uint32_t sec_size = static_cast<uint32_t>(sec.size());
            uint16_t num_relocs = static_cast<uint16_t>(sec.relocations.size());
            std::memcpy(&aux, &sec_size, 4);
            std::memcpy(reinterpret_cast<char*>(&aux) + 4, &num_relocs, 2);
            final_symbols.push_back(aux);
        }

        // 2) User symbols
        std::unordered_map<uint32_t, uint32_t> user_remap;
        for (size_t i = 0; i < symbol_order_.size(); i++) {
            uint32_t new_idx = static_cast<uint32_t>(final_symbols.size());
            user_remap[static_cast<uint32_t>(i)] = new_idx;

            auto& info = symbols_[symbol_order_[i]];
            CoffSymbol sym;
            set_name(sym, info.name);
            sym.value = info.value;
            sym.section_number = info.section_number;
            sym.type = info.type;
            sym.storage_class = info.storage_class;
            sym.number_of_aux_symbols = 0;
            final_symbols.push_back(sym);
        }

        // 3) Remapear relocações
        for (auto& sec : sections_) {
            for (auto& reloc : sec.relocations) {
                if (reloc.symbol_table_index & SECTION_SYM_FLAG) {
                    uint32_t sec_idx = reloc.symbol_table_index & ~SECTION_SYM_FLAG;
                    reloc.symbol_table_index = sections_[sec_idx].final_symbol_index;
                } else {
                    auto it = user_remap.find(reloc.symbol_table_index);
                    if (it != user_remap.end()) {
                        reloc.symbol_table_index = it->second;
                    }
                }
            }
        }

        // Calcular offsets
        uint32_t num_sec = static_cast<uint32_t>(sections_.size());
        uint32_t file_offset = static_cast<uint32_t>(
            sizeof(CoffHeader) + num_sec * sizeof(CoffSectionHeader));

        struct Layout { uint32_t data_off, reloc_off; };
        std::vector<Layout> layouts(num_sec);

        for (size_t i = 0; i < num_sec; i++) {
            layouts[i].data_off = file_offset;
            file_offset += static_cast<uint32_t>(sections_[i].size());
            layouts[i].reloc_off = file_offset;
            file_offset += static_cast<uint32_t>(
                sections_[i].relocations.size() * sizeof(CoffRelocation));
        }

        uint32_t sym_table_off = file_offset;

        // Escrever header
        CoffHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        hdr.machine = IMAGE_FILE_MACHINE_AMD64;
        hdr.number_of_sections = static_cast<uint16_t>(num_sec);
        hdr.pointer_to_symbol_table = sym_table_off;
        hdr.number_of_symbols = static_cast<uint32_t>(final_symbols.size());
        file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

        // Escrever section headers
        for (size_t i = 0; i < num_sec; i++) {
            auto& sec = sections_[i];
            CoffSectionHeader shdr;
            std::memset(&shdr, 0, sizeof(shdr));

            if (sec.name.size() <= 8) {
                std::memcpy(shdr.name, sec.name.c_str(), sec.name.size());
            } else {
                uint32_t soff = add_str(sec.name);
                std::string ref = "/" + std::to_string(soff);
                std::memcpy(shdr.name, ref.c_str(), std::min(ref.size(), size_t(8)));
            }

            shdr.size_of_raw_data = static_cast<uint32_t>(sec.size());
            shdr.pointer_to_raw_data = sec.size() > 0 ? layouts[i].data_off : 0;
            shdr.pointer_to_relocations = sec.relocations.empty() ?
                0 : layouts[i].reloc_off;
            shdr.number_of_relocations = static_cast<uint16_t>(sec.relocations.size());
            shdr.characteristics = sec.characteristics;
            file.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));
        }

        // Escrever dados + relocações
        for (size_t i = 0; i < num_sec; i++) {
            auto& sec = sections_[i];
            if (!sec.data.empty())
                file.write(reinterpret_cast<const char*>(sec.data.data()), sec.data.size());
            for (auto& reloc : sec.relocations)
                file.write(reinterpret_cast<const char*>(&reloc), sizeof(CoffRelocation));
        }

        // Escrever symbol table
        for (auto& sym : final_symbols)
            file.write(reinterpret_cast<const char*>(&sym), sizeof(CoffSymbol));

        // Escrever string table
        file.write(reinterpret_cast<const char*>(&strtab_size), 4);
        if (!string_table.empty())
            file.write(reinterpret_cast<const char*>(string_table.data()), string_table.size());

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

#endif // JPLANG_COFF_EMITTER_HPP