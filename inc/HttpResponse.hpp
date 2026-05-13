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
#include <cerrno>

#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include "RequestHandler.hpp"
#include "HttpConstants.hpp"
#include "Utils.hpp"

#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192

class HttpResponse
{
    private:
        int _status_code;
        std::string _body_last_modif_date;
        std::size_t _body_len;
        std::string _full_path;

        std::string _status_line;
        static std::map<int, std::string> _status_msg_map;

        static std::map<std::string, std::string> _ext_map;

        typedef std::pair<std::string, std::string> HeaderPair;
        std::vector<HeaderPair> _headers_vector;

        std::string _body;
       
        std::string _full_response;

    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    //Prepa input data

    void _prepare_from_handler(const RequestHandler& response_ctx);

    //Method
    int _handle_get(void);
    int _handle_post(const HttpRequest &request, const RequestHandler &response_ctx);
    int _handle_delete(void);
    int _handle_static_post(const HttpRequest &request);

    //Reponse generator
    void _prepare_response_data(const HttpRequest &request);
    void _build_status_line(const HttpRequest &request);
    void _build_headers_map(const HttpRequest &request);
    void _add_header(const std::string& key, const std::string& value);

    void _add_header_vector(const std::string& key, const std::string& value);


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
        void build(const HttpRequest& request,  const RequestHandler& response_ctx, int &status_code);
        const std::string &get_full_response()const;

        //reset function for each turn of RUN loop, to clean up the old content inside!!
        void reset(void);

        //init static data for any connection http response
        static void init_response_map();
};

#endif




