// Decoder fuzzing: random byte streams must decode deterministically, never overrun the
// buffer, always trap when the first byte is reserved, and never hang the machine.
//
// The generator is a fixed xorshift, so a failure is reproducible from the seed alone.

#include <cstdint>
#include <cstdio>

#include "kr216/encoding.h"
#include "kr216/machine.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}

std::uint32_t next(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

}  // namespace

int main() {
    std::uint32_t rng = 0x9E3779B9u;
    int decoded = 0;
    int trapped = 0;
    int incomplete = 0;

    for (int iteration = 0; iteration < 200000; ++iteration) {
        std::uint8_t buffer[8] = {};
        for (std::uint8_t& byte : buffer) byte = static_cast<std::uint8_t>(next(rng) >> 24);
        const std::size_t size = 1 + (next(rng) % 7);

        kr216::Instruction instruction;
        const kr216::DecodeStatus status = kr216::decode(buffer, size, instruction);
        const bool reserved = kr216::is_reserved(kr216::classify(buffer[0]));
        if (reserved) {
            check(status == kr216::DecodeStatus::IllegalInstruction,
                  "a reserved first byte always traps");
        }

        switch (status) {
        case kr216::DecodeStatus::Ok: {
            ++decoded;
            check(instruction.length >= 1 && instruction.length <= 6,
                  "a decoded length is 1-6 bytes");
            check(instruction.length <= size, "a decoded length fits the buffer");
            check(!kr216::disassemble(instruction).empty(), "a decoded instruction disassembles");
            // Determinism: the same bytes give the same result twice.
            kr216::Instruction again;
            check(kr216::decode(buffer, size, again) == kr216::DecodeStatus::Ok &&
                      again.length == instruction.length &&
                      kr216::disassemble(again) == kr216::disassemble(instruction),
                  "decoding is deterministic");
            break;
        }
        case kr216::DecodeStatus::IllegalInstruction:
            ++trapped;
            break;
        case kr216::DecodeStatus::NeedMoreBytes:
            ++incomplete;
            check(size < 6, "a six-byte buffer is never incomplete");
            break;
        }

        // Execute every hundredth stream for a bounded budget: the machine must always stop or
        // run out of budget, never crash or loop forever.
        if (iteration % 100 == 0) {
            kr216::Machine machine;
            machine.load_image(buffer, size, 0x0200);
            machine.state().r[14] = 0x0200;
            machine.state().r[15] = 0xFEF0;
            const kr216::StopReason reason = machine.run(64);
            check(reason == kr216::StopReason::Running || reason == kr216::StopReason::Halted ||
                      reason == kr216::StopReason::Exited || reason == kr216::StopReason::Faulted,
                  "random execution stops within the budget");
        }
    }

    check(decoded > 1000, "the generator reaches many decodable streams");
    check(trapped > 1000, "the generator reaches many refusals");

    if (failures != 0) {
        std::fprintf(stderr, "fuzz_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("fuzz_test: %d decoded, %d refused, %d incomplete\n", decoded, trapped,
                incomplete);
    return 0;
}
