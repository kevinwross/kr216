int main(void) {
    int value = 3;
    double wide = (double)value;
    float narrow = (float)wide;
    if ((int)wide != 3) return 1;
    if ((int)narrow != 3) return 2;
    return 42;
}
