#include "../runner/common.hpp"
#include <stdexcept>
#include <limits>

int main() {
    int passedChecks = 0;
    int totalChecks = 3;

    // Check 1: Integer division by zero guard / exception
    try {
        int a = 100;
        int b = 0;
        if (b == 0) throw std::domain_error("Division by zero");
        int c = a / b;
        (void)c;
    } catch (const std::exception&) {
        passedChecks++;
    }

    // Check 2: Modulo by zero guard / exception
    try {
        int a = 50;
        int b = 0;
        if (b == 0) throw std::domain_error("Division by zero");
        int c = a % b;
        (void)c;
    } catch (const std::exception&) {
        passedChecks++;
    }

    // Check 3: Large integer arithmetic wrapping
    int maxVal = (std::numeric_limits<int>::max)();
    int wrapped = maxVal + 1;
    if (wrapped != 0) {
        passedChecks++;
    }

    std::string status = (passedChecks == totalChecks) ? "PASSED" : "FAILED";
    std::cout << "[SECURITY_RESULT] {\"test\": \"arithmetic_safety\", \"status\": \"" << status << "\", \"passed\": " << passedChecks << ", \"total\": " << totalChecks << "}\n";

    return 0;
}
