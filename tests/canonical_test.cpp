// Canonical equivalence: every short form must do exactly what its F0 encoding does.
//
// The ISA makes the generic form the reference (D34), so a compression that computes a
// different result — a wrong flag, a wrong operand order on a store, a wrong address step — is
// caught here without needing the assembler. Each case seeds two identical machines, runs the
// short form on one and the generic form on the other, and compares everything the ISA says
// the instruction touches.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "kr216/machine.h"

namespace {

int failures = 0;


struct MachinePair {
    kr216::Machine a;
    kr216::Machine b;
};

void load_program(kr216::Machine& machine, const std::vector<std::uint8_t>& bytes) {
    std::uint8_t buffer[16] = {};
    std::memcpy(buffer, bytes.data(), bytes.size());
    machine.load_image(buffer, bytes.size(), 0x0200);
    machine.state().r[14] = 0x0200;
    machine.state().r[15] = 0xFEF0;
}

std::vector<std::uint8_t> f0(std::uint8_t opcode, std::uint8_t spec1, std::uint8_t spec2,
                             std::initializer_list<std::uint8_t> extension = {}) {
    std::vector<std::uint8_t> bytes = {0xF0, opcode, spec1, spec2};
    for (const auto byte : extension) bytes.push_back(byte);
    return bytes;
}

// Runs one instruction of each form and compares every architectural result except PC, which
// legitimately differs because the encodings have different lengths.
void compare(const char* name, const std::vector<std::uint8_t>& short_form,
             const std::vector<std::uint8_t>& generic_form, void (*setup)(kr216::Machine&)) {
    kr216::Machine a;
    kr216::Machine b;
    load_program(a, short_form);
    load_program(b, generic_form);
    setup(a);
    setup(b);
    a.run(1);
    b.run(1);

    if (a.reason() != kr216::StopReason::Running || b.reason() != kr216::StopReason::Running) {
        std::fprintf(stderr, "FAIL %s: a form stopped the machine (%d, %d)\n", name,
                     static_cast<int>(a.reason()), static_cast<int>(b.reason()));
        ++failures;
        return;
    }
    if (a.state().r[12] != 0 || b.state().r[12] != 0) {
        std::fprintf(stderr, "FAIL %s: a form trapped (EFP %04X, %04X)\n", name,
                     a.state().r[12], b.state().r[12]);
        ++failures;
        return;
    }
    for (int index = 0; index < 16; ++index) {
        if (index == 14) continue;  // PC
        if (a.state().r[index] != b.state().r[index]) {
            std::fprintf(stderr, "FAIL %s: r%d = %04X vs %04X\n", name, index,
                         a.state().r[index], b.state().r[index]);
            ++failures;
        }
    }
    if (a.state().r[11] != b.state().r[11]) {
        std::fprintf(stderr, "FAIL %s: flags %04X vs %04X\n", name, a.state().r[11],
                     b.state().r[11]);
        ++failures;
    }
    for (int index = 0; index < 4; ++index) {
        if (a.state().f[index] != b.state().f[index]) {
            std::fprintf(stderr, "FAIL %s: f%d differs\n", name, index);
            ++failures;
        }
    }
    // The data window and the stack window are where these cases can write.
    for (std::uint32_t address = 0x3F00; address < 0x4100; ++address) {
        if (a.read_memory(static_cast<std::uint16_t>(address)) !=
            b.read_memory(static_cast<std::uint16_t>(address))) {
            std::fprintf(stderr, "FAIL %s: memory %04X differs\n", name, address);
            ++failures;
            break;
        }
    }
    for (std::uint32_t address = 0xFEE0; address < 0xFF00; ++address) {
        if (a.read_memory(static_cast<std::uint16_t>(address)) !=
            b.read_memory(static_cast<std::uint16_t>(address))) {
            std::fprintf(stderr, "FAIL %s: stack %04X differs\n", name, address);
            ++failures;
            break;
        }
    }
}

void seed_alu(kr216::Machine& machine) {
    machine.state().r[2] = 0x1234;
    machine.state().r[3] = 0x4321;
    machine.state().r[11] = 0x0004;  // carry set, to prove ADDC/SUBB read it the same way
}

void seed_shift(kr216::Machine& machine) {
    machine.state().r[2] = 0x8001;
    machine.state().r[3] = 0x0003;
}

void seed_bytes(kr216::Machine& machine) {
    machine.state().r[2] = 0xABCD;
    machine.state().r[3] = 0x4000;
    machine.write_memory(0x4000, 0x80);
    machine.write_memory(0x4001, 0x34);
    machine.write_memory(0x4002, 0x56);
    machine.write_memory(0x4003, 0x78);
    machine.write_memory(0x4100, 0);
}

void seed_data(kr216::Machine& machine) {
    machine.state().r[2] = 0x1357;
    machine.state().r[3] = 0x4000;
    machine.write_memory_word(0x4000, 0xAAAA);
    machine.write_memory_word(0x4002, 0xBBBB);
    machine.write_memory_word(0x4004, 0xCCCC);
}

void seed_absolute(kr216::Machine& machine) {
    machine.state().r[2] = 0x1357;
    machine.write_memory_word(0x4000, 0xAAAA);
}

void seed_jump(kr216::Machine& machine) {
    machine.state().r[3] = 0x0300;
}

}  // namespace

int main() {
    compare("MOV reg", {0x80, 0x23}, f0(0x00, 0x02, 0x03), seed_alu);
    compare("MOV imm16", {0x52, 0x34, 0x12}, f0(0x00, 0x02, 0x10, {0x34, 0x12}), seed_data);
    compare("ADD reg", {0x81, 0x23}, f0(0x01, 0x02, 0x03), seed_alu);
    compare("ADD imm16", {0xC7, 0x20, 0x05}, f0(0x01, 0x02, 0x10, {0x05, 0x00}), seed_alu);
    compare("ADDC reg", {0x82, 0x23}, f0(0x02, 0x02, 0x03), seed_alu);
    compare("SUB reg", {0x83, 0x23}, f0(0x03, 0x02, 0x03), seed_alu);
    compare("SUBB reg", {0x84, 0x23}, f0(0x04, 0x02, 0x03), seed_alu);
    compare("AND reg", {0x85, 0x23}, f0(0x05, 0x02, 0x03), seed_alu);
    compare("OR reg", {0x86, 0x23}, f0(0x06, 0x02, 0x03), seed_alu);
    compare("XOR reg", {0x87, 0x23}, f0(0x07, 0x02, 0x03), seed_alu);
    compare("CMP reg", {0x88, 0x23}, f0(0x08, 0x02, 0x03), seed_alu);
    compare("CMP imm16", {0xCA, 0x20, 0x05}, f0(0x08, 0x02, 0x10, {0x05, 0x00}), seed_alu);
    compare("MUL reg", {0x89, 0x23}, f0(0x09, 0x02, 0x03), seed_alu);
    compare("MULH reg", {0x8A, 0x23}, f0(0x0A, 0x02, 0x03), seed_shift);
    compare("SHL reg", {0x8B, 0x23}, f0(0x0B, 0x02, 0x03), seed_shift);
    compare("SHR reg", {0x8C, 0x23}, f0(0x0C, 0x02, 0x03), seed_shift);
    compare("SAR reg", {0x8D, 0x23}, f0(0x0D, 0x02, 0x03), seed_shift);
    compare("SHL imm16", {0xD4, 0x20, 0x03}, f0(0x0B, 0x02, 0x10, {0x03, 0x00}), seed_shift);
    compare("DIV reg", {0xD9, 0x23}, f0(0x17, 0x02, 0x03), seed_shift);
    compare("DIVS reg", {0xDA, 0x23}, f0(0x18, 0x02, 0x03), seed_shift);

    compare("LD [r3]", {0x8E, 0x23}, f0(0x0E, 0x02, 0x23), seed_data);
    compare("LD [r3]+", {0x93, 0x23}, f0(0x0E, 0x02, 0x33), seed_data);
    compare("LD -[r3]", {0x98, 0x23}, f0(0x0E, 0x02, 0x43), seed_data);
    compare("LD [r3+d8]", {0xC0, 0x23, 0x02}, f0(0x0E, 0x02, 0x53, {0x02}), seed_data);
    compare("LDBU [r3]", {0x90, 0x23}, f0(0x10, 0x02, 0x23), seed_bytes);
    compare("LDBS [r3]", {0x91, 0x23}, f0(0x11, 0x02, 0x23), seed_bytes);
    compare("LDBS neg", {0x91, 0x23}, f0(0x11, 0x02, 0x23), seed_bytes);
    compare("ST [r3]", {0x8F, 0x23}, f0(0x0F, 0x02, 0x23), seed_data);
    compare("ST [r3]+", {0x94, 0x23}, f0(0x0F, 0x02, 0x33), seed_data);
    compare("ST -[r3]", {0x99, 0x23}, f0(0x0F, 0x02, 0x43), seed_data);
    compare("ST [r3+d8]", {0xC1, 0x23, 0x02}, f0(0x0F, 0x02, 0x53, {0x02}), seed_data);
    compare("STB [r3]", {0x92, 0x23}, f0(0x12, 0x02, 0x23), seed_bytes);
    compare("LEA [r3+d8]", {0xCE, 0x23, 0x04}, f0(0x13, 0x02, 0x53, {0x04}), seed_data);
    compare("LEA [sp+d16]", {0xF1, 0x42, 0x06, 0x00}, f0(0x13, 0x02, 0x6F, {0x06, 0x00}),
            seed_data);
    compare("LDBU absolute", {0xF1, 0x02, 0x00, 0x40},
            f0(0x10, 0x02, 0x60, {0x00, 0x40}), seed_absolute);

    compare("NEG", {0x9D, 0x20}, f0(0x14, 0x02, 0x00), seed_alu);
    compare("NOT", {0x9E, 0x20}, f0(0x15, 0x02, 0x00), seed_alu);
    compare("SXTB", {0xDB, 0x20, 0x00}, f0(0x16, 0x02, 0x00), seed_bytes);
    compare("JMP reg", {0x9F, 0x30}, f0(0x19, 0x03, 0x00), seed_jump);
    compare("JMP absolute", {0xCF, 0x00, 0x03},
            f0(0x19, 0x10, 0x00, {0x00, 0x03}), seed_jump);

    if (failures != 0) {
        std::fprintf(stderr, "canonical_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("canonical_test: short and generic forms agree\n");
    return 0;
}
