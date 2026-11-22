int main(void) {
    long long value = 0x100000000LL;
    long long three = 3LL;
    long long product = 0x300000000LL;
    long long negative = 0 - product;
    long long expected = 0 - value;
    if ((negative / three) != expected) return 1;
    if ((negative % three) != 0LL) return 2;
    return 42;
}
