#ifndef __ML_CPU_THREADS_H__
#define __ML_CPU_THREADS_H__

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <map>
#include <queue>
#include <stdexcept>
#include <memory>

class MainThreadDispatcher
{
public:
    static MainThreadDispatcher& GetInstance()
    {
        static MainThreadDispatcher instance;
        return instance;
    }

    void QueueFunction(std::function<void()> func)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        functionQueue.push(func);
    }

    void ProcessQueue()
    {
        std::queue<std::function<void()>> tempQueue;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tempQueue.swap(functionQueue);
        }

        while (!tempQueue.empty())
        {
            tempQueue.front()();
            tempQueue.pop();
        }
    }

private:
    MainThreadDispatcher() = default;
    ~MainThreadDispatcher() = default;

    std::queue<std::function<void()>> functionQueue;
    std::mutex queueMutex;
};

class ML_CPU_Threads
{
public:
    ML_CPU_Threads(int numThreads);
    ~ML_CPU_Threads();

    template<typename Function>
    void Start(Function func);

    void Stop();

    template<typename Function>
    void Stop(Function func);

    bool IsWorking() const;

    static std::thread::id GetMainThreadId() { return mainThreadId; }
    static bool IsMainThread() { return std::this_thread::get_id() == mainThreadId; }

    template<typename Function>
    static void ExecuteOnMainThread(Function func)
    {
        MainThreadDispatcher::GetInstance().QueueFunction(func);
    }

    static void ProcessMainThreadQueue()
    {
        MainThreadDispatcher::GetInstance().ProcessQueue();
    }

    int GetTotalThreads() { return totalThreads; }

private:
    struct ThreadData {
        std::thread thread;
        std::atomic<bool> stopRequested;
        std::atomic<bool> isWorking;
        std::function<void()> function;
        std::mutex mutex;
        std::condition_variable condition;
        bool hasWork;
        size_t functionId;

        ThreadData(const ThreadData&) = delete;
        ThreadData& operator=(const ThreadData&) = delete;

        ThreadData() = default;

        ThreadData(ThreadData&& other) noexcept
            : thread(std::move(other.thread))
            , stopRequested(other.stopRequested.load())
            , isWorking(other.isWorking.load())
            , function(std::move(other.function))
            , mutex()
            , condition()
            , hasWork(other.hasWork)
            , functionId(other.functionId)
        {
        }

        ThreadData& operator=(ThreadData&& other) noexcept
        {
            if (this != &other)
            {
                thread = std::move(other.thread);
                stopRequested = other.stopRequested.load();
                isWorking = other.isWorking.load();
                function = std::move(other.function);
                hasWork = other.hasWork;
                functionId = other.functionId;
            }
            return *this;
        }
    };

    template<typename Function>
    static size_t getFunctionId(Function func)
    {
        return reinterpret_cast<size_t>(*reinterpret_cast<void**>(&func));
    }

    std::vector<std::unique_ptr<ThreadData>> threadsData;
    int totalThreads;
    std::mutex functionMapMutex;
    std::map<size_t, std::vector<int>> functionToThreads;

    static std::thread::id mainThreadId;
    static bool mainThreadIdInitialized;
};

std::thread::id ML_CPU_Threads::mainThreadId;
bool ML_CPU_Threads::mainThreadIdInitialized = false;

ML_CPU_Threads::ML_CPU_Threads(int numThreads) : totalThreads(numThreads)
{
    if (mainThreadIdInitialized && std::this_thread::get_id() != mainThreadId)
    {
        throw std::runtime_error("ML_CPU_Threads must be constructed in the main thread!");
    }

    if (!mainThreadIdInitialized)
    {
        mainThreadId = std::this_thread::get_id();
        mainThreadIdInitialized = true;
    }

    threadsData.reserve(totalThreads);
    for (int i = 0; i < totalThreads; ++i)
    {
        threadsData.emplace_back(std::make_unique<ThreadData>());
        auto& data = *threadsData[i];

        data.stopRequested = false;
        data.isWorking = false;
        data.hasWork = false;
        data.functionId = 0;

        data.thread = std::thread([this, i]()
            {
                auto& data = *threadsData[i];

                while (!data.stopRequested)
                {
                    std::unique_lock<std::mutex> lock(data.mutex);
                    data.condition.wait(lock, [&data]()
                        {
                            return data.hasWork || data.stopRequested;
                        });

                    if (data.stopRequested) break;

                    if (data.hasWork)
                    {
                        data.isWorking = true;
                        lock.unlock();

                        data.function();

                        lock.lock();
                        data.isWorking = false;
                        data.hasWork = false;

                        std::lock_guard<std::mutex> mapLock(functionMapMutex);
                        if (data.functionId != 0) {
                            auto& threads = functionToThreads[data.functionId];
                            threads.erase(std::remove(threads.begin(), threads.end(), i), threads.end());
                            if (threads.empty()) {
                                functionToThreads.erase(data.functionId);
                            }
                        }
                        data.functionId = 0;
                    }
                }
            });
    }
}

ML_CPU_Threads::~ML_CPU_Threads()
{
    Stop();

    for (auto& data : threadsData)
    {
        data->stopRequested = true;
        data->condition.notify_one();

        if (data->thread.joinable())
        {
            data->thread.join();
        }
    }
}

template<typename Function>
void ML_CPU_Threads::Start(Function func)
{
    int numThreads = 1;
    int threadsToUse = (numThreads <= 0) ? 1 : std::min(numThreads, totalThreads);
    size_t funcId = getFunctionId(func);

    std::lock_guard<std::mutex> mapLock(functionMapMutex);
    auto& threads = functionToThreads[funcId];

    int assigned = 0;
    for (int i = 0; i < totalThreads && assigned < threadsToUse; ++i)
    {
        if (!threadsData[i]->isWorking && !threadsData[i]->hasWork)
        {
            std::lock_guard<std::mutex> lock(threadsData[i]->mutex);
            threadsData[i]->function = func;
            threadsData[i]->functionId = funcId;
            threadsData[i]->hasWork = true;
            threadsData[i]->stopRequested = false;
            threads.push_back(i);
            assigned++;
            threadsData[i]->condition.notify_one();
        }
    }

    if (assigned < threadsToUse)
    {
        std::cout << "Warning: Only " << assigned << " threads available for execution. Requested: " << threadsToUse << std::endl;
    }
}

void ML_CPU_Threads::Stop()
{
    for (auto& data : threadsData)
    {
        data->stopRequested = true;
    }

    for (auto& data : threadsData)
    {
        while (data->isWorking)
        {
            std::this_thread::yield();
        }
    }

    std::lock_guard<std::mutex> mapLock(functionMapMutex);
    functionToThreads.clear();
}

template<typename Function>
void ML_CPU_Threads::Stop(Function func)
{
    size_t funcId = getFunctionId(func);
    std::vector<int> threadsToStop;

    {
        std::lock_guard<std::mutex> mapLock(functionMapMutex);
        auto it = functionToThreads.find(funcId);
        if (it != functionToThreads.end())
        {
            threadsToStop = it->second;
            functionToThreads.erase(it);
        }
    }

    for (int threadId : threadsToStop)
    {
        if (threadId >= 0 && threadId < totalThreads)
        {
            threadsData[threadId]->stopRequested = true;
        }
    }

    for (int threadId : threadsToStop)
    {
        if (threadId >= 0 && threadId < totalThreads)
        {
            while (threadsData[threadId]->isWorking)
            {
                std::this_thread::yield();
            }
        }
    }
}

bool ML_CPU_Threads::IsWorking() const
{
    for (const auto& data : threadsData)
    {
        if (data->isWorking) return true;
    }
    return false;
}

#endif // !__ML_CPU_THREADS_H__