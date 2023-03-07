#ifndef BOMBOWE_ROBOTY_DEFINITIONS_H
#define BOMBOWE_ROBOTY_DEFINITIONS_H

/*
This is a file containing all necessary structure definitions,
aliases, definitions and classes
*/

// Debug constant.
#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <boost/asio.hpp>
#include "options-parser.h"

struct Player;
struct Bomb;
struct Position;
struct Event;

using PlayerId = uint8_t;
using Score = uint32_t;
using BombId = uint32_t;

using scores_map_t = std::map<PlayerId , Score>;
using players_map_t = std::map<PlayerId , Player>;
using players_list_t = std::vector<Player>;
using player_id_set_t = std::set<PlayerId>;
using player_id_list_t = std::vector<PlayerId>;
using player_positions_map_t = std::map<PlayerId , Position>;
using positions_set_t = std::set<Position>;
using positions_list_t = std::vector<Position>;
using bombs_map_t = std::map<BombId, Bomb>;
using player_id_and_player_t = std::pair<PlayerId, Player>;
using player_id_and_score_t = std::pair<PlayerId, Score>;
using events_list_t = std::vector<Event>;

#define UDP_BUFFER_LENGTH           65507
#define SERVER_MESSAGES_NUMBER      4
#define TCP_BUFFER_LENGTH           2048
#define DIRECTIONS_NUMBER           4
#define GUI_MESSAGES_NUMBER         3
#define CLIENT_MESSAGES_NUMBER      4

// Some enums used in the task description.
enum ClientMessage {
    Join, ClientPlaceBomb, ClientPlaceBlock, ClientMove
};

enum ServerMessage {
    Hello, AcceptedPlayer, GameStarted, Turn, GameEnded, Unexpected
};

enum State {
    Lobby, Game, SendJoin
};

enum GuiMessage {
    PlaceBomb, PlaceBlock, Move
};

enum EventType {
    BombPlaced, BombExploded, PlayerMoved, BlockPlaced
};

// A structure for an object position on the board.
struct Position {
    uint16_t x;
    uint16_t y;

    Position(uint16_t x, uint16_t y): x(x), y(y){};

    bool operator< (const Position &right) const {
        if (x < right.x) return true;
        else if (x == right.x) return y < right.y;
        else return false;
    }

    Position() = default;
};

// A structure for a bomb - contains bomb position and its timer.
struct Bomb {
    Position position{};
    uint16_t timer;

    Bomb(const Position& position, uint16_t timer):
         position(position), timer(timer){};

    Bomb() = default;
};

// A structure for a player - contains player name and address.
struct Player {
    std::string name;
    std::string address;

    Player(std::string name, std::string address):
            name(std::move(name)), address(std::move(address)){};
};

// A structure for storing all game parameters
// provided by the server in a "Hello" message.
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

// A structure for storing all game attributes necessary
// to create a message for gui.
struct GameState {
    uint16_t turn;
    player_positions_map_t player_positions;
    players_map_t players;
    positions_set_t blocks;
    bombs_map_t bombs;
    positions_set_t explosions;
    scores_map_t scores;

    GameState(players_map_t &players_map, positions_set_t &initial_blocks,
              player_positions_map_t &initial_player_positions) {

        turn = 0;
        players = players_map;
        blocks = initial_blocks;
        player_positions = initial_player_positions;
        bombs = {};
        explosions = {};
        scores = {};

        for (auto &player: players_map)
            scores.insert(std::make_pair(player.first, 0));

        if (initial_player_positions.empty()) {
            for (auto &player: players_map)
                player_positions.insert(std::make_pair(player.first, Position(0,0)));
        }
    }
    GameState() = default;
};

// A structure representing the "receiving from server sending to gui" thread's
// data concerning the game. It contains all the necessary information
// to communicate with server and gui.
struct GameData {
    bool waiting_for_hello;
    GameParameters parameters;
    GameState game_state;
    players_map_t lobby_players;

    GameData() {
        waiting_for_hello = true;
        lobby_players = {};
    }
};

// A structure handling client's connections with server and gui.
// Uses boost::asio.
struct ConnectionsData {
    boost::asio::io_context io_context; // create io_context object

    boost::asio::ip::udp::socket gui_socket{io_context};
    boost::asio::ip::tcp::socket server_socket{io_context};
    boost::asio::ip::udp::endpoint gui_endpoint{};
    boost::asio::ip::tcp::endpoint server_endpoint{};
    std::string player_name;

    ConnectionsData(ClientOptions options) {
        std::string gui_address, gui_port, server_address, server_port;
        get_hostname_and_port(options.gui_address, gui_address, gui_port);
        get_hostname_and_port(options.server_address, server_address, server_port);

        // connect to server
        boost::asio::ip::tcp::resolver server_r(io_context);
        server_endpoint = *server_r.resolve(server_address, server_port);

        server_socket.connect(server_endpoint);
        if (debug) std::cerr << "connected to " << server_endpoint << "\n";

        server_socket.set_option(boost::asio::ip::tcp::no_delay(true));

        // create socket for communication with gui
        gui_socket = boost::asio::ip::udp::socket(
            io_context,boost::asio::ip::udp::endpoint(
                boost::asio::ip::udp::v6(), options.port));

        boost::asio::ip::udp::resolver gui_r(io_context);
        gui_endpoint = *gui_r.resolve(gui_address, gui_port).begin();

        player_name = options.player_name;
    }
};

// A struct for game event.
// Handles all types of events.
struct Event {
    uint8_t event_id;
    PlayerId player_id;
    BombId bomb_id = 0;
    Position position;
    player_id_list_t robots_destroyed;
    positions_list_t blocks_destroyed;

    Event() = default;
    void initialize_bomb_placed(BombId id, Position p) {
        event_id = (uint8_t) EventType::BombPlaced;
        bomb_id = id;
        position = p;
    }

    void initialize_bomb_exploded(BombId id, player_id_list_t &robots,
                                  positions_list_t &blocks) {

        event_id = (uint8_t) EventType::BombExploded;
        bomb_id = id;
        robots_destroyed = robots;
        blocks_destroyed = blocks;
    }

    void initialize_player_moved(PlayerId id, Position p) {
        event_id = (uint8_t) EventType::PlayerMoved;
        player_id = id;
        position = p;
    }

    void initialize_block_placed(Position p) {
        event_id = (uint8_t) EventType::BlockPlaced;
        position = p;
    }
};

// A buffer for storing messages from server.
// Handles all the difficulties caused by tcp communication.
class Buffer {
private:
    char *msg_buffer;
    size_t parsed_length; // parsed length in bytes
    size_t message_length; // message length in bytes
    boost::asio::ip::tcp::socket &socket;

public:
    // The constructor creates char array for message.
    explicit Buffer(boost::asio::ip::tcp::socket &socket): socket(socket) {
        msg_buffer = new char[TCP_BUFFER_LENGTH];
        message_length = 0;
        parsed_length = 0; // at the beginning we've parsed nothing
    }

    // This function receives a new portion of bytes into a buffer
    size_t receive_new_data() {
        msg_buffer = new char[TCP_BUFFER_LENGTH];
        size_t length = socket.receive(
                boost::asio::buffer(msg_buffer, TCP_BUFFER_LENGTH));

        message_length = length;
        parsed_length = 0;

        return length;
    }

    // This function receives a new portion of bytes into a new buffer
    // starting at [right_shift] position.
    size_t receive(size_t right_shift, char *new_buffer) {
        size_t length = socket.receive(
                boost::asio::buffer(new_buffer + right_shift,
                                    TCP_BUFFER_LENGTH - right_shift));

        if (debug)
            std::cerr << "received additional " << length << " bytes from server\n";

        return length;
    }

    // This function replaces the old buffer with a new buffer.
    // If a variable got split, [bytes_to_copy] bytes are copied from
    // the previous buffer to the beginning of a new buffer.
    void new_buffer(size_t bytes_to_copy) {
        char *new_buffer = new char[TCP_BUFFER_LENGTH];
        memcpy(new_buffer, msg_buffer + parsed_length, bytes_to_copy);

        message_length = receive(bytes_to_copy, new_buffer) + bytes_to_copy;
        parsed_length = 0;
        msg_buffer = new_buffer;
    }

    // This function reads uint8_t from the buffer
    // If necessary, calls new_buffer.
    uint8_t get_u8() {
        while (message_length - parsed_length < sizeof(uint8_t))
            new_buffer(message_length - parsed_length);

        uint8_t result = *((uint8_t*) (msg_buffer + parsed_length));
        parsed_length += sizeof(uint8_t);
        return result;
    }

    // The 3 following functions are analogical to the function above.
    uint16_t get_u16() {
        while (message_length - parsed_length < sizeof(uint16_t))
            new_buffer(message_length - parsed_length);

        uint16_t result = ntohs(*((uint16_t*) (msg_buffer + parsed_length)));
        parsed_length += sizeof(uint16_t);
        return result;
    }

    uint32_t get_u32() {
        while (message_length - parsed_length < sizeof(uint32_t))
            new_buffer(message_length - parsed_length);

        uint32_t result = ntohl(*((uint32_t*) (msg_buffer + parsed_length)));
        parsed_length += sizeof(uint32_t);
        return result;
    }

    std::string get_string(size_t len) {
        while (message_length - parsed_length < len)
            new_buffer(message_length - parsed_length);

        char result[len + 1];
        memcpy(result, msg_buffer + parsed_length, len);
        result[len] = 0;
        parsed_length += len;

        return {result};
    }

    // getters
    [[nodiscard]] size_t get_parsed_len() const { return parsed_length; }
    [[nodiscard]] size_t get_msg_len() const { return message_length; }

    // destructor
    ~Buffer() {
        delete[] msg_buffer;
    }
};


#endif //BOMBOWE_ROBOTY_DEFINITIONS_H
