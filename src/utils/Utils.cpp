#include "Utils.hpp"

Utils::Utils(void)
{

}
        
Utils::~Utils(void)
{

}

std::string Utils::formatHttpDate(time_t raw_time)
{
    char buf[100];
    struct tm *tm = gmtime(&raw_time);
    
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm);    
    return (std::string(buf));
}