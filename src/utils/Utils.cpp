#include "Utils.hpp"

Utils::Utils(void) {}

Utils::~Utils(void) {}

std::string Utils::formatHttpDate(time_t raw_time)
{
    char       buf[100];
    struct tm* tm = gmtime(&raw_time);

    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm);
    return (std::string(buf));
}

void Utils::expect_semicolon(const std::vector<Token>& tokens, size_t& pos)
{
    const std::string& current_line = Utils::toString(tokens[pos].line);

    if (pos >= tokens.size()) {
        throw std::runtime_error("Syntax error on line [" + current_line +
                                 "Unexpected end of file, missing ';'");
    }
    if (tokens[pos].content != ";") {
        throw std::runtime_error("Syntax error on line [" + current_line +
                                 "] expected ';' but found '" + tokens[pos].content + "'");
    }
    pos++;
}

std::string Utils::generate_unique_id()
{
    std::stringstream ss;
    ss << std::time(NULL);
    ss << "_" << std::clock();
    ss << "_" << std::rand() << std::rand();
    return ss.str();
}

void Utils::replaceAll(std::string& input)
{
    for (std::size_t i = 0; i < input.size(); i++) {
        if (input[i] == '-') input[i] = '_';
    }
}

void Utils::toUpper(std::string& input)
{
    for (std::size_t i = 0; i < input.size(); i++) { input[i] = std::toupper(input[i]); }
}

static int char_to_lower(int c)
{ return std::tolower(static_cast<unsigned char>(c)); }

void Utils::to_lowercase(std::string& str)
{ std::transform(str.begin(), str.end(), str.begin(), char_to_lower); }

bool Utils::is_digit_str(const std::string& str)
{
    if (str.empty()) return (false);

    for (std::size_t i = 0; i < str.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) return (false);
    }
    return (true);
}

int Utils::convertThreeDigit(const std::string& str)
{
    if (str.length() != 3) return (-1);

    std::stringstream ss(str);
    int               result;

    if ((ss >> result) && ss.eof()) return (result);
    return (-1);
}
