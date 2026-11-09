#include <cstring>
#include <iostream>
#include <string>

int main()
{
    char buffer[16] = "first";
    char* saved_ptr = buffer;
    std::string saved_copy = buffer;

    std::strcpy(buffer, "second");

    std::cout << std::boolalpha
              << "same address: " << (saved_ptr == buffer) << '\n';
    std::cout << "pointer sees: " << saved_ptr << '\n';
    std::cout << "copy sees: " << saved_copy << '\n';
}
