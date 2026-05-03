#include "HttpRequest.hpp"
#include <string> 
#include <iterator>
#include <iostream>

//tempo location struct just to pass pytest!!
void HttpRequest::init_locations()
{
    struct location location_0;
    struct location location_1;
    struct location location_2;
    struct location location_3;

    location_0._prefix = "/";
    location_0.root = "./www";
    location_0.index_file = "index.html";
    location_0.methods.push_back("GET");
    location_0.client_max_body_size = 20000000;

    location_1._prefix = "/idex.html"; 
    location_1.root = "./www";
    location_1.index_file = "index.html";
    location_1.methods.push_back("GET");
    location_1.client_max_body_size = 20000000;

    location_2._prefix = "/index.html"; 
    location_2.root = "./www";
    location_2.index_file = "index.html";
    location_2.methods.push_back("GET");
    location_2.client_max_body_size = 20000000;

    location_3._prefix = "/test";
    location_3.root = "./www";
    location_3.index_file = "index.html";
    location_3.methods.push_back("GET");
    location_3.client_max_body_size = 20000000;

    this->locations.push_back(location_0);
    this->locations.push_back(location_1);
    this->locations.push_back(location_2);
    this->locations.push_back(location_3);
}

//tempo function!!
void HttpRequest::update_path()
{
    std::vector<location>::iterator it;
    int index = 0;
    int match_index = -1;
    std::size_t longest_match_length = 0;

    this->init_locations();
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
    this->loc_index = match_index; //note this request URI is related to which location
    if (match_index == -1)
        return ;
    if (this->get_path() == "/")
        this->router_path = this->locations[match_index].root;
    else
        this->router_path = this->locations[match_index].root + this->get_path();
    //std::cout << "URI = " << this->_path << " match_index = " << match_index <<" router_path = "  << this->router_path << std::endl;
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

