#include <vector>
int live = 0;
struct Item {
    int value;
    Item() : value(0) { ++live; }
    explicit Item(int v) : value(v) { ++live; }
    Item(const Item& other) : value(other.value) { ++live; }
    ~Item() { --live; }
};
int main() {
    {
        std::vector<Item> items;
        items.push_back(Item(3));
        items.push_back(Item(4));
        if (items.size() != 2) return 1;
        if (live != 2) return 2;
        if (items.data()[0].value != 3) return 3;
        items.pop_back();
        if (live != 1) return 4;
        items.clear();
        if (live != 0) return 5;
    }
    if (live != 0) return 6;
    return 42;
}


