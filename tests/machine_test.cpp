// Semantic tests: one behaviour per case, asserting the full post-state the ISA specifies.
//
// Each fixture loads a short program at 0x0200, sets registers and steps. Where a case tests
// a trap, the vector table entry is seeded first so the handler address is observable.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "kr216/machine.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}

constexpr std::uint16_t kZ = 0x0001;
constexpr std::uint16_t kN = 0x0002;
constexpr std::uint16_t kC = 0x0004;
constexpr std::uint16_t kV = 0x0008;
constexpr std::uint16_t kI = 0x0010;
constexpr std::uint16_t kT = 0x0020;

struct Fixture {
    kr216::Machine machine;
    static constexpr std::uint16_t kBase = 0x0200;

    explicit Fixture(std::initializer_list<std::uint8_t> code) {
        std::uint8_t bytes[64] = {};
        std::size_t count = 0;
        for (const auto byte : code) {
            if (count < sizeof bytes) bytes[count++] = byte;
        }
        machine.load_image(bytes, count, kBase);
        machine.state().r[14] = kBase;
        machine.state().r[15] = 0xFEF0;
    }

    void run(std::size_t count = 1) { machine.run(count); }
    void set(int reg, std::uint16_t value) { machine.state().r[reg] = value; }
    [[nodiscard]] std::uint16_t get(int reg) const {
        if (reg == 0) return 0;
        if (reg == 1) return 1;
        return machine.state().r[reg];
    }
    [[nodiscard]] bool flag(std::uint16_t bit) const { return (machine.state().r[11] & bit) != 0; }
    [[nodiscard]] std::uint16_t mem(std::uint16_t address) const {
        return machine.read_memory_word(address);
    }
    void poke(std::uint16_t address, std::uint16_t value) {
        machine.write_memory_word(address, value);
    }
    void vector(kr216::TrapCode code, std::uint16_t handler) {
        machine.write_memory_word(static_cast<std::uint16_t>(0x0100 + 2 * static_cast<int>(code)),
                                  handler);
    }
};

void test_constant_registers() {
    Fixture f({0x80, 0x20,  // mov r2, r0
               0x80, 0x31,  // mov r3, r1
               0x50, 0x05, 0x00,  // mov r0, #5 (write discarded)
               0x51, 0x07, 0x00});  // mov r1, #7
    f.run(4);
    check(f.get(2) == 0, "R0 reads as zero");
    check(f.get(3) == 1, "R1 reads as one");
    check(f.machine.state().r[0] == 0, "writes to R0 leave its storage untouched");
    check(f.machine.state().r[1] == 0, "writes to R1 leave its storage untouched");
    check(f.get(2) == 0, "a discarded write to R1 still reads as one");
}

void test_add_flags() {
    Fixture f({0x52, 0x05, 0x00, 0x53, 0x03, 0x00, 0x81, 0x23});
    f.run(3);
    check(f.get(2) == 8, "ADD result");
    check(!f.flag(kZ) && !f.flag(kN) && !f.flag(kC) && !f.flag(kV), "ADD flags clear");
}

void test_add_overflow() {
    Fixture f({0x52, 0xFF, 0x7F, 0x53, 0x01, 0x00, 0x81, 0x23});
    f.run(3);
    check(f.get(2) == 0x8000, "ADD overflow result");
    check(f.flag(kN) && f.flag(kV) && !f.flag(kC) && !f.flag(kZ), "ADD overflow flags");
}

void test_add_carry() {
    Fixture f({0x52, 0xFF, 0xFF, 0x53, 0x01, 0x00, 0x81, 0x23});
    f.run(3);
    check(f.get(2) == 0, "ADD wrap result");
    check(f.flag(kZ) && f.flag(kC) && !f.flag(kN) && !f.flag(kV), "ADD wrap flags");
}

void test_addc_and_subb() {
    // ADDC with C set: 0xFFFF + 0 + 1 = 0 with carry out.
    Fixture a({0x52, 0xFF, 0xFF, 0x53, 0x00, 0x00, 0x82, 0x23});
    a.machine.state().r[11] = kC;
    a.run(3);
    check(a.get(2) == 0, "ADDC includes the incoming carry");
    check(a.flag(kZ) && a.flag(kC), "ADDC carry-out flags");

    // SUBB with C set: 0 - 1 - 1 = 0xFFFE with borrow.
    Fixture b({0x52, 0x00, 0x00, 0x53, 0x01, 0x00, 0x84, 0x23});
    b.machine.state().r[11] = kC;
    b.run(3);
    check(b.get(2) == 0xFFFE, "SUBB includes the incoming borrow");
    check(b.flag(kN) && !b.flag(kC), "SUBB borrow flags");
}

void test_sub_and_cmp() {
    Fixture f({0x52, 0x01, 0x00, 0x53, 0x02, 0x00, 0x83, 0x23});
    f.run(3);
    check(f.get(2) == 0xFFFF, "SUB result");
    check(f.flag(kN) && !f.flag(kC) && !f.flag(kZ), "SUB borrow flags");

    Fixture g({0x52, 0x05, 0x00, 0x88, 0x20});  // cmp r2, r0
    g.run(2);
    check(g.get(2) == 5, "CMP does not write");
    check(!g.flag(kZ) && g.flag(kC), "CMP 5-0 flags");
}

void test_logic() {
    Fixture f({0x52, 0x0F, 0x00, 0x53, 0x03, 0x00, 0x85, 0x23});  // and
    f.run(3);
    check(f.get(2) == 0x03, "AND result");
    check(!f.flag(kZ) && !f.flag(kN) && !f.flag(kC) && !f.flag(kV), "AND flags");

    Fixture n({0x52, 0x00, 0x00, 0x9E, 0x20});  // not r2 of zero
    n.run(2);
    check(n.get(2) == 0xFFFF, "NOT result");
    check(n.flag(kN) && !n.flag(kZ), "NOT flags");
}

void test_inc_dec_neg_sxtb() {
    Fixture i({0x52, 0xFF, 0xFF, 0x22, 0x32, 0x9D, 0x20, 0xDB, 0x20, 0x00});
    i.run(5);
    // inc 0xFFFF -> 0 with carry and zero; dec -> 0xFFFF; neg -> 1; sxtb of 1 -> 1.
    check(i.get(2) == 1, "INC/DEC/NEG/SXTB chain");
    check(!i.flag(kN) && !i.flag(kZ), "final flags after SXTB");

    Fixture s({0x52, 0x80, 0x00, 0xDB, 0x20, 0x00});  // sxtb r2 of 0x0080
    s.run(2);
    check(s.get(2) == 0xFF80, "SXTB sign-extends");
    check(s.flag(kN), "SXTB sets N");
}

void test_shifts() {
    Fixture shl({0x52, 0x00, 0x80, 0xD4, 0x20, 0x01});  // 0x8000 << 1
    shl.run(2);
    check(shl.get(2) == 0, "SHL wraps out");
    check(shl.flag(kC) && shl.flag(kZ) && !shl.flag(kN), "SHL carry out and zero");

    Fixture shr({0x52, 0x01, 0x00, 0xD5, 0x20, 0x01});
    shr.run(2);
    check(shr.get(2) == 0, "SHR result");
    check(shr.flag(kC) && shr.flag(kZ), "SHR carry out and zero");

    Fixture sar({0x52, 0x00, 0x80, 0xD6, 0x20, 0x01});
    sar.run(2);
    check(sar.get(2) == 0xC000, "SAR fills the sign");
    check(sar.flag(kN) && !sar.flag(kC), "SAR flags");
}

void test_multiply_divide() {
    Fixture mul({0x52, 0x00, 0x01, 0x53, 0x03, 0x00, 0x89, 0x23});
    mul.run(3);
    check(mul.get(2) == 0x0300, "MUL low word");

    Fixture mulh({0x52, 0x00, 0x80, 0x53, 0x00, 0x80, 0x8A, 0x23});  // -32768 * -32768
    mulh.run(3);
    check(mulh.get(2) == 0x4000, "MULH signed high word");

    Fixture div({0x52, 0x0A, 0x00, 0x53, 0x03, 0x00, 0xD9, 0x23});
    div.run(3);
    check(div.get(2) == 3, "DIV quotient");

    Fixture divs({0x52, 0xF6, 0xFF, 0x53, 0x03, 0x00, 0xDA, 0x23});  // -10 / 3
    divs.run(3);
    check(divs.get(2) == 0xFFFD, "DIVS truncates toward zero");

    Fixture zero({0x52, 0x01, 0x00, 0x53, 0x00, 0x00, 0xD9, 0x23});
    zero.vector(kr216::TrapCode::IllegalOperand, 0x0300);
    zero.run(3);
    check(zero.machine.state().r[14] == 0x0300, "divide by zero traps to vector 3");
    check(zero.machine.state().r[12] != 0, "divide by zero builds a trap frame");
    check(zero.machine.read_memory_word(static_cast<std::uint16_t>(zero.machine.state().r[12] + 2)) ==
              Fixture::kBase + 6,
          "the trap frame saves the faulting instruction address");
}

void test_loads_and_stores() {
    // ld r2, [r3] / st [r3], r2 / post-increment / pre-decrement / bytes / LEA.
    Fixture f({0x8E, 0x23, 0x93, 0x23, 0x98, 0x23, 0x90, 0x23, 0x91, 0x23, 0x92, 0x23,
               0xCE, 0x23, 0x04});
    f.set(3, 0x4000);
    f.poke(0x3FFE, 0xAAAA);
    f.poke(0x4000, 0x1234);
    f.poke(0x4002, 0x5678);
    f.poke(0x4004, 0x00FF);
    f.run(1);  // ld r2, [r3]
    check(f.get(2) == 0x1234, "LD indirect");
    f.run(1);  // ld r2, [r3]+
    check(f.get(2) == 0x1234 && f.get(3) == 0x4002, "LD post-increment");
    f.run(1);  // ld r2, -[r3]
    check(f.get(2) == 0x1234 && f.get(3) == 0x4000, "LD pre-decrement");
    f.run(1);  // ldbu r2, [r3]
    check(f.get(2) == 0x34, "LDBU zero-extends");
    f.run(1);  // ldbs r2, [r3]
    check(f.get(2) == 0x34, "LDBS of a positive byte");
    f.run(1);  // stb [r3], r2
    check((f.machine.read_memory(0x4000) & 0xFF) == 0x34, "STB writes the low byte");
    f.run(1);  // lea r2, [r3+4]
    check(f.get(2) == 0x4004, "LEA forms the address");

    // Displacement forms and byte sign extension.
    Fixture g({0xC1, 0x23, 0x02, 0xC0, 0x23, 0x02});
    g.set(2, 0xABCD);
    g.set(3, 0x4000);
    g.run(2);
    check(g.mem(0x4002) == 0xABCD, "ST with displacement");
    check(g.get(2) == 0xABCD, "LD with displacement");

    Fixture h({0xC3, 0x23, 0x00});  // ldbs r2, [r3+0]
    h.set(3, 0x4000);
    h.machine.write_memory(0x4000, 0x80);
    h.run(1);
    check(h.get(2) == 0xFF80, "LDBS sign-extends a negative byte");
}

void test_stack_and_frames() {
    // push #0x0006 (R1,R2), pop #0x0006.
    Fixture p({0xD7, 0x06, 0x00, 0xD8, 0x06, 0x00});
    p.set(2, 0x1234);
    p.run(1);
    check(p.get(15) == 0xFEF0 - 4, "PUSH #mask moves SP by 4");
    check(p.mem(0xFEEE) == 1, "PUSH stores R1 at the higher address");
    check(p.mem(0xFEEC) == 0x1234, "PUSH stores the highest register lowest");
    p.set(2, 0);
    p.run(1);
    check(p.get(2) == 0x1234 && p.get(15) == 0xFEF0, "POP #mask reverses PUSH");

    Fixture e({0xF7, 0x04, 0x00, 0x03, 0xDC, 0x00, 0x03});
    e.set(8, 0xAAAA);
    e.set(9, 0xBBBB);
    e.run(1);  // enter #4, #0x0300
    check(e.get(13) == 0xFEEE, "ENTER points FP at the saved FP slot");
    check(e.mem(0xFEEE) == 0, "ENTER saves the caller's FP");
    check(e.mem(0xFEEC) == 0xAAAA && e.mem(0xFEEA) == 0xBBBB, "ENTER pushes the mask in order");
    check(e.get(15) == 0xFEE6, "ENTER subtracts the frame");
    e.set(8, 0x1111);
    e.set(9, 0x2222);
    e.run(1);  // leave #0x0300
    check(e.get(8) == 0xAAAA && e.get(9) == 0xBBBB, "LEAVE restores the mask");
    check(e.get(13) == 0 && e.get(15) == 0xFEF0, "LEAVE restores FP and SP");
}

void test_calls_and_branches() {
    // call 0x0300; (at 0x0300) ret
    Fixture c({0xD0, 0x00, 0x03, 0x00, 0x00});
    c.machine.write_memory(0x0300, 0x70);  // ret
    c.set(15, 0xFEF0);
    c.run(1);
    check(c.machine.state().r[14] == 0x0300, "CALL reaches its target");
    check(c.get(15) == 0xFEEE && c.mem(0xFEEE) == 0x0203, "CALL pushes the return address");
    c.run(1);
    check(c.machine.state().r[14] == 0x0203 && c.get(15) == 0xFEF0, "RET returns and unwinds");

    Fixture b({0x52, 0x00, 0x00, 0x88, 0x20, 0xA0, 0x02});  // cmp r2,r0; beq +2
    b.run(3);
    check(b.machine.state().r[14] == 0x0209, "BEQ branches when Z is set");

    Fixture n({0x52, 0x01, 0x00, 0x88, 0x20, 0xA0, 0x02});
    n.run(3);
    check(n.machine.state().r[14] == 0x0207, "BEQ falls through when Z is clear");
}

void test_traps_and_iret() {
    // BRK traps to the breakpoint vector with the BRK address saved.
    Fixture b({0x72});
    b.vector(kr216::TrapCode::Breakpoint, 0x0300);
    b.run(1);
    check(b.machine.state().r[14] == 0x0300, "BRK enters vector 1");
    check(b.machine.state().r[12] == 0xFEF0 - 6, "BRK builds a six-byte frame");
    check(b.machine.read_memory_word(0xFEF0 - 4) == 0x0200, "BRK saves its own address");
    check(!b.flag(kI), "trap entry clears I");

    // An unallocated encoding traps to vector 2.
    Fixture u({0xDD, 0x00, 0x00});
    u.vector(kr216::TrapCode::IllegalInstruction, 0x0310);
    u.run(1);
    check(u.machine.state().r[14] == 0x0310, "illegal encoding enters vector 2");

    // A load from the null page traps to vector 4.
    Fixture a({0x52, 0x00, 0x00, 0x8E, 0x20});  // ld r2, [r0]
    a.vector(kr216::TrapCode::AddressError, 0x0320);
    a.run(2);
    check(a.machine.state().r[14] == 0x0320, "null-page access enters vector 4");

    // IRET restores the saved PC and flags.
    Fixture i({0x77});  // iret
    i.machine.state().r[12] = 0xFEEA;
    i.machine.write_memory_word(0xFEEA, 0);
    i.machine.write_memory_word(0xFEEA + 2, 0x0201);
    i.machine.write_memory_word(0xFEEA + 4, 0x0003);
    i.run(1);
    check(i.machine.state().r[14] == 0x0201, "IRET restores PC");
    check(i.machine.state().r[15] == 0xFEF0, "IRET restores SP");
    check(i.machine.state().r[11] == 0x0003, "IRET restores flags");
}

void test_sys_and_system() {
    // exit(42): R2 = status, R7 = 0
    Fixture e({0x52, 0x2A, 0x00, 0x57, 0x00, 0x00, 0x73});  // ld r2,#42; ld r7,#0; sys
    e.run(3);
    check(e.machine.reason() == kr216::StopReason::Exited, "SYS exit stops the machine");
    check(e.machine.exit_code() == 42, "SYS exit carries the status");

    // write(fd, address, length): R7=1, R2=fd, R3=address, R4=length.
    Fixture w({0x52, 0x01, 0x00, 0x57, 0x01, 0x00, 0x53, 0x00, 0x40, 0x54, 0x02, 0x00, 0x73});
    w.machine.write_memory(0x4000, 'h');
    w.machine.write_memory(0x4001, 'i');
    w.run(5);
    check(w.machine.console_output() == "hi", "SYS write reaches the console");
    check(w.get(2) == 2, "SYS write returns the byte count");

    // Unknown call number traps to vector 3.
    Fixture u({0x57, 0x09, 0x00, 0x73});
    u.vector(kr216::TrapCode::IllegalOperand, 0x0300);
    u.run(2);
    check(u.machine.state().r[14] == 0x0300, "unknown SYS number traps");

    // HALT stops the machine; RDCYCLE reads the counter; EI/DI change I.
    Fixture h({0x78, 0x74, 0x75, 0x76});
    h.run(4);
    check(h.machine.reason() == kr216::StopReason::Halted, "HALT stops the machine");
    check(h.get(2) == 0, "RDCYCLE reads the low cycle word");
    check(!h.flag(kI), "DI clears I after EI");
}

void test_fpu() {
    const std::uint64_t one_point_five = 0x3FF8000000000000ULL;
    const std::uint64_t two_point_two_five = 0x4002000000000000ULL;

    Fixture sum({0xF3, 0x01});  // fadd f0, f1
    sum.machine.state().f[0] = one_point_five;
    sum.machine.state().f[1] = two_point_two_five;
    sum.run(1);
    check(sum.machine.state().f[0] == 0x400E000000000000ULL, "FADD binary64");

    Fixture cmp({0xF3, 0x81});  // fcmp f0, f1
    cmp.machine.state().f[0] = one_point_five;
    cmp.machine.state().f[1] = two_point_two_five;
    cmp.run(1);
    check(cmp.flag(kN) && !cmp.flag(kZ) && !cmp.flag(kV), "FCMP less-than flags");

    // FRND rounds through binary32; the expected value is computed by the host at run time.
    Fixture round({0xF3, 0x90});  // frnd f0
    round.machine.state().f[0] = 0x3FD5555555555555ULL;  // 1/3 in binary64
    round.run(1);
    double third = 0.0;
    const std::uint64_t third_bits = 0x3FD5555555555555ULL;
    std::memcpy(&third, &third_bits, sizeof third);
    const double rounded = static_cast<double>(static_cast<float>(third));
    std::uint64_t expected = 0;
    std::memcpy(&expected, &rounded, sizeof expected);
    check(round.machine.state().f[0] == expected, "FRND rounds through binary32");

    // FST writes the binary64 pattern, then FLD reads a different one back.
    Fixture memory({0xF4, 0x10, 0x20, 0xF4, 0x00, 0x20});  // fst f0,[r2]; fld f0,[r2]
    memory.set(2, 0x4000);
    memory.machine.state().f[0] = two_point_two_five;
    memory.run(1);
    check(memory.mem(0x4000) == 0x0000 && memory.mem(0x4002) == 0x0000 &&
              memory.mem(0x4004) == 0x0000 && memory.mem(0x4006) == 0x4002,
          "FST stores the binary64 pattern");
    memory.machine.write_memory_word(0x4006, 0x3FF0);  // 1.0
    memory.run(1);
    check(memory.machine.state().f[0] == 0x3FF0000000000000ULL, "FLD loads the binary64 pattern");

    // FPUSH/FPOP round-trip through the stack.
    Fixture fpstack({0xF6, 0x01, 0xF6, 0x05});  // fpush f1; fpop f1
    fpstack.machine.state().f[1] = two_point_two_five;
    fpstack.run(2);
    check(fpstack.get(15) == 0xFEF0 && fpstack.machine.state().f[1] == two_point_two_five,
          "FPUSH/FPOP round-trip");

    // I2F / F2I round-trip a negative value.
    Fixture conv({0x52, 0xFB, 0xFF, 0xF5, 0x00, 0x20, 0xF5, 0x20, 0x20});
    conv.run(3);
    check(conv.get(2) == 0xFFFB, "I2F and F2I round-trip a negative value");
}
}  // namespace

int main() {
    test_constant_registers();
    test_add_flags();
    test_add_overflow();
    test_add_carry();
    test_addc_and_subb();
    test_sub_and_cmp();
    test_logic();
    test_inc_dec_neg_sxtb();
    test_shifts();
    test_multiply_divide();
    test_loads_and_stores();
    test_stack_and_frames();
    test_calls_and_branches();
    test_traps_and_iret();
    test_sys_and_system();
    test_fpu();

    if (failures != 0) {
        std::fprintf(stderr, "machine_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("machine_test: all semantic cases passed\n");
    return 0;
}
