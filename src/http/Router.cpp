#include "Router.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include "HttpConstants.hpp"

Router::Router(void)
{}

Router::~Router(void)
{}

RouterCtx  Router::build_router_context(const HttpRequest& req, const ServerConfig& server, int& status_code)
{
    struct RouterCtx ctx;

    ctx.server = &server;
    ctx.loc = find_location(req, server);

    //如果没有在location内部找到支持的方法，那么立刻返回ctx (ctx 已经为初始化过，路径为空)
    status_code = check_supported_method(req, server, ctx.loc);
    if (status_code != SUCCESS)
        return (ctx);

    ctx.full_path = build_full_path(req, server, ctx.loc);

    if (!ctx.loc)
        ctx.final_root = server.get_root();
    else
    {
        ctx.final_root = ctx.loc->root;
        if (!ctx.loc->cgi_path.empty() && !ctx.loc->cgi_ext.empty()) 
        {
            std::size_t pos = ctx.full_path.find_last_of('/');
            if (pos != std::string::npos)
            {
                std::string file_name = ctx.full_path.substr(pos + 1, ctx.full_path.length());
                pos = file_name.find_last_of('.');
                if (pos != std::string::npos)
                {
                    std::string file_name_ext = file_name.substr(pos, file_name.length()); 
                    if (file_name_ext == ctx.loc->cgi_ext)
                        ctx.is_cgi_potential = true;
                }
            }
        }
    }
   
    std::cout << "final Physic path = " << ctx.full_path << std::endl;
    std::cout << "final root = " << ctx.final_root << std::endl;
    if (ctx.loc)
    {
        std::cout << "location prefix = " << ctx.loc->_prefix << std::endl;
        std::cout << "location root = " << ctx.loc->root << std::endl;
    }
    return (ctx);
}

const location* Router::find_location(const HttpRequest& req, const ServerConfig& server)
{
    std::vector<location>::const_iterator it;
    int index = 0;
    int match_index = -1;
    std::size_t longest_match_length = 0;

    //pre-requis (TO DO):: we should normalize the format of URI before the match process (remove "../.." "///" "//" extra)
    if (server.get_locations().empty())
        return (NULL);
    for (it = server.get_locations().begin(); it != server.get_locations().end(); it++)
    {
        if (Router::is_valid_prefix_loc(it, req.get_path()))
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

int Router::check_supported_method(const HttpRequest&req, const ServerConfig& server, const location *loc)
{
    bool method_exist = false;
    bool method_allow = false;
    const std::vector<std::string>* loc_methods = NULL;

    if (loc)
        loc_methods = &(loc->methods);
    else
        loc_methods = &(server.get_methods());

    for (std::size_t i = 0; i < server.get_methods().size(); i++)
    {
        if (req.get_method() == server.get_methods()[i])
        {
            method_exist = true;
            break ;
        }
    }
    if (method_exist == false)
        return (NO_METHOD);

    for (std::size_t i = 0; i < loc_methods->size(); i++)
    {
        if (req.get_method() == (*loc_methods)[i])
        {
	    std::cout << (*loc_methods)[i] << std::endl;
            method_allow = true;
            break ;
        }
    }
    if (method_allow == false)
    {
        return (METHOD_NOT_ALLOWED);
    }
    return (SUCCESS);
}

std::string Router::build_full_path(const HttpRequest& req, const ServerConfig& server, const location *loc)
{
    std::string full_path;
    std::string relative_path;
    std::string prefix;
    bool is_post_dir = false;

    if (!loc)
    {
        full_path = server.get_root();
        return (full_path);
    }
    if (req.get_path() == "/")
        full_path = loc->root;
    else
    {
        prefix =  loc->_prefix;
        relative_path = req.get_path().substr(prefix.length());
        std::cout << "relative path = " << relative_path << std::endl;
        if (relative_path[0] == '/')
            relative_path.erase(0, 1);
        std::cout << "AFTER relative path = " << relative_path << std::endl;

        if (relative_path.empty() && req.get_method() == "POST" && server.get_root() == loc->root)
        {
            relative_path = req.get_path();
            if (relative_path[0] == '/')
                relative_path.erase(0, 1);
            is_post_dir = true; 
        }

        if (loc->root[loc->root.length() - 1] == '/')
            full_path = loc->root + relative_path;
        else
        {
            full_path = loc->root + "/" + relative_path;
            if (is_post_dir)
                full_path += "/";
        }
    }
    std::cout << "FULL PATH " << full_path << std::endl;
    return (full_path);
}

bool Router::is_valid_prefix_loc(std::vector<location>::const_iterator it, const std::string& uri)
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
