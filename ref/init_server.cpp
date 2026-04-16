#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <vector>

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
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
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

    //1.创建一个可以存储struct pollfd的vector容器
    std::vector<struct pollfd> fds;
   
    //2.为passive_socket 创建一个struct pollfd结构体
    struct pollfd listen_pfd;
    listen_pfd.fd = server_fd;
    //events 想要追踪的事件
    listen_pfd.events = POLLIN;
    //revents 内核根据想要追踪的事件的返回值
    listen_pfd.revents = 0;

    //3.把这个listen_pfd存入fds数组中
    fds.push_back(listen_pfd);

    addr_len = sizeof(client_addr);

    std::cout << "Server is running... Press Ctrl+C to stop." << std::endl;

    while (true) 
    {
        //fds.data() 数组的第一个元素的内存地址
        // timeout = -1，当服务端没有收到任何客户端的信息的时候，程序阻塞在poll处（防止忙占 CPU爆满）
        // 哈哈没错在非阻塞模式下，我们仍然有阻塞模式存在为了让轮询阶段提高效率
        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0)
            break;
        for (int i = 0; i < fds.size(); i++)
        {
            //无就绪事件在此fd发生
            if (fds[i].revents == 0)
                continue ;
            
            //有可读数据事件就绪
            if (fds[i].revents & POLLIN)
            {
                //有新的客户端想要连接 
                if (fds[i].fd == server_fd)
                {
                    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                    if (client_fd < 0) 
                    {
                        std::cerr << "Accept failed" << std::endl;
                        continue;  // 42 精神：一个连接失败不应该让整个服务器挂掉
                    }
                    //设置client_fd为非阻塞模式
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);

                    //4. 为了active socket创建一个struct pollfd结构体
                    struct pollfd client_pfd;
                    client_pfd.fd = client_fd;
                    client_pfd.events = POLLIN;
                    client_pfd.revents = 0;

                    //5. 把这个结构体存入vector容器
                    fds.push_back(client_pfd);
                }
                //已经被连接上的客户端想要发送一个请求
                else
                {
                    handle_client(fds[i].fd);
                } 
            }
            //缓冲区被情况，可写入数据事件就绪，send 发送文件过大
            if (fds[i].revents & POLLOUT)
            {

            }
            //处理异常关闭
            if (fds[i].revents & (POLLERR | POLLHUP))
            {


            }
        }
}
