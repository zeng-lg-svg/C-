#include <climits>
#include <iostream>
#include <string>
#include <unordered_map>

struct ChatServer {
    std::string name;
    int con_count;
};

int main()
{
    std::unordered_map<std::string, ChatServer> servers;
    servers["chatserver1"] = {"chatserver1", 3};
    servers["chatserver2"] = {"chatserver2", 1};
if (servers.empty()) {
    std::cerr << "no configured servers\n";
    return 1;
}
    auto selected = servers.begin()->second;

    for (const auto& entry : servers) {
        if (entry.second.con_count < selected.con_count) {
            selected = entry.second;
        }
    }
if (selected.con_count == INT_MAX) {
    std::cerr << "no usable server stats\n";
    return 1;
}
    std::cout << "selected: " << selected.name << '\n';
    std::cout << "count: " << selected.con_count << '\n';
}
