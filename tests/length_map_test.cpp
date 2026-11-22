// The first-byte length map, pinned against kr216-isa.md section 3.1 and Appendix B.
//
// A decoder that misclassifies a byte either fetches the wrong number of bytes or fails to
// trap a reserved encoding, and neither failure is visible until a program runs. This test
// walks all 256 first bytes and checks the classes, the counts and the minimum lengths.

#include <cstdint>
#include <cstdio>

#include "kr216/encoding.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what, unsigned byte, int expected, int actual) {
    if (!ok) {
        std::fprintf(stderr, "FAIL byte 0x%02X: %s (expected %d, got %d)\n", byte, what, expected,
                     actual);
        ++failures;
    }
}

using kr216::LengthClass;

struct Range {
    std::uint8_t low;
    std::uint8_t high;
    LengthClass cls;
};

// The map as the ISA document states it, in byte order. The table is exhaustive and
// disjoint; `classify` must agree with it byte for byte.
constexpr Range kRanges[] = {
    {0x00, 0x4F, LengthClass::Short},
    {0x50, 0x6F, LengthClass::Register},
    {0x70, 0x7F, LengthClass::Short},
    {0x80, 0xBF, LengthClass::Compact},
    {0xC0, 0xDF, LengthClass::Normal},
    {0xE0, 0xEF, LengthClass::Register},
    {0xF0, 0xF0, LengthClass::Generic},
    {0xF1, 0xF1, LengthClass::LongInteger},
    {0xF2, 0xF2, LengthClass::Reserved},
    {0xF3, 0xF3, LengthClass::FpRegister},
    {0xF4, 0xF4, LengthClass::FpLoadStore},
    {0xF5, 0xF5, LengthClass::FpConvert},
    {0xF6, 0xF6, LengthClass::FpStack},
    {0xF7, 0xF7, LengthClass::EnterShort},
    {0xF8, 0xF8, LengthClass::EnterLong},
    {0xF9, 0xFF, LengthClass::Reserved},
};

constexpr int kClassCount = static_cast<int>(LengthClass::Reserved) + 1;

// Fixed length per class, or the minimum for the two variable-length prefixes.
constexpr std::uint8_t kExpectedMinimum[kClassCount] = {
    1,  // Short
    3,  // Register
    2,  // Compact
    3,  // Normal
    4,  // Generic, 4-6
    4,  // LongInteger
    2,  // FpRegister
    3,  // FpLoadStore, 3-4
    3,  // FpConvert
    2,  // FpStack
    4,  // EnterShort
    5,  // EnterLong
    0,  // Reserved
};

}  // namespace

int main() {
    int counts[kClassCount] = {};

    for (int byte = 0; byte < 256; ++byte) {
        const Range* match = nullptr;
        for (const Range& range : kRanges) {
            if (byte >= range.low && byte <= range.high) {
                match = &range;
                break;
            }
        }
        if (match == nullptr) {
            std::fprintf(stderr, "FAIL byte 0x%02X: the expected table does not cover it\n", byte);
            ++failures;
            continue;
        }
        const LengthClass actual = kr216::classify(static_cast<std::uint8_t>(byte));
        expect(actual == match->cls, "class", static_cast<unsigned>(byte),
               static_cast<int>(match->cls), static_cast<int>(actual));
        ++counts[static_cast<int>(actual)];
    }

    // Counts pin the table's shape: two Short runs, two Register runs, one each of the rest.
    const int expected_counts[kClassCount] = {96, 48, 64, 32, 1, 1, 1, 1, 1, 1, 1, 1, 8};
    int total = 0;
    for (int cls = 0; cls < kClassCount; ++cls) {
        if (counts[cls] != expected_counts[cls]) {
            std::fprintf(stderr, "FAIL class %d: %d bytes, expected %d\n", cls, counts[cls],
                         expected_counts[cls]);
            ++failures;
        }
        total += counts[cls];
    }
    if (total != 256) {
        std::fprintf(stderr, "FAIL the map covers %d bytes, expected 256\n", total);
        ++failures;
    }

    for (int cls = 0; cls < kClassCount; ++cls) {
        const auto actual = kr216::minimum_length(static_cast<LengthClass>(cls));
        if (actual != kExpectedMinimum[cls]) {
            std::fprintf(stderr, "FAIL class %d: minimum length %u, expected %u\n", cls,
                         static_cast<unsigned>(actual),
                         static_cast<unsigned>(kExpectedMinimum[cls]));
            ++failures;
        }
    }

    // Reserved encodings are exactly 0xF2 and 0xF9-0xFF; everything else has an instruction.
    for (int byte = 0; byte < 256; ++byte) {
        const bool reserved = kr216::is_reserved(kr216::classify(static_cast<std::uint8_t>(byte)));
        const bool expected_reserved = byte == 0xF2 || byte >= 0xF9;
        expect(reserved == expected_reserved, "reserved", static_cast<unsigned>(byte),
               static_cast<int>(expected_reserved), static_cast<int>(reserved));
    }

    if (failures != 0) {
        std::fprintf(stderr, "length_map_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("length_map_test: 256 first bytes classified, %d classes\n", kClassCount);
    return 0;
}
