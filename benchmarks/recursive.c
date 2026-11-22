int fibonacci(int value) {
    if (value < 2) return value;
    return fibonacci(value - 1) + fibonacci(value - 2);
}
int main(void) {
    return fibonacci(10) == 55 ? 42 : 1;
}
