int main(void) {
    long long value = 0x100000000LL;
    long long three = 3LL;
    long long expected = 0x300000000LL;
    long long product = value * three;
    return product == expected ? 42 : 1;
}
