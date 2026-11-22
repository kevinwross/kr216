// Frame tests: ENTER and LEAVE across every even frame size, several masks, nesting and an
// SP adjustment between them (the alloca case). The checks are the saved FP slot, the mask
// push order, the frame subtraction and full restoration.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "kr216/machine.h"

namespace {

int failures = 0;

constexpr std::uint16_t kStartSp = 0xFEF0;

void check(bool ok, const char* what, unsigned frame, unsigned mask) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s frame=%u mask=%04X\n", what, frame, mask);
        ++failures;
    }
}

unsigned popcount(std::uint16_t mask) {
    unsigned count = 0;
    for (int bit = 0; bit < 16; ++bit) {
        if ((mask & (1u << bit)) != 0) ++count;
    }
    return count;
}

struct Program {
    kr216::Machine machine;

    explicit Program(std::vector<std::uint8_t> bytes) {
        std::uint8_t buffer[16] = {};
        for (std::size_t index = 0; index < bytes.size(); ++index) buffer[index] = bytes[index];
        machine.load_image(buffer, bytes.size(), 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = kStartSp;
        machine.state().r[13] = 0x1234;  // the caller's FP, to prove it is saved
        machine.state().r[8] = 0xAAAA;
        machine.state().r[9] = 0xBBBB;
    }

    void run(std::size_t count) { machine.run(count); }
    [[nodiscard]] std::uint16_t reg(int index) const { return machine.state().r[index]; }
    [[nodiscard]] std::uint16_t mem(std::uint16_t address) const {
        return machine.read_memory_word(address);
    }
};

// F7 carries an 8-bit frame (0-254, even) and a 16-bit mask: four bytes in total.
std::vector<std::uint8_t> enter(std::uint16_t frame, std::uint16_t mask) {
    return {0xF7, static_cast<std::uint8_t>(frame & 0xFF), static_cast<std::uint8_t>(mask & 0xFF),
            static_cast<std::uint8_t>(mask >> 8)};
}

std::vector<std::uint8_t> leave(std::uint16_t mask) {
    return {0xDC, static_cast<std::uint8_t>(mask & 0xFF),
            static_cast<std::uint8_t>(mask >> 8)};
}

void test_frame_sizes() {
    for (unsigned frame = 0; frame <= 254; frame += 2) {
        for (const std::uint16_t mask : {std::uint16_t{0x0000}, std::uint16_t{0x0300}, std::uint16_t{0x00C0}}) {
            Program program(enter(static_cast<std::uint16_t>(frame), mask));
            program.run(1);
            const std::uint16_t saved_fp = static_cast<std::uint16_t>(kStartSp - 2);
            const unsigned count = popcount(mask);
            const std::uint16_t expected_sp =
                static_cast<std::uint16_t>(saved_fp - 2 * count - frame);
            check(program.reg(13) == saved_fp, "FP points at the saved FP slot", frame, mask);
            check(program.mem(saved_fp) == 0x1234, "the caller's FP is saved", frame, mask);
            check(program.reg(15) == expected_sp, "SP subtracts the mask and the frame", frame,
                  mask);
            // The mask pushes ascending, so the highest register sits at the lowest address:
            // the block starts at saved_fp - 2*count and grows with descending register number.
            std::uint16_t address = static_cast<std::uint16_t>(saved_fp - 2 * count);
            for (int bit = 15; bit >= 0; --bit) {
                if ((mask & (1u << bit)) == 0) continue;
                check(program.mem(address) == program.reg(bit), "mask value is on the stack",
                      frame, mask);
                address = static_cast<std::uint16_t>(address + 2);
            }
        }
    }
}

void test_leave_restores() {
    for (unsigned frame = 0; frame <= 254; frame += 2) {
        const std::uint16_t mask = 0x0300;
        Program program(enter(static_cast<std::uint16_t>(frame), mask));
        program.run(1);
        // Clobber the saved registers and the frame area, then LEAVE must restore them.
        program.machine.state().r[8] = 0x1111;
        program.machine.state().r[9] = 0x2222;
        std::vector<std::uint8_t> leave_bytes = leave(mask);
        std::uint8_t code[3] = {leave_bytes[0], leave_bytes[1], leave_bytes[2]};
        for (int index = 0; index < 3; ++index) {
            program.machine.write_memory(static_cast<std::uint16_t>(0x0200 + 4 + index),
                                         code[index]);
        }
        program.run(1);
        check(program.reg(8) == 0xAAAA && program.reg(9) == 0xBBBB, "LEAVE restores the mask",
              frame, mask);
        check(program.reg(13) == 0x1234, "LEAVE restores FP", frame, mask);
        check(program.reg(15) == kStartSp, "LEAVE restores SP", frame, mask);
    }
}

void test_alloca() {
    // ENTER a frame, move SP down by 6 (the alloca sequence), then LEAVE must still find the
    // mask and the saved FP because it starts from FP, not from SP.
    Program program(enter(8, 0x0300));
    program.run(1);
    const std::uint16_t expected_sp = static_cast<std::uint16_t>(kStartSp - 2 - 4 - 8);
    check(program.reg(15) == expected_sp, "ENTER lands the frame", 8, 0x0300);
    program.machine.state().r[15] = static_cast<std::uint16_t>(program.reg(15) - 6);
    program.machine.state().r[8] = 0x1111;
    program.machine.state().r[9] = 0x2222;
    std::vector<std::uint8_t> leave_bytes = leave(0x0300);
    for (int index = 0; index < 3; ++index) {
        program.machine.write_memory(static_cast<std::uint16_t>(0x0200 + 4 + index),
                                     leave_bytes[static_cast<std::size_t>(index)]);
    }
    program.run(1);
    check(program.reg(8) == 0xAAAA && program.reg(9) == 0xBBBB, "alloca: mask restored", 8,
          0x0300);
    check(program.reg(13) == 0x1234 && program.reg(15) == kStartSp,
          "alloca: FP and SP restored", 8, 0x0300);
}

void test_nesting() {
    // enter #4, #0x0300 ; enter #2, #0x0006 ; leave #0x0006 ; leave #0x0300
    Program program(enter(4, 0x0300));
    program.run(1);
    std::vector<std::uint8_t> inner = enter(2, 0x0006);  // R1 and R2
    std::vector<std::uint8_t> outer_leave = leave(0x0300);
    std::vector<std::uint8_t> inner_leave = leave(0x0006);
    std::uint8_t code[16];
    std::copy(inner.begin(), inner.end(), code);
    std::copy(inner_leave.begin(), inner_leave.end(), code + inner.size());
    std::copy(outer_leave.begin(), outer_leave.end(), code + inner.size() + inner_leave.size());
    for (std::size_t index = 0; index < inner.size() + inner_leave.size() + outer_leave.size();
         ++index) {
        program.machine.write_memory(static_cast<std::uint16_t>(0x0200 + 4 + index), code[index]);
    }
    program.run(3);  // inner ENTER, inner LEAVE, outer LEAVE
    check(program.reg(13) == 0x1234, "nesting: FP restored after the outer LEAVE", 4, 0x0300);
    check(program.reg(15) == kStartSp, "nesting: SP restored after the outer LEAVE", 4, 0x0300);
    check(program.reg(8) == 0xAAAA, "nesting: outer mask restored", 4, 0x0300);
}

}  // namespace

int main() {
    test_frame_sizes();
    test_leave_restores();
    test_alloca();
    test_nesting();

    if (failures != 0) {
        std::fprintf(stderr, "frames_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("frames_test: frames passed\n");
    return 0;
}
