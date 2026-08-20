#include "../runner/common.hpp"
#include <stdexcept>
#include <string>

class DatabaseError : public std::runtime_error {
public:
    int errorCode;
    DatabaseError(int code, const std::string& msg)
        : std::runtime_error(msg), errorCode(code) {}
};

static int finallyCount = 0;

void deepUnwind(int depth) {
    struct ScopeExit {
        ~ScopeExit() { finallyCount++; }
    } guard;

    if (depth <= 0) {
        throw DatabaseError(500, "Connection timed out in deep stack");
    }
    deepUnwind(depth - 1);
}

int main() {
    int passedChecks = 0;
    int totalChecks = 3;

    finallyCount = 0;
    bool caughtCustom = false;

    try {
        deepUnwind(20);
    } catch (const DatabaseError& dbErr) {
        caughtCustom = true;
        if (dbErr.errorCode == 500) {
            passedChecks++;
        }
    }

    if (caughtCustom) {
        passedChecks++;
    }

    if (finallyCount == 21) {
        passedChecks++;
    }

    std::string status = (passedChecks == totalChecks) ? "PASSED" : "FAILED";
    std::cout << "[SECURITY_RESULT] {\"test\": \"exception_unwinding\", \"status\": \"" << status << "\", \"passed\": " << passedChecks << ", \"total\": " << totalChecks << "}\n";

    return 0;
}
