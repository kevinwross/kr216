struct Pair {
    int a;
    int b;
};

struct Pair bump(struct Pair pair) {
    pair.a = pair.a + 1;
    return pair;
}

int main(void) {
    struct Pair pair;
    pair.a = 1;
    pair.b = 2;
    pair = bump(pair);
    return pair.a == 2 && pair.b == 2 ? 42 : 1;
}
