#include "../runner/common.hpp"

int runStringBenchmark(int count) {
    int checksum = 0;
    for (int i = 0; i < count; i++) {
        std::string tag = "ITEM_" + std::to_string(i);
        std::string formatted = "ID: [" + tag + "] - Status: OK - Code: " + std::to_string(i * 7);
        
        checksum = (checksum + static_cast<int>(formatted.length())) % 1000000007;

        if (formatted.length() > 8) {
            std::string sub = formatted.substr(4, 10 - 4);
            checksum = (checksum + static_cast<int>(sub.length())) % 1000000007;
        }
    }
    return checksum;
}

int main() {
    // Warmup
    runStringBenchmark(100);

    UmeBench::run("string_concat_and_manipulation", []() {
        int res = runStringBenchmark(20000);
        return std::to_string(res);
    });

    return 0;
}
