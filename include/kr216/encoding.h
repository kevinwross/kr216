#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace kr216 {

// The length class that a first byte selects (kr216-isa.md section 3.1). Two escape
// prefixes report a minimum length only: `F0` carries 4-6 bytes and `F4` 3-4, decided by
// the specifier and mode bytes that follow, so `decode` is what reports the real length.
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

// Every architectural operation the decoder can produce. `Bcc` carries its condition in
// operand a; `Setcc` carries it in operand b and takes it into the mnemonic (`setlt`).
enum class Op : std::uint8_t {
    Push, Pop, Inc, Dec, Call, Ret, Nop, Brk, Sys, Ei, Di, Halt, Iret, Rdcycle,
    Mov, Add, Addc, Sub, Subb, And, Or, Xor, Cmp, Mul, Mulh, Shl, Shr, Sar,
    Ld, St, Ldbu, Ldbs, Stb, Lea, Sxtb, Neg, Not, Div, Divs,
    Bcc, Bra, Jmp, Setcc,
    Enter, Leave,
    Fadd, Fsub, Fmul, Fdiv, Fneg, Fabs, Fsqrt, Fmov, Fcmp, Frnd,
    Fld, Fst, Flds, Fsts,
    I2f, U2f, F2i, F2u, Fpush, Fpop,
};

// An address operand's shape (kr216-isa.md section 9). `Absolute` has no base register: the
// displacement is the address. `Displacement8`/`Displacement16` carry the base in `reg`.
enum class AddressMode : std::uint8_t {
    Absolute,
    Indirect,
    PostIncrement,
    PreDecrement,
    Displacement8,
    Displacement16,
};

enum class OperandKind : std::uint8_t {
    None,
    Register,
    FpRegister,
    Immediate,
    Address,
    Mask,
    Condition,
    Relative,
};

// How an operand's value is written. Negative values always print in decimal; `Number`
// prints a non-negative value as minimal hex, `Mask` as four digits, `Address` as four.
enum class OperandFormat : std::uint8_t { Decimal, Number, Mask, Address };

struct Operand {
    OperandKind kind = OperandKind::None;
    OperandFormat format = OperandFormat::Number;
    std::uint8_t reg = 0;                    // register or FP-register number
    AddressMode mode = AddressMode::Indirect;  // meaning for OperandKind::Address
    std::int32_t value = 0;                  // immediate, displacement, mask, condition, relative
};

// A decoded instruction in assembly order: `a` is the first written operand, `b` the second.
// For stores that makes `a` the address and `b` the data, and for `LD Rd,[..]` the reverse.
struct Instruction {
    Op op = Op::Nop;
    std::uint8_t length = 0;
    Operand a{};
    Operand b{};
    bool valid = false;
};

enum class DecodeStatus : std::uint8_t {
    Ok,
    NeedMoreBytes,       // the buffer ends inside the instruction; fetch more and retry
    IllegalInstruction,  // reserved encoding, reserved field, or an illegal operand
};

// Decodes one instruction from `bytes`. On `Ok` the length is in `out.length`; on
// `IllegalInstruction` the encoding raises trap vector 2; on `NeedMoreBytes` nothing is
// consumed and the caller must extend the buffer.
DecodeStatus decode(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept;

// Canonical assembly text for a decoded instruction, matching kr216-isa.md's Appendix A.
std::string disassemble(const Instruction& instruction);

}  // namespace kr216
