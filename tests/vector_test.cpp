// The Appendix A encoding vectors, the reserved encodings and the decoder's edge cases.
//
// Each vector is the document's own `asm -> bytes` row: the decoder must consume exactly the
// stated number of bytes and the disassembler must reproduce the assembly text. A wrong
// length or a wrong operand order fails here rather than in the emulator's state tests.

#include <cstdint>
#include <cstdio>
#include <string>

#include "kr216/encoding.h"

namespace {

int failures = 0;

struct Vector {
    std::uint8_t bytes[6];
    std::uint8_t length;
    const char* text;
};

// kr216-isa.md Appendix A, in document order.
constexpr Vector kAppendixA[] = {
    {{0x0D}, 1, "push fp"},
    {{0x1D}, 1, "pop fp"},
    {{0x70}, 1, "ret"},
    {{0x71}, 1, "nop"},
    {{0x72}, 1, "brk"},
    {{0x73}, 1, "sys"},
    {{0x74}, 1, "ei"},
    {{0x75}, 1, "di"},
    {{0x76}, 1, "halt"},
    {{0x77}, 1, "iret"},
    {{0x78}, 1, "rdcycle"},
    {{0x22}, 1, "inc r2"},
    {{0x32}, 1, "dec r2"},
    {{0x43}, 1, "call r3"},
    {{0x80, 0x30}, 2, "mov r3, r0"},
    {{0x81, 0x34}, 2, "add r3, r4"},
    {{0x88, 0x40}, 2, "cmp r4, r0"},
    {{0x90, 0x42}, 2, "ldbu r4, [r2]"},
    {{0x91, 0x42}, 2, "ldbs r4, [r2]"},
    {{0x9F, 0x30}, 2, "jmp r3"},
    {{0x80, 0xDF}, 2, "mov fp, sp"},
    {{0x9D, 0x20}, 2, "neg r2"},
    {{0x9E, 0x20}, 2, "not r2"},
    {{0xA0, 0x06}, 2, "beq +6"},
    {{0xBF, 0xF7}, 2, "bra -9"},
    {{0xC0, 0x45, 0x03}, 3, "ld r4, [r5+3]"},
    {{0xC1, 0x45, 0x03}, 3, "st [r5+3], r4"},
    {{0x54, 0x34, 0x00}, 3, "ld r4, #0x34"},
    {{0xD3, 0x2B, 0x00}, 3, "setlt r2"},
    {{0xD4, 0x20, 0x03}, 3, "shl r2, #3"},
    {{0xD9, 0x23}, 3, "div r2, r3"},
    {{0xDB, 0x40, 0x00}, 3, "sxtb r4"},
    {{0xD0, 0x00, 0x02}, 3, "call 0x0200"},
    {{0xCF, 0x00, 0x02}, 3, "jmp 0x0200"},
    {{0xD7, 0x00, 0x03}, 3, "push #0x0300"},
    {{0xD8, 0x00, 0x03}, 3, "pop #0x0300"},
    {{0xDC, 0x00, 0x03}, 3, "leave #0x0300"},
    {{0x52, 0x34, 0x12}, 3, "ld r2, #0x1234"},
    {{0x66, 0x00, 0x40}, 3, "ld r6, [0x4000]"},
    {{0xE6, 0x00, 0x40}, 3, "st [0x4000], r6"},
    {{0xF1, 0x02, 0x00, 0x40}, 4, "ldbu r2, [0x4000]"},
    {{0xF1, 0x42, 0x06, 0x00}, 4, "lea r2, [sp+6]"},
    {{0xF3, 0x01}, 2, "fadd f0, f1"},
    {{0xF3, 0x0B}, 2, "fadd f2, f3"},
    {{0xF3, 0x81}, 2, "fcmp f0, f1"},
    {{0xF3, 0x94}, 2, "frnd f1"},
    {{0xF4, 0x01, 0x20, 0x08}, 4, "fld f0, [r2+8]"},
    {{0xF4, 0x14, 0x20}, 3, "fst f1, [r2]"},
    {{0xF4, 0x35, 0x20, 0x04}, 4, "fsts f1, [r2+4]"},
    {{0xF5, 0x00, 0x20}, 3, "i2f f0, r2"},
    {{0xF5, 0x20, 0x20}, 3, "f2i r2, f0"},
    {{0xF6, 0x01}, 2, "fpush f1"},
    {{0xF6, 0x05}, 2, "fpop f1"},
    {{0xF7, 0x08, 0x00, 0x03}, 4, "enter #8, #0x0300"},
    {{0xF8, 0x2C, 0x01, 0x00, 0x00}, 5, "enter #300, #0"},
    {{0xF0, 0x00, 0x02, 0x03}, 4, "mov r2, r3"},
    {{0xF0, 0x0E, 0x02, 0x53, 0x04}, 5, "ld r2, [r3+4]"},
    {{0xF0, 0x00, 0x02, 0x10, 0x00, 0x40}, 6, "mov r2, #0x4000"},
    {{0xF0, 0x0E, 0x02, 0x63, 0x06, 0x00}, 6, "ld r2, [r3+6]"},
    {{0xF0, 0x0F, 0x02, 0x53, 0x02}, 5, "st [r3+2], r2"},
};

struct Illegal {
    std::uint8_t bytes[6];
    std::uint8_t length;
    const char* why;
};

// Encodings the decoder must reject: reserved fields a hand-written stream can contain, and
// reserved fields the assembler would never emit.
constexpr Illegal kIllegal[] = {
    {{0x9D, 0x21}, 2, "NEG with a non-zero Rb"},
    {{0x9F, 0x21}, 2, "JMP with a non-zero Rb"},
    {{0xD3, 0x2E, 0x00}, 3, "SETcc with condition 14"},
    {{0xD3, 0x20, 0x01}, 3, "SETcc with a non-zero reserved byte"},
    {{0xF3, 0x56}, 2, "FABS with a non-zero Fs"},
    {{0xF5, 0x01, 0x20}, 3, "I2F with reserved low bits set"},
    {{0xF7, 0x09, 0x00, 0x00}, 4, "ENTER with an odd frame"},
    {{0xD7, 0x00, 0x20}, 3, "PUSH with mask bit 13 set"},
    {{0xF0, 0x0F, 0x02, 0x03, 0x00}, 5, "ST with a register where an address belongs"},
    {{0xF0, 0x00, 0x82, 0x03}, 4, "F0 with a reserved specifier bit"},
};

bool check_vector(const Vector& vector) {
    kr216::Instruction instruction;
    const kr216::DecodeStatus status = kr216::decode(vector.bytes, vector.length, instruction);
    if (status != kr216::DecodeStatus::Ok) {
        std::fprintf(stderr, "FAIL %s: decode status %d\n", vector.text, static_cast<int>(status));
        ++failures;
        return false;
    }
    if (instruction.length != vector.length) {
        std::fprintf(stderr, "FAIL %s: length %u, expected %u\n", vector.text,
                     static_cast<unsigned>(instruction.length),
                     static_cast<unsigned>(vector.length));
        ++failures;
        return false;
    }
    const std::string text = kr216::disassemble(instruction);
    if (text != vector.text) {
        std::fprintf(stderr, "FAIL \"%s\": disassembled as \"%s\"\n", vector.text, text.c_str());
        ++failures;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    // 1. Every Appendix A vector decodes to its documented length and text.
    for (const Vector& vector : kAppendixA) check_vector(vector);

    // 2. A vector's bytes decode the same when the buffer runs past the instruction: the
    //    length comes from the encoding, not from the buffer.
    for (const Vector& vector : kAppendixA) {
        kr216::Instruction instruction;
        const auto status = kr216::decode(vector.bytes, sizeof vector.bytes, instruction);
        if (status != kr216::DecodeStatus::Ok || instruction.length != vector.length ||
            kr216::disassemble(instruction) != vector.text) {
            std::fprintf(stderr, "FAIL %s: decoding from a longer buffer differs\n", vector.text);
            ++failures;
        }
    }

    // 3. Truncating a multi-byte instruction by one byte asks for more.
    for (const Vector& vector : kAppendixA) {
        if (vector.length < 2) continue;
        kr216::Instruction instruction;
        const auto status = kr216::decode(vector.bytes, vector.length - 1, instruction);
        if (status != kr216::DecodeStatus::NeedMoreBytes) {
            std::fprintf(stderr, "FAIL %s: truncation reported %d\n", vector.text,
                         static_cast<int>(status));
            ++failures;
        }
    }

    // 4. The unallocated encodings trap, and every allocated one decodes from a zero payload.
    //    Four short rows are allocated but name a register a mask may not contain
    //    (`PUSH`/`POP` of PC or SP), so they are expected to trap as illegal operands.
    struct ReservedRange {
        std::uint8_t low;
        std::uint8_t high;
        const char* why;
    };
    constexpr ReservedRange kReservedEncodings[] = {
        {0x79, 0x7F, "short system rows"},
        {0xDD, 0xDF, "normal opcodes 29-31"},
        {0xF2, 0xF2, "escape F2"},
        {0xF9, 0xFF, "escape F9-FF"},
    };
    constexpr std::uint8_t kIllegalRegisterRows[] = {0x0E, 0x0F, 0x1E, 0x1F};

    int refused = 0;
    int allocated = 0;
    for (int byte = 0; byte < 256; ++byte) {
        bool expect_illegal = false;
        for (const ReservedRange& range : kReservedEncodings) {
            if (byte >= range.low && byte <= range.high) expect_illegal = true;
        }
        for (const std::uint8_t row : kIllegalRegisterRows) {
            if (byte == row) expect_illegal = true;
        }
        kr216::Instruction instruction;
        std::uint8_t payload[6] = {};
        payload[0] = static_cast<std::uint8_t>(byte);

        if (expect_illegal) {
            ++refused;
            if (kr216::decode(payload, sizeof payload, instruction) !=
                kr216::DecodeStatus::IllegalInstruction) {
                std::fprintf(stderr, "FAIL byte 0x%02X did not trap\n", byte);
                ++failures;
            }
            continue;
        }

        ++allocated;
        const kr216::DecodeStatus status = kr216::decode(payload, sizeof payload, instruction);
        if (status != kr216::DecodeStatus::Ok) {
            std::fprintf(stderr, "FAIL allocated byte 0x%02X: reported %d\n", byte,
                         static_cast<int>(status));
            ++failures;
            continue;
        }
        const auto cls = kr216::classify(static_cast<std::uint8_t>(byte));
        if (instruction.length < kr216::minimum_length(cls) || instruction.length > 6) {
            std::fprintf(stderr, "FAIL byte 0x%02X: length %u outside the class bounds\n", byte,
                         static_cast<unsigned>(instruction.length));
            ++failures;
        }
    }
    if (refused != 22 || allocated != 234) {
        std::fprintf(stderr, "FAIL %d refused and %d allocated bytes, expected 22 and 234\n",
                     refused, allocated);
        ++failures;
    }

    // 5. Reserved fields and illegal operands trap.
    for (const Illegal& illegal : kIllegal) {
        kr216::Instruction instruction;
        const auto status = kr216::decode(illegal.bytes, illegal.length, instruction);
        if (status != kr216::DecodeStatus::IllegalInstruction) {
            std::fprintf(stderr, "FAIL %s: reported %d\n", illegal.why, static_cast<int>(status));
            ++failures;
        }
    }

    if (failures != 0) {
        std::fprintf(stderr, "vector_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("vector_test: %d vectors, %d illegal cases, %d refused bytes, %d allocated\n",
                static_cast<int>(sizeof kAppendixA / sizeof kAppendixA[0]),
                static_cast<int>(sizeof kIllegal / sizeof kIllegal[0]), refused, allocated);
    return 0;
}
