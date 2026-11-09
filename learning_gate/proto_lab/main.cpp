#include <iostream>
#include <string>
#include "message.pb.h"

int main()
{
    message::GetChatServerReq request;
    request.set_uid(1109);

    // 对象 → 二进制字节
    std::string bytes;
    if (!request.SerializeToString(&bytes)) {
        std::cerr << "encode failed\n";
        return 1;
    }

    // 二进制字节 → 另一个对象
    message::GetChatServerReq restored;
    if (!restored.ParseFromString(bytes)) {
        std::cerr << "decode failed\n";
        return 1;
    }

    std::cout << "bytes: " << bytes.size() << '\n';
    std::cout << "uid: " << restored.uid() << '\n';
}
