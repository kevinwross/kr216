// kr216-emu: load a K216 image, run it, and report why it stopped.
//
// The console's output goes to stdout and the diagnostics to stderr, so a guest program's
// bytes survive redirection. The exit status follows the guest's `exit` call, which is what a
// test harness checks.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "kr216/image.h"
#include "kr216/machine.h"

namespace {

void usage(const char* program) {
    std::fprintf(stderr,
                 "usage: %s [--max-instructions N] [--input TEXT] [--dump-disassembly] image\n"
                 "\n"
                 "  --max-instructions N  stop after N instructions (default 10000000)\n"
                 "  --input TEXT          console input the guest can read with SYS read\n"
                 "  --dump-disassembly    print the executable section's instructions first\n",
                 program);
}

}  // namespace

int main(int argc, char** argv) {
    std::size_t budget = 10'000'000;
    std::string input;
    bool dump = false;
    const char* image_path = nullptr;

    // The four debugging instruments described in debug-instruments.md. They are
    // per-invocation state, not machine state, so they live beside the other
    // options; the machine exposes step(), state() and read_memory_word() for them.
    bool watch = false;
    std::uint16_t watch_address = 0;
    bool breakpoint = false;
    std::uint16_t break_address = 0;
    unsigned break_limit = 1;
    unsigned break_hits = 0;
    bool page = false;
    std::uint16_t page_base = 0;
    bool frame_pointer = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--max-instructions" && index + 1 < argc) {
            budget = static_cast<std::size_t>(std::strtoull(argv[++index], nullptr, 0));
        } else if (argument == "--input" && index + 1 < argc) {
            input = argv[++index];
        } else if (argument == "--dump-disassembly") {
            dump = true;
        } else if (argument == "--watch" && index + 1 < argc) {
            watch_address = static_cast<std::uint16_t>(std::strtoul(argv[++index], nullptr, 0));
            watch = true;
        } else if (argument == "--break" && index + 1 < argc) {
            break_address = static_cast<std::uint16_t>(std::strtoul(argv[++index], nullptr, 0));
            breakpoint = true;
        } else if (argument == "--break-max" && index + 1 < argc) {
            break_limit = static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 0));
        } else if (argument == "--watch-page" && index + 1 < argc) {
            page_base = static_cast<std::uint16_t>(std::strtoul(argv[++index], nullptr, 0) & 0xF000u);
            page = true;
        } else if (argument == "--watch-fp") {
            frame_pointer = true;
        } else if (argument == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!argument.empty() && argument[0] == '-') {
            usage(argv[0]);
            return 2;
        } else {
            image_path = argv[index];
        }
    }
    if (image_path == nullptr) {
        usage(argv[0]);
        return 2;
    }

    std::ifstream stream(image_path, std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "kr216-emu: cannot open %s\n", image_path);
        return 2;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                          std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) {
        std::fprintf(stderr, "kr216-emu: cannot read %s\n", image_path);
        return 2;
    }

    kr216::Image image;
    std::string error;
    if (!kr216::Image::parse(bytes.data(), bytes.size(), image, error)) {
        std::fprintf(stderr, "kr216-emu: %s\n", error.c_str());
        return 2;
    }

    kr216::Machine machine;
    if (!kr216::load(image, bytes.data(), machine, error)) {
        std::fprintf(stderr, "kr216-emu: %s\n", error.c_str());
        return 2;
    }
    machine.set_console_input(input);

    if (dump) {
        for (const kr216::ImageSection& section : image.sections) {
            if (!section.allocated() || !section.executable()) continue;
            std::uint16_t address = section.address;
            const std::uint32_t end = section.address + section.size;
            while (address < end) {
                std::uint8_t code[6] = {};
                for (int offset = 0; offset < 6; ++offset) {
                    const std::uint16_t at = static_cast<std::uint16_t>(address + offset);
                    code[offset] = at >= section.address && at < end ? machine.read_memory(at) : 0;
                }
                kr216::Instruction instruction;
                if (kr216::decode(code, sizeof code, instruction) != kr216::DecodeStatus::Ok) {
                    std::printf("%04X  ?\n", address);
                    break;
                }
                std::printf("%04X  ", address);
                for (int offset = 0; offset < instruction.length; ++offset) {
                    std::printf("%02X ", code[offset]);
                }
                std::printf(" %s\n", kr216::disassemble(instruction).c_str());
                address = static_cast<std::uint16_t>(address + instruction.length);
            }
        }
    }

    // With an instrument asked for, the single run() call becomes a step loop:
    // each instrument has to sample before and after one instruction, which run()
    // does not offer.
    kr216::StopReason reason = kr216::StopReason::Running;
    bool stopped_at_break = false;
    if (!watch && !breakpoint && !page && !frame_pointer) {
        reason = machine.run(budget);
    } else {
        std::vector<std::uint8_t> page_before;
        std::size_t done = 0;
        while (done < budget) {
            const std::uint16_t pc = machine.state().r[14];
            if (breakpoint && pc == break_address) {
                std::printf("break at 0x%04X: ", static_cast<unsigned>(pc));
                for (unsigned index = 2; index < 10; ++index) {
                    std::printf("r%u=0x%04X ", index,
                                static_cast<unsigned>(machine.state().r[index]));
                }
                std::printf("fp=0x%04X sp=0x%04X\n",
                            static_cast<unsigned>(machine.state().r[13]),
                            static_cast<unsigned>(machine.state().r[15]));
                if (++break_hits >= break_limit) {
                    // A breakpoint ends the run where it stopped, so the budget
                    // message must not claim the program ran out of instructions.
                    stopped_at_break = true;
                    break;
                }
            }
            const std::uint16_t fp_before = machine.state().r[13];
            const std::uint16_t watched_before =
                watch ? machine.read_memory_word(watch_address) : 0;
            if (page && page_before.empty()) {
                page_before.resize(4096, 0);
                for (std::uint32_t byte = 0; byte < 4096; ++byte) {
                    page_before[byte] =
                        machine.read_memory(static_cast<std::uint16_t>(page_base + byte));
                }
            }
            reason = machine.step();
            ++done;
            if (watch) {
                const std::uint16_t watched_after = machine.read_memory_word(watch_address);
                if (watched_after != watched_before) {
                    std::printf("watch 0x%04X at pc=0x%04X: 0x%04X -> 0x%04X\n",
                                static_cast<unsigned>(watch_address),
                                static_cast<unsigned>(pc),
                                static_cast<unsigned>(watched_before),
                                static_cast<unsigned>(watched_after));
                }
            }
            if (page) {
                for (std::uint32_t byte = 0; byte < 4096; ++byte) {
                    const std::uint8_t value =
                        machine.read_memory(static_cast<std::uint16_t>(page_base + byte));
                    if (value != page_before[byte]) {
                        std::printf("page 0x%04X at pc=0x%04X: 0x%02X -> 0x%02X\n",
                                    static_cast<unsigned>(page_base + byte),
                                    static_cast<unsigned>(pc),
                                    static_cast<unsigned>(page_before[byte]),
                                    static_cast<unsigned>(value));
                        page_before[byte] = value;
                    }
                }
            }
            if (frame_pointer && machine.state().r[13] != fp_before) {
                std::printf("fp 0x%04X -> 0x%04X at pc=0x%04X\n",
                            static_cast<unsigned>(fp_before),
                            static_cast<unsigned>(machine.state().r[13]),
                            static_cast<unsigned>(pc));
            }
            if (reason != kr216::StopReason::Running) {
                break;
            }
        }
    }
    std::fwrite(machine.console_output().data(), 1, machine.console_output().size(), stdout);

    switch (reason) {
    case kr216::StopReason::Exited:
        return machine.exit_code();
    case kr216::StopReason::Halted:
        if (stopped_at_break) {
            return 0;
        }
        std::fprintf(stderr, "kr216-emu: halted after %llu instructions\n",
                     static_cast<unsigned long long>(machine.state().retired));
        return 0;
    case kr216::StopReason::Running:
        std::fprintf(stderr, "kr216-emu: instruction budget exhausted (%zu)\n", budget);
        return 3;
    case kr216::StopReason::Faulted:
        std::fprintf(stderr, "kr216-emu: faulted with no room for a trap frame\n");
        return 4;
    }
    return 4;
}
