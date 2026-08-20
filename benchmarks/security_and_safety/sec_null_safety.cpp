#include "../runner/common.hpp"
#include <optional>
#include <memory>
#include <stdexcept>

class User {
public:
    std::string name;
    User(std::string name) : name(std::move(name)) {}
    std::string getName() const { return name; }
};

int main() {
    int passedChecks = 0;
    int totalChecks = 4;

    // Check 1: std::optional / pointer null coalescing with null
    std::optional<std::string> nullStr = std::nullopt;
    std::string fallback = nullStr.value_or("FallbackValue");
    if (fallback == "FallbackValue") {
        passedChecks++;
    }

    // Check 2: null coalescing with non-null value
    std::optional<std::string> activeStr = "ActiveValue";
    std::string notFallback = activeStr.value_or("FallbackValue");
    if (notFallback == "ActiveValue") {
        passedChecks++;
    }

    // Check 3: Calling method on null smart pointer checked
    std::shared_ptr<User> nullUser = nullptr;
    try {
        if (!nullUser) {
            throw std::runtime_error("Null pointer dereference");
        }
        nullUser->getName();
    } catch (const std::exception&) {
        passedChecks++;
    }

    // Check 4: Null comparison equality
    std::shared_ptr<User> u1 = nullptr;
    std::shared_ptr<User> u2 = nullptr;
    if (u1 == u2 && u1 == nullptr) {
        passedChecks++;
    }

    std::string status = (passedChecks == totalChecks) ? "PASSED" : "FAILED";
    std::cout << "[SECURITY_RESULT] {\"test\": \"null_safety\", \"status\": \"" << status << "\", \"passed\": " << passedChecks << ", \"total\": " << totalChecks << "}\n";

    return 0;
}
