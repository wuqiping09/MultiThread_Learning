#include "threadpool.h"
#include <iostream>

ThreadPool::ThreadPool(const int n): stop(false) {
    for (int i = 0; i < n; ++i) {
        threads.emplace_back(std::thread([this]() {
            while (true) {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this]() {
                    return stop || !Q.empty();
                });
                if (stop) {
                    return;
                }
                auto f = Q.front();
                Q.pop();
                lock.unlock();
                f();
            }
        }));
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        stop = true;
    }
    cv.notify_all();
    for (auto &t : threads) {
        t.join();
    }
}

template<class F, class... Arg>
void ThreadPool::enqueue(F &&f, Arg&&... arg) {
    std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Arg>(arg)...);
    {
        std::unique_lock<std::mutex> lock(mutex);
        Q.emplace(std::move(task));
    }
    cv.notify_one();
}

int main() {
    ThreadPool tp(3);
    for (int i = 0; i < 10; ++i) {
        tp.enqueue([i](int x, int y) {
            std::cout << "The " << i << "th task starts" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Result is " << x + y << std::endl;
            std::cout << "The " << i << "th task ends" << std::endl;
        }, i, i);
    }
    return 0;
}