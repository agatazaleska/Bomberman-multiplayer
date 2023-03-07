#ifndef BOMBOWE_ROBOTY_MESSAGE_SERIALIZER_H
#define BOMBOWE_ROBOTY_MESSAGE_SERIALIZER_H

#include <iostream>
#include "definitions.h"

// A simple helper function - copies [size] bytes from src to dest pointer.
void put_data_into_buffer(char *&dest_ptr, void *src, size_t size);

// The functions below serialize different types of messages and put them into
// a given buffer. They use helper functions defined in message-serializer.cpp

size_t serialize_lobby_message(GameParameters &parameters, char buffer[], players_map_t players);

size_t serialize_game_message(GameParameters &parameters, GameState &state, char buffer[]);

void serialize_hello_message(char buffer[], ServerOptions &options);

void serialize_accepted_player_message(char buffer[], std::string &name,
                                       std::string &address, uint8_t id);

void serialize_game_started_message(char buffer[], players_map_t &players);

void serialize_turn_message(char buffer[], events_list_t &events, uint16_t turn_nr);


#endif //BOMBOWE_ROBOTY_MESSAGE_SERIALIZER_H
