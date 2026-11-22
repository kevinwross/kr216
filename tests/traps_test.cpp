// Trap tests: every vector, the six-byte frame, EFP chaining, IRET, the trace flag, a trap
// inside a handler, and the two cases where the machine cannot build a frame at all.

#include <cstdint>
#include <cstdio>

#include "kr216/machine.h"

namespace {

int failures = 0;

constexpr std::uint16_t kZ = 0x0001;
constexpr std::uint16_t kN = 0x0002;
constexpr std::uint16_t kC = 0x0004;
constexpr std::uint16_t kI = 0x0010;
constexpr std::uint16_t kT = 0x0020;
constexpr std::uint16_t kStartSp = 0xFEF0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}

struct Program {
    kr216::Machine machine;

    explicit Program(std::initializer_list<std::uint8_t> code) {
        std::uint8_t buffer[64] = {};
        std::size_t count = 0;
        for (const auto byte : code) {
            if (count < sizeof buffer) buffer[count++] = byte;
        }
        machine.load_image(buffer, count, 0x0200);
        machine.state().r[14] = 0x0200;
        machine.state().r[15] = kStartSp;
    }

    void vector(kr216::TrapCode code, std::uint16_t handler) {
        machine.write_memory_word(static_cast<std::uint16_t>(0x0100 + 2 * static_cast<int>(code)),
                                  handler);
    }

    void set(int index, std::uint16_t value) { machine.state().r[index] = value; }
    [[nodiscard]] std::uint16_t reg(int index) const { return machine.state().r[index]; }
    [[nodiscard]] std::uint16_t word(std::uint16_t address) const {
        return machine.read_memory_word(address);
    }
    void run(std::size_t count) { machine.run(count); }
};

void expect_frame(Program& program, const char* name, std::uint16_t saved_pc,
                  std::uint16_t old_flags) {
    const std::uint16_t frame = static_cast<std::uint16_t>(kStartSp - 6);
    check(program.reg(12) == frame, name);
    check(program.reg(15) == frame, name);
    check(program.word(frame) == 0, name);          // the caller's EFP was zero
    check(program.word(static_cast<std::uint16_t>(frame + 2)) == saved_pc, name);
    check(program.word(static_cast<std::uint16_t>(frame + 4)) == old_flags, name);
    check((program.reg(11) & kI) == 0, name);       // interrupts are off in a handler
}

void test_each_vector() {
    // Vector 1: BRK.
    {
        Program program({0x72});
        program.vector(kr216::TrapCode::Breakpoint, 0x0300);
        program.run(1);
        check(program.reg(14) == 0x0300, "BRK enters vector 1");
        expect_frame(program, "BRK frame", 0x0200, 0);
    }
    // Vector 2: an unallocated encoding.
    {
        Program program({0xDD, 0x00});
        program.vector(kr216::TrapCode::IllegalInstruction, 0x0310);
        program.run(1);
        check(program.reg(14) == 0x0310, "illegal encoding enters vector 2");
        expect_frame(program, "illegal-instruction frame", 0x0200, 0);
    }
    // Vector 3: an unknown SYS number and a divide by zero.
    {
        Program program({0x57, 0x09, 0x00, 0x73});
        program.vector(kr216::TrapCode::IllegalOperand, 0x0320);
        program.run(2);
        check(program.reg(14) == 0x0320, "unknown SYS enters vector 3");
        expect_frame(program, "SYS frame", 0x0203, 0);
    }
    {
        Program program({0x52, 0x01, 0x00, 0x53, 0x00, 0x00, 0xD9, 0x23});
        program.vector(kr216::TrapCode::IllegalOperand, 0x0330);
        program.run(3);
        check(program.reg(14) == 0x0330, "divide by zero enters vector 3");
        expect_frame(program, "divide frame", 0x0206, 0);
    }
    // Vector 4: the null page and the reserved window.
    {
        Program program({0x52, 0x00, 0x00, 0x8E, 0x20});  // ld r2, [r0]
        program.vector(kr216::TrapCode::AddressError, 0x0340);
        program.run(2);
        check(program.reg(14) == 0x0340, "null-page access enters vector 4");
        expect_frame(program, "address frame", 0x0203, 0);
    }
    {
        // ld r2, [0xFF80] through the F1 absolute form.
        Program reserved({0xF1, 0x02, 0x80, 0xFF});
        reserved.vector(kr216::TrapCode::AddressError, 0x0360);
        reserved.run(1);
        check(reserved.reg(14) == 0x0360, "reserved-window access enters vector 4");
    }
}

void test_iret_restores() {
    Program program({0x72});  // brk
    program.vector(kr216::TrapCode::Breakpoint, 0x0300);
    program.machine.write_memory(0x0300, 0x77);  // iret
    program.run(1);                              // brk
    // A breakpoint saves the BRK's own address: IRET resumes the instruction, which is what a
    // debugger wants. A handler that wants to skip it advances [EFP+2].
    const std::uint16_t frame = static_cast<std::uint16_t>(kStartSp - 6);
    program.machine.write_memory_word(static_cast<std::uint16_t>(frame + 2), 0x0201);
    program.run(1);  // iret
    check(program.reg(14) == 0x0201, "IRET resumes at the handler-adjusted PC");
    check(program.reg(15) == kStartSp, "IRET restores SP");
    check(program.reg(12) == 0, "IRET restores EFP");
}

void test_iret_restores_flags() {
    Program program({0x72});
    program.vector(kr216::TrapCode::Breakpoint, 0x0300);
    program.machine.write_memory(0x0300, 0x77);
    // T stays clear so the IRET does not trace itself; the other bits must survive the round
    // trip through the frame.
    program.set(11, static_cast<std::uint16_t>(kZ | kN | kC | kI));
    program.run(2);  // brk, iret
    check(program.reg(11) == (kZ | kN | kC | kI), "IRET restores the flags exactly");
}

void test_trace() {
    // With T set, one instruction raises the breakpoint vector and saves the next PC.
    Program program({0x78, 0x00});  // rdcycle, nop
    program.vector(kr216::TrapCode::Breakpoint, 0x0300);
    program.machine.write_memory(0x0300, 0x77);  // iret
    program.set(11, kT);
    program.run(1);  // rdcycle, then the trace trap
    check(program.reg(14) == 0x0300, "trace raises vector 1");
    check(program.word(static_cast<std::uint16_t>(kStartSp - 4)) == 0x0201,
          "trace saves the next instruction address");
    // IRET restores T, so the IRET instruction itself is traced: the next trap saves the PC the
    // IRET returned to, which proves it resumed at 0x0201.
    program.run(1);  // iret, then its own trace trap
    check(program.reg(14) == 0x0300, "the traced IRET re-enters the handler");
    check(program.word(static_cast<std::uint16_t>(program.reg(12) + 2)) == 0x0201,
          "the IRET resumed at the instruction after the traced one");
}
void test_nested_traps() {
    // A handler that itself traps: the second frame chains through EFP.
    Program program({0x72});  // brk
    program.vector(kr216::TrapCode::Breakpoint, 0x0300);
    program.machine.write_memory(0x0300, 0x72);  // brk again inside the handler
    program.run(2);
    const std::uint16_t outer = static_cast<std::uint16_t>(kStartSp - 6);
    const std::uint16_t inner = static_cast<std::uint16_t>(outer - 6);
    check(program.reg(12) == inner, "nested trap builds a second frame");
    check(program.word(inner) == outer, "the inner frame chains to the outer EFP");
    check(program.word(static_cast<std::uint16_t>(inner + 2)) == 0x0300,
          "the inner frame saves the handler address");
}

void test_no_room_for_frame() {
    // SP too low for six bytes: the machine stops rather than recursing.
    Program program({0x72});
    program.vector(kr216::TrapCode::Breakpoint, 0x0300);
    program.set(15, 0x0002);
    program.run(1);
    check(program.machine.reason() == kr216::StopReason::Faulted,
          "a trap with no room for a frame faults");
    check(program.reg(12) == 0, "the failed frame leaves EFP untouched");

    // IRET with no frame is a fault, not a jump to address zero.
    Program iret({0x77});
    iret.run(1);
    check(iret.machine.reason() == kr216::StopReason::Faulted, "IRET without a frame faults");
}

}  // namespace

int main() {
    test_each_vector();
    test_iret_restores();
    test_iret_restores_flags();
    test_trace();
    test_nested_traps();
    test_no_room_for_frame();

    if (failures != 0) {
        std::fprintf(stderr, "traps_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("traps_test: traps passed\n");
    return 0;
}
