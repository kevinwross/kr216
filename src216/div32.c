long divide(long left, long right) { return left / right; }
long remainder(long left, long right) { return left % right; }
int main(void) {
    if (divide(120000L, 400L) != 300L) return 1;
    if (remainder(120001L, 400L) != 1L) return 2;
    return 42;
}
