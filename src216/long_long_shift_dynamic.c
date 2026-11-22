int main(void) {
    long long value = 0x100000000LL;
    int count = 4;
    long long shifted = value << count;
    long long expected = 0x1000000000LL;
    if (shifted != expected) return 1;
    if ((shifted >> count) != value) return 2;
    long long negative = 0 - 16;
    int two = 2;
    long long expected_negative = 0 - 4;
    if ((negative >> two) != expected_negative) return 3;
    return 42;
}
