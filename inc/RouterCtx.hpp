#ifndef ROUTER_CTX_HPP
# define ROUTER_CTX_HPP

#include <string>
#include "Location.hpp"
#include "ServerConfig.hpp"

struct RouterCtx
{
    const ServerConfig* server;
    const location* loc;
    std::string final_root; //according to the location section
    std::string full_path;
    bool is_cgi_potential; //first check if cgi or not (but not enough, shoud double check in RequestHandler)
    RouterCtx(): server(NULL), loc(NULL), final_root(""),full_path(""), is_cgi_potential(false){}
};

#endif
