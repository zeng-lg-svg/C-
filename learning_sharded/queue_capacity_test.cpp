#include "../server/ChatServer2/ShardedExecutor.h"
#include <future>
#include <iostream>
#include <vector>

int main() {
    ShardedExecutor executor(1, 1);
    std::promise<void> started, release;
    auto started_future = started.get_future();
    auto release_future = release.get_future().share();
    std::vector<int> order;

    if (!executor.post("session", [&] {
        started.set_value();
        release_future.wait();
        order.push_back(1);
    })) return 1;
    if (started_future.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value();
        return 2;
    }

    bool ordinary_queued = executor.post("session", [&] { order.push_back(2); });
    bool ordinary_rejected = !executor.post("session", [&] { order.push_back(9); });
    bool cleanup_queued = executor.postCritical("session", [&] { order.push_back(3); });
    release.set_value();
    executor.shutdown();
    bool cleanup_rejected_after_stop = !executor.postCritical("session", [] {});

    if (!ordinary_queued || !ordinary_rejected || !cleanup_queued ||
        !cleanup_rejected_after_stop || order != std::vector<int>({1, 2, 3})) {
        std::cerr << "FAIL: queue limit, cleanup order or shutdown\n";
        return 3;
    }
    std::cout << "PASS: bounded queue, cleanup order, drain, reject after stop\n";
}
