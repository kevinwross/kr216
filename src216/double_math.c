int main(void) {
    double a = 6.0;
    double b = 2.0;
    if ((int)(a + b) != 8) return 1;
    if ((int)(a - b) != 4) return 2;
    if ((int)(a * b) != 12) return 3;
    if ((int)(a / b) != 3) return 4;
    return 42;
}
