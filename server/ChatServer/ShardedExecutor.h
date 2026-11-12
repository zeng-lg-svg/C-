#pragma once
// 每个分片拥有一个先进先出队列和一个工作线程。
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class ShardedExecutor {
    struct Shard {
        std::mutex mutex;
        std::condition_variable ready;
        std::queue<std::function<void()>> tasks;
        bool stopping = false;
        std::thread worker;
    };
    std::vector<std::unique_ptr<Shard>> shards_;
    std::atomic<bool> stopping_{false};
    const std::size_t max_pending_per_shard_;
    std::mutex shutdown_mutex_;

    // 只在取任务时持锁，业务执行期间不占用队列锁。
    static void run(Shard& shard) {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(shard.mutex);
                shard.ready.wait(lock, [&] { return shard.stopping || !shard.tasks.empty(); });
                if (shard.stopping && shard.tasks.empty()) return;
                task = std::move(shard.tasks.front());
                shard.tasks.pop();
            }
            try { task(); }
            catch (const std::exception& e) { std::cerr << "task error: " << e.what() << '\n'; }
            catch (...) { std::cerr << "unknown task error\n"; }
        }
    }
public:
    explicit ShardedExecutor(std::size_t count, std::size_t max_pending_per_shard = 1024)
        : max_pending_per_shard_(max_pending_per_shard) {
        if (count == 0 || max_pending_per_shard == 0)
            throw std::invalid_argument("shard count and pending limit must be positive");
        for (std::size_t i = 0; i < count; ++i) shards_.emplace_back(new Shard());
        try {
            for (auto& shard : shards_) {
                Shard* ptr = shard.get();
                ptr->worker = std::thread([ptr] { run(*ptr); });
            }
        } catch (...) {
            shutdown();
            throw;
        }
    }
    ShardedExecutor(const ShardedExecutor&) = delete;
    ShardedExecutor& operator=(const ShardedExecutor&) = delete;
    ~ShardedExecutor() { shutdown(); }

    // 同一会话固定映射到同一分片；不同会话可能发生哈希碰撞。
    std::size_t shardFor(const std::string& session) const {
        return std::hash<std::string>{}(session) % shards_.size();
    }
    bool post(const std::string& session, std::function<void()> task) {
        return enqueue(session, std::move(task), false);
    }
    // 清理任务绕过普通任务的容量限制，仍与同会话任务按入队顺序执行。
    bool postCritical(const std::string& session, std::function<void()> task) {
        return enqueue(session, std::move(task), true);
    }
private:
    bool enqueue(const std::string& session, std::function<void()> task, bool critical) {
        if (!task || stopping_.load()) return false;
        Shard& shard = *shards_[shardFor(session)];
        {
            std::lock_guard<std::mutex> lock(shard.mutex);
            if (stopping_.load() || shard.stopping) return false;
            // 普通队列过载时拒绝入队；清理任务不受此上限约束。
            if (!critical && shard.tasks.size() >= max_pending_per_shard_) return false;
            shard.tasks.push(std::move(task));
        }
        shard.ready.notify_one();
        return true;
    }
public:
    // 应由外部线程调用；先停止生产者，再排空任务并等待线程退出。
    void shutdown() {
        std::lock_guard<std::mutex> guard(shutdown_mutex_);
        if (stopping_.exchange(true)) return;
        for (auto& shard : shards_) {
            {
                std::lock_guard<std::mutex> lock(shard->mutex);
                shard->stopping = true;
            }
            shard->ready.notify_one();
        }
        for (auto& shard : shards_) if (shard->worker.joinable()) shard->worker.join();
    }
};
