#include <stdio.h>
#include <string>
#include <vector>

int main() {
    std::string text("kr216");
    std::vector<int> values;
    for (int index = 0; index < 4; ++index) {
        values.push_back(index * index);
    }
    text.append(" ready");
    printf("%s: %d values, last=%d\n", text.c_str(),
           static_cast<int>(values.size()), values.back());
    return text.size() == 11 && values.size() == 4 ? 42 : 1;
}
