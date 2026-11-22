#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "kr216/machine.h"

namespace kr216 {

// One section of a `K216` executable image (ADR 0054 in the compiler repository).
struct ImageSection {
    std::string name;
    std::uint16_t type = 0;     // 1 progbits, 8 nobits
    std::uint16_t flags = 0;    // 1 alloc, 2 write, 4 execute, 8 debug
    std::uint16_t address = 0;  // absolute address in the 64 KiB space
    std::uint32_t offset = 0;   // payload offset in the file
    std::uint32_t size = 0;

    [[nodiscard]] bool allocated() const noexcept { return (flags & 0x0001) != 0; }
    [[nodiscard]] bool writable() const noexcept { return (flags & 0x0002) != 0; }
    [[nodiscard]] bool executable() const noexcept { return (flags & 0x0004) != 0; }
    [[nodiscard]] bool debug() const noexcept { return (flags & 0x0008) != 0; }
    [[nodiscard]] bool nobits() const noexcept { return type == 8; }
};

struct Image {
    std::uint16_t entry = 0;
    std::uint32_t image_base = 0;
    std::vector<ImageSection> sections;
    std::uint32_t debug_offset = 0;
    std::uint32_t debug_size = 0;

    // Parses and validates a container. Every check that a malformed image can fail is here,
    // so `load` never copies a byte from an image that would trap on a later access.
    static bool parse(const std::uint8_t* data, std::size_t size, Image& out, std::string& error);
};

// Copies the allocated sections into the machine, zeros the nobits ranges, writes the entry
// into the reset vector word at 0x0100 and resets the machine, so execution begins at the
// image's entry.
bool load(const Image& image, const std::uint8_t* data, Machine& machine,
          std::string& error);

}  // namespace kr216
