#ifndef ROUTER_HPP
# define ROUTER_HPP

#include <string> 
#include <iterator>
#include <iostream>

#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include "Location.hpp"
//#include "Connection.hpp"
#include "RouterCtx.hpp"

//stateless Router machine should not include ptr location and full_path data
class Router
{
    public:
        static RouterCtx build_router_context(const HttpRequest& req, const ServerConfig& server, int &status_code);

    private:
        static const location* find_location(const HttpRequest& req, const ServerConfig& server);
        static std::string build_full_path(const HttpRequest& req, const ServerConfig& server, const location* loc);
        static bool is_valid_prefix_loc(std::vector<location>::const_iterator it, const std::string &uri);
        static int check_supported_method(const HttpRequest& req, const ServerConfig& server, const location* loc);

        Router(void);
        ~Router(void);

        Router(const Router& other);
        Router& operator=(const Router& other);
};

#endif
