int main(void) {
    long long value = 0x100000000LL;
    long long expected = 0x1000000000LL;
    long long shifted = value << 4;
    if (shifted != expected) return 1;
    if ((shifted >> 4) != value) return 2;
    long long negative = 0 - 16;
    long long expected_negative = 0 - 4;
    if ((negative >> 2) != expected_negative) return 3;
    return 42;
}
