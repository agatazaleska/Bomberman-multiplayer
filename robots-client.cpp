#include <thread>
#include <boost/asio.hpp>
#include "options-parser.h"
#include "definitions.h"
#include "message-serializer.h"
#include "game-handler.h"

// Global variable defining current state.
// In addition to Game and Lobby, I defined SendJoin state which
// signals that we should send join message to the server.
State current_state = SendJoin;
// The variable is atomic - I ensure that using mutex.
std::mutex curr_state_mutex;

// Helper functions for getting and setting current_state.
static State get_state() {
    curr_state_mutex.lock();
    State s = current_state;
    curr_state_mutex.unlock();
    return s;
}

static void set_state(State state) {
    curr_state_mutex.lock();
    current_state = state;
    curr_state_mutex.unlock();
}

// This function sends join request with a given player name to the server.
void send_join_request(ConnectionsData &connections) {
    std::string name = connections.player_name;

    // Calculating message size and putting it (the message) into a buffer.
    size_t message_size = 2 * sizeof(uint8_t) + name.length();
    char join_msg[message_size];
    char *msg_ptr = join_msg;
    auto msg_type = (uint8_t) ClientMessage::Join;
    uint8_t name_len = (uint8_t) name.length();

    put_data_into_buffer(msg_ptr, &msg_type, sizeof(uint8_t));
    put_data_into_buffer(msg_ptr, &name_len, sizeof(uint8_t));

    memcpy(msg_ptr, name.c_str(), name_len);

    connections.server_socket.send(boost::asio::buffer(
        std::string(join_msg, message_size)));

    if (debug) {
        std::cerr << "sent " << message_size <<
        " bytes to " << connections.server_endpoint << "\n";
    }
}

// Helper function serializing lobby message and then sending it to gui.
void send_lobby_to_gui(GameData &status, ConnectionsData &connections,
                       char *buffer) {

    size_t message_size = serialize_lobby_message(status.parameters, buffer,
                                                  status.lobby_players);

    connections.gui_socket.send_to(
            boost::asio::buffer(buffer,message_size), connections.gui_endpoint);

    if (debug) {
        std::cerr << "sent " << message_size <<
        " bytes to " << connections.gui_endpoint << "\n";
    }
}

// Helper function serializing game message and then sending it to gui.
void send_game_to_gui(GameData &status, ConnectionsData &connections,
                      char *buffer) {

    size_t message_size = serialize_game_message(
            status.parameters,status.game_state, buffer);

    connections.gui_socket.send_to(
            boost::asio::buffer(buffer, message_size),connections.gui_endpoint);

    if (debug) {
        std::cerr << "sent " << message_size <<
        " bytes to " << connections.gui_endpoint << "\n";
    }
}

// This function checks gui message correctness:,
// checks whether the first byte describes a valid message type
// and then compares actual message length with expected message length.
bool check_gui_message_correctness(char *gui_message, size_t len) {
    char *msg_ptr = gui_message;
    uint8_t message_type = (*((uint8_t*) msg_ptr));
    if (message_type >= GUI_MESSAGES_NUMBER) return false;

    uint8_t dir;
    msg_ptr += 1;

    switch ((GuiMessage) message_type) {
        case PlaceBomb:
            if (debug) std::cerr << "received PlaceBomb message from gui\n";
            break;
        case PlaceBlock:
            if (debug) std::cerr << "received PlaceBlock message from gui\n";
            if (len != 1) return false;
            break;
        case Move:
            if (debug) std::cerr << "received Move message from gui\n";
            if (len != 2) return false;
            dir = *((uint8_t*) msg_ptr);
            if (dir >= DIRECTIONS_NUMBER) return false;
            break;
        default:
            return false;
    }
    return true;
}

// This function parses (probably) hello message and sets GameParameters accordingly.
void parse_hello_message(Buffer &msg_buffer, GameParameters &g) {
    uint8_t message_type = msg_buffer.get_u8();
    if ((ServerMessage) message_type != Hello) exit(EXIT_FAILURE);

    if (debug) std::cerr << "received Hello message from server\n";

    uint8_t server_name_len = msg_buffer.get_u8();
    std::string server_name = msg_buffer.get_string(server_name_len);

    uint8_t player_count = msg_buffer.get_u8();
    uint16_t size_x = msg_buffer.get_u16();
    uint16_t size_y = msg_buffer.get_u16();

    uint16_t game_length = msg_buffer.get_u16();
    uint16_t explosion_radius = msg_buffer.get_u16();
    uint16_t bomb_timer = msg_buffer.get_u16();

    g = GameParameters(server_name, player_count, size_x, size_y,
                       game_length, explosion_radius, bomb_timer);
}

// This function reads player map from the server and creates new GameState.
GameState handle_game_started(Buffer &msg_buffer) {
    players_map_t players{};
    uint32_t map_len = msg_buffer.get_u32();

    for (size_t i = 0; i < map_len; i++)
        players.insert(get_player_data(msg_buffer));

    positions_set_t initial_blocks{};
    player_positions_map_t initial_player_positions{};
    return {players, initial_blocks, initial_player_positions};
}

// This function checks if players scores are calculated correctly.
// Returns true if they are, false if they aren't.
bool handle_game_ended(Buffer &msg_buffer, GameState &state) {
    scores_map_t player_scores{};
    uint32_t map_len = msg_buffer.get_u32();

    for (size_t i = 0; i < map_len; i++)
        player_scores.insert(get_player_score(msg_buffer));

    for (auto score: state.scores)
        if (score.second != player_scores[score.first]) return false;

    return true;
}

// This function parses a message received in Lobby state.
// Exits if the first byte doesn't represent a valid message type.
ServerMessage parse_lobby_msg(Buffer &msg_buffer, GameState &g,
                              players_map_t &lobby_players, GameParameters &params) {

    uint8_t message_type = msg_buffer.get_u8();
    if (message_type > SERVER_MESSAGES_NUMBER) exit(EXIT_FAILURE);

    positions_set_t initial_blocks{};
    player_positions_map_t initial_player_positions{};
    switch((ServerMessage) message_type) {
        case AcceptedPlayer: // adding player
            if (debug) std::cerr << "received AcceptedPlayer message from server\n";
            lobby_players.insert(get_player_data(msg_buffer));
            return AcceptedPlayer;
        case GameStarted: // starting game
            if (debug) std::cerr << "received GameStarted message from server\n";
            g = handle_game_started(msg_buffer);
            return GameStarted;
        case Turn: // starting game
            if (debug) std::cerr << "received Turn message from server\n";
            g = GameState(lobby_players, initial_blocks, initial_player_positions);
            aggregate_game_state(msg_buffer, g, params);
            return Turn;
        default: // ignoring other message types
            return Unexpected;
    }
}

// This function parses a message received in Game state.
// Exits if the first byte doesn't represent a valid message type.
ServerMessage parse_in_game_msg(Buffer &msg_buffer, GameState &g, GameParameters &params) {

    uint8_t message_type = msg_buffer.get_u8();
    if (message_type > SERVER_MESSAGES_NUMBER) exit(EXIT_FAILURE);

    switch((ServerMessage) message_type) {
        case Turn: // aggregating game state
            if (debug) std::cerr << "received Turn message from server\n";
            g.explosions = {};
            aggregate_game_state(msg_buffer, g, params);
            return Turn;
        case GameEnded: // ending game
            if (debug) std::cerr << "received GameEnded message from server\n";
            if (!handle_game_ended(msg_buffer, g))
                std::cerr << "INCORRECT SCORES CALCULATION\n";
            return GameEnded;
        default: // we ignore other message types
            return Unexpected;
    }
}

// This function handles a message stored in a buffer received from the server.
// Acts accordingly to received message.
void handle_server_message(GameData &status, Buffer &msg_buffer,
                           ConnectionsData &connections) {

    char buffer_for_gui[UDP_BUFFER_LENGTH];
    if (status.waiting_for_hello) {
        parse_hello_message(msg_buffer, status.parameters);
        status.waiting_for_hello = false;
        send_lobby_to_gui(status, connections, buffer_for_gui);
    }
    else {
        if (get_state() == Lobby || get_state() == SendJoin) {
            ServerMessage msg = parse_lobby_msg(msg_buffer, status.game_state,
                                                status.lobby_players,status.parameters);

            if (msg == GameStarted || msg == Turn) set_state(Game);
            else if (msg == AcceptedPlayer) {
                send_lobby_to_gui(status, connections, buffer_for_gui);
            }
        }
        else {
            ServerMessage msg = parse_in_game_msg(msg_buffer, status.game_state,
                                                  status.parameters);

            if (msg == Turn) {
                send_game_to_gui(status, connections, buffer_for_gui);
            }
            else if (msg == GameEnded) {
                status.lobby_players = {};
                send_lobby_to_gui(status, connections, buffer_for_gui);
                set_state(SendJoin);
            }
        }
    }
}

// Function executed by a thread.
// It receives messages from the server in an endless loop and
// sends lobby and game state to gui.
void handle_game_info_from_server(ConnectionsData &connections) {
    GameData status;

    try {
        while (true) {
            Buffer buffer(connections.server_socket);
            size_t message_size = buffer.receive_new_data();

            if (debug) {
                std::cerr << "received " << message_size <<
                " bytes from " << connections.server_endpoint << "\n";
            }

            while(buffer.get_msg_len() != buffer.get_parsed_len()) {
                handle_server_message(status, buffer, connections);
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

// Function executed by another thread.
// It receives messages from gui in an endless loop and sends
// user input to the server.
void handle_user_input_from_gui(ConnectionsData &connections) {
    char gui_message[UDP_BUFFER_LENGTH];

    try {
        while (true) {
            boost::asio::ip::udp::endpoint sender_endpoint;
            size_t message_size = connections.gui_socket.receive_from(
                boost::asio::buffer(gui_message, UDP_BUFFER_LENGTH),
                                           sender_endpoint);

            if (debug) {
                std::cerr << "received " << message_size <<
                " bytes from " << connections.gui_endpoint << "\n";
            }

            if (check_gui_message_correctness(gui_message, message_size)) {
                if (get_state() == SendJoin) {
                    send_join_request(connections);
                    set_state(Lobby);
                }
                else {
                    gui_message[0] = char ((int) gui_message[0] + 1);
                    connections.server_socket.send(
                        boost::asio::buffer(gui_message, message_size));

                    if (debug) {
                        std::cerr << "sent " << message_size <<
                        " bytes to " << connections.server_endpoint << "\n";
                    }
                }
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

// The main function. Connects client to gui and server.
// Launches two threads executing above functions.
int main(int argc, char* argv[]) {
    ClientOptions options;
    if (!check_client_options(options, argc, argv)) exit(EXIT_FAILURE);

    try {
        ConnectionsData connections_data(options);

        std::string player_name = options.player_name;
        std::thread gui_receiver_thread(handle_user_input_from_gui,
                                        std::ref(connections_data));

        std::thread server_receiver_thread(handle_game_info_from_server,
                                           std::ref(connections_data));

        gui_receiver_thread.join();
        server_receiver_thread.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
