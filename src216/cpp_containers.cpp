#include <string>
#include <vector>
int main() {
    std::string text("hello");
    text.append(" world");
    std::vector<int> values;
    values.push_back(3);
    values.push_back(4);
    if (text.size() != 11) return 1;
    if (values.size() != 2) return 2;
    if (values[0] + values[1] != 7) return 3;
    return 42;
}
