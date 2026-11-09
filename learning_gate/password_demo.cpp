#include <sodium.h>
#include <iostream>
#include <string>

int main()
{
    if (sodium_init() < 0) {
        std::cerr << "libsodium initialization failed\n";
        return 1;
    }

    // 仅用于本次练习的固定测试密码
    const std::string password = "Demo123456!";

    char hash[crypto_pwhash_STRBYTES];

    // 生成包含随机盐和算法参数的密码哈希字符串
    if (crypto_pwhash_str(
            hash,
            password.c_str(),
            password.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        std::cerr << "Password hashing failed\n";
        return 1;
    }

    const bool correctAccepted =
        crypto_pwhash_str_verify(
            hash, password.c_str(), password.size()) == 0;

    const std::string wrongPassword = "wrong-password";

    const bool wrongAccepted =
        crypto_pwhash_str_verify(
            hash, wrongPassword.c_str(), wrongPassword.size()) == 0;

    std::cout << std::boolalpha;
    std::cout << "Correct password accepted: " << correctAccepted << '\n';
    std::cout << "Wrong password accepted: " << wrongAccepted << '\n';

    if (!correctAccepted || wrongAccepted) {
        std::cerr << "Verification test failed\n";
        return 1;
    }

    // 输出一条包含真实哈希的测试账号插入语句
    std::cout
        << "INSERT INTO users (email, password_hash) VALUES "
        << "('student@example.com', '" << hash << "');\n";
}
