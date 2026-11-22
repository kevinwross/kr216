#include "kr216/encoding.h"

#include <cstdio>

namespace kr216 {
namespace {

const char* condition_name(std::int32_t condition) noexcept {
    static const char* const kNames[] = {"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
                                         "hi", "ls", "ge", "lt", "gt", "le"};
    if (condition < 0 || condition > 13) return "??";
    return kNames[condition];
}

const char* mnemonic(Op op) noexcept {
    switch (op) {
    case Op::Push: return "push";
    case Op::Pop: return "pop";
    case Op::Inc: return "inc";
    case Op::Dec: return "dec";
    case Op::Call: return "call";
    case Op::Ret: return "ret";
    case Op::Nop: return "nop";
    case Op::Brk: return "brk";
    case Op::Sys: return "sys";
    case Op::Ei: return "ei";
    case Op::Di: return "di";
    case Op::Halt: return "halt";
    case Op::Iret: return "iret";
    case Op::Rdcycle: return "rdcycle";
    case Op::Mov: return "mov";
    case Op::Add: return "add";
    case Op::Addc: return "addc";
    case Op::Sub: return "sub";
    case Op::Subb: return "subb";
    case Op::And: return "and";
    case Op::Or: return "or";
    case Op::Xor: return "xor";
    case Op::Cmp: return "cmp";
    case Op::Mul: return "mul";
    case Op::Mulh: return "mulh";
    case Op::Shl: return "shl";
    case Op::Shr: return "shr";
    case Op::Sar: return "sar";
    case Op::Ld: return "ld";
    case Op::St: return "st";
    case Op::Ldbu: return "ldbu";
    case Op::Ldbs: return "ldbs";
    case Op::Stb: return "stb";
    case Op::Lea: return "lea";
    case Op::Sxtb: return "sxtb";
    case Op::Neg: return "neg";
    case Op::Not: return "not";
    case Op::Div: return "div";
    case Op::Divs: return "divs";
    case Op::Bcc: return "b";
    case Op::Bra: return "bra";
    case Op::Jmp: return "jmp";
    case Op::Setcc: return "set";  // the condition is appended by disassemble()
    case Op::Enter: return "enter";
    case Op::Leave: return "leave";
    case Op::Fadd: return "fadd";
    case Op::Fsub: return "fsub";
    case Op::Fmul: return "fmul";
    case Op::Fdiv: return "fdiv";
    case Op::Fneg: return "fneg";
    case Op::Fabs: return "fabs";
    case Op::Fsqrt: return "fsqrt";
    case Op::Fmov: return "fmov";
    case Op::Fcmp: return "fcmp";
    case Op::Frnd: return "frnd";
    case Op::Fld: return "fld";
    case Op::Fst: return "fst";
    case Op::Flds: return "flds";
    case Op::Fsts: return "fsts";
    case Op::I2f: return "i2f";
    case Op::U2f: return "u2f";
    case Op::F2i: return "f2i";
    case Op::F2u: return "f2u";
    case Op::Fpush: return "fpush";
    case Op::Fpop: return "fpop";
    }
    return "??";
}

std::string register_name(std::uint8_t reg) noexcept {
    switch (reg) {
    case 11: return "flags";
    case 12: return "efp";
    case 13: return "fp";
    case 14: return "pc";
    case 15: return "sp";
    default: return "r" + std::to_string(reg);
    }
}

std::string hex(std::int32_t value, int digits) {
    char buffer[16] = {};
    if (digits == 0) {
        std::snprintf(buffer, sizeof buffer, "%X", static_cast<unsigned>(value));
    } else {
        std::snprintf(buffer, sizeof buffer, "%0*X", digits, static_cast<unsigned>(value));
    }
    return std::string("0x") + buffer;
}

std::string format_operand(const Operand& operand) {
    switch (operand.kind) {
    case OperandKind::None:
        return {};
    case OperandKind::Register:
        return register_name(operand.reg);
    case OperandKind::FpRegister:
        return "f" + std::to_string(operand.reg);
    case OperandKind::Immediate:
        if (operand.format == OperandFormat::Address) return hex(operand.value, 4);
        if (operand.format == OperandFormat::Decimal || operand.value < 0) {
            return "#" + std::to_string(operand.value);
        }
        return "#" + hex(operand.value, 0);
    case OperandKind::Mask:
        return operand.value == 0 ? "#0" : "#" + hex(operand.value, 4);
    case OperandKind::Address: {
        const std::string base = register_name(operand.reg);
        switch (operand.mode) {
        case AddressMode::Absolute:
            return "[" + hex(operand.value, 4) + "]";
        case AddressMode::Indirect:
            return "[" + base + "]";
        case AddressMode::PostIncrement:
            return "[" + base + "]+";
        case AddressMode::PreDecrement:
            return "-[" + base + "]";
        case AddressMode::Displacement8:
        case AddressMode::Displacement16:
            break;
        }
        return "[" + base + (operand.value >= 0 ? "+" : "") + std::to_string(operand.value) + "]";
    }
    case OperandKind::Condition:
        return condition_name(operand.value);
    case OperandKind::Relative:
        return (operand.value >= 0 ? "+" : "") + std::to_string(operand.value);
    }
    return {};
}

}  // namespace

std::string disassemble(const Instruction& instruction) {
    if (!instruction.valid) return "(illegal)";

    std::string text;
    if (instruction.op == Op::Bcc) {
        text = std::string("b") + condition_name(instruction.a.value);
    } else if (instruction.op == Op::Setcc) {
        text = std::string("set") + condition_name(instruction.b.value);
    } else {
        text = mnemonic(instruction.op);
    }

    std::string operands;
    const auto append = [&operands](const std::string& operand) {
        if (operand.empty()) return;
        if (!operands.empty()) operands += ", ";
        operands += operand;
    };
    if (instruction.op == Op::Bcc) {
        append(format_operand(instruction.b));
    } else if (instruction.op == Op::Setcc) {
        append(format_operand(instruction.a));
    } else {
        append(format_operand(instruction.a));
        append(format_operand(instruction.b));
    }
    if (!operands.empty()) text += " " + operands;
    return text;
}

}  // namespace kr216
