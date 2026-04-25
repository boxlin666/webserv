#ifndef UTILS_HPP
# define UTILS_HPP

#include <sstream>
#include <string>
#include <ctime>

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
};

#include "Utils.tpp"

#endif