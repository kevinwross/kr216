long add(long a, long b) {
    return a + b;
}

int main(void) {
    long result = add(32768, 1);
    return result == 32769L ? 42 : 1;
}
