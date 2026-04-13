#ifndef CLUSTER_HPP
# define CLUSTER_HPP

#include "PassiveSocket.hpp"
#include "Server.hpp"
#include "Connection.hpp"
#include <map>
#include <vector>

class Cluster
{
    private:
        std::map<int, Server *> _server_map;
        std::map<int, Connection *> _connection_map;
       
        //TO ADD LATER
        //std::vector<struct pollfd> _poll_fds;

        Cluster(const Cluster& other);
        Cluster& operator=(const Cluster& other);

    public:
        Cluster(void);
        ~Cluster(void);

        //TO DO LATER
        //void    setup(const ConfigParser& config);

        void    run(void);
};

#endif