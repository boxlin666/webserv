#ifndef CLUSTER_HPP
# define CLUSTER_HPP

#include "PassiveSocket.hpp"
#include "Server.hpp"
#include "Connection.hpp"
#include <map>
#include <vector>
#include <poll.h>

class Cluster
{
    private:
        std::map<int, Connection *> _connection_map;
        std::map<int, PassiveSocket *> _socket_map;
        std::vector <Server*> _servers;
        // std::vector <ServerConfig*> _virtual_servers;

        std::vector<struct pollfd> _poll_fds;

        Cluster(const Cluster& other);
        Cluster& operator=(const Cluster& other);

    public:
        Cluster(void);
        ~Cluster(void);

        //TODO
        //void    setup(const ConfigParser& config);

        // just temporary member function, supposed to be replaced by setup(const ConfigParser& config)
        void    setup(void);

        void    handle_new_connection(int listen_fd, PassiveSocket* passive_socket);
        void    close_connection(size_t poll_idx);
        bool    handle_client_data(size_t poll_idx);
        
        // void add_config
        void    run(void);


};

#endif