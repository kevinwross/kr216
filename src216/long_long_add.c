int main(void) {
    long long a = 0x100000000LL;
    long long b = 0x100000001LL;
    long long sum = a + b;
    return sum == 0x200000001LL ? 42 : 1;
}
