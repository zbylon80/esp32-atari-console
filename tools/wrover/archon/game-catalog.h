#pragma once
#include <stdint.h>
#include <stddef.h>
enum class GameProfile {Archon,Dropzone,Robbo,Joystick,Goonies,Karate,Spy,Misja};
struct GameInfo {const char* id;const char* title;const char* file;const char* hint;GameProfile profile;int cartType;const uint8_t* data;size_t bytes;const char* players;const char* disk2;bool originalOs;const char* help;const char* about;};
struct GameMedia {const char* file;const uint8_t* data;size_t bytes;const char* sha256;};
extern const GameMedia console_media[];
extern const int console_media_count;
extern const GameInfo console_games[];
extern const int console_game_count;
inline const GameInfo& game_info(int index){return console_games[index];}
