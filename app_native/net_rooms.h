// The app's side of the network rooms (mh4n/rooms.h, Azahar's multiplayer rooms): joining, hosting and leaving from
// the settings panel's Multiplayer tab, the room's chat and notices kept as lines, and a lobby's list of rooms fetched
// on a thread of its own. The runtime's local-wireless service talks to the process's room client by itself, so a room
// joined here is the one the game's Local Play (the Gathering Hall) reaches; nothing is passed to the game.
//
// No room server is built in: a room's address is the player's to type, or picked from a lobby's list or this Wi-Fi's.
// The lobby comes set to a community one (settings_panel.cpp DEFAULT_LOBBY), and the player can type another or none.
// Everything here runs on the app's main thread (the interface's), but for the lobby's fetch, which has its own.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mh4n/hub.h"
#include "mh4n/rooms.h"

// Something the room said, or this phone did in it, for the chat and the notices over the game
struct NetLine {
    enum Kind { Chat, Mine, Joined, Left, Kicked, Banned, Failed, YouJoined, YouLeft } kind = Chat;
    std::string nick, text;  // who; what they said (Chat, Mine), the room's name (YouJoined)
    mh4n::rooms::Error error = mh4n::rooms::Error::None;  // (Failed)
    double time = 0;         // seconds since the app started, when it came
    unsigned serial = 0;     // goes up by one with each line
};

// Why a join or a host request was not even tried
enum class NetProblem { None, Nickname, Address, Port, PortBusy };

// Each frame, whether the settings are open or not: the room's events into the lines (the last 60).
void net_poll();
const std::vector<NetLine>& net_lines();

// "host" or "host:port" (24872 when it has none): false when it is not an address the room client can join.
bool net_parse_address(const std::string& text, std::string* host, std::uint16_t* port);
// "1".."65535"
bool net_parse_port(const std::string& text, std::uint16_t* port);

// Joins the room at `address` (a room this phone hosts is closed first, unless it is that one). The answer comes as
// the client's state (mh4n::rooms::GetStatus).
NetProblem net_join(const std::string& address, const std::string& nickname, const std::string& password);
// Opens a room on this phone (UDP `port`, `slots` hunters, `password` or none) and joins it.
NetProblem net_host(const std::string& port, int slots, const std::string& nickname, const std::string& password);
// This phone's IPv4 addresses (not the loopback), the ones others join a room it hosts at: Wi-Fi's first
std::vector<std::string> net_local_addresses();
void net_leave();       // leaves the room (a room this phone hosts stays open)
// Crossplay ([Network] Crossplay, Settings -> Multiplayer): hub servers and a Citra/Azahar room at the same time, as MHGU's.
// Off: hub servers only, a room joined replaces them.
// What to play online with ([Network] PlayWith, Settings > Multiplayer > Play online with): this app's hub
// servers only (the default), a Citra Room (Azahar / Citra) only, or Mixed: both at once. In Mixed the game's frames go to both, so a
// hub the game hosts is also in the room: Azahar and Citra players in it play with the hub servers' players too, all of them
// reaching the one that hosts.
enum NetPlayWith { PLAY_HUBS = 0, PLAY_ROOMS = 1, PLAY_MIXED = 2 };
int net_play_with();
void net_set_play_with(int mode);  // saves it, and applies it now: the hubs or the room that the mode leaves out end
bool net_uses_hubs();              // hubs or Mixed
bool net_uses_rooms();             // rooms or Mixed
bool net_crossplay();              // Mixed: hubs and a room together
void net_hub_apply();              // uses the servers now (after one was changed): none, or not in this mode, stops the hubs
void net_leave_hubs();  // stops using hub servers (a room joined stays)
void net_close_room();  // leaves, and closes the room this phone hosts
// Sends a chat line to the room (and keeps it as this phone's): false when not in a room.
bool net_say(const std::string& text);

// A lobby's list of rooms (GET <lobby>/lobby), fetched on a thread of its own
struct NetLobby {
    bool busy = false, done = false;  // fetching; fetched (list holds the answer)
    std::string url;                  // the address it was asked of
    mh4n::rooms::LobbyList list;
};
void net_lobby_fetch(const std::string& urls);  // one lobby, or several apart by spaces: their rooms as one list
// The lobbies (Citra Web API URLs) as a list ([Network] LobbyUrls; empty: the two that come with the app, "off": none; up to 8)
std::vector<std::string> net_lobby_list();
bool net_lobby_list_set(const std::vector<std::string>& list);  // false: too many
std::string net_lobby_urls_text();                              // the list apart by spaces ("" when none)
std::vector<std::string> net_lobby_default_list();
NetLobby net_lobby();

// ---- hub servers (mh4n/hub.h): the same local play through an MHGURecomp hub server instead of a room. Set in
// mh4u-app.ini's [Network] (HubServers, HubKey; HubFamily is not used while mh4n::hub::kCrossRegion is on), and
// used by the Multiplayer tab's "Hub servers" section. Players of every region with the same matchmaking key see each
// other's hubs.
// What the Hub chat is scoped to: 16 hex digits of the hub this device hosts or plays on (its host's MAC, the matchmaking key
// and the game's family), the same on every device in that hub and on none outside it; "" when it is in no hub.
std::string net_hub_chat_scope();
// The servers as a list ([Network] HubServers: the servers apart by spaces; empty is the default server, "off" none; up to
// mh4n::hub::kMaxServers). net_hub_server_list gives them as they are used (the default one when none was typed, none
// when it is off); _set saves a list (an empty one is "off"; false: too many). A file of the previous build, with the two
// fields HubServer1 and HubServer2, is read as the list they made, and they go when a list is saved. Call net_hub_apply
// after a change.
std::vector<std::string> net_hub_server_list();
bool net_hub_server_list_set(const std::vector<std::string>& list);
// The list as the one text the hub client takes (mh4n::hub::ParseServers): "off" when there are none
std::string net_hub_servers_text();
// Starts using the servers the settings name (a room joined or hosted is left): the answer comes as the client's state.
bool net_hub_join(std::string* error);
// Asks each server (on a thread of its own) whether it answers, and how many hubs of this game it lists
struct NetHubCheck {
    bool busy = false, done = false;
    std::vector<mh4n::hub::ServerCheck> servers;
};
void net_hub_check();
NetHubCheck net_hub_checked();
