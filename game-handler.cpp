#include "game-handler.h"

// This function reads and returns player id and player data from a buffer.
player_id_and_player_t get_player_data(Buffer &msg_buffer) {
    PlayerId id = msg_buffer.get_u8();

    uint8_t name_len = msg_buffer.get_u8();
    std::string player_name = msg_buffer.get_string(name_len);

    uint8_t address_len = msg_buffer.get_u8();
    std::string player_address = msg_buffer.get_string(address_len);

    return std::make_pair(id, Player(player_name, player_address));
}

// This function reads and returns player id and score from a buffer.
player_id_and_score_t get_player_score(Buffer &msg_buffer) {
    PlayerId id = msg_buffer.get_u8();
    Score score = msg_buffer.get_u32();

    return std::make_pair(id, score);
}

// This function reads information about a new bomb from a buffer
// and places it in the bomb set.
static void add_bomb(Buffer &msg_buffer, GameState &state, uint16_t timer) {
    BombId id = msg_buffer.get_u32();
    uint16_t x = msg_buffer.get_u16();
    uint16_t y = msg_buffer.get_u16();

    // add a new bomb to the set
    state.bombs.insert(std::make_pair(id, Bomb(Position(x, y), timer)));
}

// This function calculates explosion fields
// and adds them to the explosions set.
static void add_explosion_fields(BombId bomb, uint16_t radius, GameState &state,
                          uint16_t size_x, uint16_t size_y) {

    uint16_t pos_x = state.bombs[bomb].position.x;
    uint16_t pos_y = state.bombs[bomb].position.y;

    state.explosions.insert(Position(pos_x, pos_y)); // explosion center
    if (state.blocks.find(Position(pos_x, pos_y)) != state.blocks.end()) return;

    for (uint16_t i = 1; i <= radius; i++) { // up
        if (pos_y + i == size_y) break; // break if we "get out of board"
        state.explosions.insert(Position(pos_x, (uint16_t) (pos_y + i)));
        // if there is a block on explosion's path, it stops the explosion from spreading
        if (state.blocks.find(Position(pos_x, (uint16_t) (pos_y + i))) != state.blocks.end()) break;
    }

    // analogically
    for (uint16_t i = 1; i <= radius; i++) { // down
        if (pos_y - i == -1) break;
        state.explosions.insert(Position(pos_x, (uint16_t) (pos_y - i)));
        if (state.blocks.find(Position(pos_x, (uint16_t) (pos_y - i))) != state.blocks.end()) break;
    }

    for (uint16_t i = 1; i <= radius; i++) { // left
        if (pos_x - i == -1) break;
        state.explosions.insert(Position((uint16_t) (pos_x - i), pos_y));
        if (state.blocks.find(Position((uint16_t) (pos_x - i), pos_y)) != state.blocks.end()) break;
    }

    for (uint16_t i = 1; i <= radius; i++) { // right
        if (pos_x + i == size_x) break;
        state.explosions.insert(Position((uint16_t) (pos_x + i), pos_y));
        if (state.blocks.find(Position((uint16_t) (pos_x + i), pos_y)) != state.blocks.end()) break;
    }
}

// This function reads and returns position from a buffer.
static Position get_position(Buffer &msg_buffer) {
    uint16_t x = msg_buffer.get_u16();
    uint16_t y = msg_buffer.get_u16();
    return {x, y};
}

// This function simulates explosion - adds destroyed robots to players_killed,
// destroyed block positions to blocks_destroyed and explosion positions
// to state.explosions.
static void simulate_explosion(Buffer &msg_buffer, uint16_t radius, GameState &state,
                        uint16_t size_x, uint16_t size_y, player_id_set_t &players_killed,
                        positions_set_t &blocks_destroyed) {

    BombId bomb_id = msg_buffer.get_u32();
    add_explosion_fields(bomb_id, radius, state, size_x, size_y);
    state.bombs.erase(bomb_id);

    // helper sets - players_killed and blocks_destroyed
    // ensure that a (player doesn't die)/(block doesn't explode) twice in a turn
    uint32_t destroyed_players = msg_buffer.get_u32();
    for (size_t i = 0; i < destroyed_players; i++) {
        PlayerId player = msg_buffer.get_u8();
        players_killed.insert(player);
    }

    uint32_t destroyed_blocks = msg_buffer.get_u32();
    for (size_t i = 0; i < destroyed_blocks; i++) {
        Position block_position = get_position(msg_buffer);
        blocks_destroyed.insert(block_position);
    }
}

// This function reads new player position from a buffer and
// changes it in GameState.
static void move_player(Buffer &msg_buffer, GameState &state) {
    PlayerId id = msg_buffer.get_u8();
    Position pos = get_position(msg_buffer);

    // delete old, insert new player position
    state.player_positions.erase(id);
    state.player_positions.insert(std::make_pair(id, pos));
}

// This function reads new block position from a buffer and
// adds the block GameState.
static void add_block(Buffer &msg_buffer, GameState &state) {
    Position block_position = get_position(msg_buffer);
    state.blocks.insert(block_position);
}

// This function calculates game state after one turn.
void aggregate_game_state(Buffer &msg_buffer, GameState &state,
                          GameParameters &params) {

    // firstly, reduce every bomb's timer
    for (const auto& bomb: state.bombs)
        state.bombs[bomb.first].timer = (uint16_t) (state.bombs[bomb.first].timer - 1);

    player_id_set_t players_killed = {};
    positions_set_t blocks_destroyed = {};
    uint16_t turn = msg_buffer.get_u16();
    state.turn = turn;

    uint32_t events_list_size = msg_buffer.get_u32();
    for (size_t i = 0; i < events_list_size; i++) {
        uint8_t event_type = msg_buffer.get_u8();

        switch(EventType (event_type)) {
            case BombPlaced:
                add_bomb(msg_buffer, state, params.bomb_timer);
                break;
            case BombExploded:
                simulate_explosion(msg_buffer, params.explosion_radius, state,
                                   params.size_x, params.size_y, players_killed,
                                   blocks_destroyed);
                break;
            case PlayerMoved:
                move_player(msg_buffer, state);
                break;
            case BlockPlaced:
                add_block(msg_buffer, state);
                break;
        }
    }
    for (auto player: players_killed)
        state.scores[player] += 1; // add scores
    for (const auto& block: blocks_destroyed)
        state.blocks.erase(block); // destroy blocks
}
