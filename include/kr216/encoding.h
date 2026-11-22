#pragma once

#include <cstdint>

namespace kr216 {

// The length class that a first byte selects (kr216-isa.md section 3.1). Two escape
// prefixes report a minimum length only: `F0` carries 4-6 bytes and `F4` 3-4, decided by
// the specifier and mode bytes that follow, so a caller that needs the exact length must
// decode them.
enum class LengthClass : std::uint8_t {
    Short,        // 0x00-0x4F and 0x70-0x7F   1 byte
    Register,     // 0x50-0x6F and 0xE0-0xEF   3 bytes
    Compact,      // 0x80-0xBF                 2 bytes
    Normal,       // 0xC0-0xDF                 3 bytes
    Generic,      // 0xF0                      4-6 bytes
    LongInteger,  // 0xF1                      4 bytes
    FpRegister,   // 0xF3                      2 bytes
    FpLoadStore,  // 0xF4                      3-4 bytes
    FpConvert,    // 0xF5                      3 bytes
    FpStack,      // 0xF6                      2 bytes
    EnterShort,   // 0xF7                      4 bytes
    EnterLong,    // 0xF8                      5 bytes
    Reserved,     // 0xF2 and 0xF9-0xFF        illegal instruction, trap vector 2
};

// Classifies an instruction by its first byte alone, before any further fetch.
[[nodiscard]] LengthClass classify(std::uint8_t first) noexcept;

// The fixed length of a class, or the minimum length for `Generic` and `FpLoadStore`.
// `Reserved` reports 0: there is no instruction to fetch.
[[nodiscard]] std::uint8_t minimum_length(LengthClass cls) noexcept;

// True when the class is an unallocated encoding, which raises an illegal-instruction trap
// (kr216-isa.md section 3.2).
[[nodiscard]] bool is_reserved(LengthClass cls) noexcept;

}  // namespace kr216
