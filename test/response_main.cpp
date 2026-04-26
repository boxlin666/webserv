#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include <iostream>

int main() {
    // 0. 初始化全局 Map（状态码、后缀名映射等）
    HttpResponse::init_response_map();

    // 1. 创建对象
    HttpRequest req;
    HttpResponse res;
    ServerConfig config; 
    
    // 注意：在这里建议给 config 设置一个 root 路径，
    // 否则 HttpResponse 在查找 ./www/index.html 时可能会因为找不到根目录而失败。
    // config.set_root("."); 

    std::cout << "--- [Step 1] Manually Filling HttpRequest using Setters ---" << std::endl;

    // 2. 使用 Setter 函数填充数据 (模拟解析后的结果)
    req.set_method("GET");
    req.set_path("/index.html");
    req.set_version("HTTP/1.1");
    
    // 填充 Header
    req.set_header("Host", "localhost");
    req.set_header("User-Agent", "Manual-Mock-Client");
    req.set_header("Accept", "text/html");
    req.set_header("Connection", "keep-alive");

    // 填充 Body (如果是 GET，通常为空)
    req.set_body(""); 

    // 关键：手动设置状态为 FINISHED
    // 只有状态为 FINISHED，HttpResponse::build 内部的逻辑才会继续执行
    req.set_state(HttpRequest::PARSE_FINISHED);

    std::cout << "✅ HttpRequest members filled via setters." << std::endl;

    std::cout << "\n--- [Step 2] Building HttpResponse ---" << std::endl;

    // 3. 触发 Response 构建
    // build 会根据 req 的内容去磁盘找文件，并填充 res 的内部成员
    res.build(req, config);

    // 4. 截获输出
    const std::string& final_response = res.get_full_response();

    std::cout << "--- [Step 3] Verification ---" << std::endl;
    if (final_response.empty()) {
        std::cout << "❌ Error: Response is empty! Check if the file path exists." << std::endl;
    } else {
        std::cout << "Full Response captured:\n" << std::endl;
        std::cout << "=====================================" << std::endl;
        std::cout << final_response << std::endl;
        std::cout << "=====================================" << std::endl;
    }

    // 5. 可选：测试 HttpResponse 的隔离 Setter
    /*
    std::cout << "\n--- [Bonus] Testing HttpResponse Isolation ---" << std::endl;
    res.reset();
    res.set_status_code(404);
    res.set_body("File Not Found by Mock");
    res.force_append_full_response(); 
    std::cout << res.get_full_response() << std::endl;
    */

    return 0;
}