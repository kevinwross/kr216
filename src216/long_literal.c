int puts(const char *text);

long value = 32768;

int main(void) {
    long next = value + 1;
    if (next != 32769L) {
        return 1;
    }
    puts("long ok");
    return 42;
}
