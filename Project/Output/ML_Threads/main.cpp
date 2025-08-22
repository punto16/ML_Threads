#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <chrono>

void clearLine() {
    std::cout << "\r\033[K";
}

void Log(const std::string& text) {
    std::cout << text << std::endl;
}

bool isPrime(long long n) {
    if (n <= 1) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;

    for (long long i = 3; i <= std::sqrt(n); i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

void CalculatePrimes(const long long target)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    Log("Starting intensive calculation...");

    std::vector<long long> primes;
    primes.reserve(700000);

    auto individualStart = std::chrono::high_resolution_clock::now();
    bool isTargetPrime = isPrime(target);
    auto individualEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> individualTime = individualEnd - individualStart;

    Log("Time to check if " + std::to_string(target) + " is prime: " +
        std::to_string(individualTime.count()) + " seconds");
    Log("Is " + std::to_string(target) + " prime? " + (isTargetPrime ? "Yes" : "No"));

    int dotCounter = 0;
    for (long long i = 2; i <= target; ++i) {
        if (isPrime(i)) {
            primes.push_back(i);
        }

        if (i % 100000 == 0) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentTime - startTime;

            std::string dots;
            dotCounter = (dotCounter % 3) + 1;
            for (int j = 0; j < dotCounter; j++) {
                dots += ".";
            }

            std::string padding;
            for (int j = 0; j < (3 - dotCounter); j++) {
                padding += " ";
            }

            clearLine();
            std::cout << "Processing" + dots + padding + " " + std::to_string(i) + "/" +
                std::to_string(target) + " (" +
                std::to_string(static_cast<double>(i) / target * 100) +
                "%), Elapsed: " + std::to_string(elapsed.count()) + "s" << std::flush;
        }
    }

    std::cout << std::endl;

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = endTime - startTime;

    Log("Calculation completed!");
    Log("Total time: " + std::to_string(totalTime.count()) + " seconds");
    Log("Found " + std::to_string(primes.size()) + " prime numbers");

    Log("First 5 primes:");
    for (int i = 0; i < 5 && i < primes.size(); ++i) {
        Log(std::to_string(primes[i]));
    }

    Log("Last 5 primes:");
    for (int i = std::max(0, (int)primes.size() - 5); i < primes.size(); ++i) {
        Log(std::to_string(primes[i]));
    }

    double primesPerSecond = primes.size() / totalTime.count();
    Log("Performance: " + std::to_string(primesPerSecond) + " primes/second");
}

int main()
{
    CalculatePrimes(10000000);  // 10 million (10^7)
    return 0;
}