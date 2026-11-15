// Calls into the app's Java side (android_main.cpp), from the SDL thread: the interface's thread.
#pragma once

// A static void method of GameActivity without arguments: "showSaveImport", "showSaveExport", "showGameFolder",
// "showShareLog", "showLicenses", "showBuilder", "showDriverImport", "showCheatImport", "showModImport",
// "showModFolderImport", "showExtraImport", "showShortcut", "restartApp".
void app_call_activity(const char* method);

#include <string>

// A normal Android text field over the game (TextFieldDialog, through GameActivity's static methods): the phone's own keyboard
// and clipboard. kind: 0 any, 1 nickname, 2 address, 3 digits, 4 password, 5 chat. The caller polls the state each frame:
// 0 none, 1 open, 2 accepted (app_text_field_result has the text), 3 cancelled; app_text_field_reset ends it.
void app_text_field_show(const std::string& title, const std::string& hint, const std::string& initial, int kind, int max_len,
                         const std::string& error);
int app_text_field_state();
std::string app_text_field_result();
void app_text_field_reset();

// The chat button's bottom sheet (ChatSheet). update gives it what to show: the chat's name and its lines ("kind US who US
// text US serial", apart by RS: see ChatSheet.java); the others poll what the player did.
void app_chat_sheet_show(int opacity, const std::string& title, const std::string& hint, const std::string& send);
bool app_chat_sheet_open();
void app_chat_sheet_update(const std::string& channel, const std::string& lines, const std::string& hint);
void app_chat_sheet_error(const std::string& text);
std::string app_chat_sheet_take_send();  // a line to say ("": none)
bool app_chat_sheet_take_next();         // the player asked for the next chat
std::string app_chat_sheet_take_join();  // the serial of a line with a room shared in it that was tapped ("": none)
void app_chat_sheet_close();
