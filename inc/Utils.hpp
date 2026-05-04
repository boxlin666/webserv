#ifndef UTILS_HPP
# define UTILS_HPP

#include <sstream>
#include <string>
#include <ctime>
#include "ConfigParser.hpp"

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
};

#include "Utils.tpp"

#endif