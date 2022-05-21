#include "options-parser.h"

void get_hostname_and_port(std::string &address, std::string &hostname, std::string &port) {
    size_t found = address.find_last_of(":");
    if (found == std::string::npos) {
        printf("incorrect address format");
        exit(EXIT_FAILURE);
    }

    hostname = address.substr(0, found);
    port = address.substr(found + 1);
}

bool check_if_option_provided(p_options::variables_map &options_map,
                              const std::string &option) {

    if (!options_map.count(option)) {
        std::cout << "Provide parameter: " << option << "\n";
        return false;
    }
    return true;
}

bool check_program_options(Options &options, int argc, char *argv[]) {
    p_options::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "produce help message")
            ("gui-address,d", p_options::value<std::string>(&options.gui_address),
             "<(host name):(port) lub (IPv4):(port) lub (IPv6):(port)>")
            ("player-name,n", p_options::value<std::string>(&options.player_name),
             "player name")
            ("port,p", p_options::value<uint16_t>(&options.port), "port")
            ("server-address,s", p_options::value<std::string>(&options.server_address),
             "server address");

    p_options::variables_map options_map;
    p_options::store(p_options::parse_command_line(argc, argv, desc), options_map);
    p_options::notify(options_map);

    if (options_map.count("help")) {
        std::cout << desc << "\n";
        exit(EXIT_SUCCESS);
    }

    bool all_options_provided = true;
    all_options_provided &= check_if_option_provided(options_map, "gui-address");
    all_options_provided &= check_if_option_provided(options_map, "player-name");
    all_options_provided &= check_if_option_provided(options_map, "port");
    all_options_provided &= check_if_option_provided(options_map, "server-address");

    return all_options_provided;
}
