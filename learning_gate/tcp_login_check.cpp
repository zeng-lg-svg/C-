#include <boost/asio.hpp>
#include <iostream>
#include <exception>
#include <string>
#include <json/json.h>
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
int main()
{
try{
    boost::asio::io_context ioc;

    // 下一行：创建名为 socket 的 TCP 套接字，构造参数传入 ioc
    boost::asio::ip::tcp::socket socket(ioc);
    boost::asio::ip::tcp::endpoint endpoint(
    boost::asio::ip::make_address("127.0.0.1"),8990);
    socket.connect(endpoint);
    std::cout << "connected" << std::endl;
    std::string token;
std::cout << "Enter token: ";
std::cin >> token;
Json::Value root;
root["uid"] = 1109;
root["token"] = token;
Json::StreamWriterBuilder writer;
writer["indentation"] = "";
std::string body = Json::writeString(writer, root);
std::cout << "body bytes: " << body.size() << std::endl;

std::uint16_t net_id = htons(1005);
std::uint16_t net_len =
    htons(static_cast<std::uint16_t>(body.size()));
char header[4];
std::memcpy(header, &net_id, 2);
std::memcpy(header + 2, &net_len, 2);
boost::asio::write(socket, boost::asio::buffer(header, sizeof(header)));
boost::asio::write(socket, boost::asio::buffer(body));

char response_header[4];
boost::asio::read(socket, boost::asio::buffer(response_header));
std::uint16_t response_id;
std::uint16_t response_len;
std::memcpy(&response_id, response_header, 2);       // 前两字节：消息编号
std::memcpy(&response_len, response_header + 2, 2);  // 后两字节：正文长度
response_id = ntohs(response_id);
response_len = ntohs(response_len);
std::string response_body(response_len, '\0');
boost::asio::read(socket, boost::asio::buffer(response_body));
std::cout << "response id: " << response_id << '\n';
std::cout << response_body << std::endl;
    // 放入你已有的代码：从创建 ioc 到打印 connected
} catch (const std::exception& e) {
    std::cerr << "Connect failed: " << e.what() << std::endl;
    return 1;
}
}
