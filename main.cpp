#include <iostream>
#include <fstream>
#include <list>
#include <thread>
#include <mutex>
#include <future>
#include <Windows.h>

void f() {
    std::fstream fs("1.txt", std::ios::out);
    if (!fs.is_open()) {
        std::cout << "open 1.txt error" << std::endl;
    }
    for (int i = 0; i < 10; ++i) {
        fs << i << std::endl;
    }
    fs.close();
}

void test1() {
    auto a = [](std::unique_ptr<int> up) {
        std::cout << "thread starts" << std::endl;
        std::cout << *up.get() << std::endl;
        std::cout << "thread ends" << std::endl;
    };
    std::unique_ptr<int> up(new int(1));
    std::thread t1(a, std::move(up));
    t1.join();
    std::cout << "main thread id: " << std::this_thread::get_id() << std::endl;
    std::cout << "main thread ends" << std::endl;
}

class A {
public:
    A();
    void operator()() {
        std::cout << "thread starts" << std::endl;
        std::cout << "thread ends" << std::endl;
    }
    void inList1();
    void inList2();
    void outList1();
    void outList2();

private:
    std::list<int> L;
    std::mutex m;
    std::condition_variable cv;
    CRITICAL_SECTION cs;
};

A::A() {
    InitializeCriticalSection(&this->cs);
}

void A::inList1() {
    for (int i = 0; i < 1000; ++i) {
        std::unique_lock<std::mutex> lg(this->m);
        this->L.push_back(i);
        std::cout << "put " << i << " into the list" << std::endl;
        this->cv.notify_all();
    }
}

void A::inList2() {
    for (int i = 0; i < 1000; ++i) {
        EnterCriticalSection(&this->cs);
        EnterCriticalSection(&this->cs); // can enter critical section multiple times
        this->L.push_back(i);
        std::cout << "put " << i << " into the list" << std::endl;
        LeaveCriticalSection(&this->cs);
        LeaveCriticalSection(&this->cs); // must leave critical section the same times as enter
    }
}

void A::outList1() {
    for (int i = 0; i < 2000; ++i) {
        std::unique_lock<std::mutex> lg(this->m);
        if (this->L.empty()) {
            std::cout << "outList is Called, but the list is empty" << std::endl;
        } else {
            int x = this->L.front();
            this->L.pop_front();
            std::cout << "take " << x << " out of the list" << std::endl;
        }
    }
}

void A::outList2() {
    while (true) {
        std::unique_lock<std::mutex> ul(this->m);
        // if return false, 
        //     then unlock the mutex and wait until notify_one is called, then the unique lock tries to lock the mutex again
        // else go ahead
        this->cv.wait(ul, [this]() {
            return !this->L.empty();
        });
        int x = this->L.front();
        this->L.pop_front();
        std::cout << "take " << x << " out of the list" << std::endl;
        std::cout << "thread id is " << std::this_thread::get_id() << std::endl;
        ul.unlock();
    }
}

void test2() {
    A a;
    std::thread outThread1(&A::outList2, &a);
    std::thread outThread2(&A::outList2, &a);
    std::thread inThread(&A::inList1, &a);
    outThread1.join();
    outThread2.join();
    inThread.join();
}

void testMutex() {
    std::mutex m1;
    m1.lock();
    m1.unlock();

    std::recursive_mutex rm;
    rm.lock();
    rm.lock(); // can lock multiple times
    rm.unlock();
    rm.unlock();

    std::timed_mutex tm;
    if (tm.try_lock_for(std::chrono::milliseconds(100))) {
        std::cout << "lock the mutex" << std::endl;
        tm.unlock();
    } else {
        std::cout << "did not lock the mutex" << std::endl;
    }
    if (tm.try_lock_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(100))) {
        std::cout << "lock the mutex" << std::endl;
        tm.unlock();
    } else {
        std::cout << "did not lock the mutex" << std::endl;
    }
}

void testLockGuard() {
    std::mutex m1, m2;

    std::lock_guard<std::mutex> lg1(m1); // locked

    m2.lock();
    std::lock_guard<std::mutex> lg2(m2, std::adopt_lock); // need to be locked before this statement
}

void testUniqueLock() {
    std::mutex m1, m2, m3, m4, m5, m6;

    std::unique_lock<std::mutex> ul1(m1); // locked

    m2.lock();
    std::unique_lock<std::mutex> ul2(m2, std::adopt_lock); // need to be locked before this statement

    std::unique_lock<std::mutex> ul3(m3, std::defer_lock); // do not lock now
    ul3.lock(); // lock now

    std::unique_lock<std::mutex> ul4(m4, std::try_to_lock); // if not locked, do not wait here
    if (ul4.owns_lock()) {
        std::cout << "ul4 is locked" << std::endl;
    } else {
        std::cout << "ul4 try to lock failed" << std::endl;
    }

    std::unique_lock<std::mutex> ul5(m5, std::defer_lock);
    if (ul5.try_lock()) { // similar to try_to_lock()
        std::cout << "ul5 is locked" << std::endl;
    } else {
        std::cout << "ul5 is not locked" << std::endl;
    }

    std::unique_lock<std::mutex> ul6(m6);
    std::mutex *p = ul6.release(); // unrelate ul6 and m6
    p->unlock();

    std::unique_lock<std::mutex> ul7(std::move(ul1)); // unrelate ul1 and m1, relate ul7 and m1
}

std::atomic<int> x;

void testAtomic() {
    auto a = []() {
        for (int i = 0; i < 10000000; ++i) {
            ++x;
        }
    };
    std::thread t1(a);
    std::thread t2(a);
    t1.join();
    t2.join();
    std::cout << x << std::endl;

    auto b = []() {
        for (int i = 0; i < 10000000; ++i) {
            x = x + 1; // atomic fails
        }
    };
    std::thread t3(b);
    std::thread t4(b);
    t3.join();
    t4.join();
    std::cout << x << std::endl;

    std::atomic<int> y(x.load());
    y.store(10);
}

std::mutex mySingletonMutex;
std::once_flag mySingletonFlag;

class MySingleton {
public:
    static MySingleton* getInstance();

    class MySingletonDelete {
    public:
        ~MySingletonDelete();
    };

private:
    static MySingleton *instance;
    MySingleton() {}
    static void createInstance();
};

MySingleton *MySingleton::instance = nullptr;

MySingleton* MySingleton::getInstance() {
    if (instance == nullptr) {
        std::unique_lock<std::mutex> ul(mySingletonMutex);
        if (instance == nullptr) {
            // instance = new MySingleton();
            // static MySingletonDelete msd;
        }
    }
    std::call_once(mySingletonFlag, createInstance);
    return instance;
}

void MySingleton::createInstance() {
    std::chrono::milliseconds sleepTime(1000);
    std::this_thread::sleep_for(sleepTime);
    instance = new MySingleton();
    static MySingletonDelete msd;
}

MySingleton::MySingletonDelete::~MySingletonDelete() {
    if (MySingleton::instance != nullptr) {
        delete MySingleton::instance;
        MySingleton::instance = nullptr;
    }
}

void testMySingleton() {
    MySingleton *p = MySingleton::getInstance();
    MySingleton *q = MySingleton::getInstance();
    std::cout << p << " " << q << std::endl;
}

void testFuture() {
    auto a = []() -> int {
        std::cout << "thread starts, thread id is " << std::this_thread::get_id() << std::endl;
        std::chrono::milliseconds sleepTime(3000);
        std::this_thread::sleep_for(sleepTime);
        std::cout << "thread ends, thread id is " << std::this_thread::get_id() << std::endl;
        return 3;
    };

    std::cout << "main thread id is " << std::this_thread::get_id() << std::endl;

    std::future<int> f1 = std::async(a); // default is async | deferred, whether to create a thread depends on kernel
    std::future_status fs1 = f1.wait_for(std::chrono::seconds(0));
    if (fs1 == std::future_status::deferred) {
        std::cout << "the thread has not been created" << std::endl;
    }
    std::cout << f1.get() << std::endl; // wait until the thread returns

    std::future<int> f2 = std::async(std::launch::deferred, a); // do not create the thread until get or wait is called, actually will not create a thread
    
    std::future<int> f3 = std::async(a);
    std::future_status fs2 = f3.wait_for(std::chrono::seconds(2));
    if (fs2 == std::future_status::timeout) {
        std::cout << "the thread has not returned yet" << std::endl;
    } else if (fs2 == std::future_status::ready) {
        std::cout << "the thread has returned" << std::endl;
    }

    std::shared_future<int> sf1(std::move(f3));
    std::cout << sf1.get() << std::endl;
    std::shared_future<int> sf2(f3.share());
}

void testPackageTask() {
    auto a = [](int x) -> int {
        std::cout << "thread starts, thread id is " << std::this_thread::get_id() << std::endl;
        std::chrono::milliseconds sleepTime(3000);
        std::this_thread::sleep_for(sleepTime);
        std::cout << "thread ends, thread id is " << std::this_thread::get_id() << std::endl;
        return 3 + x;
    };
    std::cout << "main thread id is " << std::this_thread::get_id() << std::endl;
    std::packaged_task<int(int)> pt1(a);
    std::thread t1(std::ref(pt1), 10);
    t1.join();
    std::future<int> f1 = pt1.get_future();
    std::cout << f1.get() << std::endl;

    std::packaged_task<int(int)> pt2(a);
    pt2(10);
    std::future<int> f2 = pt2.get_future();
    std::cout << f2.get() << std::endl;
}

void testPromise() {
    auto a = [](std::promise<int> &p, int x) {
        x += 10;
        std::chrono::milliseconds sleepTime(3000);
        std::this_thread::sleep_for(sleepTime);
        p.set_value(x);
    };
    std::promise<int> p;
    std::thread t1(a, std::ref(p), 3);
    t1.join();
    std::future<int> f1 = p.get_future();
    std::cout << f1.get() << std::endl;
}

int main() {
    testMutex();
    return 0;
}
