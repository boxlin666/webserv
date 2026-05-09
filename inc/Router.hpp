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
    std::string final_root; //according to the location section
    std::string full_path;
    bool is_cgi_potential; //first check if cgi or not (but not enough, shoud double check in RequestHandler)
    RouterCtx(): loc(NULL), final_root(""),full_path(""), is_cgi_potential(false){}
};

//stateless Router machine should not include ptr location and full_path data
class Router
{
    public:
        static RouterCtx build_router_context(const HttpRequest& req, const ServerConfig& server);

    private:
        static const location* find_location(const HttpRequest& req, const ServerConfig& server);
        static std::string build_full_path(const HttpRequest& req, const ServerConfig& server, const location* loc);
        static bool is_valid_prefix_loc(std::vector<location>::const_iterator it, const std::string &uri);

        Router(void);
        ~Router(void);

        Router(const Router& other);
        Router& operator=(const Router& other);
};

#endif
