#include "message-serializer.h"

// Helper function - copies [size] bites from src ptr to dest ptr.
void put_data_into_buffer(char *&dest_ptr, void *src, size_t size) {
    memcpy(dest_ptr, src, size);
    dest_ptr += size;
}

static void put_uint_32_into_buffer(char *&dest_ptr, uint32_t number) {
    uint32_t number_to_send = htonl(number);
    put_data_into_buffer(dest_ptr, &number_to_send, sizeof(uint32_t));
}

static void put_uint_16_into_buffer(char *&dest_ptr, uint16_t number) {
    uint16_t number_to_send = htons(number);
    put_data_into_buffer(dest_ptr, &number_to_send, sizeof(uint16_t));
}

static size_t serialize_object_size(char *&dest_ptr, uint32_t size) {
    put_uint_32_into_buffer(dest_ptr, size);
    return sizeof(uint32_t);
}

static size_t serialize_players_map(char *&dest_ptr, players_map_t &map) {
    size_t total_size = serialize_object_size(dest_ptr, (uint32_t) map.size());

    for (auto &player: map) { // serialize elements in a loop
        uint8_t player_id = player.first;
        std::string player_name = player.second.name;
        std::string player_address = player.second.address;
        auto player_name_len = (uint8_t) player_name.length();
        auto player_address_len = (uint8_t) player_address.length();

        put_data_into_buffer(dest_ptr, &player_id, 1);
        put_data_into_buffer(dest_ptr, &player_name_len, 1);
        put_data_into_buffer(dest_ptr, (void *) player_name.c_str(), player_name_len);
        put_data_into_buffer(dest_ptr, &player_address_len, 1);
        put_data_into_buffer(dest_ptr, (void *) player_address.c_str(), player_address_len);

        total_size += (3 * sizeof(uint8_t) + player_name_len + player_address_len);
    }
    return total_size;
}

static size_t serialize_position(char *&dest_ptr, const Position &position) {
    put_uint_16_into_buffer(dest_ptr, position.x);
    put_uint_16_into_buffer(dest_ptr, position.y);
    return 2 * sizeof(uint16_t);
}

static size_t serialize_bomb(char *&dest_ptr, const Bomb &bomb) {
    serialize_position(dest_ptr, bomb.position);
    put_uint_16_into_buffer(dest_ptr, bomb.timer);
    return 3 * sizeof(uint16_t);
}

static size_t serialize_player_positions_map(char *&dest_ptr, player_positions_map_t &map) {
    size_t total_size = serialize_object_size(dest_ptr, (uint32_t) map.size());

    for (const auto &player_and_pos: map) { // serialize elements in a loop
        uint8_t player_id = player_and_pos.first;
        put_data_into_buffer(dest_ptr, &player_id, sizeof(uint8_t));
        serialize_position(dest_ptr, player_and_pos.second);

        total_size += (sizeof(uint8_t) + 2 * sizeof(uint16_t));
    }
    return total_size;
}

static size_t serialize_scores_map(char *&dest_ptr, scores_map_t &map) {
    size_t total_size = serialize_object_size(dest_ptr, (uint32_t) map.size());

    for (auto player_and_score: map) { // serialize elements in a loop
        uint8_t player_id = player_and_score.first;
        uint32_t score = player_and_score.second;

        put_data_into_buffer(dest_ptr, &player_id, sizeof(uint8_t));
        put_uint_32_into_buffer(dest_ptr, score);

        total_size += (sizeof(uint8_t) + sizeof(uint32_t));
    }
    return total_size;
}

static size_t serialize_position_set(char *&dest_ptr, positions_set_t &positions) {
    size_t total_size = serialize_object_size(dest_ptr, (uint32_t) positions.size());

    for(const auto &pos: positions) // serialize elements in a loop
        total_size += serialize_position(dest_ptr, pos);

    return total_size;
}

static size_t serialize_bomb_list(char *&dest_ptr, bombs_map_t &bombs) {
    size_t total_size = serialize_object_size(dest_ptr, (uint32_t) bombs.size());

    for (auto &bomb: bombs)
        total_size += serialize_bomb(dest_ptr, bomb.second);

    return total_size;
}

static void serialize_position_list(char *&dest_ptr, positions_list_t &positions) {
    serialize_object_size(dest_ptr, (uint32_t) positions.size());

    for(const auto &pos: positions) // serialize elements in a loop
        serialize_position(dest_ptr, pos);
}

static void serialize_player_id_list(char *&dest_ptr, player_id_list_t &players) {
    serialize_object_size(dest_ptr, (uint32_t) players.size());

    PlayerId player_id;
    for(const auto &id: players) { // serialize elements in a loop
        player_id = id;
        put_data_into_buffer(dest_ptr, &player_id, sizeof(uint8_t));
    }
}

size_t serialize_lobby_message(GameParameters &parameters, char buffer[],
                               players_map_t players) {

    State msg_type = Lobby;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, sizeof(uint8_t));

    uint8_t name_length = (uint8_t) parameters.server_name.length();
    put_data_into_buffer(dest_ptr, &name_length, sizeof(uint8_t));
    put_data_into_buffer(dest_ptr, (void *) parameters.server_name.c_str(),
                         name_length);

    put_data_into_buffer(dest_ptr, &parameters.players_count, sizeof(uint8_t));
    put_uint_16_into_buffer(dest_ptr, parameters.size_x);
    put_uint_16_into_buffer(dest_ptr, parameters.size_y);
    put_uint_16_into_buffer(dest_ptr, parameters.game_length);
    put_uint_16_into_buffer(dest_ptr, parameters.explosion_radius);
    put_uint_16_into_buffer(dest_ptr, parameters.bomb_timer);

    size_t total_size = (name_length + 5 * sizeof(uint16_t) + 3 * sizeof(uint8_t));
    total_size += serialize_players_map(dest_ptr, players);
    return total_size;
}

size_t serialize_game_message(GameParameters &parameters, GameState &state,
                              char buffer[]) {

    State msg_type = Game;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, sizeof(uint8_t));

    uint8_t name_length = (uint8_t) parameters.server_name.length();
    put_data_into_buffer(dest_ptr, &name_length, sizeof(uint8_t));
    put_data_into_buffer(dest_ptr, (void *) parameters.server_name.c_str(),
                         name_length);

    put_uint_16_into_buffer(dest_ptr, parameters.size_x);
    put_uint_16_into_buffer(dest_ptr, parameters.size_y);
    put_uint_16_into_buffer(dest_ptr, parameters.game_length);
    put_uint_16_into_buffer(dest_ptr, state.turn);

    size_t total_size = name_length + 2 * sizeof(uint8_t) + 4 * sizeof(uint16_t);

    total_size += serialize_players_map(dest_ptr, state.players);
    total_size += serialize_player_positions_map(dest_ptr, state.player_positions);
    total_size += serialize_position_set(dest_ptr, state.blocks);
    total_size += serialize_bomb_list(dest_ptr, state.bombs);
    total_size += serialize_position_set(dest_ptr, state.explosions);
    total_size += serialize_scores_map(dest_ptr, state.scores);

    return total_size;
}

void serialize_hello_message(char buffer[], ServerOptions &options) {
    ServerMessage msg_type = Hello;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, sizeof(uint8_t));

    uint8_t name_length = (uint8_t) options.server_name.length();
    put_data_into_buffer(dest_ptr, &name_length, sizeof(uint8_t));
    put_data_into_buffer(dest_ptr, (void *) options.server_name.c_str(),
                         name_length);

    put_data_into_buffer(dest_ptr, &options.players_count, sizeof(uint8_t));
    put_uint_16_into_buffer(dest_ptr, options.size_x);
    put_uint_16_into_buffer(dest_ptr, options.size_y);
    put_uint_16_into_buffer(dest_ptr, options.game_length);
    put_uint_16_into_buffer(dest_ptr, options.explosion_radius);
    put_uint_16_into_buffer(dest_ptr, options.bomb_timer);
}

void serialize_accepted_player_message(char buffer[], std::string &name,
                                       std::string &address, uint8_t id) {

    ServerMessage msg_type = AcceptedPlayer;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, sizeof(uint8_t));

    put_data_into_buffer(dest_ptr, &id, sizeof(uint8_t));
    uint8_t name_length = (uint8_t) name.length();
    put_data_into_buffer(dest_ptr, &name_length, sizeof(uint8_t));
    put_data_into_buffer(dest_ptr, (void *) name.c_str(),
                         name_length);

    uint8_t address_length = (uint8_t) address.length();
    put_data_into_buffer(dest_ptr, &address_length, sizeof(uint8_t));
    put_data_into_buffer(dest_ptr, (void *) address.c_str(),
                         address_length);
}

void serialize_game_started_message(char buffer[], players_map_t &players) {
    ServerMessage msg_type = GameStarted;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, sizeof(uint8_t));

    serialize_players_map(dest_ptr, players);
}

void serialize_turn_message(char buffer[], events_list_t &events, uint16_t turn_nr) {
    ServerMessage msg_type = Turn;
    char *dest_ptr = buffer;
    put_data_into_buffer(dest_ptr, &msg_type, sizeof(uint8_t));

    put_uint_16_into_buffer(dest_ptr, turn_nr);
    put_uint_32_into_buffer(dest_ptr, (uint32_t) events.size());

    uint8_t id;
    for (auto &event : events) {
        id = event.event_id;
        put_data_into_buffer(dest_ptr, &id, sizeof(uint8_t));
        switch ((EventType) id) {
            case BombPlaced:
                put_uint_32_into_buffer(dest_ptr, event.bomb_id);
                serialize_position(dest_ptr, event.position);
                break;
            case BombExploded:
                put_uint_32_into_buffer(dest_ptr, event.bomb_id);
                serialize_player_id_list(dest_ptr, event.robots_destroyed);
                serialize_position_list(dest_ptr, event.blocks_destroyed);
                break;
            case PlayerMoved:
                put_data_into_buffer(dest_ptr, &event.player_id, sizeof(uint8_t));
                serialize_position(dest_ptr, event.position);
                break;
            case BlockPlaced:
                serialize_position(dest_ptr, event.position);
                break;
        }
    }
}
