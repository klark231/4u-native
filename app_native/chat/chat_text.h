// The chat's own texts, English and Spanish side by side (chat_text.cpp): the reasons a line was not sent, the lines the
// chat writes itself, a shared key or room in words, the chats' names. Both ports show them the same.
#pragma once
#include <string>

#include "chat.h"

namespace chat {

void set_spanish(bool on);  // the menu's language
bool spanish();
inline const char *t(const char *en, const char *es) { return spanish() ? es : en; }

std::string explain(const std::string &coded);  // a reason from say(), add_friend()...: a sentence ("" stays "")
std::string line_text(const Line &l);           // a line's text as shown (the chat's own lines, a share in words)
std::string who(const Line &l);                // who said it, as shown: You, "Name (Room)", "Name (MH4U)" / "Name (MHGU)"
std::string channel_name(int channel);          // Global, General (MHGU), Hub / Room, Friends
std::string channel_help(int channel);          // who reads it
std::string status_text(Status s, int up, int total);  // the online chat's
std::string hub_chat_name();                    // the hub's or room's chat switch: Hub chat / Room chat
std::string chat_state_text(Status s, int up, int total);  // both chats in a word: Off, the online chat's, Hub chat only
std::string ago(int64_t unix_time);             // "just now", "3 min ago"

}  // namespace chat
