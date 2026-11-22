int main(void) {
    long long a = 0x100000000LL;
    long long b = 0x100000001LL;
    long long expected_sum = 0x200000001LL;
    long long expected_difference = 1LL;
    long long sum = a + b;
    long long difference = b - a;
    return sum == expected_sum && difference == expected_difference ? 42 : 1;
}
