int main(void) {
    double value = 0.0;
    int index = 0;
    while (index < 100) {
        value = value + 0.5;
        index++;
    }
    return value == 50.0 ? 42 : 1;
}
