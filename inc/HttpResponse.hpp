#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>  //tmp header missing config data

#include <cerrno>
#include <cstdio>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "HttpConstants.hpp"
#include "HttpRequest.hpp"
#include "RequestHandler.hpp"
#include "ServerConfig.hpp"
#include "Utils.hpp"

#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192

class HttpResponse {
   private:
    int         _status_code;
    std::string _body_last_modif_date;
    std::size_t _body_len;
    std::string _full_path;

    std::string                       _status_line;
    static std::map<int, std::string> _status_msg_map;

    static std::map<std::string, std::string> _ext_map;

    typedef std::pair<std::string, std::string> HeaderPair;
    std::vector<HeaderPair>                     _headers_vector;
    std::vector<HeaderPair>                     _cgi_headers_vector;

    std::string _body;

    std::string _full_response;

    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    // Prepa input data

    void _prepare_from_handler(const RequestHandler& response_ctx);

    // Method
    int _handle_get(void);
    int _handle_post(const HttpRequest& request);
    int _handle_delete(void);
    int _handle_static_post_dir(void);
    int _handle_static_post(const HttpRequest& request);

    // Reponse generator
    void _prepare_response_data(const HttpRequest& request);
    void _build_status_line(const HttpRequest& request);
    void _build_headers_map(const HttpRequest& request);
    void _add_header(const std::string& key, const std::string& value);

    void _add_header_vector(const std::string& key, const std::string& value);

    std::string _generate_date(void) const;
    std::string _generate_content_type(void) const;

    // Append all message
    void _append_full_response(void);

    // setter
    void _set_status(int code);
    void _set_body_len(size_t body_len);

    // init static data
    static void _init_status_msg_map(void);
    static void _init_ext_map(void);

    // Response CGI generator
    bool _split_cgi_header_body(const std::string& raw_output, std::string& header_part);
    void _parse_cgi_headers(const std::string& header_part);

    void        _prepare_cgi_response(const HttpRequest& request);
    void        _build_cgi_status_line(const HttpRequest& request);
    void        _build_cgi_headers_map(const HttpRequest& request);
    std::string _get_cgi_header(const std::string& key, const std::string& value_sub,
                                bool check_value) const;

    void _add_cgi_header_vector(const std::string& key, const std::string& value);

    bool _validate_cgi_content_type() const;

    bool _is_error_response() const;

   public:
    HttpResponse();
    ~HttpResponse();

    // response = status line + header + body
    void build_static_response(const HttpRequest& request, const RequestHandler& response_ctx,
                               int& status_code);

    const std::string& get_full_response() const;

    // reset function for each turn of RUN loop, to clean up the old content inside!!
    void reset(void);

    // init static data for any connection http response
    static void init_response_map();

    bool build_cgi_response(const HttpRequest& request, const std::string& cgi_output);

    std::string& build_error_response(int& status_code, std::string& error_page_path,
                                      HttpRequest& request);
};

#endif
