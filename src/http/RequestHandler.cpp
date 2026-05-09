#include "RequestHandler.hpp"
#include "HttpConstants.hpp"

RequestHandler::RequestHandler()
{
}

RequestHandler::~RequestHandler(void)
{

}

void RequestHandler::process_request_handler(const HttpRequest &req, const RouterCtx &route_ctx, int &status_code)
{
    if (route_ctx.loc)    
    {
        if (req.get_content_length() > route_ctx.loc->client_max_body_size)
        {
            status_code = BODY_TOO_LARGE;
            return ;
        }
    }

}