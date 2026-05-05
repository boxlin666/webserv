#include "Router.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

Router::Router(void)
{}

Router::~Router(void)
{}

RouterCtx  Router::build_router_context(const HttpRequest& req, const ServerConfig& server)const
{
    struct RouterCtx ctx;

    ctx.loc = find_location(req, server);
    ctx.full_path = build_full_path(req, ctx.loc);
    //std::cout << "URI = " << ctx.full_path << std::endl;
    //std::cout << "location prefix = " << ctx.loc->_prefix << std::endl;
    return (ctx);
}

const location* Router::find_location(const HttpRequest& req, const ServerConfig& server)const
{
    std::vector<location>::const_iterator it;
    int index = 0;
    int match_index = -1;
    std::size_t longest_match_length = 0;

    //pre-requis (TO DO):: we should normalize the format of URI before the match process (remove "../.." "///" "//" extra)
    for (it = server.get_locations().begin(); it != server.get_locations().end(); it++)
    {
        if (this->is_valid_prefix_loc(it, req.get_path()))
        {
            if (it->_prefix.length() > longest_match_length)
            {
                longest_match_length = it->_prefix.length();
                match_index = index;
            }
        }
        index++;
    }
    if (match_index != -1)
        return (&(server.get_locations()[match_index]));
    return (NULL);
}

std::string Router::build_full_path(const HttpRequest& req, const location *loc)const
{
    std::string full_path;

    if (!loc)
        return (full_path);
    if (req.get_path() == "/")
        full_path = loc->root;
    else
        full_path = loc->root + req.get_path();
    return (full_path);
}

bool Router::is_valid_prefix_loc(std::vector<location>::const_iterator it, const std::string& uri)const
{
    std::size_t uri_len = uri.length();
    std::size_t pre_len = it->_prefix.length(); 

    if (it->_prefix == "/") return (true);

    if (uri_len < pre_len) return (false);

    if (uri.compare(0, pre_len, it->_prefix) != 0) return (false);

    if (uri_len == pre_len) return (true);

    if (uri[pre_len] == '/') return (true);

    return (false);
}