long multiply(long left, long right) { return left * right; }
int main(void) {
    long expected = 0 - 15;
    return multiply(0 - 3, 5) == expected ? 42 : 1;
}
