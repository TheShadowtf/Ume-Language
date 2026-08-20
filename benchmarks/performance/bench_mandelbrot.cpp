#include "../runner/common.hpp"

int computeMandelbrot(int width, int height, int maxIter) {
    double xMin = -2.0;
    double xMax = 1.0;
    double yMin = -1.5;
    double yMax = 1.5;

    double dx = (xMax - xMin) / static_cast<double>(width);
    double dy = (yMax - yMin) / static_cast<double>(height);

    int checksum = 0;
    for (int py = 0; py < height; py++) {
        double y0 = yMin + static_cast<double>(py) * dy;
        for (int px = 0; px < width; px++) {
            double x0 = xMin + static_cast<double>(px) * dx;
            double x = 0.0;
            double y = 0.0;
            int iter = 0;

            while (x * x + y * y <= 4.0 && iter < maxIter) {
                double xtemp = x * x - y * y + x0;
                y = 2.0 * x * y + y0;
                x = xtemp;
                iter++;
            }

            checksum = (checksum + iter) % 1000000007;
        }
    }
    return checksum;
}

int main() {
    // Warmup
    computeMandelbrot(20, 20, 10);

    UmeBench::run("mandelbrot_float_math", []() {
        int res = computeMandelbrot(150, 150, 100);
        return std::to_string(res);
    });

    return 0;
}
