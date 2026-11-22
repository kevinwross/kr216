#include <string>
int main() {
    std::string first("abc");
    std::string second("abc");
    std::string third("abd");
    if (!(first == second)) return 1;
    if (first != second) return 2;
    if (!(first != third)) return 3;
    std::string joined = first + third;
    if (joined.size() != 6) return 5;
    return 42;
}
