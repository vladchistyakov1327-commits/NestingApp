#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// ThreadPool — пул потоков для параллельных вычислений
// Используется: параллельная оценка популяций ГА, параллельный NFP
// ═══════════════════════════════════════════════════════════════════════════════
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads = 0) {
        size_t n = threads > 0 ? threads : std::max(1u, std::thread::hardware_concurrency());
        workers_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] {
                            return stop_.load() || !tasks_.empty();
                        });
                        if (stop_.load() && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                    --pending_;
                    done_.notify_all();
                }
            });
        }
    }

    ~ThreadPool() {
        stop_.store(true);
        condition_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    // Добавить задачу и получить future
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using RetType = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<RetType> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_.load()) throw std::runtime_error("ThreadPool: enqueue on stopped pool");
            ++pending_;
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return res;
    }

    // Ждать завершения всех задач
    void waitAll() {
        std::unique_lock<std::mutex> lock(queueMutex_);
        done_.wait(lock, [this] { return pending_.load() == 0 && tasks_.empty(); });
    }

    size_t threadCount() const { return workers_.size(); }

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        queueMutex_;
    std::condition_variable           condition_;
    std::condition_variable           done_;
    std::atomic<bool>                 stop_{false};
    std::atomic<int>                  pending_{0};
};
