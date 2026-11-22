int destroyed = 0;

struct Failure {
    int code;
    explicit Failure(int value) : code(value) {}
    ~Failure() { destroyed = 1; }
};

int main() {
    try {
        throw Failure(42);
    } catch (const Failure& failure) {
        if (failure.code != 42) return 1;
    }
    return destroyed == 1 ? 42 : 2;
}
