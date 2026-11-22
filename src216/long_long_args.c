long long add64(long long left, long long right) { return left + right; }
int main(void) {
    long long left = 0x100000000LL;
    long long right = 0x100000001LL;
    long long expected = 0x200000001LL;
    return add64(left, right) == expected ? 42 : 1;
}
