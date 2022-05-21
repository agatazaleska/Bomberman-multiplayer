#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include "err.h"
#include "options-parser.h"

#define UDP_BUFFER_LENGTH   65507

struct Player;
struct Bomb;
struct Position;

using Score = uint32_t;
using PlayerId = uint8_t;

using scores_map_t = std::map<PlayerId , Score>;
using players_map_t = std::map<PlayerId , Player>;
using player_positions_map_t = std::map<PlayerId , Position>;
using player_id_list_t = std::vector<PlayerId>;
using position_list_t = std::vector<Position>;
using bomb_list_t = std::vector<Bomb>;

enum Direction {
    Up, Right, Down, Left
};

enum ClientMessage {
    Join, ClientPlaceBomb, ClientPlaceBlock, ClientMove
};

enum ServerMessage {
    Hello, AcceptedPlayer, GameStarted, Turn, GameEnded, Unknown
};

enum State {
    Lobby, Game
};

enum InputMessage {
    InputPlaceBomb, InputPlaceBlock, InputMove
};

enum GuiMessage {
    PlaceBomb, PlaceBlock, Move
};

struct Position {
    uint16_t x;
    uint16_t y;

    Position(uint16_t x, uint16_t y): x(x), y(y){};
};

struct Bomb {
    Position position;
    uint16_t timer;

    Bomb(Position position, uint16_t timer):
        position(position), timer(timer){};
};

struct Player { //player id = uint8
    std::string name;
    std::string address;

    Player(std::string name, std::string address):
        name(std::move(name)), address(std::move(address)){};
};

struct Event {
    uint8_t event_id;
    Position position;
    PlayerId player_id;
    uint32_t bomb_id;
    player_id_list_t players_list;
    position_list_t bombs_exploded;

    Event(uint8_t event_d, Position position, PlayerId player_id,
          uint32_t bomb_id, player_id_list_t players_list, position_list_t bombs_exploaded):
          event_id(event_d), position(position), player_id(player_id),
          bomb_id(bomb_id), players_list(std::move(players_list)),
          bombs_exploded(std::move(bombs_exploaded)){};
};

struct GameParameters {
    std::string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;

    GameParameters(std::string name, uint8_t players_n, uint16_t x, uint16_t y,
                   uint16_t length, uint16_t radius, uint16_t timer):
                   server_name(std::move(name)), players_count(players_n), size_x(x), size_y(y),
                   game_length(length), explosion_radius(radius), bomb_timer(timer){};

    GameParameters() = default;
};

struct GameState {
    uint16_t turn;
    player_positions_map_t player_positions;
    players_map_t players;
    position_list_t blocks;
    bomb_list_t bombs;
    position_list_t explosions;
    scores_map_t scores;

    GameState(players_map_t &players_map) {
        turn = 0;
        players = players_map;
        blocks = {};
        bombs = {};
        explosions = {};
        scores = {};
        player_positions = {};

        for (auto &player: players_map) {
            scores.insert(std::make_pair(player.first, 0));
            player_positions.insert(std::make_pair(player.first, Position(0,0)));
        }
    }
    GameState() = default;
};

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

State current_situation = Lobby;
std::mutex curr_situation_mutex;

void aggregate_game_state(GameState &state, vector<Event> &events) {

}

// robocza funkcja
void set_game_parameters(GameParameters &parameters, players_map_t &players_map) {
    parameters.server_name = "przykladowy serwer";
    parameters.players_count = 3;
    parameters.size_x = 10;
    parameters.size_y = 10;
    parameters.game_length = 1000;
    parameters.explosion_radius = 4;
    parameters.bomb_timer = 5;

    players_map_t players;
    players.insert(make_pair(1, Player("agata", "127.0.0.1:3333")));
    players.insert(make_pair(2, Player("mikolaj", "127.0.0.1:2222")));
    players.insert(make_pair(3, Player("zosia", "127.0.0.1:1111")));

    players_map = players;
}

void put_data_into_buffer(char *&dest_ptr, void *src, size_t size) {
    memcpy(dest_ptr, src, size);
    dest_ptr += size;
}

void put_uint_32_into_buffer(char *&dest_ptr, uint32_t number) {
    uint32_t number_to_send = htonl(number);
    put_data_into_buffer(dest_ptr, &number_to_send, sizeof(uint32_t));
}

void put_uint_16_into_buffer(char *&dest_ptr, uint16_t number) {
    uint16_t number_to_send = htons(number);
    put_data_into_buffer(dest_ptr, &number_to_send, sizeof(uint16_t));
}

size_t serialize_object_size(char *&dest_ptr, uint32_t size) {
    put_uint_32_into_buffer(dest_ptr, size);
    return sizeof(uint32_t);
}

size_t serialize_players_map(char *&dest_ptr, players_map_t &map) {
    size_t total_size = serialize_object_size(dest_ptr, map.size());

    for (auto &player: map) {
        uint8_t player_id = player.first;
        std::string player_name = player.second.name;
        std::string player_address = player.second.address;
        uint8_t player_name_len = player_name.length();
        uint8_t player_address_len = player_address.length();

        put_data_into_buffer(dest_ptr, &player_id, 1);
        put_data_into_buffer(dest_ptr, &player_name_len, 1);
        put_data_into_buffer(dest_ptr, (void *) player_name.c_str(), player_name_len);
        put_data_into_buffer(dest_ptr, &player_address_len, 1);
        put_data_into_buffer(dest_ptr, (void *) player_address.c_str(), player_address_len);

        total_size += (3 * sizeof(uint8_t) + player_name_len + player_address_len);
    }
    return total_size;
}

size_t serialize_position(char *&dest_ptr, Position position) {
    put_uint_16_into_buffer(dest_ptr, position.x);
    put_uint_16_into_buffer(dest_ptr, position.y);
    return 2 * sizeof(uint16_t);
}

size_t serialize_bomb(char *&dest_ptr, Bomb bomb) {
    serialize_position(dest_ptr, bomb.position);
    put_uint_16_into_buffer(dest_ptr, bomb.timer);
    return 3 * sizeof(uint16_t);
}

size_t serialize_player_positions_map(char *&dest_ptr, player_positions_map_t &map) {
    size_t total_size = serialize_object_size(dest_ptr, map.size());

    for (auto player_and_pos: map) {
        uint8_t player_id = player_and_pos.first;
        put_data_into_buffer(dest_ptr, &player_id, 1);
        serialize_position(dest_ptr, player_and_pos.second);

        total_size += (sizeof(uint8_t) + 2 * sizeof(uint16_t));
    }
    return total_size;
}

size_t serialize_scores_map(char *&dest_ptr, scores_map_t &map) {
    size_t total_size = serialize_object_size(dest_ptr, map.size());

    for (auto player_and_score: map) {
        uint8_t player_id = player_and_score.first;
        uint32_t score = player_and_score.second;

        put_data_into_buffer(dest_ptr, &player_id, 1);
        put_uint_32_into_buffer(dest_ptr, score);

        total_size += (sizeof(uint8_t) + sizeof(uint32_t));
    }
    return total_size;
}

size_t serialize_position_list(char *&dest_ptr, position_list_t &positions) {
    size_t total_size = serialize_object_size(dest_ptr, positions.size());

    for(auto pos: positions)
        total_size += serialize_position(dest_ptr, pos);

    return total_size;
}

size_t serialize_bomb_list(char *&dest_ptr, bomb_list_t &bombs) {
    size_t total_size = serialize_object_size(dest_ptr, bombs.size());

    for (auto bomb: bombs)
        total_size += serialize_bomb(dest_ptr, bomb);

    return total_size;
}

size_t prepare_lobby_message(GameParameters &parameters, char buffer[], players_map_t players) {
    State msg_type = Lobby;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, 1);

    uint8_t name_length = parameters.server_name.length();
    put_data_into_buffer(dest_ptr, &name_length, 1);
    put_data_into_buffer(dest_ptr, (void *) parameters.server_name.c_str(), name_length);
    put_data_into_buffer(dest_ptr, &parameters.players_count, 1);

    put_uint_16_into_buffer(dest_ptr, parameters.size_x);
    put_uint_16_into_buffer(dest_ptr, parameters.size_y);
    put_uint_16_into_buffer(dest_ptr, parameters.game_length);
    put_uint_16_into_buffer(dest_ptr, parameters.explosion_radius);
    put_uint_16_into_buffer(dest_ptr, parameters.bomb_timer);

    size_t total_size = (name_length + 5 * sizeof(uint16_t) + 3 * sizeof(uint8_t));
    total_size += serialize_players_map(dest_ptr, players);
    return total_size;
}

size_t prepare_game_message(GameParameters &parameters, GameState &state, char buffer[]) {
    State msg_type = Game;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, 1);

    uint8_t name_length = parameters.server_name.length();
    put_data_into_buffer(dest_ptr, &name_length, 1);
    put_data_into_buffer(dest_ptr, (void *) parameters.server_name.c_str(), name_length);

    put_uint_16_into_buffer(dest_ptr, parameters.size_x);
    put_uint_16_into_buffer(dest_ptr, parameters.size_y);
    put_uint_16_into_buffer(dest_ptr, parameters.game_length);
    put_uint_16_into_buffer(dest_ptr, state.turn);

    size_t total_size = name_length + 2 * sizeof(uint8_t) + 4 * sizeof(uint16_t);

    // czy to są ci sami playerzy co w lobby message???
    total_size += serialize_players_map(dest_ptr, state.players);
    total_size += serialize_player_positions_map(dest_ptr, state.player_positions);
    total_size += serialize_position_list(dest_ptr, state.blocks);
    total_size += serialize_bomb_list(dest_ptr, state.bombs);
    total_size += serialize_position_list(dest_ptr, state.explosions);
    total_size += serialize_scores_map(dest_ptr, state.scores);

    return total_size;
}

/*
void deserialize_server_message(char *message) {
    char *msg_ptr = message;
    auto message_type = static_cast<ServerMessage> (*((uint8_t*) message));

    switch (message_type) {
        case Hello:
            break;
        case AcceptedPlayer:
            break;
        case GameStarted:
            break;
        case Turn:
            break;
        case GameEnded:
            break;
    }
}
*/

void send_join_request(std::string &player_name, tcp::socket &server_socket) {
    size_t msg_len = 2 * sizeof(uint8_t) + player_name.length();
    char join_msg[msg_len];
    char *msg_ptr = join_msg;
    auto msg_type = (uint8_t) ClientMessage::Join;
    uint8_t name_len = player_name.length();

    memcpy(msg_ptr, &msg_type, sizeof(uint8_t));
    msg_ptr += 1;

    memcpy(msg_ptr, &name_len, sizeof(uint8_t));
    msg_ptr += 1;

    memcpy(msg_ptr, player_name.c_str(), name_len);

    server_socket.send(boost::asio::buffer(std::string(join_msg)));
    cout << "wyslano join\n";
}

void parse_message_from_gui() {

}

bool check_gui_message_correctness(char *gui_message, size_t len) {
    char *msg_ptr = gui_message;
    auto message_type = static_cast<GuiMessage> (*((uint8_t*) msg_ptr));
    Direction dir;
    msg_ptr += 1;

    switch (message_type) {
        case PlaceBomb:
            cout << "PlaceBomb\n";
        case PlaceBlock:
            cout << "PlaceBlock\n";
            if (len != 1) return false;
            break;
        case Move:
            cout << "Move\n";
            if (len != 2) return false;
            dir = (Direction) *((uint8_t*) msg_ptr);
            if (dir != Up && dir != Down && dir != Right && dir != Left) return false;
            break;
        default:
            return false;
    }
    return true;
}

bool parse_hello_message(char *message, size_t len, GameParameters &g) {
    char *msg_ptr = message;
    auto message_type = (ServerMessage) *((uint8_t*) msg_ptr);
    if (message_type != Hello) return false;

    msg_ptr += sizeof(uint8_t);
    uint8_t server_name_len = *((uint8_t*) msg_ptr);
    if (len != server_name_len + 13) return false;

    msg_ptr += sizeof(uint8_t);
    char server_name[server_name_len];
    memcpy(server_name, msg_ptr, server_name_len);
    msg_ptr += server_name_len;

    uint8_t player_count = *((uint8_t*) msg_ptr);
    msg_ptr += sizeof(uint8_t);

    uint16_t size_x = *((uint16_t*) msg_ptr);
    msg_ptr += sizeof(uint16_t);

    uint16_t size_y = *((uint16_t*) msg_ptr);
    msg_ptr += sizeof(uint16_t);

    uint16_t game_length = *((uint16_t*) msg_ptr);
    msg_ptr += sizeof(uint16_t);

    uint16_t explosion_radius = *((uint16_t*) msg_ptr);
    msg_ptr += sizeof(uint16_t);

    uint16_t bomb_timer = *((uint16_t*) msg_ptr);
    msg_ptr += sizeof(uint16_t);

    g = GameParameters(std::string(server_name), player_count, size_x, size_y,
                       game_length, explosion_radius, bomb_timer);
    return true;
}

ServerMessage parse_server_message(char *message, size_t len, GameState &g) {
    char *msg_ptr = message;
    uint8_t message_type = *((uint8_t*) msg_ptr);

    switch((ServerMessage) message_type) {
        case Hello:
            cout << "Hello\n";
            break;
        case AcceptedPlayer:
            cout << "AcceptedPlayer\n";
            break;
        case GameStarted:
            cout << "GameStarted\n";
            g = GameState({});
            return GameStarted;
        case Turn:
            cout << "Turn\n";
            //aggregate_game_state(g);
            break;
        case GameEnded:
            cout << "GameEnded\n";
            break;
    }
    return Hello;
}

void handle_game_info_from_server(udp::socket &gui_socket, tcp::socket &server_socket) {
    bool waiting_for_hello = false;
    char server_message[UDP_BUFFER_LENGTH];

    GameParameters parameters;
    GameState game_state;
    players_map_t lobby_players{};

    try {
        while (true) {
            size_t message_length = server_socket.receive(
                    boost::asio::buffer(server_message));

            if (waiting_for_hello) {
                if (parse_hello_message(server_message, message_length, parameters)) {
                    waiting_for_hello = false;
                }
            }
            else {
                ServerMessage m = parse_server_message(server_message, message_length, game_state);

                /*switch (parse_server_message(server_message, message_length, game_state)) {

                }*/
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

void handle_user_input_from_gui(udp::socket &gui_socket, tcp::socket &server_socket,
                                std::string &player_name) {

    bool first_message = true;
    bool send_join = false;
    char gui_message[UDP_BUFFER_LENGTH];

    try {
        while (true) {
            udp::endpoint sender_endpoint;
            size_t message_length = gui_socket.receive_from(
                    boost::asio::buffer(gui_message, UDP_BUFFER_LENGTH), sender_endpoint);

            if (check_gui_message_correctness(gui_message, message_length)) {
                if (first_message) {
                    curr_situation_mutex.lock();
                    if (current_situation == Lobby) send_join = true;
                    curr_situation_mutex.unlock();

                    if (send_join) send_join_request(player_name, server_socket);
                    first_message = false;
                }

            }
            //jeśli pierwsza wyślij join
            //wpp prześlij do serwera :)

        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {

    Options options;
    if (!check_program_options(options, argc, argv)) exit(EXIT_FAILURE);

    try
    {
        string gui_address, gui_port, server_address, server_port;
        get_hostname_and_port(options.gui_address, gui_address, gui_port);
        get_hostname_and_port(options.server_address, server_address, server_port);

        boost::asio::io_context io_context;
        tcp::resolver server_r(io_context);
        tcp::socket server_socket(io_context);

        tcp::resolver::results_type server_endpoints = server_r.resolve(server_address, server_port);
        boost::asio::connect(server_socket, server_endpoints);

        udp::socket gui_socket(io_context, udp::endpoint(udp::v6(), options.port));

        udp::resolver gui_r(io_context);
        udp::resolver::results_type endpoints = gui_r.resolve(gui_address, gui_port);

        std::string player_name = options.player_name;
        std::thread gui_receiver_thread([&gui_socket, &server_socket, &player_name]() {
            handle_user_input_from_gui(gui_socket, server_socket, player_name);
        });

        std::thread server_receiver_thread([&gui_socket, &server_socket]() {
            handle_game_info_from_server(gui_socket, server_socket);
        });

        gui_receiver_thread.join();
        server_receiver_thread.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
