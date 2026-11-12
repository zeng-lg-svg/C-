#include "sharded_executor.h"

int main() {
    ShardedExecutor executor(2);
    std::string a = "session-A", b;
    for (int i = 0; i < 1000; ++i) {
        auto candidate = "session-" + std::to_string(i);
        if (executor.shardFor(candidate) != executor.shardFor(a)) { b = candidate; break; }
    }
    if (b.empty()) { std::cerr << "could not find another shard\n"; return 1; }

    std::promise<void> a_started, release_a, same_done, other_done;
    auto started = a_started.get_future();
    auto release = release_a.get_future().share();
    auto same = same_done.get_future();
    auto other = other_done.get_future();
    executor.post(a, [&] { a_started.set_value(); release.wait(); });
    started.wait();
    executor.post(a, [&] { same_done.set_value(); });
    executor.post(b, [&] { other_done.set_value(); });
    const bool other_ran = other.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    const bool same_waited = same.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
    release_a.set_value();
    executor.shutdown(); // 等待已接收的任务执行完毕
    const bool same_ran = same.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    const bool rejected = !executor.post(a, [] {});
    if (!other_ran || !same_waited || !same_ran || !rejected) {
        std::cerr << "FAIL: check shard concurrency or shutdown\n"; return 1;
    }
    std::cout << "PASS: same session waits, another shard runs, shutdown drains and rejects new work\n";
}
