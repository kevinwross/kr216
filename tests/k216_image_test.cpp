// The committed K216 fixture: the compiler assembles and links it, this test loads it,
// disassembles it and runs it twice. A format or relocation change that the compiler's own
// header check would miss fails here.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "kr216/encoding.h"
#include "kr216/image.h"
#include "kr216/machine.h"

#ifndef KR216_FIXTURE_DIR
#define KR216_FIXTURE_DIR "."
#endif

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
}

const kr216::ImageSection* find_section(const kr216::Image& image, const std::string& name) {
    for (const auto& section : image.sections) {
        if (section.name == name) return &section;
    }
    return nullptr;
}

bool same_state(const kr216::CpuState& left, const kr216::CpuState& right) {
    for (int index = 0; index < 16; ++index) {
        if (left.r[index] != right.r[index]) return false;
    }
    for (int index = 0; index < 4; ++index) {
        if (left.f[index] != right.f[index]) return false;
    }
    return left.cycles == right.cycles && left.retired == right.retired;
}

void test_fixture_runs_twice() {
    const std::string path = std::string(KR216_FIXTURE_DIR) + "/hello.kr216";
    const auto bytes = read_file(path);
    check(!bytes.empty(), "fixture is readable");

    kr216::Image image;
    std::string error;
    check(kr216::Image::parse(bytes.data(), bytes.size(), image, error),
          "fixture parses as K216");
    check(image.entry == 0x0200, "fixture entry is 0x0200");
    check(image.image_base == 0x0200, "fixture base is 0x0200");

    const auto* text = find_section(image, ".text");
    check(text != nullptr && text->executable() && text->address == 0x0200,
          "fixture has executable .text at 0x0200");
    check(text != nullptr && text->size == 4, "fixture text is four bytes");
    check(text != nullptr && bytes[text->offset + 0] == 0x52 &&
              bytes[text->offset + 1] == 0x2A && bytes[text->offset + 2] == 0x00 &&
              bytes[text->offset + 3] == 0x73,
          "fixture text bytes are the expected encoding");
    const auto* reset = find_section(image, ".reset");
    check(reset != nullptr && reset->address == 0x0100 && reset->size == 2,
          "fixture carries the reset vector section");
    check(reset != nullptr && bytes[reset->offset] == 0x00 && bytes[reset->offset + 1] == 0x02,
          "fixture reset word points at 0x0200");

    if (text != nullptr) {
        kr216::Instruction first;
        const auto first_status =
            kr216::decode(bytes.data() + text->offset, text->size, first);
        check(first_status == kr216::DecodeStatus::Ok, "first instruction decodes");
        check(first.length == 3, "first instruction is three bytes");
        check(kr216::disassemble(first) == "ld r2, #0x2A",
              "first instruction disassembles to ld r2, #0x2A");
        kr216::Instruction second;
        const auto second_status =
            kr216::decode(bytes.data() + text->offset + 3, text->size - 3, second);
        check(second_status == kr216::DecodeStatus::Ok, "second instruction decodes");
        check(second.length == 1, "second instruction is one byte");
        check(kr216::disassemble(second) == "sys", "second instruction disassembles to sys");
    }

    kr216::Machine first_machine;
    check(kr216::load(image, bytes.data(), first_machine, error), "fixture loads");
    first_machine.run(10);
    check(first_machine.reason() == kr216::StopReason::Exited, "fixture exits");
    check(first_machine.exit_code() == 42, "fixture exits with 42");

    kr216::Machine second_machine;
    check(kr216::load(image, bytes.data(), second_machine, error), "fixture reloads");
    second_machine.run(10);
    check(second_machine.reason() == first_machine.reason(), "second run stops the same way");
    check(second_machine.exit_code() == first_machine.exit_code(),
          "second run exits with the same code");
    check(same_state(first_machine.state(), second_machine.state()),
          "two runs produce identical CPU state");
    bool memory_equal = true;
    for (std::uint32_t address = 0; address < kr216::Machine::kMemorySize; ++address) {
        if (first_machine.read_memory(static_cast<std::uint16_t>(address)) !=
            second_machine.read_memory(static_cast<std::uint16_t>(address))) {
            memory_equal = false;
            break;
        }
    }
    check(memory_equal, "two runs produce identical memory");
}

void test_loader_reset_is_deterministic() {
    const std::string path = std::string(KR216_FIXTURE_DIR) + "/hello.kr216";
    const auto bytes = read_file(path);
    kr216::Image image;
    std::string error;
    check(kr216::Image::parse(bytes.data(), bytes.size(), image, error),
          "determinism fixture parses");

    kr216::Machine first;
    kr216::Machine second;
    check(kr216::load(image, bytes.data(), first, error), "determinism first load");
    check(kr216::load(image, bytes.data(), second, error), "determinism second load");
    check(same_state(first.state(), second.state()), "reset state is identical");
    check(first.state().r[14] == 0x0200, "reset PC is the image entry");
}

}  // namespace

int main() {
    test_fixture_runs_twice();
    test_loader_reset_is_deterministic();

    if (failures != 0) {
        std::fprintf(stderr, "k216_image_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("k216_image_test: committed image runs, disassembles and is deterministic\n");
    return 0;
}
