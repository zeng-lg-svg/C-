cmake_minimum_required(VERSION 3.16)

project(StatusServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.81.0 EXACT CONFIG REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
pkg_check_modules(HIREDIS REQUIRED IMPORTED_TARGET hiredis)
set(MYSQLCPP_ROOT
    "/home/zzy/learning-linux/llfcchat/third_party/mysql-connector-c++-8.0.33-linux-glibc2.28-x86-64bit"
)
add_executable(StatusServer 
AsioIOServicePool.cpp
ChatGrpcClient.cpp
ConfigMgr.cpp
DistLock.cpp
message.grpc.pb.cc
message.pb.cc
MysqlDao.cpp
MysqlMgr.cpp
RedisMgr.cpp
StatusServer.cpp
StatusServiceImpl.cpp)

target_include_directories(StatusServer PRIVATE
    "${MYSQLCPP_ROOT}/include"
)

target_link_libraries(StatusServer PRIVATE
    Boost::system
    Threads::Threads
    PkgConfig::JSONCPP
    protobuf::libprotobuf
    gRPC::grpc++
    PkgConfig::HIREDIS
        Boost::filesystem
        "${MYSQLCPP_ROOT}/lib64/libmysqlcppconn.so"
    )

