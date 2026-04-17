#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <vector>

# define RECEIVE 0
# define SEND 1
# define ERROR 2

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
void handle_client(struct pollfd &client_pfd, int status)
{
    char    buffer[1024];  // 42 建议： buffer 大小要适中
    ssize_t bytes_read;

    // 1. 初始化 buffer，确保字符串有 null 终止符
    memset(buffer, 0, sizeof(buffer));

    // 2. 读取客户端发来的数据
    if (status == RECEIVE)
    {
        bytes_read = recv(client_pfd.fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read < 0) 
        {
            std::cerr << "Read error" << std::endl;
        } 
        else if (bytes_read == 0) 
        {
            std::cout << "Client closed connection" << std::endl;
        }
        //假设请求完整, 否则无法从RECEIVE模式跳转到SEND模式！
        //Prepare SEND MODE for the next time!
        client_pfd.events |= POLLOUT;
    }
    else if (status == SEND)
    {
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

        int ret = send(client_pfd.fd, response, strlen(response), 0);
        //发送数据过大，内核来不及处理
        if (ret > 0 && ret < strlen(response))
            client_pfd.events |= POLLOUT; //除了初始化加入的POLLIN 加入新的追踪事件
        else if (ret == strlen(response))//数据传输完毕
            client_pfd.events &= ~POLLOUT; //关闭POLLOUT追踪事件
        else if (ret ==-1, errno == EAGAIN || errno == EWOULDBLOCK)
            client_pfd.events |= POLLOUT;
    }
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

        //临时vector 容器 在for循环结束后把tmps_fds的头部插到vector fds容器的尾部,进行添加
        std::vector<struct pollfd>  tmp_fds;

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
                    tmp_fds.push_back(client_pfd);
                }
                //已经被连接上的客户端想要发送一个请求(recv),可读事件就绪
                else
                {
                    handle_client(fds[i], RECEIVE);
                }
            }
            //已经连接的客户，可以让服务器写东西，写数据事件就绪了！ （send）
            if (fds[i].revents & POLLOUT)
            {
                handle_client(fds[i], SEND);
            }
            //处理异常关闭
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                close(fds[i].fd); 
                fds[i].fd = -1;
            }
        }

        // 增加：把这一轮新来的新客加入主队列，为下一轮 poll 做准备
        if (!tmp_fds.empty())
            fds.insert(fds.end(), tmp_fds.begin(), tmp_fds.end());

        // 清理：把所有被标记为 -1 的“烂车厢”一次性踢走
        //if (!tmp_fds.empty())
            //to do  remove if  blabla
    }
}
