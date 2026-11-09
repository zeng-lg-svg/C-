#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>

int main()
{
    boost::asio::io_context ioc;
    auto work = std::make_unique<boost::asio::io_context::work>(ioc);

    std::thread worker([&ioc]() {
        std::cout << "run started" << std::endl;
        ioc.run();
        std::cout << "run ended" << std::endl;
    });

    std::cout << "Press Enter to submit a task" << std::endl;
    std::cin.get();

    boost::asio::post(ioc, []() {
        std::cout << "task running" << std::endl;
    });

    work.reset();
    worker.join();
}
