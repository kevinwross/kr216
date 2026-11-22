float sum3f(float a, float b, float c) { return a + b + c; }
double sum3(double a, double b, double c) { return a + b + c; }
int main(void) {
    if (sum3f(1.0f, 2.0f, 3.0f) != 6.0f) return 1;
    if (sum3(1.0, 2.0, 3.0) != 6.0) return 2;
    return 42;
}
