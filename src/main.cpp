#include <unistd.h>

#include <cstring>
#include <iostream>

#include "Server.hpp"

int main()
{
    try {
        Server my_server(8080, "./www");
        my_server.setupListener();

        while (true) {
            struct sockaddr_in client_addr;
            socklen_t          client_len = sizeof(client_addr);

            int client_fd = -1;
            while (client_fd < 0) 
            {
                client_fd = accept(my_server.getListenFd(), (struct sockaddr*)&client_addr, &client_len);
            }

            char buffer[1024];
            std::memset(buffer, 0, sizeof(buffer));
            if (recv(client_fd, buffer, 1023, 0) > 0) 
            {
                const char* response = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello from Server Class!</h1>";
                send(client_fd, response, std::strlen(response), 0);
            }
            close(client_fd);
        }
    } catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}