#ifndef BOMBOWE_ROBOTY_OPTIONS_PARSER_H

#include <cstdio>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>

#define COLON ':'

namespace p_options = boost::program_options;

// A structure for program options.
struct ClientOptions {
    std::string gui_address;
    std::string player_name;
    uint16_t port;
    std::string server_address;
};

struct ServerOptions {
    uint16_t bomb_timer;
    uint8_t players_count;
    uint64_t turn_duration;
    uint16_t explosion_radius;
    uint16_t initial_blocks;
    uint16_t  game_length;
    std::string server_name;
    uint16_t port;
    uint32_t seed; // optional
    uint16_t size_x;
    uint16_t size_y;
};

// This function checks options correctness.
bool check_client_options(ClientOptions &options, int argc, char *argv[]);

// This function parses a given address into separate string: hostname and port.
void get_hostname_and_port(std::string &address, std::string &hostname, std::string &port);

bool check_server_options(ServerOptions &options, int argc, char *argv[]);

#define BOMBOWE_ROBOTY_OPTIONS_PARSER_H

#endif //BOMBOWE_ROBOTY_OPTIONS_PARSER_H
