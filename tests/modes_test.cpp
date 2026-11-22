// The addressing-mode matrix: every mode, base register, size and direction, with the
// effective address and the base update checked separately.
//
// A failure names the cell — mode, register, size, direction — so a wrong step or an
// off-by-size base update is obvious without a debugger.

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

#include "kr216/machine.h"

namespace {

int failures = 0;

constexpr std::uint16_t kBase = 0x4000;
constexpr std::uint16_t kData = 0x5000;  // the value written by a store
constexpr std::uint16_t kPattern = 0x4100;

void check(bool ok, const char* what, const char* mode, int reg, int size, bool load) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s %s r%d size %d %s\n", what, mode, reg, size,
                     load ? "load" : "store");
        ++failures;
    }
}

std::vector<std::uint8_t> f0(std::uint8_t opcode, std::uint8_t spec1, std::uint8_t spec2,
                             std::initializer_list<std::uint8_t> extension = {}) {
    std::vector<std::uint8_t> bytes = {0xF0, opcode, spec1, spec2};
    for (const auto byte : extension) bytes.push_back(byte);
    return bytes;
}

struct Mode {
    const char* name;
    std::uint8_t code;
    std::int32_t displacement;
    std::uint8_t extension_size;
};

// The modes a load or store can use: indirect, post-increment, pre-decrement, and the two
// displacements. Absolute and PC-relative are mode 6 with a chosen base, covered separately.
constexpr Mode kModes[] = {
    {"[Rb]", 2, 0, 0},
    {"[Rb]+", 3, 0, 0},
    {"-[Rb]", 4, 0, 0},
    {"[Rb+d8]", 5, 0x10, 1},
    {"[Rb+d16]", 6, 0x0123, 2},
};

constexpr std::uint8_t kBaseRegisters[] = {2, 3, 10, 12, 13, 15};

struct Case {
    std::uint8_t mode_code;
    std::int32_t displacement;
    std::uint8_t extension_size;
    int reg;
    int size;
};

void run_load(const Case& c, const Mode& mode) {
    // ld r6, <mode> with the base register from the matrix.
    const std::uint8_t spec2 = static_cast<std::uint8_t>((c.mode_code << 4) | c.reg);
    std::vector<std::uint8_t> bytes;
    const std::uint8_t opcode = c.size == 2 ? 0x0E : 0x10;  // LD / LDBU
    if (c.extension_size == 1) {
        bytes = f0(opcode, 0x06, spec2, {static_cast<std::uint8_t>(c.displacement)});
    } else if (c.extension_size == 2) {
        bytes = f0(opcode, 0x06, spec2,
                   {static_cast<std::uint8_t>(c.displacement & 0xFF),
                    static_cast<std::uint8_t>((c.displacement >> 8) & 0xFF)});
    } else {
        bytes = f0(opcode, 0x06, spec2);
    }

    kr216::Machine machine;
    std::uint8_t program[16] = {};
    for (std::size_t index = 0; index < bytes.size(); ++index) program[index] = bytes[index];
    machine.load_image(program, bytes.size(), 0x0200);
    machine.state().r[14] = 0x0200;
    machine.state().r[15] = 0xFEF0;
    machine.state().r[c.reg] = kBase;
    for (std::uint32_t address = 0x3E00; address < 0x5200; ++address) {
        machine.write_memory(static_cast<std::uint16_t>(address),
                             static_cast<std::uint8_t>((address * 7 + 3) & 0xFF));
    }
    machine.run(1);

    const std::int32_t expected_address = static_cast<std::int32_t>(kBase) +
                                          (c.mode_code == 4 ? -c.size : 0) + c.displacement;
    std::uint16_t expected = 0;
    for (int index = 0; index < c.size; ++index) {
        const auto byte = machine.read_memory(static_cast<std::uint16_t>(expected_address + index));
        expected = static_cast<std::uint16_t>(expected | (byte << (8 * index)));
    }
    check(machine.state().r[6] == expected, "value", mode.name, c.reg, c.size, true);
    const std::int32_t expected_base = static_cast<std::int32_t>(kBase) +
                                       (c.mode_code == 3 ? c.size : 0) +
                                       (c.mode_code == 4 ? -c.size : 0);
    check(machine.state().r[c.reg] == static_cast<std::uint16_t>(expected_base), "base",
          mode.name, c.reg, c.size, true);
    check(machine.reason() == kr216::StopReason::Running, "running", mode.name, c.reg, c.size,
          true);
}

void run_store(const Case& c, const Mode& mode) {
    const std::uint8_t spec2 = static_cast<std::uint8_t>((c.mode_code << 4) | c.reg);
    std::vector<std::uint8_t> bytes;
    const std::uint8_t opcode = c.size == 2 ? 0x0F : 0x12;  // ST / STB
    if (c.extension_size == 1) {
        bytes = f0(opcode, 0x04, spec2, {static_cast<std::uint8_t>(c.displacement)});
    } else if (c.extension_size == 2) {
        bytes = f0(opcode, 0x04, spec2,
                   {static_cast<std::uint8_t>(c.displacement & 0xFF),
                    static_cast<std::uint8_t>((c.displacement >> 8) & 0xFF)});
    } else {
        bytes = f0(opcode, 0x04, spec2);
    }

    kr216::Machine machine;
    std::uint8_t program[16] = {};
    for (std::size_t index = 0; index < bytes.size(); ++index) program[index] = bytes[index];
    machine.load_image(program, bytes.size(), 0x0200);
    machine.state().r[14] = 0x0200;
    machine.state().r[15] = 0xFEF0;
    machine.state().r[c.reg] = kBase;
    machine.state().r[4] = kData;
    for (std::uint32_t address = 0x3E00; address < 0x5200; ++address) {
        machine.write_memory(static_cast<std::uint16_t>(address), 0x00);
    }
    machine.run(1);

    const std::int32_t expected_address = static_cast<std::int32_t>(kBase) +
                                          (c.mode_code == 4 ? -c.size : 0) + c.displacement;
    std::uint16_t expected = kData;
    if (c.size == 1) expected = kData & 0xFF;
    bool stored = true;
    for (int index = 0; index < c.size; ++index) {
        const auto byte = machine.read_memory(static_cast<std::uint16_t>(expected_address + index));
        const auto wanted = static_cast<std::uint8_t>((expected >> (8 * index)) & 0xFF);
        stored = stored && byte == wanted;
    }
    check(stored, "stored bytes", mode.name, c.reg, c.size, false);
    const std::int32_t expected_base = static_cast<std::int32_t>(kBase) +
                                       (c.mode_code == 3 ? c.size : 0) +
                                       (c.mode_code == 4 ? -c.size : 0);
    check(machine.state().r[c.reg] == static_cast<std::uint16_t>(expected_base), "base",
          mode.name, c.reg, c.size, false);
}

void test_matrix() {
    for (const Mode& mode : kModes) {
        for (const std::uint8_t reg : kBaseRegisters) {
            for (const int size : {1, 2}) {
                const Case c{mode.code, mode.displacement, mode.extension_size, reg, size};
                run_load(c, mode);
                run_store(c, mode);
            }
        }
    }
}

void test_absolute_and_pc_relative() {
    // Absolute: mode 6 with R0, displacement is the address.
    {
        kr216::Machine machine;
        std::uint8_t program[] = {0xF0, 0x0E, 0x06, 0x60, 0x00, 0x40};
        machine.load_image(program, sizeof program, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = 0xFEF0;
        machine.write_memory_word(0x4000, 0x1234);
        machine.run(1);
        check(machine.state().r[6] == 0x1234, "absolute load", "[addr16]", 0, 2, true);
        check(machine.state().r[0] == 0, "absolute load leaves R0", "[addr16]", 0, 2, true);
    }
    // PC-relative data is from the instruction address, not the next instruction: the load at
    // 0x0200 with displacement 0x0200 reads 0x0400.
    {
        kr216::Machine machine;
        std::uint8_t program[] = {0xF0, 0x0E, 0x06, 0x6E, 0x00, 0x02};
        machine.load_image(program, sizeof program, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = 0xFEF0;
        machine.write_memory_word(0x0400, 0x5678);
        machine.run(1);
        check(machine.state().r[6] == 0x5678, "pc-relative base", "[pc+d16]", 14, 2, true);
    }
}

}  // namespace

int main() {
    test_matrix();
    test_absolute_and_pc_relative();

    if (failures != 0) {
        std::fprintf(stderr, "modes_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("modes_test: mode matrix passed\n");
    return 0;
}
