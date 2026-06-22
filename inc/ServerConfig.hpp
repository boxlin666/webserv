#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <climits>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#include "ConfigParser.hpp"
#include "Location.hpp"
#include "Utils.hpp"

struct Token;
class ServerConfig {
   public:
    ServerConfig();
    ~ServerConfig(void);

    typedef std::pair<std::string, int> ListenAddr;

    const std::vector<ListenAddr>& get_listen_addrs() const
    { return this->_listen_addrs; }

    const std::vector<std::string>& get_servers_name() const
    { return this->_server_names; }

    const std::string& get_root() const
    { return this->_root; }

    const std::vector<std::string>& get_methods() const
    { return this->_methods; }

    const std::vector<std::string>& get_index() const
    { return this->_index; }

    const std::string& get_error_page(int status_code) const;

    const std::vector<location>& get_locations(void) const;

    std::size_t get_client_max_body(void) const
    { return this->_client_max_body_size; }

    typedef void (ServerConfig::*LocationHandler)(std::vector<Token>&, size_t&, location*);

    bool hasHandler(const std::string& directive) const;
    void executeHandler(const std::string& directive, std::vector<Token>& tokens, size_t& pos,
                        location* loc);

    void add_location(const location& loc);
    void parse(std::vector<Token>& tokens, size_t& pos);

    void print() const;

   private:
    ServerConfig(const ServerConfig& other);
    ServerConfig& operator=(const ServerConfig& other);

    std::vector<std::string>               _server_names;
    std::vector<ListenAddr>                _listen_addrs;
    std::vector<std::string>               _index;
    std::string                            _root;
    size_t                                 _client_max_body_size;
    std::map<int, std::string>             _error_pages;
    std::vector<std::string>               _methods;
    int                                    _return_code;
    std::string                            _return_url;
    std::string                            _upload_path;
    std::string                            _cgi_path;
    std::string                            _cgi_ext;
    std::string                            _cgi_script;
    bool                                   _autoindex;
    std::map<std::string, LocationHandler> _handler_map;
    std::vector<location>                  locations;

    int  string_to_port(const std::string& port_str) const;
    void _init_handlers();

    void set_listen_addrs(const std::string& port_str);

    bool validate_listen_fd(const std::string& host, const std::string& port) const;

    bool _is_valid_server_name(const std::string& host) const;

    void _fill_location_defaults(location& loc);

    void _validate_location(const location& loc) const;

    void _handle_root(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_methods(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_autoindex(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_listen(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_server_name(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_index(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_error_page(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_client_max_body_size(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_return(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_upload_path(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_cgi_path(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_cgi_ext(std::vector<Token>& tokens, size_t& pos, location* loc);
    void _handle_cgi_script(std::vector<Token>& tokens, size_t& pos, location* loc);

    void parseLocation(std::vector<Token>& tokens, size_t& pos);
};
#endif
