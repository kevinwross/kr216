int printf(const char *format, ...);

int main(void) {
    printf("%d %s %c %x\n", 42, "ok", 'A', 255);
    return 42;
}
