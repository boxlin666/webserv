#ifndef CLUSTER_HPP
#define CLUSTER_HPP

#include <poll.h>

#include <map>
#include <vector>

#include "Connection.hpp"
#include "PassiveSocket.hpp"
#include "ServerConfig.hpp"

class Connection;

class Cluster {
   private:
    std::map<int, Connection*>                 _connection_map;
    std::map<int, PassiveSocket*>              _socket_map;   // (listen_fd , PassiveSocket ptr)
    std::map<int, std::vector<ServerConfig*> > _servers_map;  //(Port Number, Server vector)
    std::map<int, Connection*>                 _cgi_fd_map;

    std::vector<struct pollfd> _poll_fds;

    void open_listener(const ConfigParser& config);
    void init_servers_map(const ConfigParser& config);
    void init_poll_listen_fds();
    void add_to_poll_fds(
        int fd);  // helper function just to fill out the struct poll_fd (listen fd Or Client fd)

    void handle_new_connection(int listen_fd, PassiveSocket* passive_socket);
    void close_connection(size_t poll_idx);

    bool handle_client_data(size_t poll_idx);
    bool handle_client_read_event(size_t poll_idx);
    bool handle_client_write_event(size_t poll_idx);

    void _process_poll_errors(size_t index);
    void _dispatch_read_event(size_t index);
    void _dispatch_write_event(size_t index);

    static bool is_invalid_fd(const struct pollfd& pfd);
    void        cleanup_inactive_fds();
    void        _manage_cgi_lifecycle();

    Cluster(const Cluster& other);
    Cluster& operator=(const Cluster& other);

   public:
    Cluster(void);
    ~Cluster(void);

    void setup(const ConfigParser& config);

    void run(void);

    void print_socket_map() const;
    void print_pfds() const;
    void print_servers_map() const;

    void register_cgi_fd(int fd, short events, Connection* conn);
    void update_client_events(int client_fd, short new_events);

    const std::vector<struct pollfd>& get_poll_fds() const;
    void unregister_cgi_fd(int fd);
};

#endif