#include "../runner/common.hpp"
#include <memory>

class TempNode {
public:
    int id;
    std::string payload;
    TempNode(int id) : id(id), payload("payload_data_string_" + std::to_string(id)) {}
};

int main() {
    int allocations = 50000;
    int checksum = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < allocations; i++) {
        auto node = std::make_shared<TempNode>(i);
        checksum = (checksum + node->id) % 1000000007;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::string status = (checksum == 249974993) ? "PASSED" : "FAILED";
    std::cout << "[SECURITY_RESULT] {\"test\": \"memory_leak_stress\", \"status\": \"" << status << "\", \"allocations\": " << allocations << ", \"time_ms\": " << timeMs << "}\n";

    return 0;
}
