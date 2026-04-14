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
        std::string _in_buff;
        std::string _out_buff;
        const Server *_server;

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
        const std::string &get_in_buff(void)const;
        const std::string &get_out_buff(void)const;

        void append_in_buff(const char *tmp_buff, ssize_t recv_len);
        
        //Just a temporary function, we hard code the http response here...
        void set_out_buff(void);
};

#endif