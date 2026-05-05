#ifndef LOCATION_HPP
# define LOCATION_HPP

#include <map>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>

struct location {
    std::string                _prefix;
    std::vector<std::string>   index;
    std::string                root;
    std::map<int, std::string> error_pages;
    bool                       autoindex;

    // access
    std::vector<std::string> methods;
    std::size_t              client_max_body_size;

    int         return_code;
    std::string return_url;

    // 处理配置文件里写的扩展指令
    std::string upload_path;
    std::string cgi_path;
    std::string cgi_ext;

    std::map<std::string, std::string> _cgi_param;  // cgi 扩展配置
    std::string index_file;
};

#endif