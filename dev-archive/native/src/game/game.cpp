// game.cpp -- RE2's answers to game.h.
#include "game.h"

namespace vn::game {

const char* ns() { return "app.ropeway."; }
std::string type(const char* rest) { return std::string(ns()) + rest; }
const char* project() { return "visceral-re2-vr"; }
bool second_character(MO* player) { return read_string(call_ptr(player, "get_Name")).rfind("pl1", 0) == 0; }   // pl1xxx = Claire

} // namespace vn::game
