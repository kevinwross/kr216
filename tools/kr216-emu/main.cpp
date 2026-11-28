// kr216: load a K216 image, run it, and report why it stopped.
//
// The console's output goes to stdout and the diagnostics to stderr, so a guest program's
// bytes survive redirection. The exit status follows the guest's `exit` call, which is what a
// test harness checks.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "kr216/image.h"
#include "kr216/machine.h"

namespace {

void usage(const char* program) {
    std::fprintf(stderr,
                 "usage: %s [--max-instructions N] [--input TEXT] [--inputfile PATH] [--dump-disassembly]\n"
                 "          [--display-input TEXT] [--display-dump] image\n"
                 "\n"
                 "  --max-instructions N  stop after N instructions (default 10000000; 0 = no limit)\n"
                 "  --max-steps N         alias for --max-instructions\n"
                 "  --input TEXT          console input the guest can read with SYS read (debug serial)\n"
                 "  --inputfile PATH      queue a file's console input, then read stdin\n"
                 "  --console             run with stdin and stdout as the guest console\n"
                 "  --display-input TEXT  scripted text for the display line editor (stdin); the input\n"
                 "                        source closes when the text is exhausted, so the guest sees EOF\n"
                 "  --display-inputfile PATH  read that display input from a file instead\n"
                 "  --display-dump        print the console bytes and the display screen after the run\n"
                 "  --dump-disassembly    print the executable section's instructions first\n",
                 program);
}

}  // namespace

int main(int argc, char** argv) {
    std::size_t budget = 10'000'000;
    std::string input;
    bool dump = false;
    bool console = false;
    bool stdin_fallback = false;
    bool display_dump = false;
    bool display_input_given = false;
    std::string display_input;
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
        if ((argument == "--max-instructions" || argument == "--max-steps") &&
            index + 1 < argc) {
            budget = static_cast<std::size_t>(std::strtoull(argv[++index], nullptr, 0));
        } else if (argument == "--input" && index + 1 < argc) {
            input = argv[++index];
        } else if (argument == "--inputfile" && index + 1 < argc) {
            const char* path = argv[++index];
            std::ifstream input_file(path, std::ios::binary);
            if (!input_file) {
                std::fprintf(stderr, "kr216: cannot open %s\n", path);
                return 2;
            }
            input.assign(std::istreambuf_iterator<char>(input_file),
                         std::istreambuf_iterator<char>());
            stdin_fallback = true;
        } else if (argument == "--console") {
            console = true;
        } else if (argument == "--display-input" && index + 1 < argc) {
            display_input = argv[++index];
            display_input_given = true;
        } else if (argument == "--display-inputfile" && index + 1 < argc) {
            const char* path = argv[++index];
            std::ifstream display_file(path, std::ios::binary);
            if (!display_file) {
                std::fprintf(stderr, "kr216: cannot open %s\n", path);
                return 2;
            }
            display_input.assign(std::istreambuf_iterator<char>(display_file),
                                 std::istreambuf_iterator<char>());
            display_input_given = true;
        } else if (argument == "--display-dump") {
            display_dump = true;
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
        std::fprintf(stderr, "kr216: cannot open %s\n", image_path);
        return 2;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                          std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) {
        std::fprintf(stderr, "kr216: cannot read %s\n", image_path);
        return 2;
    }

    kr216::Image image;
    std::string error;
    if (!kr216::Image::parse(bytes.data(), bytes.size(), image, error)) {
        std::fprintf(stderr, "kr216: %s\n", error.c_str());
        return 2;
    }

    kr216::Machine machine;
    if (!kr216::load(image, bytes.data(), machine, error)) {
        std::fprintf(stderr, "kr216: %s\n", error.c_str());
        return 2;
    }
    machine.set_console_input(input);
    if (console || stdin_fallback) {
        // A line is read only when the guest has consumed everything queued, so
        // execution blocks exactly where an interactive console would wait.
        machine.set_console_input_refill([&machine] {
            std::string line;
            if (std::getline(std::cin, line)) {
                machine.push_console_input(line + "\n");
            }
        });
    }
    if (console) {
        machine.set_console_output_sink([](char byte) {
            std::fputc(byte, stdout);
            std::fflush(stdout);
        });
    }
    if (display_input_given) {
        // The display line editor is the guest's stdin. Feed it one line at a time once a poll
        // has armed it, and close the source when the script is exhausted so the guest observes
        // DISPLAY_END_OF_INPUT and a getchar loop terminates. A last line without a newline is
        // delivered as the final line at the close.
        std::size_t position = 0;
        machine.set_display_input_refill([&machine, &display_input, &position] {
            if (machine.display().line_ready()) {
                return;
            }
            if (position < display_input.size()) {
                if (!machine.display().editor_active()) {
                    return;  // the poll that arms the editor has not run yet
                }
                std::size_t end = display_input.find('\n', position);
                if (end == std::string::npos) {
                    end = display_input.size();
                } else {
                    ++end;
                }
                machine.display().push_text(display_input.substr(position, end - position));
                position = end;
                return;
            }
            machine.display().close_input();
        });
    }

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
    // Zero is the family's "no limit" spelling. The core's run() takes a hard
    // count, so an unlimited run gets the largest representable budget; no
    // practical run reaches it, and every instrumented loop uses the same value.
    const bool unlimited = budget == 0;
    const std::size_t run_budget =
        unlimited ? std::numeric_limits<std::size_t>::max() : budget;
    kr216::StopReason reason = kr216::StopReason::Running;
    bool stopped_at_break = false;
    if (!watch && !breakpoint && !page && !frame_pointer) {
        reason = machine.run(run_budget);
    } else {
        std::vector<std::uint8_t> page_before;
        std::size_t done = 0;
        while (done < run_budget) {
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
    if (display_dump) {
        // Two named sections so a test can tell the debug serial console from stdio's display.
        // The screen is rendered as text rows: every cell code is one character, blanks are
        // spaces, and trailing blanks are trimmed.
        std::printf("console:\n");
        std::fwrite(machine.console_output().data(), 1, machine.console_output().size(), stdout);
        std::printf("\ndisplay:\n");
        const krc::display::TextDisplay& display = machine.display();
        std::uint16_t last_row = 0;
        bool any = false;
        for (std::uint16_t row = 0; row < display.rows(); ++row) {
            for (std::uint16_t col = 0; col < display.cols(); ++col) {
                if (display.cell(row, col).code != 0) {
                    last_row = row;
                    any = true;
                }
            }
        }
        if (any) {
            for (std::uint16_t row = 0; row <= last_row; ++row) {
                std::string line;
                for (std::uint16_t col = 0; col < display.cols(); ++col) {
                    const std::uint16_t code = display.cell(row, col).code;
                    line.push_back(code >= 0x20 && code < 0x7F
                                       ? static_cast<char>(code)
                                       : ' ');
                }
                while (!line.empty() && line.back() == ' ') {
                    line.pop_back();
                }
                std::printf("%s\n", line.c_str());
            }
        }
    } else if (!console) {
        std::fwrite(machine.console_output().data(), 1, machine.console_output().size(), stdout);
    }

    switch (reason) {
    case kr216::StopReason::Exited:
        return machine.exit_code();
    case kr216::StopReason::Halted:
        if (stopped_at_break) {
            return 0;
        }
        std::fprintf(stderr, "kr216: halted after %llu instructions\n",
                     static_cast<unsigned long long>(machine.state().retired));
        return 0;
    case kr216::StopReason::Running:
        if (!unlimited) {
            std::fprintf(stderr, "kr216: instruction budget exhausted (%zu)\n", budget);
        }
        return 3;
    case kr216::StopReason::Faulted:
        std::fprintf(stderr, "kr216: faulted with no room for a trap frame\n");
        return 4;
    }
    return 4;
}
