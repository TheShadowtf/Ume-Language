#include "../runner/common.hpp"
#include <vector>

int countPrimesTrial(int limit) {
    int count = 0;
    for (int num = 2; num <= limit; num++) {
        bool isPrime = true;
        for (int i = 2; i * i <= num; i++) {
            if (num % i == 0) {
                isPrime = false;
                break;
            }
        }
        if (isPrime) {
            count++;
        }
    }
    return count;
}

int sieve(int limit) {
    std::vector<bool> isPrime(limit + 1, true);
    isPrime[0] = false;
    isPrime[1] = false;

    for (int p = 2; p * p <= limit; p++) {
        if (isPrime[p]) {
            for (int multiple = p * p; multiple <= limit; multiple += p) {
                isPrime[multiple] = false;
            }
        }
    }

    int count = 0;
    for (int j = 2; j <= limit; j++) {
        if (isPrime[j]) {
            count++;
        }
    }
    return count;
}

int main() {
    // Warmup
    countPrimesTrial(100);
    sieve(100);

    UmeBench::run("primes_trial_division", []() {
        int res = countPrimesTrial(20000);
        return std::to_string(res);
    });

    UmeBench::run("primes_sieve_eratosthenes", []() {
        int res = sieve(50000);
        return std::to_string(res);
    });

    return 0;
}
