#include <stdio.h>

class Failure {
public:
    explicit Failure(int code) : code_(code) {}
    int code() const { return code_; }

private:
    int code_;
};

int cleaned = 0;

struct Guard {
    ~Guard() { cleaned = 1; }
};

int main() {
    try {
        Guard guard;
        throw Failure(42);
    } catch (const Failure& failure) {
        printf("caught %d, cleaned=%d\n", failure.code(), cleaned);
        return cleaned != 0 ? failure.code() : 1;
    }
    return 2;
}
