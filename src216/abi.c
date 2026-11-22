int puts(const char *text);

struct Pair {
    int a;
    int b;
};

int sum4(int a, int b, int c, int d) {
    return a + b + c + d;
}

int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

struct Pair make_pair(int a, int b) {
    struct Pair pair;
    pair.a = a;
    pair.b = b;
    return pair;
}

int main(void) {
    if (sum4(1, 2, 3, 4) != 10) {
        return 1;
    }
    if (sum6(1, 2, 3, 4, 5, 6) != 21) {
        return 2;
    }
    struct Pair pair = make_pair(7, 8);
    if (pair.a != 7 || pair.b != 8) {
        return 3;
    }
    if (sum4(1, 2, 3, 4) + sum6(1, 2, 3, 4, 5, 6) != 31) {
        return 4;
    }
    puts("abi ok");
    return 42;
}
