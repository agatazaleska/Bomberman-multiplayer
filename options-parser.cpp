#include "options-parser.h"

/*
Comments about usage of the functions in this file
are stored in options-parer.h header file.
In this file, I placed some comments concerning implementation details.
*/

static bool is_number(const char *string) {
    for (int i = 0; string[i] != 0; i++) {
        if (!std::isdigit(string[i])) return false;
    }
    return true;
}

void check_port_correctness(const char *string) {
    errno = 0;
    if (!is_number(string)) {
        fprintf(stderr, "%s is not a valid port number\n", string);
        exit(EXIT_FAILURE);
    }

    unsigned long port = strtoul(string, nullptr, 10);
    if (port > UINT16_MAX) {
        fprintf(stderr, "%lu is not a valid port number\n", port);
        exit(EXIT_FAILURE);
    }
}

void get_hostname_and_port(std::string &address, std::string &hostname, std::string &port) {
    size_t found = address.find_last_of(COLON); // last colon separates hostname from port
    if (found == std::string::npos) {
        std::cerr << "incorrect address format\n";
        exit(EXIT_FAILURE);
    }

    hostname = address.substr(0, found);
    port = address.substr(found + 1);
    check_port_correctness(port.c_str());
}

// This function verifies if an option has been provided.
static bool check_if_option_provided(p_options::variables_map &options_map,
                              const std::string &option) {

    if (!options_map.count(option)) { // option hasn't been provided
        std::cerr << "Provide parameter: " << option << "\n";
        return false;
    }
    return true;
}

bool check_client_options(ClientOptions &options, int argc, char *argv[]) {
    //handling options using boost::program_options

    p_options::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "produce help msg_buffer")
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

bool check_server_options(ServerOptions &options, int argc, char *argv[]) {
    //handling options using boost::program_options

    p_options::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "produce help msg_buffer")
            ("port,p", p_options::value<uint16_t>(&options.port), "port")
            ("bomb-timer,b", p_options::value<uint16_t>(&options.bomb_timer),
             "bomb timer")
            ("players-count,c", p_options::value<uint8_t>(&options.players_count),
             "players count")
            ("turn-duration,d", p_options::value<uint64_t>(&options.turn_duration),
             "turn duration")
            ("explosion-radius,e", p_options::value<uint16_t>(&options.explosion_radius),
             "explosion radius")
            ("initial-blocks,k", p_options::value<uint16_t>(&options.initial_blocks),
             "initial blocks")
            ("game-length,l", p_options::value<uint16_t>(&options.game_length),
             "game length")
            ("seed,s", p_options::value<uint32_t>(&options.seed),
             "seed")
            ("size-x,x", p_options::value<uint16_t>(&options.size_x),
             "board size x")
            ("size-y,y", p_options::value<uint16_t>(&options.size_y),
             "board size y")
            ("server-name,n", p_options::value<std::string>(&options.server_name),
             "server name");

    p_options::variables_map options_map;
    p_options::store(p_options::parse_command_line(argc, argv, desc), options_map);
    p_options::notify(options_map);

    if (options_map.count("help")) {
        std::cout << desc << "\n";
        exit(EXIT_SUCCESS);
    }

    bool all_options_provided = true;
    all_options_provided &= check_if_option_provided(options_map, "players-count");
    all_options_provided &= check_if_option_provided(options_map, "turn-duration");
    all_options_provided &= check_if_option_provided(options_map, "explosion-radius");
    all_options_provided &= check_if_option_provided(options_map, "initial-blocks");
    all_options_provided &= check_if_option_provided(options_map, "game-length");
    all_options_provided &= check_if_option_provided(options_map, "server-name");
    all_options_provided &= check_if_option_provided(options_map, "port");
    all_options_provided &= check_if_option_provided(options_map, "size-x");
    all_options_provided &= check_if_option_provided(options_map, "size-y");

    return all_options_provided;
}
