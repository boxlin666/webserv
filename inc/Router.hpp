#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <iostream>
#include <iterator>
#include <string>

#include "HttpRequest.hpp"
#include "Location.hpp"
#include "ServerConfig.hpp"
// #include "Connection.hpp"
#include "RouterCtx.hpp"

namespace Router {
RouterCtx   build_router_context(const HttpRequest& req, const ServerConfig& server,
                                 int& status_code);
std::string get_error_page_path(RouterCtx& route_ctx, int status_code);
}  // namespace Router
#endif
