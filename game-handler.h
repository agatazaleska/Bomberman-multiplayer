#ifndef BOMBOWE_ROBOTY_GAME_HANDLER_H
#define BOMBOWE_ROBOTY_GAME_HANDLER_H

#include "definitions.h"

player_id_and_player_t get_player_data(Buffer &msg_buffer);

player_id_and_score_t get_player_score(Buffer &msg_buffer);

void aggregate_game_state(Buffer &msg_buffer, GameState &state,
                          GameParameters &params);

#endif //BOMBOWE_ROBOTY_GAME_HANDLER_H
