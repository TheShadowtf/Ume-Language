#include "../runner/common.hpp"
#include <vector>
#include <unordered_map>
#include <stack>
#include <queue>

int runCollectionsBenchmark(int n) {
    int checksum = 0;

    // 1. List operations
    std::vector<int> list;
    list.reserve(n);
    for (int i = 0; i < n; i++) {
        list.push_back(i * 3 + 1);
    }

    for (int j = 0; j < n; j += 2) {
        int val = list[j];
        checksum = (checksum + val) % 1000000007;
    }

    // 2. Map operations
    std::unordered_map<std::string, int> map;
    for (int k = 0; k < n / 2; k++) {
        std::string key = "key_" + std::to_string(k);
        map[key] = k * 5;
    }

    for (int m = 0; m < n / 2; m += 3) {
        std::string key = "key_" + std::to_string(m);
        auto it = map.find(key);
        if (it != map.end()) {
            int mapVal = it->second;
            checksum = (checksum + mapVal) % 1000000007;
        }
    }

    // 3. Stack operations
    std::stack<int> stack;
    for (int s = 0; s < 1000; s++) {
        stack.push(s);
    }
    while (!stack.empty()) {
        int popped = stack.top();
        stack.pop();
        checksum = (checksum + popped) % 1000000007;
    }

    // 4. Queue operations
    std::queue<int> queue;
    for (int q = 0; q < 1000; q++) {
        queue.push(q);
    }
    while (!queue.empty()) {
        int deq = queue.front();
        queue.pop();
        checksum = (checksum + deq) % 1000000007;
    }

    return checksum;
}

int main() {
    // Warmup
    runCollectionsBenchmark(100);

    UmeBench::run("collections_list_map_queue", []() {
        int res = runCollectionsBenchmark(10000);
        return std::to_string(res);
    });

    return 0;
}
