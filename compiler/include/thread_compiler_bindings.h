#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <memory>
#include <chrono>

namespace _ume_rt {

struct Thread {
    std::function<void()> task;
    std::shared_ptr<std::thread> thread_obj;
    bool is_alive = false;

    Thread(std::function<void()> t) : task(t) {}

    void start() {
        if (!thread_obj) {
            is_alive = true;
            thread_obj = std::make_shared<std::thread>([this]() {
                task();
                is_alive = false;
            });
        }
    }

    void join() {
        if (thread_obj && thread_obj->joinable()) {
            thread_obj->join();
            is_alive = false;
        }
    }

    bool isAlive() {
        return is_alive;
    }

    static void sleep(Long milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
    
    // Stub for current thread context if needed
    static std::shared_ptr<Thread> current() {
        return nullptr; // Hard to map cleanly without thread_local mapping overhead
    }
};

struct Mutex {
    std::mutex mtx;

    Mutex() {}

    void lock() {
        mtx.lock();
    }

    void unlock() {
        mtx.unlock();
    }

    bool tryLock() {
        return mtx.try_lock();
    }
};

struct ConditionVariable {
    std::condition_variable cv;

    ConditionVariable() {}

    void wait(std::shared_ptr<Mutex> m) {
        std::unique_lock<std::mutex> lk(m->mtx, std::adopt_lock);
        cv.wait(lk);
        lk.release(); // release so Mutex::unlock can still work safely later if user wrote it
    }

    void signal() {
        cv.notify_one();
    }

    void broadcast() {
        cv.notify_all();
    }
};

struct AtomicInt {
    std::atomic<Int> val;

    AtomicInt(Int initialValue) : val(initialValue) {}

    Int get() {
        return val.load();
    }

    void set(Int value) {
        val.store(value);
    }

    Int addAndGet(Int delta) {
        return val.fetch_add(delta) + delta;
    }

    Int getAndIncrement() {
        return val.fetch_add(1);
    }

    Int getAndDecrement() {
        return val.fetch_sub(1);
    }

    bool compareAndSet(Int expected, Int update) {
        return val.compare_exchange_strong(expected, update);
    }
};

template<typename T>
struct Task {
    std::shared_ptr<std::future<T>> fut;
    
    Task() {}
    
    // Assuming compiler populates this properly or user constructs it via static method
    T get() {
        if (fut) return fut->get();
        throw std::runtime_error("Task future not initialized");
    }

    void wait() {
        if (fut) fut->wait();
    }

    bool isDone() {
        if (fut) {
            return fut->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
        return false;
    }
};

// Void specialization for Task without return type
template<>
struct Task<void> {
    std::shared_ptr<std::future<void>> fut;
    
    Task() {}
    
    void get() {
        if (fut) fut->get();
        else throw std::runtime_error("Task future not initialized");
    }

    void wait() {
        if (fut) fut->wait();
    }

    bool isDone() {
        if (fut) {
            return fut->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
        return false;
    }
};

// ── Factory functions for `new Thread(...)` and `new Mutex()` ──
// The Ume compiler maps `new Thread(lambda)` → createThread(lambda)
// and `new Mutex()` → createMutex().
namespace Thread_bindings {
    inline std::shared_ptr<Thread> createThread(std::function<void()> task) {
        return std::make_shared<Thread>(task);
    }
    inline std::shared_ptr<Mutex> createMutex() {
        return std::make_shared<Mutex>();
    }
    inline std::shared_ptr<ConditionVariable> createConditionVariable() {
        return std::make_shared<ConditionVariable>();
    }
    inline std::shared_ptr<AtomicInt> createAtomicInt(Int initial = 0) {
        return std::make_shared<AtomicInt>(initial);
    }
}

}
