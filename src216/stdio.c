/*
 * Guest evidence for stdio on the text display (kr216-display.md section 9): printf writes the
 * screen, getchar/getc read host-edited lines from the display line editor, a getchar loop that
 * ends at end of input terminates because the host closes the display input source, and the
 * console device stays a separate debug serial stream.
 *
 * The host side of this test supplies three input lines ("ada", "b", "z") and then closes the
 * input, so the tail loop observes DISPLAY_END_OF_INPUT as getchar's -1. The screen assertions
 * live in the CTest row: it checks the printf output and the host's echo, and that the debug
 * text is absent from the screen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The debug serial port: the console device, not stdin/stdout (kr216-isa.md section 14). */
extern int __krc_debug_write(const char *buffer, int count);

static int read_line(char *buffer, int capacity) {
    int count = 0;
    int c = getchar();
    if (c < 0) {
        buffer[0] = 0;
        return 0;
    }
    while (c >= 0 && c != '\n') {
        if (count + 1 < capacity) {
            buffer[count++] = (char)c;
        }
        c = getchar();
    }
    buffer[count] = 0;
    return 1;
}

int main(void) {
    char name[16];
    char second[16];
    int c;
    int index;

    /* A fixed seed makes rand() deterministic across targets: 1103515245 * 1 + 12345. */
    srand(1);

    printf("name? ");
    if (!read_line(name, (int)sizeof name)) return 1;
    printf("hello %s\n", name);

    printf("rand %d\n", rand());

    printf("second? ");
    if (!read_line(second, (int)sizeof second)) return 2;
    printf("got %s (%d)\n", second, (int)strlen(second));

    /* getc over stdin takes the same display line-editor path. */
    c = getc(stdin);
    if (c < 0) return 3;
    printf("extra %c\n", (char)c);

    __krc_debug_write("debug serial ok\n", 16);

    printf("tail: ");
    for (;;) {
        c = getchar();
        if (c < 0) {
            break;
        }
        if (c != '\n') {
            putchar(c);
        }
    }
    printf("eof\n");

    puts("bye");
    for (index = 0; index < 10; ++index) {
        putchar("putchar ok"[index]);
    }
    putchar('\n');
    return 42;
}
