#include <iostream>
#include "Utils.hpp"

void debug_request_msg_print(const std::string &variable, const std::string &content)
{
    std::cout << "\n---start of " << variable  << "---" << std::endl; 
    // 使用 C++98 的经典迭代器遍历
    for (std::string::const_iterator it = content.begin(); it != content.end(); ++it) {
        char c = *it;
        if (c == '\r') {
            std::cout << "\\r";
        } else if (c == '\n') {
            std::cout << "\\n\n"; // 打印出 \n 两个字符，然后真正换行
        } else {
            std::cout << c;
        }
    } 
    std::cout << "\n---end of " << variable  << "---\n" << std::endl;
}