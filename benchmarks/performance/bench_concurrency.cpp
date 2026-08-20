#include "../runner/common.hpp"
#include <atomic>
#include <mutex>
#include <future>

std::string runConcurrencyBenchmark() {
    std::atomic<int64_t> atomicCounter{0};
    std::mutex mtx;
    int64_t mutexProtectedSum = 0;

    // 1. Atomic increments (50,000 iterations)
    for (int i = 0; i < 50000; i++) {
        atomicCounter++;
    }

    // 2. Mutex locking (50,000 iterations)
    for (int j = 0; j < 50000; j++) {
        std::lock_guard<std::mutex> lock(mtx);
        mutexProtectedSum++;
    }

    // 3. Task / async evaluation
    auto taskFuture = std::async(std::launch::async, []() { return 42; });
    int taskRes = taskFuture.get();

    return "atomic=" + std::to_string(atomicCounter.load()) +
           ";mutex=" + std::to_string(mutexProtectedSum) +
           ";task=" + std::to_string(taskRes);
}

int main() {
    // Warmup
    runConcurrencyBenchmark();

    UmeBench::run("concurrency_atomics_and_mutex", []() {
        return runConcurrencyBenchmark();
    });

    return 0;
}
