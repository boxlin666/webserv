#ifndef UTILS_HPP
# define UTILS_HPP

#include <sstream>
#include <string>
#include <ctime>
#include <cctype>
#include <cstdlib>
#include "ConfigParser.hpp"
#include <algorithm>

struct Token;
//Every utils function could be added here!
class Utils 
{
    private:
        Utils(const Utils& other);
        Utils& operator=(const Utils& other);

    public:
        Utils(void);
        ~Utils(void);

        template <typename T>
        static std::string toString(T value);

        static std::string formatHttpDate(time_t raw_time);
        
        static void expect_semicolon(const std::vector<Token>& tokens, size_t& pos);

        static std::string generate_unique_id_pure98();

        static void replaceAll(std::string &input);

        static void toUpper(std::string &input);

        static void to_lowercase(std::string &str) ;
};

void debug_request_msg_print(const std::string &variable, const std::string &content);

void printEnvMap(const std::map<std::string, std::string>& env_map);

#include "Utils.tpp"

#endif