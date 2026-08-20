#pragma once

#include <iostream>
#include <chrono>
#include <string>
#include <functional>
#include <vector>
#include <iomanip>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace UmeBench {

inline double getPeakMemoryMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

inline double getCurrentMemoryMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

struct BenchResult {
    std::string name;
    double elapsedMs;
    double peakMemoryMB;
    std::string resultValue;
};

inline BenchResult run(const std::string& name, std::function<std::string()> task) {
    auto start = std::chrono::high_resolution_clock::now();
    std::string resultVal = task();
    auto end = std::chrono::high_resolution_clock::now();
    
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double peakMem = getPeakMemoryMB();

    std::cout << "[BENCHMARK_RESULT] "
              << "{\"name\": \"" << name << "\", "
              << "\"time_ms\": " << std::fixed << std::setprecision(3) << elapsedMs << ", "
              << "\"peak_memory_mb\": " << std::fixed << std::setprecision(2) << peakMem << ", "
              << "\"result\": \"" << resultVal << "\"}\n";

    return { name, elapsedMs, peakMem, resultVal };
}

} // namespace UmeBench
