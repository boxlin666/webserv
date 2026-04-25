#include "HttpRequest.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

int main(void) {
    HttpRequest req;

    std::cout << "===== Starting HTTP Request Parser Test (Internal String) =====" << std::endl;

    // 1. 在程序内构造 HTTP Request 字符串
    // 使用 \r\n 模拟真实的 HTTP 网络报文格式
    // 最后的 \r\n\r\n 是必须的，代表 Header 结束
    std::string raw_data = 
        "GET /contact HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: curl/8.6.0\r\n"
        "Accept: */*\r\n"
        "Content-Length: 0\r\n"
        "\r\n"; // 结尾空行

    // 如果你想测试带 Body 的 POST，可以这样写：
    /*
    raw_data = 
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world";
    */

    std::cout << "--- Raw Request Content ---" << std::endl;
    // 为了方便调试，我们打印时把 \r 可视化
    for (size_t i = 0; i < raw_data.size(); ++i) {
        if (raw_data[i] == '\r') std::cout << "\\r";
        else if (raw_data[i] == '\n') std::cout << "\\n\n";
        else std::cout << raw_data[i];
    }
    std::cout << "---------------------------" << std::endl;

    // 2. 将数据输入解析器
    bool success = req.parse(raw_data);

    // 3. 打印解析结果
    if (success || req.get_state() == HttpRequest::PARSE_FINISHED) {
        std::cout << "\n[Success] Request Parsing Finished." << std::endl;
        
        std::cout << "Method:      [" << req.get_method() << "]" << std::endl;
        std::cout << "Path:        [" << req.get_path() << "]" << std::endl;
        std::cout << "Version:     [" << req.get_version() << "]" << std::endl;

        // 打印 Headers
        std::cout << "\nHeaders:" << std::endl;
        const std::map<std::string, std::string>& headers = req.get_header_map();
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            std::cout << "  " << std::left << std::setw(20) << it->first + ":" << it->second << std::endl;
        }

        // 打印 Body
        if (!req.get_body_content().empty()) {
            std::cout << "\nBody (" << req.get_body_len() << " bytes):" << std::endl;
            std::cout << "[" << req.get_body_content() << "]" << std::endl;
        } else {
            std::cout << "\nBody: [Empty]" << std::endl;
        }

    } else {
        std::cerr << "\n[Failure] Parser is in state: " << (int)req.get_state() << std::endl;
        if (req.get_state() == HttpRequest::PARSE_ERROR)
            std::cerr << "Message: Request format is invalid (Check your line endings or spaces)." << std::endl;
        else
            std::cerr << "Message: Incomplete request (Wait, did you forget the trailing \\r\\n\\r\\n?)" << std::endl;
    }

    return (0);
}