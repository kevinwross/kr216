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

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--max-instructions" && index + 1 < argc) {
            budget = static_cast<std::size_t>(std::strtoull(argv[++index], nullptr, 0));
        } else if (argument == "--input" && index + 1 < argc) {
            input = argv[++index];
        } else if (argument == "--dump-disassembly") {
            dump = true;
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

    const kr216::StopReason reason = machine.run(budget);
    std::fwrite(machine.console_output().data(), 1, machine.console_output().size(), stdout);

    switch (reason) {
    case kr216::StopReason::Exited:
        return machine.exit_code();
    case kr216::StopReason::Halted:
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
