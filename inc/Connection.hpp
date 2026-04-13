#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class Connection
{
    private:
        int _client_fd;
        std::string _read_buff;
        std::string _write_buff;
        const Server *server;

        //TO DO LATER...
        HttpRequest *_request;
        HttpResponse *_response;

        //TO DO LATER (状态机)
        enum State 
        {
            READING, WRITING, CLOSED
        };
        State _state;

        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int client_fd, const Server *server);
        ~Connection(void);

        int getFd(void)const;
        //const std::string &get_read_buff(void)const;
        //const std::string &get_write_buff(void)const;

};

#endif