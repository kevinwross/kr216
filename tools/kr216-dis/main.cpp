// kr216-dis: print a `K216` image's executable sections as an address, bytes and assembly
// listing. The bytes are read from the section payload in the file, so the listing is what the
// linker produced, not what a previous load left in memory.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "kr216/encoding.h"
#include "kr216/image.h"

namespace {

void usage(const char* program) {
    std::fprintf(stderr, "usage: %s image\n", program);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) == "--help") {
        usage(argv[0]);
        return argc == 2 ? 0 : 2;
    }

    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "kr216-dis: cannot open %s\n", argv[1]);
        return 2;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                          std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) {
        std::fprintf(stderr, "kr216-dis: cannot read %s\n", argv[1]);
        return 2;
    }

    kr216::Image image;
    std::string error;
    if (!kr216::Image::parse(bytes.data(), bytes.size(), image, error)) {
        std::fprintf(stderr, "kr216-dis: %s\n", error.c_str());
        return 2;
    }

    for (const kr216::ImageSection& section : image.sections) {
        if (!section.allocated() || !section.executable() || section.nobits()) continue;
        std::printf("; %s at %04X, %u bytes\n", section.name.c_str(), section.address,
                    static_cast<unsigned>(section.size));
        std::uint32_t offset = 0;
        while (offset < section.size) {
            const std::uint8_t* code = bytes.data() + section.offset + offset;
            const std::size_t available = section.size - offset;
            kr216::Instruction instruction;
            const auto status = kr216::decode(code, available, instruction);
            const auto address = static_cast<std::uint16_t>(section.address + offset);
            if (status != kr216::DecodeStatus::Ok) {
                std::printf("%04X: %02X", address, code[0]);
                std::printf("                    %s\n",
                            status == kr216::DecodeStatus::NeedMoreBytes ? "?" : ".byte");
                offset += 1;
                continue;
            }
            char hex[32] = {};
            for (int index = 0; index < instruction.length; ++index) {
                std::snprintf(hex + index * 3, 4, "%02X ", code[index]);
            }
            std::printf("%04X: %-11s  %s\n", address, hex,
                        kr216::disassemble(instruction).c_str());
            offset += instruction.length;
        }
    }
    return 0;
}
