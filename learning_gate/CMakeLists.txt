cmake_minimum_required(VERSION 3.16)

project(LearningGate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.66 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
add_executable(mini_gate_cmake main.cpp)
target_link_libraries(mini_gate_cmake PRIVATE
    Boost::system
    Threads::Threads
    PkgConfig::JSONCPP
)

