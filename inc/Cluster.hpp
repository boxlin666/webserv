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
        std::map<int, Server *> _server_map; //could have multiple server for each port
        std::map<int, Connection *> _connection_map;
       
        //TO ADD LATER
        std::vector<struct pollfd> _poll_fds;

        Cluster(const Cluster& other);
        Cluster& operator=(const Cluster& other);

    public:
        Cluster(void);
        ~Cluster(void);

        //TO DO LATER
        //void    setup(const ConfigParser& config);

        // just temporary member function, supposed to be replaced by setup(const ConfigParser& config)
        void    setup(void);

        void    handle_new_connection(int listen_fd, Server* server);
        void    close_connection(size_t poll_idx);
        bool    handle_client_data(size_t poll_idx);
        
        void    run(void);


};

#endif