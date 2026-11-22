long divide(long left, long right) { return left / right; }
long remainder(long left, long right) { return left % right; }
int main(void) {
    long expected = 0 - 300;
    long expected_remainder = 0 - 1;
    if (divide(0 - 120000, 400) != expected) return 1;
    if (remainder(0 - 120001, 400) != expected_remainder) return 2;
    return 42;
}
