// Tests for the host text display device and its SYS 5 guest ABI (kr216-display.md).
//
// Every case drives the machine the way a guest would: the SYS instruction is loaded at
// 0x0200, R2/R3/R4 carry the command and its argument block, and the result is read back
// from R2. Device-level behaviour is checked through Machine::display() as well, because
// the screen is host state the guest never sees directly.

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>

#include "krc/display/display_abi.h"
#include "kr216/machine.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
}

constexpr std::uint16_t kBase = 0x0200;
constexpr std::uint16_t kBlock = 0x4000;
constexpr std::uint16_t kBuffer = 0x5000;
constexpr std::uint16_t kString = 0x4400;

struct Harness {
    kr216::Machine machine;

    explicit Harness(kr216::MachineOptions options = {}) : machine(options) {
        const std::uint8_t code[] = {0x73};  // SYS
        machine.load_image(code, 1, kBase);
        machine.state().r[15] = 0xFEF0;
    }

    std::int16_t call(std::uint16_t command, std::uint16_t block = 0,
                      std::uint16_t length = 0) {
        machine.state().r[14] = kBase;
        machine.state().r[7] = 5;
        machine.state().r[2] = command;
        machine.state().r[3] = block;
        machine.state().r[4] = length;
        machine.step();
        return static_cast<std::int16_t>(machine.state().r[2]);
    }

    void words(std::uint16_t address, std::initializer_list<std::uint16_t> values) {
        std::uint16_t offset = 0;
        for (const std::uint16_t value : values) {
            machine.write_memory_word(static_cast<std::uint16_t>(address + offset), value);
            offset = static_cast<std::uint16_t>(offset + 2);
        }
    }

    [[nodiscard]] std::uint16_t word(std::uint16_t address) const {
        return machine.read_memory_word(address);
    }

    void set_color_words(std::uint16_t address, std::uint32_t foreground,
                         std::uint32_t background) {
        words(address, {static_cast<std::uint16_t>(foreground & 0xFFFFu),
                        static_cast<std::uint16_t>(foreground >> 16),
                        static_cast<std::uint16_t>(background & 0xFFFFu),
                        static_cast<std::uint16_t>(background >> 16)});
    }

    void color_words(std::uint16_t address, std::uint32_t& foreground,
                     std::uint32_t& background) const {
        foreground = static_cast<std::uint32_t>(word(address)) |
                     (static_cast<std::uint32_t>(word(static_cast<std::uint16_t>(address + 2)))
                      << 16);
        background = static_cast<std::uint32_t>(word(static_cast<std::uint16_t>(address + 4))) |
                     (static_cast<std::uint32_t>(word(static_cast<std::uint16_t>(address + 6)))
                      << 16);
    }

    void arm_line() {
        words(kBlock, {kBuffer, 16});
    }

    std::int16_t read_line(std::uint16_t capacity = 16) {
        words(kBlock, {kBuffer, capacity});
        return call(static_cast<std::uint16_t>(krc::display::Command::ReadLine), kBlock, 4);
    }

    [[nodiscard]] std::uint16_t code(std::uint16_t row, std::uint16_t col) const {
        return machine.display().cell(row, col).code;
    }
};

void test_defaults() {
    Harness h;
    check(h.machine.display().rows() == 30, "default rows are 30");
    check(h.machine.display().cols() == 80, "default cols are 80");
    check(h.machine.display().foreground() == 0xFFFFFFFFu, "default foreground is white");
    check(h.machine.display().background() == 0xFF000000u, "default background is black");
    check(h.machine.display().cursor_row() == 0 && h.machine.display().cursor_col() == 0,
          "default cursor is home");
    check(h.machine.display().cursor_visible(), "default cursor is visible");
    check(h.code(0, 0) == 0 && h.code(29, 79) == 0, "default screen is blank");
    check(h.machine.display().cell(0, 0).foreground == 0xFFFFFFFFu,
          "blank cells carry the default foreground");
    check(h.machine.display().cell(0, 0).background == 0xFF000000u,
          "blank cells carry the default background");
}

void test_info() {
    Harness h;
    h.words(kBlock, {0, 0, 0, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Info), kBlock, 8) == 0,
          "INFO succeeds");
    check(h.word(kBlock) == krc::display::kAbiVersion, "INFO reports the ABI version");
    check(h.word(static_cast<std::uint16_t>(kBlock + 2)) == 30, "INFO reports rows");
    check(h.word(static_cast<std::uint16_t>(kBlock + 4)) == 80, "INFO reports cols");
    check(h.word(static_cast<std::uint16_t>(kBlock + 6)) == 0, "INFO reserved word is zero");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Info), kBlock, 6) == -1,
          "INFO rejects a short block");
}

void test_clear_and_colors() {
    Harness h;
    h.machine.display().put_string("hi");
    h.set_color_words(kBlock, 0x11223344u, 0x55667788u);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::SetColor), kBlock, 8) == 0,
          "SET_COLOR succeeds");
    check(h.machine.display().foreground() == 0x11223344u,
          "SET_COLOR stores the foreground verbatim");
    check(h.machine.display().background() == 0x55667788u,
          "SET_COLOR stores the background verbatim");

    h.words(kBlock, {0, 0, 0, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetColor), kBlock, 8) == 0,
          "GET_COLOR succeeds");
    std::uint32_t foreground = 0;
    std::uint32_t background = 0;
    h.color_words(kBlock, foreground, background);
    check(foreground == 0x11223344u, "GET_COLOR returns the foreground");
    check(background == 0x55667788u, "GET_COLOR returns the background");

    h.machine.display().set_cursor(3, 4);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Clear)) == 0,
          "CLEAR succeeds");
    check(h.code(0, 0) == 0 && h.code(29, 79) == 0, "CLEAR blanks every cell");
    check(h.machine.display().cell(12, 34).foreground == 0x11223344u,
          "CLEAR paints the current foreground");
    check(h.machine.display().cell(12, 34).background == 0x55667788u,
          "CLEAR paints the current background");
    check(h.machine.display().cursor_row() == 3 && h.machine.display().cursor_col() == 4,
          "CLEAR leaves the cursor alone");
}

void test_put_char() {
    Harness h;
    h.words(kBlock, {0x41});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR succeeds");
    check(h.code(0, 0) == 0x41, "PUT_CHAR stores the code");
    check(h.machine.display().cell(0, 0).foreground == 0xFFFFFFFFu,
          "PUT_CHAR uses the current foreground");
    check(h.machine.display().cursor_row() == 0 && h.machine.display().cursor_col() == 1,
          "PUT_CHAR advances the cursor");

    // Wrap at the last column.
    h.machine.display().set_cursor(0, 79);
    h.words(kBlock, {0x5A});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR at the last column succeeds");
    check(h.code(0, 79) == 0x5A, "PUT_CHAR writes the last column");
    check(h.machine.display().cursor_row() == 1 && h.machine.display().cursor_col() == 0,
          "PUT_CHAR wraps to the next row");

    // Control codes are actions, not cells.
    h.machine.display().set_cursor(2, 5);
    h.words(kBlock, {0x0A});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR newline succeeds");
    check(h.machine.display().cursor_row() == 3 && h.machine.display().cursor_col() == 0,
          "PUT_CHAR newline moves to the next row, column zero");
    check(h.code(2, 5) == 0, "PUT_CHAR newline is not stored");

    h.words(kBlock, {0x0D});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR carriage return succeeds");
    h.machine.display().set_cursor(4, 7);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR carriage return succeeds at a column");
    check(h.machine.display().cursor_row() == 4 && h.machine.display().cursor_col() == 0,
          "PUT_CHAR carriage return moves to column zero");

    h.words(kBlock, {0x08});
    h.machine.display().set_cursor(4, 3);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR backspace succeeds");
    check(h.machine.display().cursor_col() == 2, "PUT_CHAR backspace moves back one column");
}

void test_put_string() {
    Harness h;
    const char text[] = "ABCD";
    for (std::uint16_t offset = 0; offset < 4; ++offset) {
        h.machine.write_memory(static_cast<std::uint16_t>(kString + offset),
                               static_cast<std::uint8_t>(text[offset]));
    }
    h.machine.display().set_cursor(0, 78);
    h.words(kBlock, {kString, 4});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutString), kBlock, 4) == 0,
          "PUT_STRING succeeds");
    check(h.code(0, 78) == 'A' && h.code(0, 79) == 'B', "PUT_STRING fills the row");
    check(h.code(1, 0) == 'C' && h.code(1, 1) == 'D', "PUT_STRING wraps to the next row");
    check(h.machine.display().cursor_row() == 1 && h.machine.display().cursor_col() == 2,
          "PUT_STRING leaves the cursor after the last byte");

    // A newline inside the string acts like PUT_CHAR 0x0A.
    h.machine.write_memory(kString, static_cast<std::uint8_t>('X'));
    h.machine.write_memory(static_cast<std::uint16_t>(kString + 1), static_cast<std::uint8_t>(0x0A));
    h.machine.write_memory(static_cast<std::uint16_t>(kString + 2), static_cast<std::uint8_t>('Y'));
    h.machine.display().set_cursor(5, 0);
    h.words(kBlock, {kString, 3});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutString), kBlock, 4) == 0,
          "PUT_STRING with a newline succeeds");
    check(h.code(5, 0) == 'X', "PUT_STRING writes before the newline");
    check(h.code(6, 0) == 'Y', "PUT_STRING writes after the newline");

    // A bad guest string range is rejected without touching the screen or faulting.
    h.machine.display().set_cursor(0, 0);
    h.words(kBlock, {0, 4});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutString), kBlock, 4) == -1,
          "PUT_STRING rejects the null page");
    check(h.code(0, 0) == 0, "a rejected PUT_STRING writes no cell");
    h.words(kBlock, {static_cast<std::uint16_t>(0xFEFE), 4});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutString), kBlock, 4) == -1,
          "PUT_STRING rejects a range crossing into I/O");
}

void test_scroll() {
    Harness h;
    h.words(kBlock, {static_cast<std::uint16_t>('X')});
    h.machine.display().set_cursor(29, 0);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR writes the last row");
    h.words(kBlock, {0x0A});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR newline on the last row scrolls");
    check(h.code(28, 0) == 'X', "a scroll moves the last row up");
    check(h.code(29, 0) == 0, "a scroll blanks the bottom row");
    check(h.machine.display().cursor_row() == 29 && h.machine.display().cursor_col() == 0,
          "a scroll leaves the cursor on the last row");
}

void test_cursor() {
    Harness h;
    h.words(kBlock, {5, 7, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Cursor), kBlock, 6) == 0,
          "CURSOR succeeds");
    check(h.machine.display().cursor_row() == 5 && h.machine.display().cursor_col() == 7,
          "CURSOR sets the position");
    check(!h.machine.display().cursor_visible(), "CURSOR hides the cursor");

    h.words(kBlock, {5, 7, 1});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Cursor), kBlock, 6) == 0,
          "CURSOR shows the cursor");
    check(h.machine.display().cursor_visible(), "CURSOR makes the cursor visible");

    h.words(kBlock, {30, 0, 1});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Cursor), kBlock, 6) == -1,
          "CURSOR rejects a row past the screen");
    check(h.machine.display().cursor_row() == 5, "a rejected row leaves the cursor alone");
    h.words(kBlock, {0, 80, 1});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Cursor), kBlock, 6) == -1,
          "CURSOR rejects a column past the screen");
    h.words(kBlock, {0, 0, 2});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Cursor), kBlock, 6) == -1,
          "CURSOR rejects a visibility other than 0 or 1");
}

void test_bounds_and_unknown() {
    Harness h;
    h.words(kBlock, {0, 0, 0, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Info), 0, 8) == -1,
          "INFO rejects the null page");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Info),
                 static_cast<std::uint16_t>(0xFF00), 8) == -1,
          "INFO rejects the I/O window");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Info),
                 static_cast<std::uint16_t>(0xFEFF), 8) == -1,
          "INFO rejects a block crossing the I/O window");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Info), kBlock, 10) == -1,
          "INFO rejects a block of the wrong length");

    check(h.call(0x000A) == -3, "an unknown display command reports -3");
    check(h.machine.state().r[2] == 0xFFFD, "the unknown-command status is -3 in R2");

    // A no-block command ignores R3/R4.
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Clear), 0xDEAD, 0xBEEF) ==
              0,
          "CLEAR ignores the block registers");
}

void test_event_queues() {
    Harness h;
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetKey)) == -2,
          "GET_KEY reports an empty queue as -2");
    h.machine.display().push_key(0x41);
    h.machine.display().push_key(krc::display::kKeyLeft);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetKey)) == 0x41,
          "GET_KEY returns the first key");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetKey)) ==
              krc::display::kKeyLeft,
          "GET_KEY returns the extended key");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetKey)) == -2,
          "GET_KEY is empty again");

    h.words(kBlock, {0, 0, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == -2,
          "GET_CLICK reports an empty queue as -2");

    h.machine.display().push_click(3, 4, krc::display::kButtonLeft);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == 0,
          "GET_CLICK succeeds");
    check(h.word(kBlock) == 3, "GET_CLICK returns the row");
    check(h.word(static_cast<std::uint16_t>(kBlock + 2)) == 4, "GET_CLICK returns the column");
    check(h.word(static_cast<std::uint16_t>(kBlock + 4)) == krc::display::kButtonLeft,
          "GET_CLICK returns the button mask");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == -2,
          "GET_CLICK is empty again");

    h.machine.display().push_click(1, 2, krc::display::kButtonRight);
    h.machine.display().push_click(5, 6, krc::display::kButtonMiddle);
    h.words(kBlock, {0, 0, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == 0,
          "GET_CLICK returns the right-button event");
    check(h.word(static_cast<std::uint16_t>(kBlock + 4)) == krc::display::kButtonRight,
          "right button is bit 1");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == 0,
          "GET_CLICK returns the middle-button event");
    check(h.word(static_cast<std::uint16_t>(kBlock + 4)) == krc::display::kButtonMiddle,
          "middle button is bit 2");

    // A bad output pointer is reported without consuming the event.
    h.machine.display().push_click(7, 8, krc::display::kButtonLeft);
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), 0, 6) == -1,
          "GET_CLICK rejects a bad block");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == 0,
          "the rejected click is still queued");
}

void test_line_editing() {
    Harness h;
    check(h.read_line() == -2, "READ_LINE reports not-ready while the editor is armed");
    check(h.machine.display().editor_active(), "READ_LINE arms the editor");

    h.machine.display().push_text("ab");
    check(h.code(0, 0) == 'a' && h.code(0, 1) == 'b', "the editor echoes typed text");
    check(h.code(0, 2) == 0, "the editor echo is only as long as the line");
    check(h.machine.display().cursor_col() == 2, "the editor cursor follows the line");

    h.machine.display().push_key(krc::display::kKeyLeft);
    check(h.machine.display().cursor_col() == 1, "Left moves the editor cursor left");
    h.machine.display().push_text("X");
    check(h.code(0, 0) == 'a' && h.code(0, 1) == 'X' && h.code(0, 2) == 'b',
          "typing inserts at the editor cursor");
    h.machine.display().push_key(krc::display::kKeyBackspace);
    check(h.code(0, 0) == 'a' && h.code(0, 1) == 'b' && h.code(0, 2) == 0,
          "Backspace erases the character before the cursor");
    check(h.machine.display().cursor_col() == 1, "Backspace moves the cursor left");

    // The guest sees nothing until Enter.
    check(!h.machine.display().line_ready(), "an edited line is not ready before Enter");
    check(h.read_line() == -2, "polling READ_LINE keeps the editor and returns not-ready");
    check(h.code(0, 0) == 'a' && h.code(0, 1) == 'b', "polling does not disturb the echo");

    h.machine.display().push_text("\n");
    check(!h.machine.display().editor_active(), "Enter completes the line");
    check(h.machine.display().line_ready(), "the completed line is ready");
    check(h.machine.display().pending_line_length() == 2, "the completed line length is 2");

    h.machine.write_memory(static_cast<std::uint16_t>(kBuffer + 2), 0x7F);
    check(h.read_line() == 2, "READ_LINE returns the completed length");
    check(h.machine.read_memory(kBuffer) == 'a' && h.machine.read_memory(static_cast<std::uint16_t>(kBuffer + 1)) == 'b',
          "READ_LINE copies the completed line");
    check(h.machine.read_memory(static_cast<std::uint16_t>(kBuffer + 2)) == 0x7F,
          "READ_LINE writes no terminator or padding");
    check(!h.machine.display().line_ready(), "the line is consumed once");
    check(!h.machine.display().editor_active(), "the editor is inactive after a completed line");

    // Escape cancels: the echo disappears and no line is delivered.
    check(h.read_line() == -2, "READ_LINE arms a second editor");
    h.machine.display().push_text("zz");
    check(h.code(1, 0) == 'z' && h.code(1, 1) == 'z', "the second editor echoes at the cursor");
    h.machine.display().push_key(krc::display::kKeyEscape);
    check(!h.machine.display().editor_active(), "Escape cancels the editor");
    check(!h.machine.display().line_ready(), "Escape delivers no line");
    check(h.code(1, 0) == 0 && h.code(1, 1) == 0, "Escape removes the echo");

    // A cancelled editor can be armed again.
    check(h.read_line() == -2, "READ_LINE arms after a cancel");
    h.machine.display().push_text("\n");
    check(h.machine.display().line_ready(), "Enter completes an empty line");
    check(h.machine.display().pending_line_length() == 0, "the empty line has length 0");
    const std::uint16_t before = h.machine.read_memory(kBuffer);
    check(h.read_line() == 0, "READ_LINE returns 0 for an empty line");
    check(h.machine.read_memory(kBuffer) == before, "an empty line writes no bytes");
}

void test_line_editor_position_and_capacity() {
    Harness h;
    h.words(kBlock, {2, 5, 1});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::Cursor), kBlock, 6) == 0,
          "CURSOR positions the editor start");
    check(h.read_line() == -2, "READ_LINE arms at the cursor");
    h.machine.display().push_text("q");
    check(h.code(2, 5) == 'q', "the editor echoes at the arming cursor");
    check(h.machine.display().cursor_row() == 2 && h.machine.display().cursor_col() == 6,
          "the editor cursor advances from the arming cursor");

    // The capacity reserves one byte for the guest's terminator.
    Harness limited;
    check(limited.read_line(4) == -2, "READ_LINE arms with a four-byte capacity");
    limited.machine.display().push_text("abcd");
    check(limited.code(0, 2) == 'c' && limited.code(0, 3) == 0,
          "the editor stops one byte short of the capacity");
    limited.machine.display().push_text("\n");
    check(limited.read_line(4) == 3, "READ_LINE returns the capped length");
    check(limited.machine.read_memory(kBuffer) == 'a' &&
              limited.machine.read_memory(static_cast<std::uint16_t>(kBuffer + 1)) == 'b' &&
              limited.machine.read_memory(static_cast<std::uint16_t>(kBuffer + 2)) == 'c',
          "READ_LINE copies the capped line");

    // A completed line the caller's buffer cannot hold stays pending.
    Harness long_line;
    check(long_line.read_line(16) == -2, "READ_LINE arms a long line");
    long_line.machine.display().push_text("hello\n");
    check(long_line.machine.display().line_ready(), "the long line is ready");
    check(long_line.read_line(3) == -1, "READ_LINE rejects a buffer shorter than the line");
    check(long_line.machine.display().line_ready(), "the rejected line stays pending");
    check(long_line.read_line(16) == 5, "READ_LINE delivers the line to a large enough buffer");

    // A poll with a smaller capacity does not truncate what is already being edited.
    Harness shrinking;
    check(shrinking.read_line(16) == -2, "READ_LINE arms with a large capacity");
    shrinking.machine.display().push_text("abcdef");
    check(shrinking.read_line(3) == -2, "a smaller poll keeps the editor not ready");
    check(shrinking.code(0, 0) == 'a' && shrinking.code(0, 5) == 'f',
          "a smaller poll does not truncate the line");
    shrinking.machine.display().push_text("\n");
    check(shrinking.read_line(16) == 6, "the untruncated line is delivered");

    // A zero capacity is a bad argument, not a zero-length read.
    Harness zero;
    zero.words(kBlock, {kBuffer, 0});
    check(zero.call(static_cast<std::uint16_t>(krc::display::Command::ReadLine), kBlock, 4) ==
              -1,
          "READ_LINE rejects a zero capacity");
    zero.words(kBlock, {0, 16});
    check(zero.call(static_cast<std::uint16_t>(krc::display::Command::ReadLine), kBlock, 4) ==
              -1,
          "READ_LINE rejects a buffer in the null page");
}

void test_reset() {
    kr216::MachineOptions options;
    options.display_rows = 10;
    options.display_cols = 20;
    options.display_foreground = 0x00112233u;
    options.display_background = 0x00445566u;
    Harness h(options);
    check(h.machine.display().rows() == 10 && h.machine.display().cols() == 20,
          "MachineOptions sets the display geometry");
    check(h.machine.display().foreground() == 0x00112233u &&
              h.machine.display().background() == 0x00445566u,
          "MachineOptions sets the default colours");

    h.machine.display().put_string("abc");
    h.machine.display().push_key(0x41);
    h.machine.display().push_click(1, 2, krc::display::kButtonLeft);
    check(h.read_line() == -2, "READ_LINE arms before reset");
    h.machine.display().push_text("draft");

    h.machine.reset();
    check(h.machine.display().rows() == 10 && h.machine.display().cols() == 20,
          "reset keeps the construction geometry");
    check(h.machine.display().foreground() == 0x00112233u &&
              h.machine.display().background() == 0x00445566u,
          "reset restores the default colours");
    check(h.code(0, 0) == 0 && h.code(9, 19) == 0, "reset clears the screen");
    check(h.machine.display().cell(4, 4).foreground == 0x00112233u &&
              h.machine.display().cell(4, 4).background == 0x00445566u,
          "reset repaints the clear with the default colours");
    check(h.machine.display().cursor_row() == 0 && h.machine.display().cursor_col() == 0 &&
              h.machine.display().cursor_visible(),
          "reset homes and shows the cursor");
    check(!h.machine.display().editor_active(), "reset drops the line editor");
    check(!h.machine.display().line_ready(), "reset drops a completed line");
    h.words(kBlock, {0, 0, 0});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetKey)) == -2,
          "reset drops queued keys");
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::GetClick), kBlock, 6) == -2,
          "reset drops queued clicks");
    check(h.read_line() == -2, "READ_LINE arms again after reset");
}

void test_display_input_refill() {
    Harness h;
    const std::string script = "ab\n";
    std::size_t position = 0;
    h.machine.set_display_input_refill([&] {
        if (h.machine.display().line_ready()) {
            return;
        }
        if (position < script.size()) {
            if (!h.machine.display().editor_active()) {
                return;  // the poll that arms the editor has not run yet
            }
            const std::size_t end = script.find('\n', position);
            h.machine.display().push_text(script.substr(position, end + 1 - position));
            position = end + 1;
            return;
        }
        h.machine.display().close_input();
    });
    check(h.read_line(8) == -2, "READ_LINE arms the editor before the refill has text");
    check(h.read_line(8) == 2, "the refill feeds the armed editor");
    check(h.machine.read_memory(kBuffer) == 'a' &&
              h.machine.read_memory(static_cast<std::uint16_t>(kBuffer + 1)) == 'b',
          "the refilled line reaches the guest buffer");
    check(h.read_line(8) == krc::display::kStatusEndOfInput,
          "READ_LINE reports end of input once the script is exhausted");

    Harness keyed;
    keyed.machine.set_display_input_refill([&] { keyed.machine.display().close_input(); });
    check(keyed.call(static_cast<std::uint16_t>(krc::display::Command::GetKey)) ==
              krc::display::kStatusEndOfInput,
          "GET_KEY reports end of input when the host closed the display input");
}

void test_console_remains_the_debug_serial() {
    Harness h;
    const char text[] = "dbg";
    for (std::uint16_t offset = 0; offset < 3; ++offset) {
        h.machine.write_memory(static_cast<std::uint16_t>(kString + offset),
                               static_cast<std::uint8_t>(text[offset]));
    }
    h.machine.state().r[14] = kBase;
    h.machine.state().r[7] = 1;  // SYS 1: write(fd, address, length)
    h.machine.state().r[3] = kString;
    h.machine.state().r[4] = 3;
    check(h.machine.step() == kr216::StopReason::Running, "SYS write steps");
    check(h.machine.console_output() == "dbg", "SYS write reaches the console");
    check(h.code(0, 0) == 0, "a console write does not touch the display");

    h.words(kBlock, {0x41});
    check(h.call(static_cast<std::uint16_t>(krc::display::Command::PutChar), kBlock, 2) == 0,
          "PUT_CHAR succeeds beside the console");
    check(h.code(0, 0) == 0x41, "a display write reaches the display");
    check(h.machine.console_output() == "dbg", "a display write does not touch the console");
}

}  // namespace

int main() {
    test_defaults();
    test_info();
    test_clear_and_colors();
    test_put_char();
    test_put_string();
    test_scroll();
    test_cursor();
    test_bounds_and_unknown();
    test_event_queues();
    test_line_editing();
    test_line_editor_position_and_capacity();
    test_reset();
    test_display_input_refill();
    test_console_remains_the_debug_serial();

    if (failures != 0) {
        std::fprintf(stderr, "display_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("display_test: all display cases passed\n");
    return 0;
}
