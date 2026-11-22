#include <stdio.h>

class Shape {
public:
    virtual ~Shape() {}
    virtual double area() const = 0;
};

class Square : public Shape {
public:
    explicit Square(double side) : side_(side) {}
    double area() const override { return side_ * side_; }

private:
    double side_;
};

int main() {
    Shape* shape = new Square(2.5);
    const double area = shape->area();
    printf("area_x100=%d\n", static_cast<int>(area * 100.0));
    delete shape;
    return area == 6.25 ? 42 : 1;
}
