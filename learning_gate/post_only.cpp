#include <boost/asio.hpp>
#include <iostream>

int main()
{
    boost::asio::io_context ioc;

    std::cout << "before post" << std::endl;

    boost::asio::post(ioc, []() {
        std::cout << "handler running" << std::endl;
    });

    std::cout << "after post" << std::endl;
}
