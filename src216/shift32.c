long shift(long value) { return value << 4; }
int main(void) { return shift(2) == 32 ? 42 : 1; }
