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
 
    std::vector<std::string> methods;
    std::size_t              client_max_body_size;

    int                      return_code;
    std::string              return_url;

    std::string upload_path;
    std::string cgi_path;
    std::string cgi_ext;
    std::string cgi_script;
    std::string index_file;

    location()
        : _prefix(""),
          index(),
          root(""),
          error_pages(),
          autoindex(false),              
          methods(),
          client_max_body_size(1048576),   
          return_code(0),              
          return_url(""),
          upload_path(""),
          cgi_path(""),
          cgi_ext(""),
          cgi_script(""),
          index_file("")
    {}
};

#endif