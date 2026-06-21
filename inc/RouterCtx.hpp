#ifndef ROUTER_CTX_HPP
#define ROUTER_CTX_HPP

#include <string>

#include "Location.hpp"
#include "ServerConfig.hpp"

struct RouterCtx {
    const ServerConfig* server;
    const location*     loc;
    std::string         final_root;
    std::string         full_path;
    bool                is_cgi_potential;
    bool                is_redirect;
    int                 redirect_code;
    std::string         redirect_url;

    RouterCtx()
        : server(NULL),
          loc(NULL),
          final_root(""),
          full_path(""),
          is_cgi_potential(false),
          is_redirect(false),
          redirect_code(0),
          redirect_url("")
    {
    }
};

#endif
