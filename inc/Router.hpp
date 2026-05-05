#ifndef ROUTER_HPP
# define ROUTER_HPP

#include <string> 
#include <iterator>
#include <iostream>

#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include "Location.hpp"

struct RouterCtx
{
    const location* loc;
    std::string full_path;

    RouterCtx(): loc(NULL), full_path(""){}
};

//stateless Router machine should not include ptr location and full_path data
class Router
{
    public:
        Router(void);
        ~Router(void);

        RouterCtx build_router_context(const HttpRequest& req, const ServerConfig& server)const;

    private:
        const location* find_location(const HttpRequest& req, const ServerConfig& server)const;
        std::string build_full_path(const HttpRequest& req, const location* loc)const;
        bool is_valid_prefix_loc(std::vector<location>::const_iterator it, const std::string &uri)const;

        Router(const Router& other);
        Router& operator=(const Router& other);
};

#endif
