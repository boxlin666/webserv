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
    //just pick up the first Server ptr inside server_map as example
    Server *targetServer = _server_map.begin()->second;

    while (true)
    {
            struct sockaddr_in client_addr;
            socklen_t          client_len = sizeof(client_addr);
            Connection *conn = NULL;
            int client_fd = -1;
            ssize_t recv_len = -1;

            while (client_fd < 0)
            {
                client_fd = accept(targetServer->getListenFd(), (struct sockaddr*)&client_addr, &client_len);
                conn = new Connection(client_fd, targetServer);
                this->_connection_map.insert(std::make_pair(client_fd, conn));
            }

            char    tmp_buff[1024];
            std::memset(tmp_buff, 0, sizeof(tmp_buff));
           
            recv_len = recv(client_fd, tmp_buff, 1023, 0);
            if (recv_len > 0)
            {
                conn->append_in_buff(tmp_buff, recv_len);
                conn->set_out_buff();
                send(client_fd, conn->get_out_buff().c_str(), conn->get_out_buff().size(),0);
            }
            close(client_fd);
    }
}