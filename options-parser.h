#ifndef BOMBOWE_ROBOTY_OPTIONS_PARSER_H

#include <cstdio>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>

namespace p_options = boost::program_options;

struct Options {
    std::string gui_address;
    std::string player_name;
    uint16_t port;
    std::string server_address;
};

bool check_program_options(Options &options, int argc, char *argv[]);

bool check_if_option_provided(p_options::variables_map &options_map,
                              const std::string &option);

void get_hostname_and_port(std::string &address, std::string &hostname, std::string &port);

#define BOMBOWE_ROBOTY_OPTIONS_PARSER_H

#endif //BOMBOWE_ROBOTY_OPTIONS_PARSER_H
