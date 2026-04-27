#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include "Server.hpp"
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class Connection
{
    private:
        int _client_fd;
        std::string _in_buff;
        std::string _out_buff;
        PassiveSocket *_matched_socket;
        ServerConfig *_matched_server;

        HttpRequest _request;
        HttpResponse _response;

        //TO DO LATER (状态机)
        enum State 
        {
            READING, WRITING, CLOSED
        };
        State _state;

        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int client_fd, PassiveSocket *matched_socket);
        ~Connection(void);

        int getFd(void)const;
        const std::string &get_in_buff(void)const;
        const std::string &get_out_buff(void)const;

        void append_in_buff(const char *tmp_buff, ssize_t recv_len);
        
        //Just a temporary function, we hard code the http response here...
        void set_out_buff(void);

        bool handle_data(const char* raw_data, ssize_t size);
        bool check_parse_finished();
        std::string prepare_response();
};

#endif