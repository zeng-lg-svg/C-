#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <json/json.h>
#include <sstream>
#include <string>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

int main()
{
    try {
        // 提供网络操作需要的执行上下文
        net::io_context ioc;

        // 在远程服务器的回环地址监听 8080
        tcp::acceptor acceptor(
            ioc,
            tcp::endpoint(net::ip::address_v4::loopback(), 8082)
        );

        std::cout << "Listening on 127.0.0.1:8082" << std::endl;

        while (true) {
            tcp::socket socket(ioc);

            // 等待并接受一个 TCP 连接
            acceptor.accept(socket);

            beast::flat_buffer buffer;
            http::request<http::string_body> request;
            boost::system::error_code ec;

            // 读取并解析一个完整的 HTTP 请求
            http::read(socket, buffer, request, ec);

            if (ec) {
                std::cerr << "Read failed: " << ec.message() << '\n';
                continue;
            }

            // 只记录方法和路径，不打印包含密码的请求体
            std::cout << request.method_string() << " "
                      << request.target() << std::endl;

            http::response<http::string_body> response{
                http::status::ok, request.version()
            };

            response.set(http::field::content_type, "application/json");
            response.keep_alive(false);

            if (request.method() == http::verb::post &&
                request.target() == "/user_login") {
                // 配置 JSON 解析器
Json::CharReaderBuilder reader;
reader["allowComments"] = false;
reader["failIfExtra"] = true;

// 输入：HTTP 请求体；输出：解析后的对象
std::istringstream input(request.body());
Json::Value root;
std::string parseErrors;

const bool parsed =
    Json::parseFromStream(reader, input, &root, &parseErrors);

// 准备业务响应
Json::Value result(Json::objectValue);

if (!parsed || !root.isObject()) {
    result["error"] = 1001;
    result["message"] = "invalid JSON object";
} else if (!root.isMember("email") || !root.isMember("passwd")) {
    result["error"] = 1002;
    result["message"] = "missing email or passwd";
} else if (!root["email"].isString() ||
           !root["passwd"].isString()) {
    result["error"] = 1003;
    result["message"] = "email and passwd must be strings";
} else {
    const std::string email = root["email"].asString();
    const std::string passwd = root["passwd"].asString();

    if (email.empty() || passwd.empty()) {
        result["error"] = 1004;
        result["message"] = "email or passwd is empty";
    } else {
        result["error"] = 0;
        result["message"] = "parameters accepted";

        // 下一步：在这里查询数据库，验证账号密码
    }
}

// 将业务响应序列化为 JSON 字符串
Json::StreamWriterBuilder writer;
writer["indentation"] = "";
response.body() = Json::writeString(writer, result);
            } else {
                response.result(http::status::not_found);
                response.body() = R"({"message":"route not found"})";
            }

            // 根据响应体设置 Content-Length 等必要信息
            response.prepare_payload();

            http::write(socket, response, ec);

            if (ec) {
                std::cerr << "Write failed: " << ec.message() << '\n';
            }

            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
    } catch (const std::exception& e) {
        std::cerr << "Server failed: " << e.what() << '\n';
        return 1;
    }
}
