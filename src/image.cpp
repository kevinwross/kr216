#include "kr216/image.h"

#include <cstring>

namespace kr216 {
namespace {

constexpr std::size_t kHeaderSize = 64;
constexpr std::size_t kSectionRecordSize = 32;
constexpr std::uint16_t kContainerVersion = 1;
constexpr std::uint16_t kKindExecutable = 2;
constexpr std::uint16_t kTargetId = 4;
constexpr std::uint16_t kAbiVersion = 1;

constexpr std::uint16_t kTypeProgbits = 1;
constexpr std::uint16_t kTypeNobits = 8;

constexpr std::uint16_t kFlagAlloc = 0x0001;
constexpr std::uint16_t kFlagWrite = 0x0002;
constexpr std::uint16_t kFlagExec = 0x0004;
constexpr std::uint16_t kFlagDebug = 0x0008;

// Allocated sections must stay out of the null page and the I/O and reserved windows.
constexpr std::uint32_t kFirstUsable = 0x0100;
constexpr std::uint32_t kLastUsable = 0xFF00;  // exclusive

std::uint16_t read_u16(const std::uint8_t* data, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t read_u32(const std::uint8_t* data, std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(data[offset + i]) << (8 * i);
    return value;
}

bool fail(std::string& error, const char* message) {
    error = message;
    return false;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::size_t zero_at) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t index = 0; index < size; ++index) {
        const std::uint8_t byte = index >= zero_at && index < zero_at + 4 ? 0 : data[index];
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

bool string_at(const std::uint8_t* data, std::uint32_t table_offset, std::uint32_t table_size,
               std::uint32_t name_offset, std::string& out) {
    if (name_offset >= table_size) return false;
    const std::uint8_t* start = data + table_offset + name_offset;
    const std::uint8_t* end = static_cast<const std::uint8_t*>(
        std::memchr(start, 0, table_size - name_offset));
    if (end == nullptr) return false;
    out.assign(reinterpret_cast<const char*>(start), static_cast<std::size_t>(end - start));
    return true;
}

}  // namespace

bool Image::parse(const std::uint8_t* data, std::size_t size, Image& out, std::string& error) {
    out = Image{};
    error.clear();
    if (data == nullptr) return fail(error, "image: no data");
    if (size < kHeaderSize) return fail(error, "image: smaller than the 64-byte header");
    if (std::memcmp(data, "K216", 4) != 0) return fail(error, "image: bad magic, expected K216");
    if (read_u16(data, 4) != kContainerVersion) return fail(error, "image: unsupported container version");
    if (read_u16(data, 6) != kKindExecutable) return fail(error, "image: not an executable");
    if (read_u32(data, 12) != kHeaderSize) return fail(error, "image: header size is not 64");
    if (read_u16(data, 18) != kAbiVersion) return fail(error, "image: unsupported ABI version");
    if (read_u16(data, 22) != kTargetId) return fail(error, "image: target ID is not 4 (kr216)");
    if (read_u32(data, 44) != size) return fail(error, "image: file size field does not match the file");
    const std::uint32_t file_crc = read_u32(data, 48);
    if (file_crc != 0 && file_crc != crc32(data, size, 48)) {
        return fail(error, "image: CRC32 does not match");
    }

    const std::uint16_t section_count = read_u16(data, 20);
    const std::uint32_t section_table = read_u32(data, 28);
    const std::uint32_t string_table = read_u32(data, 36);
    const std::uint32_t string_size = read_u32(data, 40);
    if (static_cast<std::uint64_t>(section_table) + static_cast<std::uint64_t>(section_count) * kSectionRecordSize > size) {
        return fail(error, "image: section table runs past the file");
    }
    if (static_cast<std::uint64_t>(string_table) + string_size > size) {
        return fail(error, "image: string table runs past the file");
    }

    out.entry = read_u16(data, 16);
    out.image_base = read_u32(data, 24);
    if (out.entry == 0) return fail(error, "image: entry is zero");

    for (std::uint16_t index = 0; index < section_count; ++index) {
        const std::size_t record = section_table + static_cast<std::size_t>(index) * kSectionRecordSize;
        ImageSection section;
        section.type = read_u16(data, record + 4);
        section.flags = read_u16(data, record + 6);
        section.address = read_u16(data, record + 8);
        section.offset = read_u32(data, record + 12);
        section.size = read_u32(data, record + 16);
        const std::uint16_t alignment = read_u16(data, record + 20);
        if (section.type != kTypeProgbits && section.type != kTypeNobits) {
            return fail(error, "image: section type must be progbits (1) or nobits (8)");
        }
        if (!string_at(data, string_table, string_size, read_u32(data, record), section.name)) {
            return fail(error, "image: section name is not in the string table");
        }
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return fail(error, "image: section alignment is not a power of two");
        }
        if (!section.nobits()) {
            if (static_cast<std::uint64_t>(section.offset) + section.size > size) {
                return fail(error, "image: section payload runs past the file");
            }
        }
        if (section.debug()) {
            out.debug_offset = section.offset;
            out.debug_size = section.size;
        }
        out.sections.push_back(std::move(section));
    }

    // Validate the allocated map: inside the usable window, no overlap, entry inside an
    // executable section.
    for (const ImageSection& section : out.sections) {
        if (!section.allocated()) continue;
        const std::uint32_t start = section.address;
        const std::uint32_t end = start + section.size;
        if (start < kFirstUsable || end > kLastUsable) {
            return fail(error, "image: allocated section is outside 0x0100-0xFEFF");
        }
        if (section.debug()) {
            return fail(error, "image: a debug section may not be allocated");
        }
    }
    for (std::size_t a = 0; a < out.sections.size(); ++a) {
        if (!out.sections[a].allocated()) continue;
        for (std::size_t b = a + 1; b < out.sections.size(); ++b) {
            if (!out.sections[b].allocated()) continue;
            const std::uint32_t a_start = out.sections[a].address;
            const std::uint32_t a_end = a_start + out.sections[a].size;
            const std::uint32_t b_start = out.sections[b].address;
            const std::uint32_t b_end = b_start + out.sections[b].size;
            if (a_start < b_end && b_start < a_end) {
                return fail(error, "image: allocated sections overlap");
            }
        }
    }
    bool entry_inside = false;
    for (const ImageSection& section : out.sections) {
        if (!section.allocated() || !section.executable() || section.nobits()) continue;
        if (out.entry >= section.address && out.entry < section.address + section.size) {
            entry_inside = true;
        }
    }
    if (!entry_inside) return fail(error, "image: entry is not inside an executable section");
    return true;
}

bool load(const Image& image, const std::uint8_t* data, Machine& machine,
          std::string& error) {
    error.clear();
    if (data == nullptr) return fail(error, "image: no data");
    for (const ImageSection& section : image.sections) {
        if (!section.allocated()) continue;
        if (section.nobits()) {
            for (std::uint32_t index = 0; index < section.size; ++index) {
                machine.write_memory(static_cast<std::uint16_t>(section.address + index), 0);
            }
            continue;
        }
        for (std::uint32_t index = 0; index < section.size; ++index) {
            machine.write_memory(static_cast<std::uint16_t>(section.address + index),
                                 data[section.offset + index]);
        }
    }
    machine.write_memory_word(0x0100, image.entry);
    machine.reset();
    return true;
}

}  // namespace kr216
