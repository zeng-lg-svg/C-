#include <boost/asio.hpp>
#include <iostream>
#include <memory>

struct Session : std::enable_shared_from_this<Session> {
    boost::asio::io_context& ioc;

    explicit Session(boost::asio::io_context& ctx) : ioc(ctx) {}

    ~Session() {
        std::cout << "Session destroyed" << std::endl;
    }

    void start() {
        auto self = shared_from_this();
        boost::asio::post(ioc, [self]() {
            std::cout << "callback running" << std::endl;
        });
    }
};

int main()
{
    boost::asio::io_context ioc;
    auto session = std::make_shared<Session>(ioc);

    session->start();
    session.reset();
    std::cout << "outside pointer released" << std::endl;

    ioc.run();
}
