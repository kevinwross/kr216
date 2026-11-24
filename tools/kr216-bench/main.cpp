#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "kr216/encoding.h"
#include "kr216/image.h"
#include "kr216/machine.h"

namespace {

const char* reason_name(kr216::StopReason reason) {
    switch (reason) {
    case kr216::StopReason::Running: return "running";
    case kr216::StopReason::Halted: return "halted";
    case kr216::StopReason::Exited: return "exited";
    case kr216::StopReason::Faulted: return "faulted";
    }
    return "unknown";
}

struct Measurement {
    std::uint32_t code_bytes = 0;
    std::uint64_t static_instructions = 0;
};

bool measure(const char* path, std::uint64_t max_instructions,
             Measurement& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return false;
    }
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    kr216::Image image;
    std::string error;
    if (!kr216::Image::parse(bytes.data(), bytes.size(), image, error)) {
        std::fprintf(stderr, "%s: %s\n", path, error.c_str());
        return false;
    }
    kr216::Machine machine;
    if (!kr216::load(image, bytes.data(), machine, error)) {
        std::fprintf(stderr, "%s: %s\n", path, error.c_str());
        return false;
    }
    out = {};
    for (const kr216::ImageSection& section : image.sections) {
        if (!section.executable() || section.nobits()) {
            continue;
        }
        out.code_bytes += section.size;
        std::uint32_t offset = 0;
        while (offset < section.size) {
            std::uint8_t instruction_bytes[6] = {};
            const std::size_t available = std::min<std::size_t>(
                sizeof(instruction_bytes), section.size - offset);
            for (std::size_t index = 0; index < available; ++index) {
                instruction_bytes[index] =
                    machine.read_memory(static_cast<std::uint16_t>(
                        section.address + offset + index));
            }
            kr216::Instruction instruction;
            const auto status =
                kr216::decode(instruction_bytes, available, instruction);
            offset += status == kr216::DecodeStatus::Ok && instruction.length != 0
                          ? instruction.length
                          : 1;
            ++out.static_instructions;
        }
    }
    const kr216::StopReason reason =
        machine.run(static_cast<std::size_t>(max_instructions));
    std::printf("image=%s code_bytes=%u static_instructions=%llu "
                "bytes_per_static_instruction=%.4f reason=%s exit=%d\n",
                path, out.code_bytes,
                static_cast<unsigned long long>(out.static_instructions),
                out.static_instructions == 0
                    ? 0.0
                    : static_cast<double>(out.code_bytes) /
                          static_cast<double>(out.static_instructions),
                reason_name(reason), machine.exit_code());
    return reason == kr216::StopReason::Exited ||
           reason == kr216::StopReason::Halted;
}

} // namespace

int main(int argc, char** argv) {
    double maximum = 0.0;
    std::uint64_t max_instructions = 10000000;
    std::vector<const char*> images;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--max-density" && index + 1 < argc) {
            maximum = std::strtod(argv[++index], nullptr);
        } else if (argument == "--max-instructions" && index + 1 < argc) {
            max_instructions = std::strtoull(argv[++index], nullptr, 10);
        } else if (!argument.empty() && argument[0] == '-') {
            std::fprintf(stderr, "unknown option %s\n", argument.c_str());
            return 2;
        } else {
            images.push_back(argv[index]);
        }
    }
    if (images.empty()) {
        std::fprintf(stderr,
                     "usage: kr216-bench [--max-density N] "
                     "[--max-instructions N] <image.kr216>...\n");
        return 2;
    }
    std::uint32_t code_bytes = 0;
    std::uint64_t static_instructions = 0;
    bool ok = true;
    for (const char* image : images) {
        Measurement measurement;
        if (!measure(image, max_instructions, measurement)) {
            ok = false;
            continue;
        }
        code_bytes += measurement.code_bytes;
        static_instructions += measurement.static_instructions;
    }
    const double average =
        static_instructions == 0
            ? 0.0
            : static_cast<double>(code_bytes) /
                  static_cast<double>(static_instructions);
    std::printf("code_bytes=%u static_instructions=%llu "
                "average_bytes_per_static_instruction=%.4f\n",
                code_bytes,
                static_cast<unsigned long long>(static_instructions), average);
    if (maximum > 0.0 && average > maximum) {
        std::fprintf(stderr, "density %.4f exceeds %.4f\n", average, maximum);
        ok = false;
    }
    return ok ? 0 : 1;
}
