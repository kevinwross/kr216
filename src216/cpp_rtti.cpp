#include <typeinfo>
class Base { public: virtual ~Base() {} virtual int value() const { return 1; } };
class Derived : public Base { public: int value() const override { return 2; } };
int main() {
    Derived derived;
    Base *base = &derived;
    Derived *down = dynamic_cast<Derived *>(base);
    if (down == nullptr) return 1;
    return typeid(*base) == typeid(Derived) ? 42 : 2;
}
