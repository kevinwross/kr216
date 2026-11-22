#include <stdlib.h>
#include <string.h>

int puts(const char *text);

int main(void) {
    char *text = (char *)malloc(8);
    if (text == 0) {
        return 1;
    }
    strcpy(text, "ok");
    if (strcmp(text, "ok") != 0) {
        return 2;
    }
    if (strlen(text) != 2) {
        return 3;
    }
    memset(text, 'a', 2);
    text[2] = 0;
    puts(text);
    free(text);
    return 42;
}
