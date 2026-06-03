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

std::string Utils::generate_unique_id_pure98() 
{
    std::stringstream ss;
    // 1. 加入当前系统时间戳（精确到秒）
    ss << std::time(NULL); 
    // 2. 加入当前进程运行的时钟数（增加微秒级随机性）
    ss << "_" << std::clock(); 
    // 3. 加入两个大随机数
    ss << "_" << std::rand() << std::rand();
    
    return ss.str(); // 返回如: "1779951199_12500_218391038"
}

void Utils::replaceAll(std::string &input)
{
    for (std::size_t i = 0; i < input.size() ; i++)
    {
        if (input[i] == '-')
            input[i] = '_';
    }
}

void Utils::toUpper(std::string &input)
{
    for (std::size_t i = 0; i < input.size(); i++)
    {
        input[i] = std::toupper(input[i]);
    }
}

static int char_to_lower(int c) {
    return std::tolower(static_cast<unsigned char>(c));
}

void Utils::to_lowercase(std::string &str) {
    std::transform(str.begin(), str.end(), str.begin(), char_to_lower);
}