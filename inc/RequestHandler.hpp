#ifndef REQUEST_HANDLER_HPP
# define REQUEST_HANDLER_HPP

#include "HttpRequest.hpp"
#include "Router.hpp"

class RequestHandler
{
    public:
        RequestHandler(void);
        ~RequestHandler();

        void process_request_handler(const HttpRequest &req, const RouterCtx &route_ctx, int &status_code);
        const std::string &get_method_to_apply()const;
        const std::string &get_full_path()const;
        const std::string &get_body()const;
        std::size_t get_body_len()const;
        
    private:
        std::string _full_path;
        std::string _method_to_apply;
        bool _is_keep_alive;
        std::string& _req_body;
        std::size_t _req_body_len;

        int check_resource(const HttpRequest& req);
        int process_directory(const HttpRequest& req);
        int process_file(const struct stat& st);
        RequestHandler(const RequestHandler& other);
        RequestHandler& operator=(const RequestHandler& other);

};

#endif