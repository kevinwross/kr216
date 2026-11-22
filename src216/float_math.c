int main(void) {
    float a = 6.0f;
    float b = 2.0f;
    if ((int)(a + b) != 8) return 1;
    if ((int)(a - b) != 4) return 2;
    if ((int)(a * b) != 12) return 3;
    if ((int)(a / b) != 3) return 4;
    return 42;
}
