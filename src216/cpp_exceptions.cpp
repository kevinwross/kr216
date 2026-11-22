class Error { public: int code; explicit Error(int c) : code(c) {} };
int cleaned = 0;
struct Guard { ~Guard() { cleaned = 1; } };
int main() {
    try {
        Guard guard;
        throw Error(42);
    } catch (const Error& error) {
        return cleaned == 1 ? error.code : 1;
    }
    return 2;
}
