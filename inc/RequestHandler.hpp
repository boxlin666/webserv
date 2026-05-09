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

    private:
        RequestHandler(const RequestHandler& other);
        RequestHandler& operator=(const RequestHandler& other);

};

#endif