#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include <iostream>

int main() {
    // 0. 初始化全局 Map（状态码等）
    HttpResponse::init_response_map();

    // 1. 创建对象
    HttpRequest req;
    HttpResponse res;
    ServerConfig config; // 假设你的配置类有默认构造

    std::cout << "--- [Step 1] Manually Filling HttpRequest ---" << std::endl;

    // 2. 手动填充私有成员 (模拟解析后的结果)
    req._method = "GET";
    req._path = "./www/index.html";
    req._http_version = "HTTP/1.1";
    
    // 填充 Header
    req._header_map["Host"] = "localhost";
    req._header_map["User-Agent"] = "Manual-Mock-Client";
    req._header_map["Accept"] = "text/html";
    req._header_map["Connection"] = "keep-alive";

    // 填充 Body (如果是 POST)
    req._body_content = ""; 
    req._body_len = 0;

    // 关键：手动设置状态为 FINISHED，否则 HttpResponse 可能会拒绝处理
    req._state = HttpRequest::PARSE_FINISHED;

    std::cout << "✅ HttpRequest members filled manually." << std::endl;

    // 3. 配置 Mock (根据你的 HttpResponse 逻辑调整)
    // 确保 res 能够找到资源。如果 res 内部会调用 config.get_root()，
    // 请确保 config 此时有正确的数据。
    
    std::cout << "\n--- [Step 2] Building HttpResponse ---" << std::endl;

    // 4. 触发 Response 构建
    res.build(req, config);

    // 5. 截获输出
    const std::string& final_response = res.get_full_response();

    std::cout << "--- [Step 3] Verification ---" << std::endl;
    if (final_response.empty()) {
        std::cout << "❌ Error: Response is empty!" << std::endl;
    } else {
        std::cout << "Full Response captured:\n" << std::endl;
        std::cout << "=====================================" << std::endl;
        std::cout << final_response << std::endl;
        std::cout << "=====================================" << std::endl;
    }

    return 0;
}