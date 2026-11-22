class Failure {
public:
    explicit Failure(int code) : code_(code) {}
    int code() const { return code_; }

private:
    int code_;
};

int outer_cleaned = 0;
int inner_cleaned = 0;

struct Guard {
    int* flag;
    explicit Guard(int* target) : flag(target) {}
    ~Guard() { *flag = 1; }
};

void inner(int* outer_flag, int* inner_flag) {
    Guard guard(inner_flag);
    throw Failure(42);
}

void middle(int* outer_flag, int* inner_flag) {
    Guard guard(outer_flag);
    inner(outer_flag, inner_flag);
}

int main() {
    try {
        middle(&outer_cleaned, &inner_cleaned);
    } catch (const Failure& failure) {
        if (inner_cleaned != 1) return 1;
        if (outer_cleaned != 1) return 2;
        return failure.code();
    }
    return 3;
}
