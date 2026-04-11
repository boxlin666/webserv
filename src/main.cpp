#include <unistd.h>

int init_server(int port);

void run_server(int server_fd); 

int main(void) {
    int server_fd;

    server_fd = init_server(8080); // 获取内核分配的 fd

    run_server(server_fd); // 传入正确的 fd
    
    close(server_fd);    // 良好的习惯：用完关掉
    return (0);
}