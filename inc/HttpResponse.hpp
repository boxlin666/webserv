#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <iterator>

#include <unistd.h> //tmp header missing config data 
#include <fcntl.h>
#include <sys/stat.h> 
#include <vector>
#include <cstdio>

#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include "HttpConstants.hpp"
#include "Utils.hpp"

#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192

class HttpResponse
{
    private:
        int _status_code;
        std::string _status_line;
        static std::map<int, std::string> _status_msg_map;

        static std::map<std::string, std::string> _ext_map;

        std::map<std::string, std::string> _headers_map;
        std::string _body;
        std::string _body_last_modif_date;
        std::size_t _body_len;

 
        std::string _full_response;
        
        std::string _full_path;
       
        const ServerConfig* _config;

    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    //Prerequis
    int _check_request(const HttpRequest& req)const;
    std::string _build_full_path(const HttpRequest& req, const ServerConfig& config)const;
    int _check_resource(const HttpRequest& req);
    int _process_directory(const HttpRequest& req);
    int _process_file(const struct stat& st);

    //Method
    int _handle_get(void);
    int _handle_post(const HttpRequest& req);
    int _handle_delete(void);

    //Reponse generator
    void _prepare_response_data(const HttpRequest& req);
    void _build_status_line(const HttpRequest& req);
    void _build_headers_map(void);
    void _add_header(const std::string& key, const std::string& value);
    std::string _generate_date(void)const;
    std::string _generate_content_type(void)const;

    //Append all message
    void _append_full_response(void);

    //setter
    void _set_status(int code);
    void _set_body_len(size_t body_len);

    //init static data
    static void _init_status_msg_map(void);
    static void _init_ext_map(void);

    public:
        HttpResponse();
        ~HttpResponse();
    
        // response = status line + header + body
        void build(const HttpRequest& req, const ServerConfig& config);
        const std::string &get_full_response()const;

        //reset function for each turn of RUN loop, to clean up the old content inside!!
        void reset(void);

        //init static data for any connection http response
        static void init_response_map();
};

#endif