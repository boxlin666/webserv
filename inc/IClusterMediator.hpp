#ifndef ICLUSTER_MEDIATOR
# define ICLUSTER_MEDIATOR

#include "Connection.hpp"

class Connection;

class IClusterMediator
{
    public:
        virtual ~IClusterMediator() {}; 

        virtual void register_cgi_fd(int fd, short events, Connection* conn) = 0;
        virtual void unregister_cgi_fd(int fd) = 0;
        virtual void update_client_events(int client_fd, short new_events) = 0;
};

#endif
