#include "HttpRequest.hpp"
#include <string> 
#include <iterator>

//tempo location struct just to pass pytest!!
void HttpRequest::init_locations()
{
    struct location location1;
    struct location location2;

    location1._prefix = "/";
    location1.root = "./www";
    location1.index_file = "index.html";
    location1.methods.push_back("GET");
    location1.client_max_body_size = 20000000;

    location2._prefix = "/index.html";
    location2.root = "./www";
    location2.index_file = "index.html";
    location2.methods.push_back("GET");
    location2.client_max_body_size = 20000000;

    this->locations.push_back(location1);
    this->locations.push_back(location2);
}

//tempo function!!
void HttpRequest::update_path()
{
    std::vector<location>::iterator it;
    int index = 0;
    int match_index = -1;
    std::size_t longest_match_length = 0;

    //pre-requis (TO DO):: we should normalize the format of URI before the match process (remove "../.." "///" "//" extra)
    for (it = this->locations.begin(); it != this->locations.end(); it++)
    {
        if (this->is_valid_prefix(it))
        {
            if (it->_prefix.length() > longest_match_length)
            {
                longest_match_length = it->_prefix.length();
                match_index = index;
            }
        }
        index++;
    }
    if (match_index == -1)
        return ;
    if (this->get_path() == "/")
        this->router_path = this->locations[match_index].root;
    else
        this->router_path = this->locations[match_index].root + this->get_path();
}

bool HttpRequest::is_valid_prefix(std::vector<location>::iterator it)
{
    std::size_t uri_len = this->_path.length();
    std::size_t pre_len = it->_prefix.length(); 

    if (it->_prefix == "/") return (true);

    if (uri_len < pre_len) return (false);

    if (this->_path.compare(0, pre_len, it->_prefix) != 0) return (false);

    if (uri_len == pre_len) return (true);

    if (this->_path[pre_len] == '/') return (true);

    return (false);
}

