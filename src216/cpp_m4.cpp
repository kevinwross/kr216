template <typename T>
T twice(T value) { return value + value; }

template <typename T>
class Box {
    T value;
public:
    explicit Box(T v) : value(v) {}
    T get() const { return value; }
};

class Base {
public:
    virtual double value() const { return 1.0; }
    virtual ~Base() {}
};

class Derived : public Base {
    double factor;
public:
    explicit Derived(double f) : factor(f) {}
    double value() const override { return factor * 2.0; }
};

int main() {
    Box<int> box(21);
    if (box.get() != 21) return 1;
    if (twice<double>(2.0) != 4.0) return 2;
    Base *base = new Derived(3.0);
    const double result = base->value();
    delete base;
    return result == 6.0 ? 42 : 3;
}
