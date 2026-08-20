#include "../runner/common.hpp"
#include <vector>
#include <stdexcept>

int main() {
    int passedChecks = 0;
    int totalChecks = 4;

    std::vector<int> list = {10, 20, 30};

    // Check 1: Out of bounds positive index (using .at() for bounds checking)
    try {
        int val = list.at(100);
        (void)val;
    } catch (const std::out_of_range&) {
        passedChecks++;
    }

    // Check 2: Out of bounds negative / huge unsigned index
    try {
        // -1 cast to size_t is out of range
        int val = list.at(static_cast<size_t>(-1));
        (void)val;
    } catch (const std::out_of_range&) {
        passedChecks++;
    }

    // Check 3: Out of bounds write
    try {
        list.at(50) = 999;
    } catch (const std::out_of_range&) {
        passedChecks++;
    }

    // Check 4: Valid access
    if (list.at(1) == 20) {
        passedChecks++;
    }

    std::string status = (passedChecks == totalChecks) ? "PASSED" : "FAILED";
    std::cout << "[SECURITY_RESULT] {\"test\": \"bounds_safety\", \"status\": \"" << status << "\", \"passed\": " << passedChecks << ", \"total\": " << totalChecks << "}\n";

    return 0;
}
