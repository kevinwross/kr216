#include <string>
#include <vector>
int main() {
    std::string text("benchmark");
    std::vector<int> values;
    int index = 0;
    while (index < 10) {
        values.push_back(index);
        text.append("x");
        index++;
    }
    return text.size() == 19 && values.size() == 10 ? 42 : 1;
}
