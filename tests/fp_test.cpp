// Bit-exact FPU vectors: every result is compared with the host's own IEEE-754 binary64
// arithmetic, which is the definition of correct here. NaN results compare by "is NaN",
// because the payload a multiply produces is not specified by the ISA and differs between
// x86 and ARM.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "kr216/machine.h"

namespace {

int failures = 0;

double to_double(std::uint64_t bits) {
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

std::uint64_t to_bits(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

void check(bool ok, const char* what, std::uint64_t a, std::uint64_t b) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s with a=%016llX b=%016llX\n", what,
                     static_cast<unsigned long long>(a), static_cast<unsigned long long>(b));
        ++failures;
    }
}

// Runs one F3 instruction with f0 = a and f1 = b, and returns f0's bits afterwards.
std::uint64_t run_binary(std::uint8_t opcode, std::uint64_t a, std::uint64_t b,
                         bool& trapped) {
    kr216::Machine machine;
    const std::uint8_t program[] = {0xF3, opcode};
    machine.load_image(program, sizeof program, 0x0200);
    machine.state().r[14] = 0x0200;
    machine.state().r[15] = 0xFEF0;
    machine.state().f[0] = a;
    machine.state().f[1] = b;
    machine.run(1);
    trapped = machine.state().r[12] != 0;
    return machine.state().f[0];
}

std::uint64_t run_unary(std::uint8_t opcode, std::uint64_t a, bool& trapped) {
    kr216::Machine machine;
    const std::uint8_t program[] = {0xF3, opcode};
    machine.load_image(program, sizeof program, 0x0200);
    machine.state().r[14] = 0x0200;
    machine.state().r[15] = 0xFEF0;
    machine.state().f[0] = a;
    machine.run(1);
    trapped = machine.state().r[12] != 0;
    return machine.state().f[0];
}

void test_arithmetic() {
    const std::uint64_t values[] = {
        0x3FF0000000000000ULL,  // 1.0
        0xBFF8000000000000ULL,  // -1.5
        0x4002000000000000ULL,  // 2.25
        0x0000000000000000ULL,  // +0.0
        0x8000000000000000ULL,  // -0.0
        0x7FF0000000000000ULL,  // +inf
        0xFFF0000000000000ULL,  // -inf
        0x7FF8000000000000ULL,  // NaN
        0x0000000000000001ULL,  // smallest subnormal
        0x7FEFFFFFFFFFFFFFULL,  // DBL_MAX
        0x3FD5555555555555ULL,  // 1/3
        0x3FB999999999999AULL,  // 0.1
        0x4340000000000000ULL,  // 2^53
    };
    constexpr std::uint8_t kOpcodes[] = {0x01, 0x11, 0x21, 0x31};  // FADD FSUB FMUL FDIV
    const char* names[] = {"FADD", "FSUB", "FMUL", "FDIV"};

    int cases = 0;
    for (const std::uint64_t a : values) {
        for (const std::uint64_t b : values) {
            const double left = to_double(a);
            const double right = to_double(b);
            for (int op = 0; op < 4; ++op) {
                double expected = 0.0;
                switch (op) {
                case 0: expected = left + right; break;
                case 1: expected = left - right; break;
                case 2: expected = left * right; break;
                default: expected = left / right; break;
                }
                bool trapped = false;
                const std::uint64_t actual = run_binary(kOpcodes[op], a, b, trapped);
                ++cases;
                check(!trapped, "arithmetic trapped", a, b);
                if (std::isnan(expected)) {
                    check(std::isnan(to_double(actual)), names[op], a, b);
                } else {
                    check(actual == to_bits(expected), names[op], a, b);
                }
            }
        }
    }
    std::printf("fp_test: %d arithmetic cases\n", cases);
}

void test_unary() {
    const std::uint64_t values[] = {
        0x3FF0000000000000ULL, 0xBFF0000000000000ULL, 0x0000000000000000ULL,
        0x8000000000000000ULL, 0x4000000000000000ULL, 0x7FF0000000000000ULL,
        0xFFF0000000000000ULL, 0x7FF8000000000000ULL, 0x0000000000000001ULL,
        0x3FE0000000000000ULL,
    };
    for (const std::uint64_t a : values) {
        const double value = to_double(a);
        bool trapped = false;

        const std::uint64_t root = run_unary(0x60, a, trapped);  // FSQRT
        check(!trapped, "FSQRT trapped", a, 0);
        if (std::isnan(std::sqrt(value))) {
            check(std::isnan(to_double(root)), "FSQRT NaN", a, 0);
        } else {
            check(root == to_bits(std::sqrt(value)), "FSQRT", a, 0);
        }

        const std::uint64_t negated = run_unary(0x40, a, trapped);  // FNEG
        check(!trapped, "FNEG trapped", a, 0);
        check(negated == (a ^ 0x8000000000000000ULL), "FNEG flips the sign", a, 0);

        const std::uint64_t magnitude = run_unary(0x50, a, trapped);  // FABS
        check(magnitude == (a & 0x7FFFFFFFFFFFFFFFULL), "FABS clears the sign", a, 0);

        const std::uint64_t rounded = run_unary(0x90, a, trapped);  // FRND
        check(!trapped, "FRND trapped", a, 0);
        const double narrowed = static_cast<double>(static_cast<float>(value));
        if (std::isnan(narrowed)) {
            check(std::isnan(to_double(rounded)), "FRND NaN", a, 0);
        } else {
            check(rounded == to_bits(narrowed), "FRND rounds through binary32", a, 0);
        }
    }
}

void test_compare() {
    struct Pair {
        std::uint64_t a;
        std::uint64_t b;
    };
    const Pair pairs[] = {
        {0x3FF0000000000000ULL, 0x4000000000000000ULL},  // 1 < 2
        {0x4000000000000000ULL, 0x3FF0000000000000ULL},  // 2 > 1
        {0x3FF0000000000000ULL, 0x3FF0000000000000ULL},  // equal
        {0x7FF8000000000000ULL, 0x3FF0000000000000ULL},  // unordered
        {0x8000000000000000ULL, 0x0000000000000000ULL},  // -0 == +0
    };
    constexpr std::uint16_t kZ = 0x0001;
    constexpr std::uint16_t kN = 0x0002;
    constexpr std::uint16_t kV = 0x0008;

    for (const Pair& pair : pairs) {
        kr216::Machine machine;
        const std::uint8_t program[] = {0xF3, 0x81};  // fcmp f0, f1
        machine.load_image(program, sizeof program, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = 0xFEF0;
        machine.state().f[0] = pair.a;
        machine.state().f[1] = pair.b;
        machine.run(1);
        const double left = to_double(pair.a);
        const double right = to_double(pair.b);
        const bool unordered = std::isnan(left) || std::isnan(right);
        const bool z = !unordered && left == right;
        const bool n = !unordered && left < right;
        const std::uint16_t flags = machine.state().r[11];
        check(((flags & kZ) != 0) == z, "FCMP Z", pair.a, pair.b);
        check(((flags & kN) != 0) == n, "FCMP N", pair.a, pair.b);
        check(((flags & kV) != 0) == unordered, "FCMP V for unordered", pair.a, pair.b);
    }
}

void test_conversions() {
    // I2F of -1 and -32768.
    {
        kr216::Machine machine;
        const std::uint8_t program[] = {0x52, 0xFF, 0xFF, 0xF5, 0x00, 0x20};  // ld r2,#-1; i2f f0,r2
        machine.load_image(program, sizeof program, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = 0xFEF0;
        machine.run(2);
        check(machine.state().f[0] == to_bits(-1.0), "I2F of -1", 0, 0);
    }
    // F2I truncates toward zero and traps out of range.
    const struct {
        std::uint64_t bits;
        std::uint16_t expected;
        bool traps;
    } to_integer[] = {
        {to_bits(2.9), 2, false},
        {to_bits(-2.9), 0xFFFE, false},
        {to_bits(-32768.9), 0x8000, false},
        {to_bits(32767.9), 32767, false},
        {to_bits(32768.0), 0, true},
        {to_bits(-32769.0), 0, true},
        {0x7FF8000000000000ULL, 0, true},  // NaN
        {0x7FF0000000000000ULL, 0, true},  // +inf
    };
    for (const auto& conversion : to_integer) {
        kr216::Machine machine;
        const std::uint8_t program[] = {0xF5, 0x20, 0x20};  // f2i r2, f0
        machine.load_image(program, sizeof program, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = 0xFEF0;
        machine.state().f[0] = conversion.bits;
        machine.run(1);
        const bool trapped = machine.state().r[12] != 0;
        check(trapped == conversion.traps, "F2I trap", conversion.bits, 0);
        if (!conversion.traps) {
            check(machine.state().r[2] == conversion.expected, "F2I value", conversion.bits, 0);
        }
    }
    // F2U accepts 0..65535 and traps outside.
    const struct {
        std::uint64_t bits;
        std::uint16_t expected;
        bool traps;
    } to_unsigned[] = {
        {to_bits(65535.9), 65535, false},
        {to_bits(0.0), 0, false},
        {to_bits(70000.0), 0, true},
        {to_bits(-1.0), 0, true},
    };
    for (const auto& conversion : to_unsigned) {
        kr216::Machine machine;
        const std::uint8_t program[] = {0xF5, 0x30, 0x20};  // f2u r2, f0
        machine.load_image(program, sizeof program, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = 0xFEF0;
        machine.state().f[0] = conversion.bits;
        machine.run(1);
        const bool trapped = machine.state().r[12] != 0;
        check(trapped == conversion.traps, "F2U trap", conversion.bits, 0);
        if (!conversion.traps) {
            check(machine.state().r[2] == conversion.expected, "F2U value", conversion.bits, 0);
        }
    }
}

}  // namespace

int main() {
    test_arithmetic();
    test_unary();
    test_compare();
    test_conversions();

    if (failures != 0) {
        std::fprintf(stderr, "fp_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("fp_test: bit-exact FPU vectors passed\n");
    return 0;
}
