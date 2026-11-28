#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "kr216/encoding.h"
#include "krc/display/display_abi.h"
#include "krc/display/text_display.h"

namespace kr216 {

// Why execution stopped. `Running` means the budget ran out with the machine still able to
// continue; the other three are terminal until `reset()`.
enum class StopReason : std::uint8_t { Running, Halted, Exited, Faulted };

// Trap vectors (kr216-isa.md section 11.1). Vector 0 is the reset vector, not a trap.
enum class TrapCode : std::uint8_t {
    Breakpoint = 1,
    IllegalInstruction = 2,
    IllegalOperand = 3,
    AddressError = 4,
};

// Construction options for the devices the machine owns. The defaults are the reference
// configuration: an 80x30 text display, white on black (krc::display's defaults,
// docs/display-abi.md).
struct MachineOptions {
    std::uint16_t display_rows = krc::display::kDefaultRows;
    std::uint16_t display_cols = krc::display::kDefaultCols;
    std::uint32_t display_foreground = krc::display::kDefaultForeground;
    std::uint32_t display_background = krc::display::kDefaultBackground;
};

struct CpuState {
    std::uint16_t r[16] = {};   // R0/R1 read as constants; R11 flags, R12 EFP, R13 FP, R14 PC, R15 SP
    std::uint64_t f[4] = {};    // F0-F3 as binary64 bit patterns
    std::uint64_t cycles = 0;   // free-running counter behind RDCYCLE and the system words
    std::uint64_t retired = 0;  // instructions retired since reset (the debugger's counter)
};

// The reference machine. It owns the 64 KiB memory, the registers, the trap machinery, the
// console, the text display and the timer; it is the VM behind `kr216` and the debugger
// interface.
class Machine {
public:
    static constexpr std::size_t kMemorySize = 0x10000;
    static constexpr std::uint16_t kConsoleOut = 0xFF00;
    static constexpr std::uint16_t kConsoleIn = 0xFF02;
    static constexpr std::uint16_t kConsoleStatus = 0xFF04;

    Machine();
    explicit Machine(const MachineOptions& options);

    // The reset state of kr216-isa.md section 2.4: PC from the word at 0x0100, SP = 0xFEF0,
    // FP and EFP zero, flags zero, other registers and F0-F3 undefined (cleared here). The
    // devices reset with the CPU: the text display returns to its construction geometry and
    // colours, and the console's read position is rewound.
    void reset();

    // Runs at most `max_instructions`; returns why it stopped. `Running` means the budget ran
    // out. A stopped machine keeps returning its stop reason until `reset()`.
    StopReason run(std::size_t max_instructions);
    StopReason step();

    [[nodiscard]] StopReason reason() const noexcept { return reason_; }
    [[nodiscard]] int exit_code() const noexcept { return exit_code_; }

    // Registers for the debugger and the tests. `state()` is mutable so a harness can seed a
    // program; the instruction implementations use the same helpers so R0/R1 stay constant.
    CpuState& state() noexcept { return state_; }
    [[nodiscard]] const CpuState& state() const noexcept { return state_; }

    // Raw memory views: they bypass the trap windows and MMIO, for loading an image and for
    // the debugger's read_memory/write_memory. The CPU itself uses the checked accessors.
    [[nodiscard]] std::uint8_t read_memory(std::uint16_t address) const noexcept {
        return memory_[address];
    }
    void write_memory(std::uint16_t address, std::uint8_t value) noexcept {
        memory_[address] = value;
    }
    [[nodiscard]] std::uint16_t read_memory_word(std::uint16_t address) const noexcept;
    void write_memory_word(std::uint16_t address, std::uint16_t value) noexcept;
    bool load_image(const std::uint8_t* data, std::size_t size, std::uint16_t base);

    // Console and host services. The console is the three-register device of ISA section 14;
    // `write` and `read` reach the same input and output.
    void set_console_input(std::string input);
    // Appends without disturbing what the guest has not read yet.
    void push_console_input(std::string text);
    // Called when the guest reads console input and nothing is queued. The
    // callback may queue a line; it must not re-enter the machine.
    void set_console_input_refill(std::function<void()> refill);
    // Called for every console byte the guest writes, before it reaches the
    // accumulated buffer. An interactive console writes it through to stdout.
    void set_console_output_sink(std::function<void(char)> sink);
    [[nodiscard]] const std::string& console_output() const noexcept { return console_output_; }
    [[nodiscard]] const std::string& debug_output() const noexcept { return debug_output_; }
    void set_time(std::uint32_t seconds) noexcept { time_ = seconds; }

    // The host text display behind SYS 5, from the shared krc::display package. It is the
    // program's stdin/stdout: the C runtime's putchar/puts/printf write it and getchar/getc
    // read its host line editor. A front end renders it; the emulator itself does not draw.
    // The console above is the debug serial port, not stdio. The display resets with the
    // machine.
    [[nodiscard]] krc::display::TextDisplay& display() noexcept { return display_; }
    [[nodiscard]] const krc::display::TextDisplay& display() const noexcept { return display_; }

    // Called at the start of every SYS 5 READ_LINE and GET_KEY, before the command runs. A
    // host that scripts display input uses it to queue text once the line editor is armed (or
    // to close the input source when its script is exhausted); a windowed front end does not
    // need it. The callback may use display() but must not re-enter the machine.
    void set_display_input_refill(std::function<void()> refill);

private:
    // Checked byte access: MMIO dispatch, null-page and reserved-window traps.
    bool read8(std::uint16_t address, std::uint8_t& out);
    bool write8(std::uint16_t address, std::uint8_t value);
    bool read16(std::uint16_t address, std::uint16_t& out);
    bool write16(std::uint16_t address, std::uint16_t value);
    bool read64(std::uint16_t address, std::uint64_t& out);
    bool write64(std::uint16_t address, std::uint64_t value);
    bool read32(std::uint16_t address, std::uint32_t& out);
    bool write32(std::uint16_t address, std::uint32_t value);
    bool read_io8(std::uint16_t address, std::uint8_t& out);
    bool write_io8(std::uint16_t address, std::uint8_t value);
    bool read_io16(std::uint16_t address, std::uint16_t& out);
    bool write_io16(std::uint16_t address, std::uint16_t value);

    [[nodiscard]] std::uint16_t reg_read(std::uint8_t index) const noexcept;
    void reg_write(std::uint8_t index, std::uint16_t value) noexcept;
    [[nodiscard]] bool flag_set(std::uint16_t bit) const noexcept;
    void set_flag(std::uint16_t bit, bool on) noexcept;
    [[nodiscard]] bool condition(std::uint8_t code) const noexcept;

    void execute_one();
    bool execute(const Instruction& instruction);
    bool execute_fp(const Instruction& instruction);
    void raise_trap(TrapCode code, std::uint16_t saved_pc);
    [[nodiscard]] bool fault_address();

    // Integer operations with the flag rules of ISA section 10.
    void alu_add(std::uint8_t dest, std::uint16_t value, std::uint16_t carry_in);
    void alu_sub(std::uint8_t dest, std::uint16_t value, std::uint16_t borrow_in);
    void alu_cmp(std::uint16_t left, std::uint16_t right);
    void set_logic_flags(std::uint16_t result);
    void shift(std::uint8_t dest, std::uint16_t count, int kind);

    // Address modes. The action is applied after the access so a fault leaves the base intact.
    enum class AddressAction : std::uint8_t { None, PostIncrement, PreDecrement };
    bool effective_address(const Operand& operand, std::uint8_t size, std::uint16_t& address,
                           AddressAction& action);
    void apply_action(const Operand& operand, std::uint8_t size, AddressAction action);

    bool push(std::uint16_t value);
    bool pop(std::uint16_t& value);
    bool push_mask(std::uint16_t mask);
    bool pop_mask(std::uint16_t mask);
    bool do_sys();
    // SYS 5, the display hypercall of kr216-display.md. Never raises a trap: an unknown
    // command or an invalid guest pointer is reported through the status in R2.
    void do_display_sys();
    // A guest data range is 0x0100-0xFEFF: the null page and the I/O/reserved windows are
    // rejected rather than allowed to fault. A zero length touches nothing and is valid.
    [[nodiscard]] bool guest_range_valid(std::uint16_t address,
                                         std::uint16_t length) const noexcept;
    void console_put(char byte);

    // FPU helpers over the binary64 bit patterns.
    static double to_double(std::uint64_t bits) noexcept;
    static std::uint64_t to_bits(double value) noexcept;

    std::uint8_t memory_[kMemorySize] = {};
    CpuState state_{};
    StopReason reason_ = StopReason::Running;
    int exit_code_ = 0;

    std::uint16_t current_pc_ = 0;
    std::uint8_t current_length_ = 0;
    bool pc_set_ = false;  // set by a control-flow instruction, so execute_one must not advance

    // Console, timer and system registers.
    std::string console_input_;
    std::size_t console_input_pos_ = 0;
    std::string console_output_;
    std::function<void()> console_refill_;
    std::function<void(char)> console_sink_;
    std::string debug_output_;
    std::uint32_t time_ = 0;
    std::uint16_t timer_counter_ = 0;
    std::uint16_t timer_compare_ = 0;
    std::uint16_t timer_control_ = 0;
    krc::display::TextDisplay display_;
    std::function<void()> display_refill_;
};

}  // namespace kr216
