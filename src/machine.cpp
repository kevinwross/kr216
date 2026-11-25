#include "kr216/machine.h"

#include <cmath>
#include <cstring>

namespace kr216 {
namespace {

constexpr std::uint16_t kZ = 0x0001;
constexpr std::uint16_t kN = 0x0002;
constexpr std::uint16_t kC = 0x0004;
constexpr std::uint16_t kV = 0x0008;
constexpr std::uint16_t kI = 0x0010;
constexpr std::uint16_t kT = 0x0020;

constexpr std::uint16_t kNullPageEnd = 0x00FF;
constexpr std::uint16_t kIoBase = 0xFF00;
constexpr std::uint16_t kReservedBase = 0xFF80;

constexpr std::uint16_t kConsoleOut = 0xFF00;
constexpr std::uint16_t kConsoleIn = 0xFF02;
constexpr std::uint16_t kConsoleStatus = 0xFF04;
constexpr std::uint16_t kTimerCounter = 0xFF10;
constexpr std::uint16_t kTimerCompare = 0xFF12;
constexpr std::uint16_t kTimerControl = 0xFF14;
constexpr std::uint16_t kSystemHalt = 0xFF20;
constexpr std::uint16_t kSystemReset = 0xFF22;
constexpr std::uint16_t kSystemCycleLow = 0xFF24;
constexpr std::uint16_t kSystemCycleHigh = 0xFF26;

std::uint16_t sign_extend8(std::uint8_t value) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::int8_t>(value));
}

}  // namespace

Machine::Machine() { reset(); }

void Machine::reset() {
    state_ = CpuState{};
    reason_ = StopReason::Running;
    exit_code_ = 0;
    current_pc_ = 0;
    current_length_ = 0;
    pc_set_ = false;
    console_input_pos_ = 0;
    timer_counter_ = 0;
    timer_compare_ = 0;
    timer_control_ = 0;
    // The reset vector is the word at 0x0100 (ISA 2.4); read it raw, since it is not a trap.
    state_.r[14] = read_memory_word(0x0100);
    state_.r[15] = 0xFEF0;
    state_.r[13] = 0;
    state_.r[12] = 0;
    state_.r[11] = 0;
}

// ---- raw memory views --------------------------------------------------------------

std::uint16_t Machine::read_memory_word(std::uint16_t address) const noexcept {
    const std::uint16_t low = memory_[address];
    const std::uint16_t high = memory_[static_cast<std::uint16_t>(address + 1)];
    return static_cast<std::uint16_t>(low | (high << 8));
}

void Machine::write_memory_word(std::uint16_t address, std::uint16_t value) noexcept {
    memory_[address] = static_cast<std::uint8_t>(value & 0xFF);
    memory_[static_cast<std::uint16_t>(address + 1)] = static_cast<std::uint8_t>(value >> 8);
}

bool Machine::load_image(const std::uint8_t* data, std::size_t size, std::uint16_t base) {
    if (data == nullptr) return false;
    if (size > kMemorySize) return false;
    const std::uint32_t last = static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(size);
    if (last > kMemorySize) return false;
    std::memcpy(memory_ + base, data, size);
    return true;
}

void Machine::set_console_input(std::string input) {
    console_input_ = std::move(input);
    console_input_pos_ = 0;
}

void Machine::push_console_input(std::string text) {
    console_input_ += std::move(text);
}

void Machine::set_console_input_refill(std::function<void()> refill) {
    console_refill_ = std::move(refill);
}

void Machine::set_console_output_sink(std::function<void(char)> sink) {
    console_sink_ = std::move(sink);
}

void Machine::console_put(char byte) {
    console_output_.push_back(byte);
    if (console_sink_) {
        console_sink_(byte);
    }
}

// ---- checked memory access ---------------------------------------------------------

bool Machine::fault_address() {
    raise_trap(TrapCode::AddressError, current_pc_);
    return false;
}

bool Machine::read8(std::uint16_t address, std::uint8_t& out) {
    if (address <= kNullPageEnd || address >= kReservedBase) return fault_address();
    if (address >= kIoBase) return read_io8(address, out);
    out = memory_[address];
    return true;
}

bool Machine::write8(std::uint16_t address, std::uint8_t value) {
    if (address <= kNullPageEnd || address >= kReservedBase) return fault_address();
    if (address >= kIoBase) return write_io8(address, value);
    memory_[address] = value;
    return true;
}

bool Machine::read16(std::uint16_t address, std::uint16_t& out) {
    std::uint8_t low = 0;
    std::uint8_t high = 0;
    if (!read8(address, low)) return false;
    if (!read8(static_cast<std::uint16_t>(address + 1), high)) return false;
    out = static_cast<std::uint16_t>(low | (high << 8));
    return true;
}

bool Machine::write16(std::uint16_t address, std::uint16_t value) {
    if (!write8(address, static_cast<std::uint8_t>(value & 0xFF))) return false;
    return write8(static_cast<std::uint16_t>(address + 1), static_cast<std::uint8_t>(value >> 8));
}

bool Machine::read32(std::uint16_t address, std::uint32_t& out) {
    std::uint16_t low = 0;
    std::uint16_t high = 0;
    if (!read16(address, low)) return false;
    if (!read16(static_cast<std::uint16_t>(address + 2), high)) return false;
    out = static_cast<std::uint32_t>(low) | (static_cast<std::uint32_t>(high) << 16);
    return true;
}

bool Machine::write32(std::uint16_t address, std::uint32_t value) {
    if (!write16(address, static_cast<std::uint16_t>(value & 0xFFFF))) return false;
    return write16(static_cast<std::uint16_t>(address + 2), static_cast<std::uint16_t>(value >> 16));
}

bool Machine::read64(std::uint16_t address, std::uint64_t& out) {
    std::uint64_t value = 0;
    for (int i = 0; i < 4; ++i) {
        std::uint16_t word = 0;
        if (!read16(static_cast<std::uint16_t>(address + 2 * i), word)) return false;
        value |= static_cast<std::uint64_t>(word) << (16 * i);
    }
    out = value;
    return true;
}

bool Machine::write64(std::uint16_t address, std::uint64_t value) {
    for (int i = 0; i < 4; ++i) {
        const auto word = static_cast<std::uint16_t>((value >> (16 * i)) & 0xFFFF);
        if (!write16(static_cast<std::uint16_t>(address + 2 * i), word)) return false;
    }
    return true;
}

// ---- MMIO --------------------------------------------------------------------------

bool Machine::read_io16(std::uint16_t address, std::uint16_t& out) {
    switch (address) {
    case kConsoleOut:  // write-only
        out = 0;
        return true;
    case kConsoleIn:
        if (console_input_pos_ >= console_input_.size() && console_refill_) {
            console_refill_();
        }
        out = 0;
        if (console_input_pos_ < console_input_.size()) {
            out = static_cast<std::uint8_t>(console_input_[console_input_pos_]);
            ++console_input_pos_;
        }
        return true;
    case kConsoleStatus: {
        std::uint16_t status = 0x0002;  // TX_READY
        if (console_input_pos_ < console_input_.size()) status |= 0x0001;  // RX_READY
        out = status;
        return true;
    }
    case kTimerCounter: out = timer_counter_; return true;
    case kTimerCompare: out = timer_compare_; return true;
    case kTimerControl: out = timer_control_; return true;
    case kSystemHalt:
    case kSystemReset: out = 0; return true;
    case kSystemCycleLow: out = static_cast<std::uint16_t>(state_.cycles & 0xFFFF); return true;
    case kSystemCycleHigh: out = static_cast<std::uint16_t>((state_.cycles >> 16) & 0xFFFF);
        return true;
    default:
        return fault_address();
    }
}

bool Machine::write_io16(std::uint16_t address, std::uint16_t value) {
    switch (address) {
    case kConsoleOut:
        console_put(static_cast<char>(value & 0xFF));
        return true;
    case kConsoleIn:
    case kConsoleStatus:
        return true;  // read-only registers
    case kTimerCounter: timer_counter_ = value; return true;
    case kTimerCompare: timer_compare_ = value; return true;
    case kTimerControl: timer_control_ = value; return true;
    case kSystemHalt:
        reason_ = StopReason::Halted;
        return true;
    case kSystemReset:
        reset();
        return true;
    case kSystemCycleLow:
    case kSystemCycleHigh:
        return true;  // read-only
    default:
        return fault_address();
    }
}

bool Machine::read_io8(std::uint16_t address, std::uint8_t& out) {
    std::uint16_t value = 0;
    if (!read_io16(static_cast<std::uint16_t>(address & 0xFFFE), value)) return false;
    out = (address & 1u) != 0 ? static_cast<std::uint8_t>(value >> 8)
                              : static_cast<std::uint8_t>(value & 0xFF);
    return true;
}

bool Machine::write_io8(std::uint16_t address, std::uint8_t value) {
    const std::uint16_t reg = static_cast<std::uint16_t>(address & 0xFFFE);
    std::uint16_t current = 0;
    if (!read_io16(reg, current)) return false;
    if ((address & 1u) != 0) {
        current = static_cast<std::uint16_t>((current & 0x00FF) | (value << 8));
    } else {
        current = static_cast<std::uint16_t>((current & 0xFF00) | value);
    }
    return write_io16(reg, current);
}

// ---- registers and flags -----------------------------------------------------------

std::uint16_t Machine::reg_read(std::uint8_t index) const noexcept {
    if (index == 0) return 0;
    if (index == 1) return 1;
    if (index == 14) return static_cast<std::uint16_t>(current_pc_ + current_length_);
    return state_.r[index];
}

void Machine::reg_write(std::uint8_t index, std::uint16_t value) noexcept {
    if (index == 0 || index == 1) return;  // constants, writes discarded
    if (index == 14) return;               // PC changes only through control flow
    state_.r[index] = value;
}

bool Machine::flag_set(std::uint16_t bit) const noexcept { return (state_.r[11] & bit) != 0; }

void Machine::set_flag(std::uint16_t bit, bool on) noexcept {
    if (on) {
        state_.r[11] = static_cast<std::uint16_t>(state_.r[11] | bit);
    } else {
        state_.r[11] = static_cast<std::uint16_t>(state_.r[11] & ~bit);
    }
}

bool Machine::condition(std::uint8_t code) const noexcept {
    const bool z = flag_set(kZ);
    const bool n = flag_set(kN);
    const bool c = flag_set(kC);
    const bool v = flag_set(kV);
    switch (code) {
    case 0: return z;              // EQ
    case 1: return !z;             // NE
    case 2: return c;              // CS
    case 3: return !c;             // CC
    case 4: return n;              // MI
    case 5: return !n;             // PL
    case 6: return v;              // VS
    case 7: return !v;             // VC
    case 8: return c && !z;        // HI
    case 9: return !c || z;        // LS
    case 10: return n == v;        // GE
    case 11: return n != v;        // LT
    case 12: return !z && n == v;  // GT
    case 13: return z || n != v;   // LE
    default: return false;
    }
}

// ---- arithmetic with flags ---------------------------------------------------------

void Machine::alu_add(std::uint8_t dest, std::uint16_t value, std::uint16_t carry_in) {
    const std::uint16_t left = reg_read(dest);
    const std::uint32_t sum = static_cast<std::uint32_t>(left) + value + carry_in;
    const auto result = static_cast<std::uint16_t>(sum);
    reg_write(dest, result);
    set_flag(kZ, result == 0);
    set_flag(kN, (result & 0x8000) != 0);
    set_flag(kC, sum > 0xFFFF);
    set_flag(kV, ((left ^ result) & (value ^ result) & 0x8000) != 0);
}

void Machine::alu_sub(std::uint8_t dest, std::uint16_t value, std::uint16_t borrow_in) {
    const std::uint16_t left = reg_read(dest);
    const std::uint32_t diff = static_cast<std::uint32_t>(left) - value - borrow_in;
    const auto result = static_cast<std::uint16_t>(diff);
    reg_write(dest, result);
    set_flag(kZ, result == 0);
    set_flag(kN, (result & 0x8000) != 0);
    set_flag(kC, static_cast<std::uint32_t>(left) >= static_cast<std::uint32_t>(value) + borrow_in);
    set_flag(kV, ((left ^ value) & (left ^ result) & 0x8000) != 0);
}

void Machine::alu_cmp(std::uint16_t left, std::uint16_t right) {
    const std::uint32_t diff = static_cast<std::uint32_t>(left) - right;
    const auto result = static_cast<std::uint16_t>(diff);
    set_flag(kZ, result == 0);
    set_flag(kN, (result & 0x8000) != 0);
    set_flag(kC, static_cast<std::uint32_t>(left) >= right);
    set_flag(kV, ((left ^ right) & (left ^ result) & 0x8000) != 0);
}

void Machine::set_logic_flags(std::uint16_t result) {
    set_flag(kZ, result == 0);
    set_flag(kN, (result & 0x8000) != 0);
    set_flag(kC, false);
    set_flag(kV, false);
}

void Machine::shift(std::uint8_t dest, std::uint16_t count, int kind) {
    const std::uint16_t value = reg_read(dest);
    const unsigned n = static_cast<unsigned>(count & 0x0F);
    std::uint16_t result = value;
    bool carry = flag_set(kC);
    if (n != 0) {
        if (kind == 0) {  // SHL
            carry = ((value >> (16 - n)) & 1u) != 0;
            result = static_cast<std::uint16_t>(value << n);
        } else if (kind == 1) {  // SHR
            carry = ((value >> (n - 1)) & 1u) != 0;
            result = static_cast<std::uint16_t>(value >> n);
        } else {  // SAR
            carry = ((value >> (n - 1)) & 1u) != 0;
            result = static_cast<std::uint16_t>(static_cast<std::int16_t>(value) >> n);
        }
    }
    reg_write(dest, result);
    set_flag(kZ, result == 0);
    set_flag(kN, (result & 0x8000) != 0);
    set_flag(kV, false);
    if (n != 0) set_flag(kC, carry);
}

// ---- addresses ---------------------------------------------------------------------

bool Machine::effective_address(const Operand& operand, std::uint8_t size, std::uint16_t& address,
                                AddressAction& action) {
    action = AddressAction::None;
    const auto base = [this, &operand]() -> std::uint16_t {
        if (operand.reg == 14) return current_pc_;  // PC-relative data is from the instruction
        return reg_read(operand.reg);
    };
    switch (operand.mode) {
    case AddressMode::Absolute:
        address = static_cast<std::uint16_t>(operand.value);
        return true;
    case AddressMode::Indirect:
        address = base();
        return true;
    case AddressMode::PostIncrement:
        address = base();
        action = AddressAction::PostIncrement;
        return true;
    case AddressMode::PreDecrement: {
        const std::uint16_t decremented = static_cast<std::uint16_t>(base() - size);
        reg_write(operand.reg, decremented);
        address = decremented;
        return true;
    }
    case AddressMode::Displacement8:
    case AddressMode::Displacement16:
        address = static_cast<std::uint16_t>(base() + operand.value);
        return true;
    }
    return false;
}

void Machine::apply_action(const Operand& operand, std::uint8_t size, AddressAction action) {
    if (action != AddressAction::PostIncrement) return;
    reg_write(operand.reg, static_cast<std::uint16_t>(reg_read(operand.reg) + size));
}

// ---- stack -------------------------------------------------------------------------

bool Machine::push(std::uint16_t value) {
    const auto sp = static_cast<std::uint16_t>(state_.r[15] - 2);
    if (!write16(sp, value)) return false;
    state_.r[15] = sp;
    return true;
}

bool Machine::pop(std::uint16_t& value) {
    if (!read16(state_.r[15], value)) return false;
    state_.r[15] = static_cast<std::uint16_t>(state_.r[15] + 2);
    return true;
}

bool Machine::push_mask(std::uint16_t mask) {
    for (int bit = 0; bit < 16; ++bit) {
        if ((mask & (1u << bit)) == 0) continue;
        if (!push(reg_read(static_cast<std::uint8_t>(bit)))) return false;
    }
    return true;
}

bool Machine::pop_mask(std::uint16_t mask) {
    for (int bit = 15; bit >= 0; --bit) {
        if ((mask & (1u << bit)) == 0) continue;
        std::uint16_t value = 0;
        if (!pop(value)) return false;
        reg_write(static_cast<std::uint8_t>(bit), value);
    }
    return true;
}

// ---- traps -------------------------------------------------------------------------

void Machine::raise_trap(TrapCode code, std::uint16_t saved_pc) {
    if (reason_ != StopReason::Running) return;
    const auto vector_address =
        static_cast<std::uint16_t>(0x0100 + 2 * static_cast<std::uint16_t>(code));
    const auto frame = static_cast<std::uint16_t>(state_.r[15] - 6);
    for (int offset = 0; offset < 6; offset += 2) {
        const auto slot = static_cast<std::uint16_t>(frame + offset);
        if (slot <= kNullPageEnd || slot >= kReservedBase ||
            (slot >= kIoBase && slot < kReservedBase)) {
            reason_ = StopReason::Faulted;  // no room for the frame: stop, do not recurse
            return;
        }
    }
    write_memory_word(frame, state_.r[12]);
    write_memory_word(static_cast<std::uint16_t>(frame + 2), saved_pc);
    write_memory_word(static_cast<std::uint16_t>(frame + 4), state_.r[11]);
    state_.r[12] = frame;  // EFP
    state_.r[15] = frame;  // SP
    set_flag(kI, false);
    state_.r[14] = read_memory_word(vector_address);
    pc_set_ = true;  // the handler's address is the PC now, not the next instruction
}

// ---- SYS host services -------------------------------------------------------------

bool Machine::do_sys() {
    const std::uint16_t call = reg_read(7);
    switch (call) {
    case 0:  // exit
        exit_code_ = static_cast<int>(reg_read(2));
        reason_ = StopReason::Exited;
        return true;
    case 1: {  // write(fd, address, length)
        const std::uint16_t address = reg_read(3);
        const std::uint16_t length = reg_read(4);
        std::uint16_t written = 0;
        for (; written < length; ++written) {
            std::uint8_t byte = 0;
            if (!read8(static_cast<std::uint16_t>(address + written), byte)) return false;
            console_put(static_cast<char>(byte));
        }
        reg_write(2, written);
        return true;
    }
    case 2: {  // read(fd, address, length)
        const std::uint16_t address = reg_read(3);
        const std::uint16_t length = reg_read(4);
        if (console_input_pos_ >= console_input_.size() && console_refill_) {
            console_refill_();
        }
        std::uint16_t got = 0;
        while (got < length && console_input_pos_ < console_input_.size()) {
            const auto byte = static_cast<std::uint8_t>(console_input_[console_input_pos_]);
            if (!write8(static_cast<std::uint16_t>(address + got), byte)) return false;
            ++console_input_pos_;
            ++got;
        }
        reg_write(2, got);
        return true;
    }
    case 3:  // time: R2:R3, seconds since the epoch
        reg_write(2, static_cast<std::uint16_t>(time_ & 0xFFFF));
        reg_write(3, static_cast<std::uint16_t>(time_ >> 16));
        return true;
    case 4: {  // debug_write(address, length)
        const std::uint16_t address = reg_read(2);
        const std::uint16_t length = reg_read(3);
        for (std::uint16_t i = 0; i < length; ++i) {
            std::uint8_t byte = 0;
            if (!read8(static_cast<std::uint16_t>(address + i), byte)) return false;
            debug_output_.push_back(static_cast<char>(byte));
        }
        reg_write(2, 0);
        return true;
    }
    default:
        raise_trap(TrapCode::IllegalOperand, current_pc_);
        return false;
    }
}

// ---- execution ---------------------------------------------------------------------

StopReason Machine::run(std::size_t max_instructions) {
    for (std::size_t executed = 0; executed < max_instructions; ++executed) {
        if (reason_ != StopReason::Running) break;
        execute_one();
        if (reason_ != StopReason::Running) break;
    }
    return reason_;
}

StopReason Machine::step() { return run(1); }

double Machine::to_double(std::uint64_t bits) noexcept {
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

std::uint64_t Machine::to_bits(double value) noexcept {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

void Machine::execute_one() {
    if (reason_ != StopReason::Running) return;
    current_pc_ = state_.r[14];
    current_length_ = 0;
    pc_set_ = false;

    std::uint8_t bytes[6] = {};
    Instruction instruction;
    for (std::size_t fetched = 1; fetched <= sizeof bytes; ++fetched) {
        if (!read8(static_cast<std::uint16_t>(current_pc_ + (fetched - 1)), bytes[fetched - 1])) {
            return;  // address error raised while fetching
        }
        const DecodeStatus status = decode(bytes, fetched, instruction);
        if (status == DecodeStatus::NeedMoreBytes) continue;
        if (status == DecodeStatus::IllegalInstruction) {
            raise_trap(TrapCode::IllegalInstruction, current_pc_);
            return;
        }
        current_length_ = instruction.length;
        break;
    }
    if (current_length_ == 0) {
        raise_trap(TrapCode::IllegalInstruction, current_pc_);
        return;
    }

    if (!execute(instruction)) return;  // a trap or a stop was raised
    if (!pc_set_) state_.r[14] = static_cast<std::uint16_t>(current_pc_ + current_length_);
    ++state_.retired;
    ++state_.cycles;
    if (reason_ == StopReason::Running) {
        ++timer_counter_;
        if (flag_set(kT)) raise_trap(TrapCode::Breakpoint, state_.r[14]);
    }
}

bool Machine::execute(const Instruction& instruction) {
    const auto set_pc = [this](std::uint16_t value) {
        state_.r[14] = value;
        pc_set_ = true;
    };
    // Resolves a jump or call target: register, absolute address, relative displacement, or
    // a memory operand holding the address.
    const auto target_of = [this, &set_pc](const Operand& operand, std::uint16_t& target) -> bool {
        switch (operand.kind) {
        case OperandKind::Register:
            target = reg_read(operand.reg);
            return true;
        case OperandKind::Immediate:
            target = static_cast<std::uint16_t>(operand.value);
            return true;
        case OperandKind::Relative:
            target = static_cast<std::uint16_t>(current_pc_ + current_length_ + operand.value);
            return true;
        case OperandKind::Address: {
            std::uint16_t address = 0;
            AddressAction action = AddressAction::None;
            if (!effective_address(operand, 2, address, action)) return false;
            if (!read16(address, target)) return false;
            apply_action(operand, 2, action);
            return true;
        }
        default:
            return false;
        }
    };

    switch (instruction.op) {
    case Op::Push:
        if (instruction.a.kind == OperandKind::Mask) return push_mask(static_cast<std::uint16_t>(instruction.a.value));
        return push(reg_read(instruction.a.reg));
    case Op::Pop:
        if (instruction.a.kind == OperandKind::Mask) return pop_mask(static_cast<std::uint16_t>(instruction.a.value));
        {
            std::uint16_t value = 0;
            if (!pop(value)) return false;
            reg_write(instruction.a.reg, value);
            return true;
        }
    case Op::Inc:
        alu_add(instruction.a.reg, 1, 0);
        return true;
    case Op::Dec:
        alu_sub(instruction.a.reg, 1, 0);
        return true;
    case Op::Call: {
        std::uint16_t target = 0;
        if (!target_of(instruction.a, target)) return false;
        if (!push(static_cast<std::uint16_t>(current_pc_ + current_length_))) return false;
        set_pc(target);
        return true;
    }
    case Op::Ret: {
        std::uint16_t target = 0;
        if (!pop(target)) return false;
        set_pc(target);
        return true;
    }
    case Op::Nop:
        return true;
    case Op::Brk:
        raise_trap(TrapCode::Breakpoint, current_pc_);
        return false;
    case Op::Sys:
        return do_sys();
    case Op::Ei:
        set_flag(kI, true);
        return true;
    case Op::Di:
        set_flag(kI, false);
        return true;
    case Op::Halt:
        reason_ = StopReason::Halted;
        return true;
    case Op::Iret: {
        const std::uint16_t frame = state_.r[12];
        if (frame == 0) {
            reason_ = StopReason::Faulted;  // no trap frame to return through
            return false;
        }
        state_.r[11] = read_memory_word(static_cast<std::uint16_t>(frame + 4));
        state_.r[14] = read_memory_word(static_cast<std::uint16_t>(frame + 2));
        state_.r[12] = read_memory_word(frame);
        state_.r[15] = static_cast<std::uint16_t>(frame + 6);
        pc_set_ = true;
        return true;
    }
    case Op::Rdcycle:
        reg_write(2, static_cast<std::uint16_t>(state_.cycles & 0xFFFF));
        return true;
    case Op::Mov: {
        std::uint16_t value = 0;
        if (instruction.b.kind == OperandKind::Immediate) {
            value = static_cast<std::uint16_t>(instruction.b.value);
        } else {
            value = reg_read(instruction.b.reg);
        }
        reg_write(instruction.a.reg, value);
        return true;
    }
    case Op::Add:
    case Op::Addc: {
        const std::uint16_t value = instruction.b.kind == OperandKind::Immediate
                                        ? static_cast<std::uint16_t>(instruction.b.value)
                                        : reg_read(instruction.b.reg);
        const std::uint16_t carry = instruction.op == Op::Addc ? (flag_set(kC) ? 1 : 0) : 0;
        alu_add(instruction.a.reg, value, carry);
        return true;
    }
    case Op::Sub:
    case Op::Subb: {
        const std::uint16_t value = instruction.b.kind == OperandKind::Immediate
                                        ? static_cast<std::uint16_t>(instruction.b.value)
                                        : reg_read(instruction.b.reg);
        const std::uint16_t borrow = instruction.op == Op::Subb ? (flag_set(kC) ? 1 : 0) : 0;
        alu_sub(instruction.a.reg, value, borrow);
        return true;
    }
    case Op::Cmp: {
        const std::uint16_t value = instruction.b.kind == OperandKind::Immediate
                                        ? static_cast<std::uint16_t>(instruction.b.value)
                                        : reg_read(instruction.b.reg);
        alu_cmp(reg_read(instruction.a.reg), value);
        return true;
    }
    case Op::And:
    case Op::Or:
    case Op::Xor: {
        const std::uint16_t value = instruction.b.kind == OperandKind::Immediate
                                        ? static_cast<std::uint16_t>(instruction.b.value)
                                        : reg_read(instruction.b.reg);
        const std::uint16_t left = reg_read(instruction.a.reg);
        std::uint16_t result = 0;
        if (instruction.op == Op::And) result = static_cast<std::uint16_t>(left & value);
        if (instruction.op == Op::Or) result = static_cast<std::uint16_t>(left | value);
        if (instruction.op == Op::Xor) result = static_cast<std::uint16_t>(left ^ value);
        reg_write(instruction.a.reg, result);
        set_logic_flags(result);
        return true;
    }
    case Op::Mul:
    case Op::Mulh: {
        const std::uint16_t left = reg_read(instruction.a.reg);
        const std::uint16_t right = reg_read(instruction.b.reg);
        std::uint16_t result = 0;
        if (instruction.op == Op::Mul) {
            result = static_cast<std::uint16_t>(static_cast<std::uint32_t>(left) * right);
        } else {
            const std::int32_t product = static_cast<std::int32_t>(static_cast<std::int16_t>(left)) *
                                         static_cast<std::int16_t>(right);
            result = static_cast<std::uint16_t>(static_cast<std::uint32_t>(product) >> 16);
        }
        reg_write(instruction.a.reg, result);
        set_flag(kZ, result == 0);
        set_flag(kN, (result & 0x8000) != 0);
        set_flag(kC, false);
        set_flag(kV, false);
        return true;
    }
    case Op::Div:
    case Op::Divs: {
        const std::uint16_t left = reg_read(instruction.a.reg);
        const std::uint16_t right = reg_read(instruction.b.reg);
        if (right == 0) {
            raise_trap(TrapCode::IllegalOperand, current_pc_);
            return false;
        }
        std::uint16_t result = 0;
        if (instruction.op == Op::Div) {
            result = static_cast<std::uint16_t>(left / right);
        } else {
            const std::int32_t quotient = static_cast<std::int32_t>(static_cast<std::int16_t>(left)) /
                                          static_cast<std::int32_t>(static_cast<std::int16_t>(right));
            result = static_cast<std::uint16_t>(quotient);
        }
        reg_write(instruction.a.reg, result);
        set_flag(kZ, result == 0);
        set_flag(kN, (result & 0x8000) != 0);
        set_flag(kC, false);
        set_flag(kV, false);
        return true;
    }
    case Op::Shl:
    case Op::Shr:
    case Op::Sar: {
        const std::uint16_t count = instruction.b.kind == OperandKind::Immediate
                                        ? static_cast<std::uint16_t>(instruction.b.value)
                                        : reg_read(instruction.b.reg);
        shift(instruction.a.reg, count, instruction.op == Op::Shl ? 0
                                             : instruction.op == Op::Shr ? 1
                                                                         : 2);
        return true;
    }
    case Op::Neg: {
        const std::uint16_t value = reg_read(instruction.a.reg);
        const std::uint16_t result = static_cast<std::uint16_t>(0 - value);
        reg_write(instruction.a.reg, result);
        alu_cmp(0, value);  // flags of 0 - Rd, including the no-borrow carry
        return true;
    }
    case Op::Not: {
        const std::uint16_t result = static_cast<std::uint16_t>(~reg_read(instruction.a.reg));
        reg_write(instruction.a.reg, result);
        set_logic_flags(result);
        return true;
    }
    case Op::Sxtb: {
        const std::uint16_t result = sign_extend8(static_cast<std::uint8_t>(reg_read(instruction.a.reg)));
        reg_write(instruction.a.reg, result);
        set_logic_flags(result);
        return true;
    }
    case Op::Ld:
    case Op::Ldbu:
    case Op::Ldbs: {
        // `LD Rd, #imm` shares Op::Ld with the memory form; only the memory form has an
        // address operand.
        if (instruction.b.kind == OperandKind::Immediate) {
            reg_write(instruction.a.reg, static_cast<std::uint16_t>(instruction.b.value));
            return true;
        }
        const std::uint8_t size = instruction.op == Op::Ld ? 2 : 1;
        std::uint16_t address = 0;
        AddressAction action = AddressAction::None;
        if (!effective_address(instruction.b, size, address, action)) return false;
        std::uint16_t value = 0;
        if (size == 1) {
            std::uint8_t byte = 0;
            if (!read8(address, byte)) return false;
            if (instruction.op == Op::Ldbs) {
                value = sign_extend8(byte);
            } else {
                value = byte;
            }
        } else {
            if (!read16(address, value)) return false;
        }
        reg_write(instruction.a.reg, value);
        apply_action(instruction.b, size, action);
        return true;
    }
    case Op::St:
    case Op::Stb: {
        const std::uint8_t size = instruction.op == Op::St ? 2 : 1;
        std::uint16_t address = 0;
        AddressAction action = AddressAction::None;
        if (!effective_address(instruction.a, size, address, action)) return false;
        const std::uint16_t value = reg_read(instruction.b.reg);
        bool ok = false;
        if (size == 1) {
            ok = write8(address, static_cast<std::uint8_t>(value & 0xFF));
        } else {
            ok = write16(address, value);
        }
        if (!ok) return false;
        apply_action(instruction.a, size, action);
        return true;
    }
    case Op::Lea: {
        std::uint16_t address = 0;
        AddressAction action = AddressAction::None;
        if (!effective_address(instruction.b, 1, address, action)) return false;
        reg_write(instruction.a.reg, address);
        apply_action(instruction.b, 1, action);
        return true;
    }
    case Op::Bcc:
        if (condition(static_cast<std::uint8_t>(instruction.a.value))) {
            set_pc(static_cast<std::uint16_t>(current_pc_ + current_length_ + instruction.b.value));
        }
        return true;
    case Op::Bra:
        set_pc(static_cast<std::uint16_t>(current_pc_ + current_length_ + instruction.a.value));
        return true;
    case Op::Jmp: {
        std::uint16_t target = 0;
        if (!target_of(instruction.a, target)) return false;
        set_pc(target);
        return true;
    }
    case Op::Setcc:
        reg_write(instruction.a.reg,
                  condition(static_cast<std::uint8_t>(instruction.b.value)) ? 1 : 0);
        return true;
    case Op::Enter: {
        const auto frame = static_cast<std::uint16_t>(instruction.a.value);
        const auto mask = static_cast<std::uint16_t>(instruction.b.value);
        const auto saved = static_cast<std::uint16_t>(state_.r[15] - 2);
        if (!write16(saved, state_.r[13])) return false;
        state_.r[13] = saved;
        state_.r[15] = saved;
        if (!push_mask(mask)) return false;
        state_.r[15] = static_cast<std::uint16_t>(state_.r[15] - frame);
        return true;
    }
    case Op::Leave: {
        const auto mask = static_cast<std::uint16_t>(instruction.a.value);
        unsigned count = 0;
        for (int bit = 0; bit < 16; ++bit) {
            if ((mask & (1u << bit)) != 0) ++count;
        }
        state_.r[15] = static_cast<std::uint16_t>(state_.r[13] - 2 * count);
        if (!pop_mask(mask)) return false;
        std::uint16_t saved_fp = 0;
        if (!read16(state_.r[15], saved_fp)) return false;
        state_.r[13] = saved_fp;
        state_.r[15] = static_cast<std::uint16_t>(state_.r[15] + 2);
        return true;
    }
    case Op::Fadd:
    case Op::Fsub:
    case Op::Fmul:
    case Op::Fdiv:
    case Op::Fneg:
    case Op::Fabs:
    case Op::Fsqrt:
    case Op::Fmov:
    case Op::Fcmp:
    case Op::Frnd:
    case Op::Fld:
    case Op::Fst:
    case Op::Flds:
    case Op::Fsts:
    case Op::I2f:
    case Op::U2f:
    case Op::F2i:
    case Op::F2u:
    case Op::Fpush:
    case Op::Fpop:
        return execute_fp(instruction);
    }
    return true;
}

bool Machine::execute_fp(const Instruction& instruction) {
    const auto fp_read = [this](std::uint8_t index) { return state_.f[index]; };
    const auto fp_write = [this](std::uint8_t index, std::uint64_t bits) {
        state_.f[index] = bits;
    };

    switch (instruction.op) {
    case Op::Fadd:
        fp_write(instruction.a.reg, to_bits(to_double(fp_read(instruction.a.reg)) +
                                            to_double(fp_read(instruction.b.reg))));
        return true;
    case Op::Fsub:
        fp_write(instruction.a.reg, to_bits(to_double(fp_read(instruction.a.reg)) -
                                            to_double(fp_read(instruction.b.reg))));
        return true;
    case Op::Fmul:
        fp_write(instruction.a.reg, to_bits(to_double(fp_read(instruction.a.reg)) *
                                            to_double(fp_read(instruction.b.reg))));
        return true;
    case Op::Fdiv:
        fp_write(instruction.a.reg, to_bits(to_double(fp_read(instruction.a.reg)) /
                                            to_double(fp_read(instruction.b.reg))));
        return true;
    case Op::Fneg:
        fp_write(instruction.a.reg, fp_read(instruction.a.reg) ^ 0x8000000000000000ULL);
        return true;
    case Op::Fabs:
        fp_write(instruction.a.reg, fp_read(instruction.a.reg) & 0x7FFFFFFFFFFFFFFFULL);
        return true;
    case Op::Fsqrt:
        fp_write(instruction.a.reg, to_bits(std::sqrt(to_double(fp_read(instruction.a.reg)))));
        return true;
    case Op::Fmov:
        fp_write(instruction.a.reg, fp_read(instruction.b.reg));
        return true;
    case Op::Fcmp: {
        const double left = to_double(fp_read(instruction.a.reg));
        const double right = to_double(fp_read(instruction.b.reg));
        const bool unordered = std::isnan(left) || std::isnan(right);
        set_flag(kZ, !unordered && left == right);
        set_flag(kN, !unordered && left < right);
        set_flag(kV, unordered);
        set_flag(kC, false);
        return true;
    }
    case Op::Frnd:
        fp_write(instruction.a.reg,
                 to_bits(static_cast<double>(static_cast<float>(to_double(fp_read(instruction.a.reg))))));
        return true;
    case Op::Fld:
    case Op::Fst:
    case Op::Flds:
    case Op::Fsts: {
        const bool wide = instruction.op == Op::Fld || instruction.op == Op::Fst;
        const bool store = instruction.op == Op::Fst || instruction.op == Op::Fsts;
        const std::uint8_t size = wide ? 8 : 4;
        std::uint16_t address = 0;
        AddressAction action = AddressAction::None;
        if (!effective_address(instruction.b, size, address, action)) return false;
        if (wide) {
            if (store) {
                if (!write64(address, fp_read(instruction.a.reg))) return false;
            } else {
                std::uint64_t bits = 0;
                if (!read64(address, bits)) return false;
                fp_write(instruction.a.reg, bits);
            }
        } else {
            std::uint32_t bits = 0;
            if (store) {
                const float narrowed = static_cast<float>(to_double(fp_read(instruction.a.reg)));
                std::memcpy(&bits, &narrowed, sizeof bits);
                if (!write32(address, bits)) return false;
            } else {
                if (!read32(address, bits)) return false;
                float value = 0.0f;
                std::memcpy(&value, &bits, sizeof value);
                fp_write(instruction.a.reg, to_bits(static_cast<double>(value)));
            }
        }
        apply_action(instruction.b, size, action);
        return true;
    }
    case Op::Fpush: {
        const auto sp = static_cast<std::uint16_t>(state_.r[15] - 8);
        if (!write64(sp, fp_read(instruction.a.reg))) return false;
        state_.r[15] = sp;
        return true;
    }
    case Op::Fpop: {
        std::uint64_t bits = 0;
        if (!read64(state_.r[15], bits)) return false;
        fp_write(instruction.a.reg, bits);
        state_.r[15] = static_cast<std::uint16_t>(state_.r[15] + 8);
        return true;
    }
    case Op::I2f:
        fp_write(instruction.a.reg,
                 to_bits(static_cast<double>(static_cast<std::int16_t>(reg_read(instruction.b.reg)))));
        return true;
    case Op::U2f:
        fp_write(instruction.a.reg, to_bits(static_cast<double>(reg_read(instruction.b.reg))));
        return true;
    case Op::F2i:
    case Op::F2u: {
        const double value = to_double(fp_read(instruction.b.reg));
        const double truncated = std::trunc(value);
        if (!std::isfinite(truncated)) {
            raise_trap(TrapCode::IllegalOperand, current_pc_);
            return false;
        }
        if (instruction.op == Op::F2i) {
            if (truncated < -32768.0 || truncated > 32767.0) {
                raise_trap(TrapCode::IllegalOperand, current_pc_);
                return false;
            }
            reg_write(instruction.a.reg,
                      static_cast<std::uint16_t>(static_cast<std::int16_t>(truncated)));
        } else {
            if (truncated < 0.0 || truncated > 65535.0) {
                raise_trap(TrapCode::IllegalOperand, current_pc_);
                return false;
            }
            reg_write(instruction.a.reg, static_cast<std::uint16_t>(truncated));
        }
        return true;
    }
    default:
        return true;
    }
}

}  // namespace kr216
