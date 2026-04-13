#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

/**
 * @brief 简单的 Socket Server 初始化
 * @return 成功返回 listen 状态的 fd，失败返回 -1
 */
int init_server(int port)
{
    int                server_fd;
    struct sockaddr_in address;
    int                opt;

    opt = 1;
    // 1. 创建 Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-1);

    // 2. 设置端口复用 (避免 Address already in use 报错)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) return (-1);

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(port);

    // 3. 绑定端口
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) return (-1);

    // 4. 开始监听
    if (listen(server_fd, 3) < 0) return (-1);

    return (server_fd);
}

/**
 * @brief 处理单个客户端的请求
 * @param client_fd 已建立连接的 socket
 */
void handle_client(int client_fd)
{
    char    buffer[1024];  // 42 建议： buffer 大小要适中
    ssize_t bytes_read;

    // 1. 初始化 buffer，确保字符串有 null 终止符
    memset(buffer, 0, sizeof(buffer));

    // 2. 读取客户端发来的数据
    bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0) {
        std::cerr << "Read error" << std::endl;
    } else if (bytes_read == 0) {
        std::cout << "Client closed connection" << std::endl;
    } else {
        // 打印出浏览器发来的 HTTP Header
        std::cout << "--- Received Request ---\n"
                  << buffer << "\n------------------------" << std::endl;

        // 3. 按照 HTTP 协议回一个简单的响应
        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 18\r\n"  // <h1>Hello 42!</h1> 刚好 18 字节
            "\r\n"
            "<h1>Hello 42!</h1>";
        send(client_fd, response, strlen(response), 0);
    }
    // 4. 完成后立即关闭 client_fd
    close(client_fd);
}

/**
 * @brief 运行服务器主循环
 */
void run_server(int server_fd)
{
    int                client_fd;
    struct sockaddr_in client_addr;
    socklen_t          addr_len;

    addr_len = sizeof(client_addr);
    std::cout << "Server is running... Press Ctrl+C to stop." << std::endl;

    while (true) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;  // 42 精神：一个连接失败不应该让整个服务器挂掉
        }
        handle_client(client_fd);
    }
}
