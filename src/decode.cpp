#include "kr216/encoding.h"

namespace kr216 {
namespace {

// ---- operand builders -------------------------------------------------------------

Operand make_register(std::uint8_t reg) noexcept {
    Operand operand;
    operand.kind = OperandKind::Register;
    operand.reg = reg;
    return operand;
}

Operand make_fp_register(std::uint8_t reg) noexcept {
    Operand operand;
    operand.kind = OperandKind::FpRegister;
    operand.reg = reg;
    return operand;
}

Operand make_immediate(std::int32_t value, OperandFormat format) noexcept {
    Operand operand;
    operand.kind = OperandKind::Immediate;
    operand.format = format;
    operand.value = value;
    return operand;
}

Operand make_mask(std::uint16_t mask) noexcept {
    Operand operand;
    operand.kind = OperandKind::Mask;
    operand.format = OperandFormat::Mask;
    operand.value = mask;
    return operand;
}

Operand make_condition(std::uint8_t condition) noexcept {
    Operand operand;
    operand.kind = OperandKind::Condition;
    operand.value = condition;
    return operand;
}

Operand make_relative(std::int32_t displacement) noexcept {
    Operand operand;
    operand.kind = OperandKind::Relative;
    operand.value = displacement;
    return operand;
}

Operand make_address(AddressMode mode, std::uint8_t base,
                     std::int32_t displacement = 0) noexcept {
    Operand operand;
    operand.kind = OperandKind::Address;
    operand.mode = mode;
    operand.reg = base;
    operand.value = displacement;
    return operand;
}

// ---- small readers -----------------------------------------------------------------

std::uint16_t read_u16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::int32_t sign8(std::uint8_t value) noexcept { return static_cast<std::int8_t>(value); }

// Sign-extends a 9-bit branch displacement.
std::int32_t sign9(std::uint16_t value) noexcept {
    const std::uint16_t bits = value & 0x1FF;
    return (bits & 0x100) != 0 ? static_cast<std::int32_t>(bits) - 0x200
                               : static_cast<std::int32_t>(bits);
}

DecodeStatus illegal(Instruction& out) noexcept {
    out = Instruction{};
    return DecodeStatus::IllegalInstruction;
}

DecodeStatus need_more(Instruction& out) noexcept {
    out = Instruction{};
    return DecodeStatus::NeedMoreBytes;
}

bool mask_is_legal(std::uint16_t mask) noexcept {
    // KR216 chapter: bits 13 (FP), 14 (PC) and 15 (SP) are reserved in every mask
    // instruction (kr216-isa.md 8.7 and 10.6).
    return (mask & 0xE000) == 0;
}

// ---- short class -------------------------------------------------------------------

DecodeStatus decode_short(std::uint8_t first, Instruction& out) noexcept {
    out.length = 1;
    out.valid = true;
    const std::uint8_t row = static_cast<std::uint8_t>(first >> 4);
    const std::uint8_t reg = static_cast<std::uint8_t>(first & 0x0F);

    switch (row) {
    case 0x0:
    case 0x1:
        if (reg >= 14) return illegal(out);  // PC and SP may not appear in a mask
        out.op = row == 0 ? Op::Push : Op::Pop;
        out.a = make_register(reg);
        return DecodeStatus::Ok;
    case 0x2:
        out.op = Op::Inc;
        out.a = make_register(reg);
        return DecodeStatus::Ok;
    case 0x3:
        out.op = Op::Dec;
        out.a = make_register(reg);
        return DecodeStatus::Ok;
    case 0x4:
        out.op = Op::Call;
        out.a = make_register(reg);
        return DecodeStatus::Ok;
    case 0x7: {
        if (first > 0x78) return illegal(out);  // 0x79-0x7F are reserved
        const Op system[] = {Op::Ret, Op::Nop, Op::Brk, Op::Sys, Op::Ei,
                             Op::Di,  Op::Halt, Op::Iret, Op::Rdcycle};
        out.op = system[first - 0x70];
        return DecodeStatus::Ok;
    }
    default:
        return illegal(out);
    }
}

// ---- register class (three bytes, 16-bit payload) -----------------------------------

DecodeStatus decode_register(std::uint8_t first, const std::uint8_t* bytes, std::size_t size,
                             Instruction& out) noexcept {
    if (size < 3) return need_more(out);
    const std::uint8_t reg = static_cast<std::uint8_t>(first & 0x0F);
    const std::uint16_t payload = read_u16(bytes + 1);
    out.length = 3;
    out.valid = true;

    if (first <= 0x5F) {  // LD Rd, #imm16
        out.op = Op::Ld;
        out.a = make_register(reg);
        out.b = make_immediate(payload, OperandFormat::Number);
    } else if (first <= 0x6F) {  // LD Rd, [addr16]
        out.op = Op::Ld;
        out.a = make_register(reg);
        out.b = make_address(AddressMode::Absolute, 0, payload);
    } else {  // ST [addr16], Rd
        out.op = Op::St;
        out.a = make_address(AddressMode::Absolute, 0, payload);
        out.b = make_register(reg);
    }
    return DecodeStatus::Ok;
}

// ---- compact class -----------------------------------------------------------------

DecodeStatus decode_compact(std::uint8_t first, const std::uint8_t* bytes, std::size_t size,
                            Instruction& out) noexcept {
    if (size < 2) return need_more(out);
    // The branch form packs its fields from the most significant bit, so the 16-bit value is
    // big-endian: `101 CCCC ddddddddd` across the two bytes.
    const std::uint16_t branch = static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);

    if (first <= 0x9F) {  // register form: 100 OOOOO Ra Rb
        const std::uint8_t code = static_cast<std::uint8_t>(first & 0x1F);
        const std::uint8_t ra = static_cast<std::uint8_t>(bytes[1] >> 4);
        const std::uint8_t rb = static_cast<std::uint8_t>(bytes[1] & 0x0F);
        out.length = 2;
        out.valid = true;

        switch (code) {
        case 0: out.op = Op::Mov; out.a = make_register(ra); out.b = make_register(rb); break;
        case 1: out.op = Op::Add; out.a = make_register(ra); out.b = make_register(rb); break;
        case 2: out.op = Op::Addc; out.a = make_register(ra); out.b = make_register(rb); break;
        case 3: out.op = Op::Sub; out.a = make_register(ra); out.b = make_register(rb); break;
        case 4: out.op = Op::Subb; out.a = make_register(ra); out.b = make_register(rb); break;
        case 5: out.op = Op::And; out.a = make_register(ra); out.b = make_register(rb); break;
        case 6: out.op = Op::Or; out.a = make_register(ra); out.b = make_register(rb); break;
        case 7: out.op = Op::Xor; out.a = make_register(ra); out.b = make_register(rb); break;
        case 8: out.op = Op::Cmp; out.a = make_register(ra); out.b = make_register(rb); break;
        case 9: out.op = Op::Mul; out.a = make_register(ra); out.b = make_register(rb); break;
        case 10: out.op = Op::Mulh; out.a = make_register(ra); out.b = make_register(rb); break;
        case 11: out.op = Op::Shl; out.a = make_register(ra); out.b = make_register(rb); break;
        case 12: out.op = Op::Shr; out.a = make_register(ra); out.b = make_register(rb); break;
        case 13: out.op = Op::Sar; out.a = make_register(ra); out.b = make_register(rb); break;
        case 14:
            out.op = Op::Ld;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::Indirect, rb);
            break;
        case 15:
            out.op = Op::St;
            out.a = make_address(AddressMode::Indirect, rb);
            out.b = make_register(ra);
            break;
        case 16:
            out.op = Op::Ldbu;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::Indirect, rb);
            break;
        case 17:
            out.op = Op::Ldbs;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::Indirect, rb);
            break;
        case 18:
            out.op = Op::Stb;
            out.a = make_address(AddressMode::Indirect, rb);
            out.b = make_register(ra);
            break;
        case 19:
            out.op = Op::Ld;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::PostIncrement, rb);
            break;
        case 20:
            out.op = Op::St;
            out.a = make_address(AddressMode::PostIncrement, rb);
            out.b = make_register(ra);
            break;
        case 21:
            out.op = Op::Ldbu;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::PostIncrement, rb);
            break;
        case 22:
            out.op = Op::Ldbs;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::PostIncrement, rb);
            break;
        case 23:
            out.op = Op::Stb;
            out.a = make_address(AddressMode::PostIncrement, rb);
            out.b = make_register(ra);
            break;
        case 24:
            out.op = Op::Ld;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::PreDecrement, rb);
            break;
        case 25:
            out.op = Op::St;
            out.a = make_address(AddressMode::PreDecrement, rb);
            out.b = make_register(ra);
            break;
        case 26:
            out.op = Op::Ldbu;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::PreDecrement, rb);
            break;
        case 27:
            out.op = Op::Ldbs;
            out.a = make_register(ra);
            out.b = make_address(AddressMode::PreDecrement, rb);
            break;
        case 28:
            out.op = Op::Stb;
            out.a = make_address(AddressMode::PreDecrement, rb);
            out.b = make_register(ra);
            break;
        case 29:
        case 30:
            if (rb != 0) return illegal(out);
            out.op = code == 29 ? Op::Neg : Op::Not;
            out.a = make_register(ra);
            break;
        default:  // 31
            if (rb != 0) return illegal(out);
            out.op = Op::Jmp;
            out.a = make_register(ra);
            break;
        }
        return DecodeStatus::Ok;
    }

    // branch form: 101 CCCC ddddddddd
    const std::uint8_t condition = static_cast<std::uint8_t>((branch >> 9) & 0x0F);
    const std::int32_t displacement = sign9(branch);
    out.length = 2;
    out.valid = true;
    if (condition == 14) {
        out.op = Op::Call;
        out.a = make_relative(displacement);
    } else if (condition == 15) {
        out.op = Op::Bra;
        out.a = make_relative(displacement);
    } else {
        out.op = Op::Bcc;
        out.a = make_condition(condition);
        out.b = make_relative(displacement);
    }
    return DecodeStatus::Ok;
}

// ---- normal class ------------------------------------------------------------------

DecodeStatus decode_normal(std::uint8_t first, const std::uint8_t* bytes, std::size_t size,
                           Instruction& out) noexcept {
    if (size < 3) return need_more(out);
    const std::uint8_t code = static_cast<std::uint8_t>(first & 0x1F);
    const std::uint8_t p0 = bytes[1];
    const std::uint8_t p1 = bytes[2];
    const std::uint8_t r = static_cast<std::uint8_t>(p0 >> 4);
    const std::uint8_t base = static_cast<std::uint8_t>(p0 & 0x0F);
    const std::uint16_t payload = read_u16(bytes + 1);
    out.length = 3;
    out.valid = true;

    switch (code) {
    case 0:
        out.op = Op::Ld;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement8, base, sign8(p1));
        break;
    case 1:
        out.op = Op::St;
        out.a = make_address(AddressMode::Displacement8, base, sign8(p1));
        out.b = make_register(r);
        break;
    case 2:
        out.op = Op::Ldbu;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement8, base, sign8(p1));
        break;
    case 3:
        out.op = Op::Ldbs;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement8, base, sign8(p1));
        break;
    case 4:
        out.op = Op::Stb;
        out.a = make_address(AddressMode::Displacement8, base, sign8(p1));
        out.b = make_register(r);
        break;
    case 5:  // LD Rd, #s8
        if (base != 0) return illegal(out);
        out.op = Op::Ld;
        out.a = make_register(r);
        out.b = make_immediate(sign8(p1), OperandFormat::Number);
        break;
    case 6:  // LD Rd, #u8
        if (base != 0) return illegal(out);
        out.op = Op::Ld;
        out.a = make_register(r);
        out.b = make_immediate(p1, OperandFormat::Number);
        break;
    case 7:
    case 8:
    case 9:
        if (base != 0) return illegal(out);
        out.op = code == 7 ? Op::Add : (code == 8 ? Op::Sub : Op::Cmp);
        out.a = make_register(r);
        out.b = make_immediate(sign8(p1), OperandFormat::Number);
        break;
    case 10:
        if (base != 0) return illegal(out);
        out.op = Op::Cmp;
        out.a = make_register(r);
        out.b = make_immediate(p1, OperandFormat::Number);
        break;
    case 11:
    case 12:
    case 13:
        if (base != 0) return illegal(out);
        out.op = code == 11 ? Op::And : (code == 12 ? Op::Or : Op::Xor);
        out.a = make_register(r);
        out.b = make_immediate(p1, OperandFormat::Number);
        break;
    case 14:
        out.op = Op::Lea;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement8, base, sign8(p1));
        break;
    case 15:
        out.op = Op::Jmp;
        out.a = make_immediate(payload, OperandFormat::Address);
        break;
    case 16:
        out.op = Op::Call;
        out.a = make_immediate(payload, OperandFormat::Address);
        break;
    case 17:
    case 18:
        if (base != 0) return illegal(out);
        out.op = code == 17 ? Op::Ldbu : Op::Ldbs;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement8, 14, sign8(p1));
        break;
    case 19: {
        const std::uint8_t condition = static_cast<std::uint8_t>(p0 & 0x0F);
        if (p1 != 0 || condition >= 14) return illegal(out);
        out.op = Op::Setcc;
        out.a = make_register(r);
        out.b = make_condition(condition);
        break;
    }
    case 20:
    case 21:
    case 22:
        if (base != 0) return illegal(out);
        out.op = code == 20 ? Op::Shl : (code == 21 ? Op::Shr : Op::Sar);
        out.a = make_register(r);
        out.b = make_immediate(p1, OperandFormat::Decimal);
        break;
    case 23: {
        const std::uint16_t mask = read_u16(bytes + 1);
        if (!mask_is_legal(mask)) return illegal(out);
        out.op = Op::Push;
        out.a = make_mask(mask);
        break;
    }
    case 24: {
        const std::uint16_t mask = read_u16(bytes + 1);
        if (!mask_is_legal(mask)) return illegal(out);
        out.op = Op::Pop;
        out.a = make_mask(mask);
        break;
    }
    case 25:
    case 26:
        out.op = code == 25 ? Op::Div : Op::Divs;
        out.a = make_register(r);
        out.b = make_register(base);
        break;
    case 27:
        if (base != 0 || p1 != 0) return illegal(out);
        out.op = Op::Sxtb;
        out.a = make_register(r);
        break;
    case 28: {
        const std::uint16_t mask = read_u16(bytes + 1);
        if (!mask_is_legal(mask)) return illegal(out);
        out.op = Op::Leave;
        out.a = make_mask(mask);
        break;
    }
    default:  // 29-31 reserved
        return illegal(out);
    }
    return DecodeStatus::Ok;
}

// ---- escape F1: long integer -------------------------------------------------------

DecodeStatus decode_f1(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    if (size < 4) return need_more(out);
    const std::uint8_t opcode = bytes[1];
    const std::uint8_t code = static_cast<std::uint8_t>(opcode >> 4);
    const std::uint8_t r = static_cast<std::uint8_t>(opcode & 0x0F);
    const std::uint16_t field = read_u16(bytes + 2);
    out.length = 4;
    out.valid = true;

    switch (code) {
    case 0:
        out.op = Op::Ldbu;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Absolute, 0, field);
        break;
    case 1:
        out.op = Op::Ldbs;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Absolute, 0, field);
        break;
    case 2:
        out.op = Op::Stb;
        out.a = make_address(AddressMode::Absolute, 0, field);
        out.b = make_register(r);
        break;
    case 3:
        out.op = Op::Lea;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement16, 14, field);
        break;
    case 4:
        out.op = Op::Lea;
        out.a = make_register(r);
        out.b = make_address(AddressMode::Displacement16, 15, field);
        break;
    default:
        return illegal(out);
    }
    return DecodeStatus::Ok;
}

// ---- escape F3: FP register-register -----------------------------------------------

DecodeStatus decode_f3(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    if (size < 2) return need_more(out);
    const std::uint8_t payload = bytes[1];
    const std::uint8_t code = static_cast<std::uint8_t>(payload >> 4);
    const std::uint8_t fd = static_cast<std::uint8_t>((payload >> 2) & 0x03);
    const std::uint8_t fs = static_cast<std::uint8_t>(payload & 0x03);
    out.length = 2;
    out.valid = true;

    switch (code) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 7:
    case 8: {
        const Op ops[] = {Op::Fadd, Op::Fsub, Op::Fmul, Op::Fdiv, Op::Fmov, Op::Fcmp};
        const int index = code < 4 ? code : (code == 7 ? 4 : 5);
        out.op = ops[index];
        out.a = make_fp_register(fd);
        out.b = make_fp_register(fs);
        break;
    }
    case 4:
    case 5:
    case 6:
    case 9: {
        if (fs != 0) return illegal(out);
        const Op ops[] = {Op::Fneg, Op::Fabs, Op::Fsqrt, Op::Frnd};
        const int index = code < 7 ? code - 4 : 3;
        out.op = ops[index];
        out.a = make_fp_register(fd);
        break;
    }
    default:
        return illegal(out);
    }
    return DecodeStatus::Ok;
}

// ---- escape F4: FP load/store ------------------------------------------------------

DecodeStatus decode_f4(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    if (size < 3) return need_more(out);
    const std::uint8_t payload = bytes[1];
    const std::uint8_t code = static_cast<std::uint8_t>(payload >> 4);
    const std::uint8_t fd = static_cast<std::uint8_t>((payload >> 2) & 0x03);
    const std::uint8_t mm = static_cast<std::uint8_t>(payload & 0x03);
    const std::uint8_t base = static_cast<std::uint8_t>(bytes[2] >> 4);
    if ((bytes[2] & 0x0F) != 0) return illegal(out);
    if (code > 3) return illegal(out);

    const AddressMode mode = mm == 0   ? AddressMode::Indirect
                             : mm == 1 ? AddressMode::Displacement8
                             : mm == 2 ? AddressMode::PostIncrement
                                       : AddressMode::PreDecrement;
    const std::size_t length = mm == 1 ? 4u : 3u;
    if (size < length) return need_more(out);
    const std::int32_t displacement = mm == 1 ? sign8(bytes[3]) : 0;

    out.length = static_cast<std::uint8_t>(length);
    out.valid = true;
    const Op ops[] = {Op::Fld, Op::Fst, Op::Flds, Op::Fsts};
    out.op = ops[code];
    // FP loads and stores both name the FP register first: `fst f1, [r2]` (kr216-isa.md
    // Appendix A), unlike the integer `st [r5+3], r4`.
    out.a = make_fp_register(fd);
    out.b = make_address(mode, base, displacement);
    return DecodeStatus::Ok;
}

// ---- escape F5: FP conversion ------------------------------------------------------

DecodeStatus decode_f5(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    if (size < 3) return need_more(out);
    const std::uint8_t payload = bytes[1];
    const std::uint8_t code = static_cast<std::uint8_t>(payload >> 4);
    const std::uint8_t fd = static_cast<std::uint8_t>((payload >> 2) & 0x03);
    const std::uint8_t r = static_cast<std::uint8_t>(bytes[2] >> 4);
    if ((payload & 0x03) != 0 || (bytes[2] & 0x0F) != 0) return illegal(out);
    if (code > 3) return illegal(out);

    out.length = 3;
    out.valid = true;
    const Op ops[] = {Op::I2f, Op::U2f, Op::F2i, Op::F2u};
    out.op = ops[code];
    if (code <= 1) {  // integer to FP
        out.a = make_fp_register(fd);
        out.b = make_register(r);
    } else {  // FP to integer
        out.a = make_register(r);
        out.b = make_fp_register(fd);
    }
    return DecodeStatus::Ok;
}

// ---- escape F6: FP push/pop --------------------------------------------------------

DecodeStatus decode_f6(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    if (size < 2) return need_more(out);
    const std::uint8_t payload = bytes[1];
    const std::uint8_t code = static_cast<std::uint8_t>(payload >> 2);
    const std::uint8_t fd = static_cast<std::uint8_t>(payload & 0x03);
    if (code > 1) return illegal(out);

    out.length = 2;
    out.valid = true;
    out.op = code == 0 ? Op::Fpush : Op::Fpop;
    out.a = make_fp_register(fd);
    return DecodeStatus::Ok;
}

// ---- escape F7/F8: ENTER -----------------------------------------------------------

DecodeStatus decode_enter(std::uint8_t first, const std::uint8_t* bytes, std::size_t size,
                          Instruction& out) noexcept {
    const std::size_t length = first == 0xF7 ? 4u : 5u;
    if (size < length) return need_more(out);
    const std::uint16_t frame = first == 0xF7 ? bytes[1] : read_u16(bytes + 1);
    const std::uint16_t mask = first == 0xF7 ? read_u16(bytes + 2) : read_u16(bytes + 3);
    if ((frame & 1u) != 0 || !mask_is_legal(mask)) return illegal(out);

    out.length = static_cast<std::uint8_t>(length);
    out.valid = true;
    out.op = Op::Enter;
    out.a = make_immediate(frame, OperandFormat::Decimal);
    out.b = make_mask(mask);
    return DecodeStatus::Ok;
}

// ---- escape F0: generic two-operand form -------------------------------------------

struct Spec {
    std::uint8_t mode = 0;
    std::uint8_t reg = 0;
};

bool parse_spec(std::uint8_t byte, Spec& spec) noexcept {
    if ((byte & 0x80) != 0) return false;  // bit 7 reserved
    spec.mode = static_cast<std::uint8_t>((byte >> 4) & 0x07);
    spec.reg = static_cast<std::uint8_t>(byte & 0x0F);
    if (spec.mode == 1 && spec.reg != 0) return false;  // immediate has no register
    return spec.mode != 7;
}

std::uint8_t extension_size(std::uint8_t mode) noexcept {
    if (mode == 1 || mode == 6) return 2;
    if (mode == 5) return 1;
    return 0;
}

enum class Role : std::uint8_t { Binary, Load, Store, Unary, Jump };

struct F0Info {
    Op op = Op::Nop;
    Role role = Role::Binary;
    std::uint8_t op1_modes = 0;
    std::uint8_t op2_modes = 0;
};

constexpr std::uint8_t kMode0 = 1u << 0;                       // register
constexpr std::uint8_t kMode01 = kMode0 | (1u << 1);           // register or immediate
constexpr std::uint8_t kAddressModes = (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6);
constexpr std::uint8_t kAnyMode = 0x7F;                        // modes 0-6

bool f0_info(std::uint8_t code, F0Info& info) noexcept {
    switch (code) {
    case 0x00: info = {Op::Mov, Role::Binary, kMode0, kMode01}; return true;
    case 0x01: info = {Op::Add, Role::Binary, kMode0, kMode01}; return true;
    case 0x02: info = {Op::Addc, Role::Binary, kMode0, kMode01}; return true;
    case 0x03: info = {Op::Sub, Role::Binary, kMode0, kMode01}; return true;
    case 0x04: info = {Op::Subb, Role::Binary, kMode0, kMode01}; return true;
    case 0x05: info = {Op::And, Role::Binary, kMode0, kMode01}; return true;
    case 0x06: info = {Op::Or, Role::Binary, kMode0, kMode01}; return true;
    case 0x07: info = {Op::Xor, Role::Binary, kMode0, kMode01}; return true;
    case 0x08: info = {Op::Cmp, Role::Binary, kMode0, kMode01}; return true;
    case 0x09: info = {Op::Mul, Role::Binary, kMode0, kMode0}; return true;
    case 0x0A: info = {Op::Mulh, Role::Binary, kMode0, kMode0}; return true;
    case 0x0B: info = {Op::Shl, Role::Binary, kMode0, kMode01}; return true;
    case 0x0C: info = {Op::Shr, Role::Binary, kMode0, kMode01}; return true;
    case 0x0D: info = {Op::Sar, Role::Binary, kMode0, kMode01}; return true;
    case 0x0E: info = {Op::Ld, Role::Load, kMode0, kAddressModes}; return true;
    case 0x0F: info = {Op::St, Role::Store, kMode0, kAddressModes}; return true;
    case 0x10: info = {Op::Ldbu, Role::Load, kMode0, kAddressModes}; return true;
    case 0x11: info = {Op::Ldbs, Role::Load, kMode0, kAddressModes}; return true;
    case 0x12: info = {Op::Stb, Role::Store, kMode0, kAddressModes}; return true;
    case 0x13: info = {Op::Lea, Role::Load, kMode0, kAddressModes}; return true;
    case 0x14: info = {Op::Neg, Role::Unary, kMode0, kMode0}; return true;
    case 0x15: info = {Op::Not, Role::Unary, kMode0, kMode0}; return true;
    case 0x16: info = {Op::Sxtb, Role::Unary, kMode0, kMode0}; return true;
    case 0x17: info = {Op::Div, Role::Binary, kMode0, kMode0}; return true;
    case 0x18: info = {Op::Divs, Role::Binary, kMode0, kMode0}; return true;
    case 0x19: info = {Op::Jmp, Role::Jump, kAnyMode, kMode0}; return true;
    case 0x1A: info = {Op::Call, Role::Jump, kAnyMode, kMode0}; return true;
    default: return false;
    }
}

Operand spec_operand(const std::uint8_t* bytes, const Spec& spec, std::size_t& offset,
                     bool jump_target) noexcept {
    switch (spec.mode) {
    case 0:
        return make_register(spec.reg);
    case 1: {
        const std::uint16_t value = read_u16(bytes + offset);
        offset += 2;
        return jump_target ? make_immediate(value, OperandFormat::Address)
                           : make_immediate(value, OperandFormat::Number);
    }
    case 2:
        return make_address(AddressMode::Indirect, spec.reg, 0);
    case 3:
        return make_address(AddressMode::PostIncrement, spec.reg, 0);
    case 4:
        return make_address(AddressMode::PreDecrement, spec.reg, 0);
    case 5: {
        const std::int32_t displacement = sign8(bytes[offset]);
        offset += 1;
        return make_address(AddressMode::Displacement8, spec.reg, displacement);
    }
    default: {  // 6
        const std::int32_t displacement = read_u16(bytes + offset);
        offset += 2;
        return make_address(AddressMode::Displacement16, spec.reg, displacement);
    }
    }
}

DecodeStatus decode_f0(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    if (size < 4) return need_more(out);
    F0Info info{};
    if (!f0_info(bytes[1], info)) return illegal(out);

    Spec first{};
    Spec second{};
    if (!parse_spec(bytes[2], first) || !parse_spec(bytes[3], second)) return illegal(out);
    if ((info.op1_modes & (1u << first.mode)) == 0) return illegal(out);
    if ((info.op2_modes & (1u << second.mode)) == 0) return illegal(out);
    if (info.role == Role::Unary || info.role == Role::Jump) {
        if (bytes[3] != 0x00) return illegal(out);  // unused operand must be encoded as zero
    }

    const std::uint8_t ext1 = extension_size(first.mode);
    const std::uint8_t ext2 = extension_size(second.mode);
    if (ext1 != 0 && ext2 != 0) return illegal(out);  // at most one extension keeps F0 <= 6 bytes
    const std::size_t length = 4u + ext1 + ext2;
    if (size < length) return need_more(out);

    std::size_t offset = 4;
    const bool jump = info.role == Role::Jump;
    const Operand one = spec_operand(bytes, first, offset, jump);
    const Operand two = spec_operand(bytes, second, offset, false);

    out.length = static_cast<std::uint8_t>(length);
    out.valid = true;
    out.op = info.op;
    switch (info.role) {
    case Role::Binary:
    case Role::Load:
        out.a = one;
        out.b = two;
        break;
    case Role::Store:
        out.a = two;
        out.b = one;
        break;
    case Role::Unary:
    case Role::Jump:
        out.a = one;
        break;
    }
    return DecodeStatus::Ok;
}

}  // namespace

DecodeStatus decode(const std::uint8_t* bytes, std::size_t size, Instruction& out) noexcept {
    out = Instruction{};
    if (bytes == nullptr || size == 0) return need_more(out);
    const std::uint8_t first = bytes[0];
    switch (classify(first)) {
    case LengthClass::Short: return decode_short(first, out);
    case LengthClass::Register: return decode_register(first, bytes, size, out);
    case LengthClass::Compact: return decode_compact(first, bytes, size, out);
    case LengthClass::Normal: return decode_normal(first, bytes, size, out);
    case LengthClass::Generic: return decode_f0(bytes, size, out);
    case LengthClass::LongInteger: return decode_f1(bytes, size, out);
    case LengthClass::FpRegister: return decode_f3(bytes, size, out);
    case LengthClass::FpLoadStore: return decode_f4(bytes, size, out);
    case LengthClass::FpConvert: return decode_f5(bytes, size, out);
    case LengthClass::FpStack: return decode_f6(bytes, size, out);
    case LengthClass::EnterShort:
    case LengthClass::EnterLong: return decode_enter(first, bytes, size, out);
    case LengthClass::Reserved: break;
    }
    return illegal(out);
}

}  // namespace kr216
