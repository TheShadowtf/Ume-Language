#include "../runner/common.hpp"
#include <memory>

class Point2D {
public:
    double x;
    double y;

    Point2D(double x, double y) : x(x), y(y) {}
    virtual ~Point2D() = default;

    virtual double magnitude() {
        return x * x + y * y;
    }
};

class ColoredPoint : public Point2D {
public:
    int color;

    ColoredPoint(double x, double y, int color) : Point2D(x, y), color(color) {}

    double magnitude() override {
        return Point2D::magnitude() + static_cast<double>(color);
    }
};

int runObjectBenchmark(int count) {
    double totalMag = 0.0;
    for (int i = 0; i < count; i++) {
        double fx = static_cast<double>(i % 100);
        double fy = static_cast<double>((i + 1) % 100);
        int col = i % 16;
        auto pt = std::make_shared<ColoredPoint>(fx, fy, col);
        totalMag += pt->magnitude();
    }
    return static_cast<int>(totalMag) % 1000000007;
}

int main() {
    // Warmup
    runObjectBenchmark(100);

    UmeBench::run("oop_instantiation_and_dispatch", []() {
        int res = runObjectBenchmark(50000);
        return std::to_string(res);
    });

    return 0;
}
