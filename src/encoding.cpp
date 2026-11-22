#include "kr216/encoding.h"

namespace kr216 {
namespace {

// The ranges are the first-byte map in kr216-isa.md section 3.1, in byte order, with the
// gaps closed by the classes that do not own a contiguous run.
constexpr bool in_range(std::uint8_t value, std::uint8_t low, std::uint8_t high) noexcept {
    return value >= low && value <= high;
}

}  // namespace

LengthClass classify(std::uint8_t first) noexcept {
    if (in_range(first, 0x00, 0x4F)) return LengthClass::Short;
    if (in_range(first, 0x50, 0x6F)) return LengthClass::Register;
    if (in_range(first, 0x70, 0x7F)) return LengthClass::Short;
    if (in_range(first, 0x80, 0xBF)) return LengthClass::Compact;
    if (in_range(first, 0xC0, 0xDF)) return LengthClass::Normal;
    if (in_range(first, 0xE0, 0xEF)) return LengthClass::Register;

    switch (first) {
    case 0xF0: return LengthClass::Generic;
    case 0xF1: return LengthClass::LongInteger;
    case 0xF2: return LengthClass::Reserved;
    case 0xF3: return LengthClass::FpRegister;
    case 0xF4: return LengthClass::FpLoadStore;
    case 0xF5: return LengthClass::FpConvert;
    case 0xF6: return LengthClass::FpStack;
    case 0xF7: return LengthClass::EnterShort;
    case 0xF8: return LengthClass::EnterLong;
    default: return LengthClass::Reserved;  // 0xF9-0xFF
    }
}

std::uint8_t minimum_length(LengthClass cls) noexcept {
    switch (cls) {
    case LengthClass::Short: return 1;
    case LengthClass::Register: return 3;
    case LengthClass::Compact: return 2;
    case LengthClass::Normal: return 3;
    case LengthClass::Generic: return 4;
    case LengthClass::LongInteger: return 4;
    case LengthClass::FpRegister: return 2;
    case LengthClass::FpLoadStore: return 3;
    case LengthClass::FpConvert: return 3;
    case LengthClass::FpStack: return 2;
    case LengthClass::EnterShort: return 4;
    case LengthClass::EnterLong: return 5;
    case LengthClass::Reserved: return 0;
    }
    return 0;
}

bool is_reserved(LengthClass cls) noexcept { return cls == LengthClass::Reserved; }

}  // namespace kr216
