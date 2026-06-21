#include "Cluster.hpp"
#include "ConfigParser.hpp"
#include "HttpResponse.hpp"

bool validate_ext_config_file(const std::string& config_filename)
{
    std::size_t pos_ext = config_filename.find_last_of(".");
    if (pos_ext == std::string::npos) return (false);

    std::string config_ext = config_filename.substr(pos_ext);
    if (config_ext != ".conf") return (false);

    return (true);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: ./webserv <config_file>\n";
        return 1;
    }

    std::string config_filename(argv[1]);
    if (!validate_ext_config_file(config_filename)) {
        std::cerr << "Incorrect extension of configuration file\n";
        return 1;
    }

    try {
        ConfigParser parser;
        parser.build_config_map(argv[1]);
        parser.print();

        Cluster webserv;
        HttpResponse::init_response_map();
        webserv.setup(parser);
        webserv.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
