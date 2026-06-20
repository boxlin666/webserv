#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <cstdlib>
#include <iterator>
#include <vector>
#include <utility>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <climits>
#include <unistd.h>

#include "ConfigParser.hpp"
#include "Utils.hpp"
#include "Location.hpp"

struct Token;
class ServerConfig {
   public:
    ServerConfig();
    ~ServerConfig(void);

    typedef std::pair<std::string, int> ListenAddr;
 
    const std::vector<ListenAddr>  &get_listen_addrs() const 
    { return this->_listen_addrs; }

    const std::vector<std::string>  &get_servers_name() const 
    { return this->_server_names; }

    const std::string &get_root() const
    { return this->_root; }

    const std::vector<std::string> &get_methods() const
    { return this->_methods; }

    const std::vector<std::string> &get_index() const
    { return this->_index; }

    const std::string &get_error_page(int status_code) const;

    const std::vector<location>& get_locations(void)const;

    typedef void (ServerConfig::*LocationHandler)(std::vector<Token>&, size_t&, location*);

    bool hasHandler(const std::string& directive) const;
    void executeHandler(const std::string& directive, std::vector<Token>& tokens, size_t& pos,
                        location* loc);

    void fill_location_defaults(location& loc);
    void add_location(const location& loc);
    void parse(std::vector<Token>& tokens, size_t& pos);

    void print() const;


   private:
    ServerConfig(const ServerConfig& other);
    ServerConfig& operator=(const ServerConfig& other);

    std::vector<std::string> _server_names;

    std::vector<ListenAddr> _listen_addrs; // host:port_number 必须成对出现（ex: 127.0.0.0:8080）

    std::vector<std::string>   _index;
    std::string                _root;
    size_t                     _client_max_body_size;
    std::map<int, std::string> _error_pages;
    std::vector<std::string>   _methods;

    int         _return_code;  // 初始化时建议设为 0
    std::string _return_url;

    // 处理配置文件里写的扩展指令
    std::string _upload_path;
    std::string _cgi_path;
    std::string _cgi_ext;
    std::string _cgi_script;

    bool _autoindex;

    std::map<std::string, LocationHandler> _handler_map;
    int string_to_port(const std::string& port_str)const;
    void                                   _init_handlers();  // 在构造函数中调用，初始化映射表

    void set_listen_addrs(const std::string& port_str);
    
    bool validate_listen_fd(const std::string& host, const std::string& port)const;

    bool _is_valid_server_name(const std::string& host)const;

    void _validate_location(const location& new_loc) const;

    // 具体的指令处理器（小函数）
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

    std::vector<location>
        locations;  // 最长前缀匹配 不需要使用map,需要循环遍历vector得到最长匹配（我是说run
                    // connection阶段）

    void parseLocation(std::vector<Token>& tokens, size_t& pos);
};
#endif
