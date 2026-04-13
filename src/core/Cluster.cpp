#include "Cluster.hpp"

Cluster::Cluster(void)
{

}

Cluster::~Cluster(void)
{
    //TO DO LATER
    // delete ptr inside _server_map _connection_map 
}

//void    Cluster::setup(const ConfigParse& config);
//temporary one, should be rewritten!
//init new_server hard_code instead of providing config input data
void    Cluster::setup(void)
{
    Server *new_server = new Server(8080, "./www");
    new_server->setupListener();
    int listen_fd = new_server->getListenFd();

    this->_server_map[listen_fd] = new_server;
}

void    Cluster::run(void)
{
    //temporary setup should be later replaced by poll funciton !
    Server *targetServer = _server_map.begin()->second;

    while (true)
    {
            struct sockaddr_in client_addr;
            socklen_t          client_len = sizeof(client_addr);

            int client_fd = -1;
            while (client_fd < 0)
            {
                client_fd = accept(targetServer->getListenFd(), (struct sockaddr*)&client_addr, &client_len);
            }
            //NEED TO BE REPLACED BY CONNECTION INSTANCE...
            char buffer[1024];
            std::memset(buffer, 0, sizeof(buffer));
            if (recv(client_fd, buffer, 1023, 0) > 0) 
            {
                const char* response = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello from Server Class!</h1>";
                send(client_fd, response, std::strlen(response), 0);
            }
            close(client_fd);
    }
}