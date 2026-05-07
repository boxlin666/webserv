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


    // 构造一个带有 Message Body 的 GET 请求
    // 注意：虽然 GET 带 Body 不常见，但 HTTP 协议允许这样做，
    // 你的服务器必须能通过 Content-Length 正确识别并截取它。
    
    std::string raw_data = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: curl/8.5.0\r\n"
        "Accept: */*\r\n"
        "Content-Length: 22\r\n"  // 必须与 Body 长度严格一致
        "\r\n"                    // Header 结束标志
        "this is my secret body"; // 正好 22 个字符

    

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
    (void) success;
    // 3. 打印解析结果
    if (req.get_state() == HttpRequest::PARSE_FINISHED) {
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
        if (!req.get_body().empty()) {
            std::cout << "\nBody (" << req.get_body_len() << " bytes):" << std::endl;
            std::cout << "[" << req.get_body() << "]" << std::endl;
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