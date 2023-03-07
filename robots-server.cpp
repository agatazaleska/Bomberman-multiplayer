// Robots-server unfortunately is not finished.
// It doesn't do much, but it's honest work.

#include <iostream>
#include <random>
#include <cstdint>

#include <boost/asio.hpp>
#include "options-parser.h"
#include "definitions.h"
#include "message-serializer.h"

using boost::asio::ip::tcp;

// This function initializes game based on given options.
GameState initialize_game(ServerOptions &options, players_map_t &players) {
    std::minstd_rand random(options.seed); // should be global

    positions_set_t initial_blocks;
    player_positions_map_t player_positions;
    uint16_t block_x, block_y, player_x, player_y;
    for (int i = 0; i < options.initial_blocks; i++) {
        block_x =  (uint16_t) random() % options.size_x;
        block_y = (uint16_t) random() % options.size_y;
        initial_blocks.insert(Position(block_x, block_y));
    }

    for (auto &player: players) {
        player_x = (uint16_t) random() % options.size_x;
        player_y = (uint16_t) random() % options.size_y;
        player_positions.insert(std::make_pair(player.first,
                                               Position(player_x,player_y)));
    }

    return {players, initial_blocks, player_positions};
}

// This functions sends hello message to the client.
void send_hello_message(ServerOptions &options, tcp::socket &socket) {
    size_t msg_len = (options.server_name.length() + 5 * sizeof(uint16_t) + 3 * sizeof(uint8_t));
    char hello_msg[msg_len];

    serialize_hello_message(hello_msg, options);
    boost::asio::write(socket, boost::asio::buffer(std::string(hello_msg, msg_len)));
}

// This functions sends accepted_player message to the client.
void send_accepted_player(Player &player, uint8_t player_id, tcp::socket &socket) {

    std::string name = player.name, address = player.address;
    size_t msg_len = (name.length() + address.length()+ 4 * sizeof(uint8_t));
    char accepted_player[msg_len];

    serialize_accepted_player_message(accepted_player, name, address, player_id); // tu next id
    boost::asio::write(socket, boost::asio::buffer(std::string(accepted_player, msg_len)));
}

// This functions calculates game_started message length.
size_t get_game_started_len(players_map_t &players) {
    size_t result = sizeof(uint8_t) + sizeof(uint32_t);

    for (auto &player : players) {
        result += 3 * sizeof(uint8_t); // id size + 2 sizes of strings
        result += player.second.name.length();
        result += player.second.address.length();
    }

    return result;
}

// This functions sends game_started to the client
void send_game_started(players_map_t &players, tcp::socket &socket) {
    size_t msg_len = get_game_started_len(players);
    char game_started[msg_len];

    serialize_game_started_message(game_started, players);
    boost::asio::write(socket, boost::asio::buffer(std::string(game_started, msg_len)));
}

// This functions calculates turn message length.
size_t get_turn_len(events_list_t &events) {
    size_t result = sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t);
    uint8_t id;

    for (auto &event : events) {
        id = event.event_id;
        result += sizeof(uint8_t); // event id
        switch ((EventType) id) {
            case BombPlaced:
                result += sizeof(BombId) + 2 * sizeof(uint16_t); // player id and position
                break;
            case BombExploded:
                result += sizeof(BombId) + 2 * sizeof(uint32_t);
                result += (event.blocks_destroyed.size()) * (2 * sizeof(uint16_t));
                result += (event.robots_destroyed.size()) * (sizeof (PlayerId));
                break;
            case PlayerMoved:
                result += sizeof(PlayerId) + 2 * sizeof(uint16_t); // player id and position
                break;
            case BlockPlaced:
                result += 2 * sizeof(uint16_t); // position
                break;
        }
    }
    return result;
}

// This functions sends turn message to the client.
void send_turn(events_list_t &events, tcp::socket &socket) {
    uint16_t next_id = 0;
    size_t msg_len = get_turn_len(events);
    char turn_msg[msg_len];

    serialize_turn_message(turn_msg, events, next_id);
    boost::asio::write(socket, boost::asio::buffer(std::string(turn_msg, msg_len)));
}

// This functions sends the first turn message to the client.
void send_turn0(GameState &game_state, tcp::socket &socket) {
    events_list_t events{};
    for (auto player_pos : game_state.player_positions) {
        Event event;
        event.initialize_player_moved(player_pos.first, player_pos.second);
        events.push_back(event);
    }

    for (auto block : game_state.blocks) {
        Event event;
        event.initialize_block_placed(block);
        events.push_back(event);
    }
    send_turn(events, socket);
}

// This function receives message from the client until it receives join request.
Player wait_for_join(tcp::socket &socket, std::string &client_address) {
    Buffer buffer(socket);

    while (true) {
        size_t message_size = buffer.receive_new_data();
        if (debug) {
            std::cerr << "received " << message_size <<
                      " bytes from " << client_address << "\n";
        }

        uint8_t player_name_len, direction;
        std::string player_name;
        while (buffer.get_parsed_len() < buffer.get_msg_len()) {
            uint8_t message_type = buffer.get_u8();
            if (message_type >= CLIENT_MESSAGES_NUMBER)
                std::cout << "disconnect client\n";

            switch ((ClientMessage) message_type) {
                case Join:
                    player_name_len = buffer.get_u8();
                    player_name = buffer.get_string(player_name_len);
                    return {player_name, client_address}; // returning new player data
                case ClientPlaceBomb:
                    break;
                case ClientPlaceBlock:
                    break;
                case ClientMove:
                    direction = buffer.get_u8();
                    if (direction >= 4) exit(EXIT_FAILURE);
                    break;
            }
        }
    }
}

// This function handles client connection. It sends hello message
// to the client and accepts him after receiving join request.
// Starts game.
void handle_client_connection(ServerOptions &options, tcp::socket &socket,
                              std::string &client_address) {

    players_map_t players{};
    send_hello_message(options, socket);
    Player new_player = wait_for_join(socket, client_address);
    uint8_t player_id = 0;
    send_accepted_player(new_player, player_id, socket);
    players.insert(std::make_pair(player_id, new_player));
    send_game_started(players, socket);

    GameState game_state = initialize_game(options, players);
    send_turn0(game_state, socket);
}

// Main function - handles simple connection with the client
int main(int argc, char *argv[]) {
    ServerOptions options;
    options.seed = (uint32_t) time(nullptr);
    if (!check_server_options(options, argc, argv)) exit(EXIT_FAILURE);
    std::cout << "listening on port " << options.port << "\n";

    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v6(), options.port));

        tcp::socket socket(io_context);
        acceptor.accept(socket);
        std::string client_address = socket.remote_endpoint().address().to_string();
        std::string c_port = std::to_string(socket.remote_endpoint().port());
        client_address.append(":");
        client_address.append(c_port);

        handle_client_connection(options, socket, client_address);
    }
    catch (std::exception &e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}