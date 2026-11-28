/*
 * Guest evidence for the SYS 5 text display: every runtime wrapper is called and the statuses
 * are checked against kr216-display.md. The program prints "display ok" and returns 42 when the
 * host answers as documented. Screen contents are host state the guest cannot read, so the
 * cells themselves are asserted by the host's display_test; this row proves the guest-side
 * marshalling, the wrapper ABI and the emulator's SYS 5 dispatch end to end.
 */

#include <stdio.h>

#include <krc/display.h>

/* The raw hypercall, so the ABI statuses are exercised without a wrapper in between. */
extern int __krc_display(int command, const void *block, unsigned int length);

struct display_info_words {
    unsigned int abi_version;
    unsigned int rows;
    unsigned int cols;
    unsigned int reserved;
};

struct display_cursor_words {
    unsigned int row;
    unsigned int column;
    unsigned int visible;
};

int main(void) {
    struct display_info_words info;
    uint32_t foreground = 0;
    uint32_t background = 0;
    char line[8];
    int row = 0;
    int column = 0;
    int buttons = 0;

    if (display_clear() != DISPLAY_OK) return 1;
    if (display_rows() != 30) return 2;
    if (display_cols() != 80) return 3;

    if (__krc_display(DISPLAY_CMD_INFO, &info, (unsigned int)sizeof info) !=
        DISPLAY_OK) {
        return 4;
    }
    if (info.abi_version != DISPLAY_ABI_VERSION || info.rows != 30 || info.cols != 80 ||
        info.reserved != 0) {
        return 5;
    }

    if (display_set_color(0x11223344UL, 0x55667788UL) != DISPLAY_OK) return 6;
    if (display_get_color(&foreground, &background) != DISPLAY_OK) return 7;
    if (foreground != 0x11223344UL || background != 0x55667788UL) return 8;

    if (display_putc('A') != DISPLAY_OK) return 9;
    if (display_puts("hello") != DISPLAY_OK) return 10;
    if (display_cursor(1, 2, 1) != DISPLAY_OK) return 11;

    /* Out-of-range cursor arguments are rejected, not clamped or trapped. */
    if (display_cursor(30, 0, 1) != DISPLAY_BAD_ARGUMENT) return 12;
    if (display_cursor(0, 80, 1) != DISPLAY_BAD_ARGUMENT) return 13;
    {
        struct display_cursor_words cursor;
        cursor.row = 0;
        cursor.column = 0;
        cursor.visible = 2;
        if (__krc_display(DISPLAY_CMD_CURSOR, &cursor, (unsigned int)sizeof cursor) !=
            DISPLAY_BAD_ARGUMENT) {
            return 14;
        }
    }

    /* Polled input is empty on this run; the host line editor is armed by the first call. */
    if (display_get_key() != DISPLAY_NOT_READY) return 15;
    if (display_get_click(&row, &column, &buttons) != DISPLAY_NOT_READY) return 16;
    if (display_read_line(line, (int)sizeof line) != DISPLAY_NOT_READY) return 17;
    if (display_read_line(line, 0) != DISPLAY_BAD_ARGUMENT) return 18;

    /* Unknown commands and bad blocks are statuses, not traps. */
    if (__krc_display(10, 0, 0) != DISPLAY_UNKNOWN_COMMAND) return 19;
    if (__krc_display(DISPLAY_CMD_INFO, 0, 8) != DISPLAY_BAD_ARGUMENT) return 20;
    if (__krc_display(DISPLAY_CMD_INFO, &info, 6) != DISPLAY_BAD_ARGUMENT) return 21;

    if (display_clear() != DISPLAY_OK) return 22;

    puts("display ok");
    return 42;
}
