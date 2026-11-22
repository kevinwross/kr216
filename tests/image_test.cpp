// The K216 image container (ADR 0054) and the loader that turns it into a runnable machine.
//
// The builder writes the bytes the compiler's image writer will emit, so these tests pin the
// format as well as the loader: a field the writer would get wrong fails here.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "kr216/image.h"
#include "kr216/machine.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}

void put_u16(std::vector<std::uint8_t>& data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
}

std::uint32_t crc32(const std::vector<std::uint8_t>& data, std::size_t zero_at) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t index = 0; index < data.size(); ++index) {
        const std::uint8_t byte = index >= zero_at && index < zero_at + 4 ? 0 : data[index];
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

struct Builder {
    std::vector<std::uint8_t> data;
    std::uint32_t text_offset = 0;
    std::uint32_t debug_offset = 0;
    std::uint16_t text_address = 0x0200;
    std::uint16_t bss_address = 0x4100;
    std::uint32_t bss_size = 0;

    explicit Builder(std::vector<std::uint8_t> text, std::uint32_t bss = 0, bool debug = true)
        : bss_size(bss) {
        const std::string strings("\0.text\0.bss\0.kdbg", 18);  // trailing NUL is part of the table
        const std::uint32_t section_table = 64;
        const std::uint32_t string_offset = static_cast<std::uint32_t>(64 + 3 * 32);
        const std::uint32_t payload = static_cast<std::uint32_t>((string_offset + strings.size() + 1) & ~1u);
        text_offset = payload;
        debug_offset = (payload + static_cast<std::uint32_t>(text.size()) + 1) & ~1u;
        data.assign(debug_offset + 4, 0);

        // Header.
        data[0] = 'K';
        data[1] = '2';
        data[2] = '1';
        data[3] = '6';
        put_u16(data, 4, 1);               // container version
        put_u16(data, 6, 2);               // executable
        put_u32(data, 8, debug ? 1u : 0u); // flags: debug payload present
        put_u32(data, 12, 64);             // header size
        put_u16(data, 16, text_address);   // entry
        put_u16(data, 18, 1);              // ABI version
        put_u16(data, 20, 3);              // section count
        put_u16(data, 22, 4);              // target ID
        put_u32(data, 24, text_address);   // image base
        put_u32(data, 28, section_table);  // section table offset
        put_u32(data, 32, 0);
        put_u32(data, 36, string_offset);  // string table offset
        put_u32(data, 40, static_cast<std::uint32_t>(strings.size()));
        put_u32(data, 44, static_cast<std::uint32_t>(data.size()));
        put_u32(data, 48, 0);              // CRC, filled last

        // Section table.
        auto section = [this](std::size_t index, std::uint32_t name, std::uint16_t type,
                              std::uint16_t flags, std::uint16_t address, std::uint32_t offset,
                              std::uint32_t size) {
            const std::size_t at = 64 + index * 32;
            put_u32(data, at, name);
            put_u16(data, at + 4, type);
            put_u16(data, at + 6, flags);
            put_u16(data, at + 8, address);
            put_u16(data, at + 10, 0);
            put_u32(data, at + 12, offset);
            put_u32(data, at + 16, size);
            put_u16(data, at + 20, 2);
        };
        section(0, 1, 1, 0x0001 | 0x0004, text_address, text_offset,
                static_cast<std::uint32_t>(text.size()));
        section(1, 7, 8, 0x0001 | 0x0002, bss_address, 0, bss_size);
        section(2, 12, 1, 0x0008, 0, debug_offset, 4);

        // String table (starts with a NUL, then NUL-terminated names).
        std::copy_n(strings.data(), strings.size(), data.data() + string_offset);
        std::copy_n(text.data(), text.size(), data.data() + text_offset);
        std::fill_n(data.data() + debug_offset, 4, std::uint8_t{0xA5});
        put_u32(data, 48, crc32(data, 48));
    }
};

bool parse(const std::vector<std::uint8_t>& bytes, kr216::Image& image, std::string& error) {
    return kr216::Image::parse(bytes.data(), bytes.size(), image, error);
}

const std::vector<std::uint8_t> kExit42 = {0x52, 0x2A, 0x00,  // ld r2, #42
                                           0x57, 0x00, 0x00,  // ld r7, #0
                                           0x73};             // sys (exit)

void test_valid_image_runs() {
    Builder builder(kExit42);
    kr216::Image image;
    std::string error;
    check(parse(builder.data, image, error), "valid image parses");
    check(image.entry == 0x0200, "entry is the section address");
    check(image.sections.size() == 3, "three sections");
    check(image.debug_size == 4, "debug payload reported");
    check(image.sections[0].executable(), "text is executable");

    kr216::Machine machine;
    check(kr216::load(image, builder.data.data(), machine, error),
          "valid image loads");
    check(machine.state().r[14] == 0x0200, "reset vector points at the entry");
    check(machine.state().r[15] == 0xFEF0, "reset sets SP");
    machine.run(10);
    check(machine.reason() == kr216::StopReason::Exited, "image runs to exit");
    check(machine.exit_code() == 42, "image exits with 42");
}

void test_bss_is_zeroed() {
    Builder builder(kExit42, 8);
    kr216::Image image;
    std::string error;
    check(parse(builder.data, image, error), "image with bss parses");
    kr216::Machine machine;
    for (int offset = 0; offset < 8; ++offset) machine.write_memory(static_cast<std::uint16_t>(0x4100 + offset), 0xFF);
    check(kr216::load(image, builder.data.data(), machine, error),
          "image with bss loads");
    bool zeroed = true;
    for (int offset = 0; offset < 8; ++offset) zeroed = zeroed && machine.read_memory(static_cast<std::uint16_t>(0x4100 + offset)) == 0;
    check(zeroed, "nobits section is zeroed");
}

void test_rejections() {
    std::string error;

    Builder magic(kExit42);
    magic.data[0] = 'X';
    kr216::Image image;
    check(!parse(magic.data, image, error) && error.find("magic") != std::string::npos,
          "bad magic is rejected");

    Builder target(kExit42);
    put_u16(target.data, 22, 3);
    check(!parse(target.data, image, error) && error.find("target") != std::string::npos,
          "a foreign target ID is rejected");

    Builder abi(kExit42);
    put_u16(abi.data, 18, 9);
    check(!parse(abi.data, image, error) && error.find("ABI") != std::string::npos,
          "a foreign ABI version is rejected");

    Builder entry(kExit42);
    put_u16(entry.data, 16, 0x5000);
    put_u32(entry.data, 48, 0);  // the mutation invalidates the CRC; test the entry check
    check(!parse(entry.data, image, error) && error.find("entry") != std::string::npos,
          "an entry outside an executable section is rejected");

    Builder crc(kExit42);
    put_u32(crc.data, 48, 0x12345678);
    check(!parse(crc.data, image, error) && error.find("CRC") != std::string::npos,
          "a bad CRC is rejected");

    Builder overlap(kExit42);
    put_u16(overlap.data, 64 + 32 + 8, 0x0202);  // move .bss onto .text
    put_u32(overlap.data, 48, 0);
    check(!parse(overlap.data, image, error) && error.find("overlap") != std::string::npos,
          "overlapping allocated sections are rejected");

    Builder truncated(kExit42);
    check(!kr216::Image::parse(truncated.data.data(), truncated.data.size() - 1, image, error) &&
              error.find("size") != std::string::npos,
          "a truncated file is rejected");
}

}  // namespace

int main() {
    test_valid_image_runs();
    test_bss_is_zeroed();
    test_rejections();

    if (failures != 0) {
        std::fprintf(stderr, "image_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("image_test: container and loader cases passed\n");
    return 0;
}
