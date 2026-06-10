#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include <stdexcept>

class ThreadPool
{
public:
    using Task = std::function<void()>;
    ThreadPool(size_t count) : _stop(false)
    {
        for (size_t i = 0; i < count; ++i) {
            _threads.emplace_back(&ThreadPool::run, this);
        }
    }

    ~ThreadPool()
    {
        stop();
    }

    void stop()
    {
        _stop.store(true);
        _task_cond.notify_all();
        for (auto& thread : _threads) {
            if (thread.joinable())
                thread.join();
        }
    }

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(_task_mutex);
            _tasks.emplace([task]() { (*task)(); });
        }

        _task_cond.notify_one();

        return future;
    }

private:
    void run()
    {
        while (!_stop.load()) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(_task_mutex);
                _task_cond.wait(lock, [this]() {
                    return !_tasks.empty() || _stop.load();
                });

                if (_stop.load()) break;

                task = std::move(_tasks.front());
                _tasks.pop();
            }

            if (task) task();
        }
    }

private:
    std::vector<std::thread>    _threads;
    std::queue<Task>            _tasks;
    std::mutex                  _task_mutex;
    std::condition_variable     _task_cond;
    std::atomic<bool>           _stop;
};

class GlobalThreadPool
{
public:
    static GlobalThreadPool& getInstance() {
        static GlobalThreadPool inst;
        return inst;
    }

    GlobalThreadPool(const GlobalThreadPool&) = delete;
    GlobalThreadPool& operator=(const GlobalThreadPool&) = delete;

    void init(size_t concurrency = 4) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (!_pool) _pool = std::make_unique<ThreadPool>(concurrency);
        if (!_serial_pool) _serial_pool = std::make_unique<ThreadPool>(1); 
    }

    void stop() {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_pool) _pool->stop();
        if (_serial_pool) _serial_pool->stop();
    }

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) {
        using return_type = typename std::invoke_result<F, Args...>::type;

        if (!_pool) {
            std::promise<return_type> prom;
            prom.set_exception(std::make_exception_ptr(std::runtime_error("not init")));
            return prom.get_future();
        }

        return _pool->enqueue(std::forward<F>(f), std::forward<Args>(args)...);
    }

    template <typename F, typename... Args>
    auto enqueue_serial(F&& f, Args&&... args) {
        using return_type = typename std::invoke_result<F, Args...>::type;

        if (!_serial_pool) {
            std::promise<return_type> prom;
            prom.set_exception(std::make_exception_ptr(std::runtime_error("not init")));
            return prom.get_future();
        }

        return _serial_pool->enqueue(std::forward<F>(f), std::forward<Args>(args)...);
    }

private:
    GlobalThreadPool() = default;
    ~GlobalThreadPool() { stop(); }

    std::unique_ptr<ThreadPool> _pool;       
    std::unique_ptr<ThreadPool> _serial_pool;
    std::mutex _mtx;
};