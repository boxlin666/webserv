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

void Utils::expect_semicolon(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size()) {
        throw std::runtime_error("Syntax error: Unexpected end of file, missing ';'");
    }
    
    if (tokens[pos].content != ";") {
        throw std::runtime_error("Syntax error: expected ';' but found '" + tokens[pos].content + "'");
    }
    
    pos++;
}