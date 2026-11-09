#include <boost/asio.hpp>
#include <chrono>
#include <iostream>

int main()
{
    boost::asio::io_context ioc;
    boost::asio::steady_timer timer(
        ioc, std::chrono::seconds(2));

    timer.async_wait([](const boost::system::error_code& ec) {
        if (ec) {
            std::cout << "timer error: " << ec.message() << '\n';
            return;
        }
        std::cout << "timer fired" << std::endl;
    });

    std::cout << "before run" << std::endl;
    ioc.run();
    std::cout << "after run" << std::endl;
}
