#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
public:
    ThreadPool(const int n);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool();

    template<class F, class... Arg>
    void enqueue(F &&f, Arg&&... arg);

private:
    bool stop;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> Q;
    std::mutex mutex;
    std::condition_variable cv;
};

#endif