#ifndef CLUSTER_HPP
# define CLUSTER_HPP

#include <map>
#include <vector>
#include <poll.h>

#include "PassiveSocket.hpp"
#include "Connection.hpp"
#include "ServerConfig.hpp"

class Cluster
{
    private:
        std::map<int, Connection *> _connection_map;
        std::map<int, PassiveSocket *> _socket_map; // (listen_fd , PassiveSocket ptr)
        std::map<int, std::vector<ServerConfig*> > _servers_map; //(Port Number, Server vector)

        std::vector<struct pollfd> _poll_fds;

        void    open_listener(const ConfigParser& config);
        void    init_servers_map(const ConfigParser& config);
        void    init_poll_listen_fds();
        void    add_to_poll_fds(int fd); //helper function just to fill out the struct poll_fd (listen fd Or Client fd) 

        void    handle_new_connection(int listen_fd, PassiveSocket* passive_socket);
        void    close_connection(size_t poll_idx);
     
        bool    handle_client_data(size_t poll_idx);
        bool    handle_client_read_event(size_t poll_idx);
        bool    handle_client_write_event(size_t poll_idx);

        static bool    is_invalid_fd(const struct pollfd& pfd);
        void    cleanup_inactive_fds();


        Cluster(const Cluster& other);
        Cluster& operator=(const Cluster& other);

    public:
        Cluster(void);
        ~Cluster(void);

        //TODO
        void    setup(const ConfigParser& config);
         
        // void add_config
        void    run(void);

        // void send 分片发送


        //print test check!
        void    print_socket_map()const;
        void    print_pfds()const;
        void    print_servers_map()const;
};

#endif