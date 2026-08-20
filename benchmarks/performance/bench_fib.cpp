#include "../runner/common.hpp"

int fibRecursive(int n) {
    if (n <= 1) return n;
    return fibRecursive(n - 1) + fibRecursive(n - 2);
}

int fibIterative(int n) {
    int a = 0;
    int b = 1;
    for (int i = 0; i < n; i++) {
        int temp = (a + b) % 1000000007;
        a = b;
        b = temp;
    }
    return a;
}

int main() {
    // Warmup
    fibRecursive(10);
    fibIterative(10);

    UmeBench::run("fib_recursive", []() {
        int res = fibRecursive(28);
        return std::to_string(res);
    });

    UmeBench::run("fib_iterative", []() {
        int res = fibIterative(1000000);
        return std::to_string(res);
    });

    return 0;
}
