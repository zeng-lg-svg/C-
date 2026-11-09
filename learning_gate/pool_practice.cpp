#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <iostream>
#include <utility>
#include <thread>
#include<functional>
#include <stdexcept>
class MiniPool {
public:
    std::unique_ptr<int> take();
void giveBack(std::unique_ptr<int> con);
    MiniPool() {
    pool_.push(std::make_unique<int>(101));
}
private:
    std::queue<std::unique_ptr<int>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

std::unique_ptr<int> MiniPool::take() {
    // 你来实现
std::unique_lock<std::mutex> lock_(mutex_);

cv_.wait(lock_, [this]() {
    if (!stopped_ && pool_.empty()) {
        std::cout << "[pool] empty, waiting" << std::endl;
    }
    return stopped_ || !pool_.empty();
});

if (stopped_) {
    return nullptr;
}

std::unique_ptr<int> k = std::move(pool_.front());
pool_.pop();
return k;
}

void MiniPool::giveBack(std::unique_ptr<int> con) {
    // 你来实现
    std::unique_lock<std::mutex> lock_(mutex_);
    if (stopped_ || !con) return;
    pool_.push(std::move(con));
    cv_.notify_one();
}
class Defer {
public:
    Defer(std::function<void()> func) : func_(func) {}
    ~Defer() { func_(); }

private:
    std::function<void()> func_;
};
bool checkDemo(MiniPool& pool) {
    auto con = pool.take();
    if (!con) return false;

    // 在这里创建局部 Defer
    // 让析构回调执行：pool.giveBack(std::move(con));
	Defer guard([&pool,&con](){pool.giveBack(std::move(con));});
    throw std::runtime_error("simulated query failure"); // 模拟密码错误，提前返回
}
int main() {
    MiniPool pool;

    try {
        checkDemo(pool);
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << std::endl;
    }

    auto con = pool.take();
    if (!con) return 1;

    std::cout << "available id: " << *con << std::endl;
    pool.giveBack(std::move(con));
}
