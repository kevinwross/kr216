long shift_left(long value, int count) { return value << count; }
long shift_right(long value, int count) { return value >> count; }
int main(void) {
    int count = 20;
    if (shift_left(1L, count) != 1048576L) return 1;
    if (shift_right(1048576L, 4) != 65536L) return 2;
    return 42;
}
