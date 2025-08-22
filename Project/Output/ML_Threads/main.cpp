#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <chrono>

#include "ML_Threads.h"


std::mutex coutMutex;

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

void CalculatePrimes(const long long start, const long long end)
{
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Starting intensive calculation from " << start << " to " << end << "..." << std::endl;
    }

    std::vector<long long> primes;
    primes.reserve(700000);

    int dotCounter = 0;
    for (long long i = start; i <= end; ++i) {
        if (isPrime(i)) {
            primes.push_back(i);
        }

        if (i % 100000 == 0) {
            std::string dots;
            dotCounter = (dotCounter % 3) + 1;
            for (int j = 0; j < dotCounter; j++) {
                dots += ".";
            }

            std::string padding;
            for (int j = 0; j < (3 - dotCounter); j++) {
                padding += " ";
            }

            std::lock_guard<std::mutex> lock(coutMutex);
            clearLine();
            std::cout << "Processing" + dots + padding + " " + std::to_string(i) + "/" +
                std::to_string(end) + " (" +
                std::to_string(static_cast<double>(i - start) / (end - start) * 100) +
                "%)" << std::flush;
        }
    }

    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << std::endl;

    Log("Found " + std::to_string(primes.size()) + " prime numbers");

    if (!primes.empty()) {
        Log("First 5 primes in range:");
        for (int i = 0; i < 5 && i < primes.size(); ++i) {
            Log(std::to_string(primes[i]));
        }

        Log("Last 5 primes in range:");
        for (int i = std::max(0, (int)primes.size() - 5); i < primes.size(); ++i) {
            Log(std::to_string(primes[i]));
        }
    }
}

int main()
{
    ML_CPU_Threads thread(10);
    long long primes_range = 10000000;

    Log("Primes Calculation WITHOUT using multithreading");
    auto startTime = std::chrono::high_resolution_clock::now();

    CalculatePrimes(0, primes_range);  // 10 million (10^7)

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = endTime - startTime;
    Log("Calculation completed!");
    Log("Total time: " + std::to_string(totalTime.count()) + " seconds");

    Log("\n\n\n\n\n\n\n");
    Log("Primes Calculation using multithreading");

    startTime = std::chrono::high_resolution_clock::now();

    int numThreads = thread.GetTotalThreads();
    long long segmentSize = primes_range / numThreads;

    for (int i = 0; i < numThreads; i++) {
        long long start = i * segmentSize;
        long long end = (i == numThreads - 1) ? primes_range : (i + 1) * segmentSize - 1;

        thread.Start([start, end]() {
            CalculatePrimes(start, end);
            });
    }

    while (thread.IsWorking()) {
        std::this_thread::yield();
    }

    endTime = std::chrono::high_resolution_clock::now();
    totalTime = endTime - startTime;
    Log("Calculation completed!");
    Log("Total time: " + std::to_string(totalTime.count()) + " seconds");

    return 0;
}