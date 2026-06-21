#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <vector>

#include "HttpRequest.hpp"
#include "Router.hpp"

class RequestHandler {
   public:
    RequestHandler(void);
    ~RequestHandler();

    void process_request_handler(const HttpRequest& req, const RouterCtx& route_ctx,
                                 int& status_code);

    void               _set_res_body_len(size_t body_len);
    const std::string& get_full_path() const;
    std::size_t        get_res_body_len() const;
    bool               is_cgi_mode() const;
    bool               is_auto_index() const;
    const std::string& get_body_last_modif_date() const;

   private:
    std::string _full_path;
    std::string _parent_path;
    std::string _req_body;
    std::string _body_last_modif_date;
    std::size_t _res_body_len;
    std::string _multipart_filename;
    bool        _is_auto_index;
    bool        _is_cgi_mode;

    int dispatch_resource_check(const HttpRequest& req, const RouterCtx& route_ctx);
    int cgi_resource_validator(const RouterCtx& route_ctx);
    int existing_resource_validator(const RouterCtx& route_ctx);
    int creatable_resource_validator(void);

    int process_directory(const RouterCtx& router_ctx);
    int process_file(const struct stat& st, const std::string& tmp_full_path);
    int extract_parent_path(void);
    int check_ext_post_file(void) const;

    RequestHandler(const RequestHandler& other);
    RequestHandler& operator=(const RequestHandler& other);
};

#endif