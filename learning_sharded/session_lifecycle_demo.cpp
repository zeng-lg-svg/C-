// 独立模型，不使用真实 socket、Redis 或 ChatServer。
#include "sharded_executor.h"
struct Session {
    explicit Session(std::string value) : id(std::move(value)) {}
    std::string id;
    std::atomic<bool> closed{false};
    std::atomic<bool> cleanup_enqueued{false};
    std::atomic<bool> online{false};
    std::atomic<int> cleanup_count{0};
};

bool closeAndQueueCleanup(ShardedExecutor& executor, const std::shared_ptr<Session>& s,
                          std::function<void()> cleanup) {
    s->closed.store(true); // 模拟网络侧标记连接已关闭。
    if (s->cleanup_enqueued.exchange(true)) return false;
    if (!executor.post(s->id, std::move(cleanup))) {
        s->cleanup_enqueued.store(false); // 投递失败，调用方必须处理清理尚未执行的情况。
        return false;
    }
    return true;
}

int main() {
    using namespace std::chrono;
    ShardedExecutor executor(2);

    // 场景一：登录检查前，已观察到连接关闭。
    auto first = std::make_shared<Session>("session-first");
    std::promise<void> login_started, resume_login, cleanup_done;
    auto started = login_started.get_future();
    auto resume = resume_login.get_future().share();
    auto cleaned = cleanup_done.get_future();
    executor.post(first->id, [first, &login_started, resume] {
        login_started.set_value();
        resume.wait();
        if (!first->closed.load()) first->online.store(true);
    });
    started.wait();
    bool queued = closeAndQueueCleanup(executor, first, [first, &cleanup_done] {
        first->online.store(false);
        first->cleanup_count.fetch_add(1);
        cleanup_done.set_value();
    });
    bool duplicate = closeAndQueueCleanup(executor, first, [] {});
    resume_login.set_value();
    bool case1 = cleaned.wait_for(seconds(2)) == std::future_status::ready &&
                 queued && !duplicate && !first->online.load() && first->cleanup_count.load() == 1;

    // 场景二：检查之后、写入在线状态之前发生断开。
    auto second = std::make_shared<Session>("session-second");
    std::promise<void> checked, allow_publish, published, allow_cleanup, cleanup_done2;
    auto check_done = checked.get_future();
    auto publish_gate = allow_publish.get_future().share();
    auto published_done = published.get_future();
    auto cleanup_gate = allow_cleanup.get_future().share();
    auto cleaned2 = cleanup_done2.get_future();
    executor.post(second->id, [second, &checked, publish_gate, &published] {
        if (second->closed.load()) return;
        checked.set_value();
        publish_gate.wait();
        second->online.store(true);
        published.set_value();
    });
    check_done.wait();
    bool queued2 = closeAndQueueCleanup(executor, second, [second, cleanup_gate, &cleanup_done2] {
        cleanup_gate.wait();
        second->online.store(false);
        second->cleanup_count.fetch_add(1);
        cleanup_done2.set_value();
    });
    allow_publish.set_value();
    bool transient = published_done.wait_for(seconds(2)) == std::future_status::ready &&
                     second->closed.load() && second->online.load();
    allow_cleanup.set_value();
    bool case2 = cleaned2.wait_for(seconds(2)) == std::future_status::ready &&
                 queued2 && !second->online.load() && second->cleanup_count.load() == 1;

    executor.shutdown();
    auto third = std::make_shared<Session>("session-third");
    bool rejected = !closeAndQueueCleanup(executor, third, [third] { third->cleanup_count.fetch_add(1); });
    if (!case1 || !transient || !case2 || !rejected) {
        std::cerr << "FAIL: lifecycle model\n"; return 1;
    }
    std::cout << "PASS: close before check, duplicate cleanup suppressed\n"
              << "PASS: close after check can temporarily publish online; queued cleanup removes it\n"
              << "PASS: post-shutdown cleanup rejected and must be handled by caller\n";
}
