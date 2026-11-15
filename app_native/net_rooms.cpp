// The app's side of the network rooms (net_rooms.h).
#include "net_rooms.h"

#include "app_settings.h"
#include "chat_app.h"
#include "net_lan.h"

#include <SDL3/SDL.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <strings.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace rooms = mh4n::rooms;

namespace {

constexpr size_t kMaxLines = 60;

std::vector<NetLine> g_lines;
unsigned g_serial = 0;
rooms::State g_state = rooms::State::Idle;  // the state the lines last told of

void add(NetLine line) {
    // (a chat line, someone's or this phone's: the port's chat too, as its Room - chat_app.cpp)
    if (line.kind == NetLine::Chat || line.kind == NetLine::Mine) chat_app_room_line(line.nick, line.text, line.kind == NetLine::Mine);
    line.time = SDL_GetTicks() / 1000.0;
    line.serial = ++g_serial;
    g_lines.push_back(std::move(line));
    if (g_lines.size() > kMaxLines) g_lines.erase(g_lines.begin(), g_lines.begin() + (g_lines.size() - kMaxLines));
}

bool in_room(rooms::State state) { return state == rooms::State::Joined || state == rooms::State::Moderator; }

// The lobby's fetch: on the heap and never freed, as its thread may outlive everything else at the app's end
struct LobbyState {
    std::mutex mutex;
    NetLobby lobby;
};
LobbyState& lobby_state() {
    static LobbyState* state = new LobbyState;
    return *state;
}

std::vector<std::string> SplitServers(const std::string& text) {
    std::vector<std::string> words;
    std::string cur;
    for (const char ch : text + " ") {
        if (ch == ' ' || ch == ',' || ch == ';' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (!cur.empty()) words.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    return words;
}
bool IsOff(const std::vector<std::string>& words) {
    return words.size() == 1 && !strcasecmp(words[0].c_str(), "off");
}
}  // namespace

void net_poll() {
    net_lan_poll();  // (the answers to searches on this Wi-Fi while this phone hosts a room)
    // Hub servers are always on, as in MHGU: used at the first frame (Server 1 "off" and no Server 2: none). The game's
    // Local Play then finds hubs with nothing to press; the hub waits for the game's console ID by itself (Client::JoinHubs).
    static bool auto_used = false;
    if (!auto_used) {
        auto_used = true;
        net_hub_apply();
    }
    std::vector<rooms::Event> events = rooms::TakeEvents();
    if (events.empty()) return;
    const rooms::Status status = rooms::GetStatus();  // (this phone's nickname, the room's name)
    // (the hub servers also show as Joined: only a room (room_session) is "you joined", "you left")
    static bool g_real_room = false;
    if (g_real_room && !status.room_session && status.state != rooms::State::Idle) g_real_room = false;
    // (the room lost or refused: its state comes before the failure, in the same batch; "you left" only without one)
    bool failed = false, left = false;
    for (const rooms::Event& e : events) {
        NetLine line;
        switch (e.kind) {
        case rooms::Event::Kind::StateChanged:
            if (status.room_session && in_room(e.state) && !in_room(g_state)) {
                line.kind = NetLine::YouJoined;
                line.text = status.room.name;
                add(line);
                g_real_room = true;
            } else if (e.state == rooms::State::Idle && g_real_room) {
                left = true;
                g_real_room = false;
            }
            g_state = e.state;
            break;
        case rooms::Event::Kind::Failed:
            failed = true;
            line.kind = NetLine::Failed;
            line.error = e.error;
            add(line);
            break;
        case rooms::Event::Kind::Chat:
            line.kind = NetLine::Chat;
            line.nick = e.nickname;
            line.text = e.message;
            add(line);
            break;
        case rooms::Event::Kind::MemberJoined:
        case rooms::Event::Kind::MemberLeft:
        case rooms::Event::Kind::MemberKicked:
        case rooms::Event::Kind::MemberBanned:
            if (e.nickname == status.nickname && e.kind == rooms::Event::Kind::MemberJoined) break;  // (this phone)
            line.kind = e.kind == rooms::Event::Kind::MemberJoined ? NetLine::Joined
                        : e.kind == rooms::Event::Kind::MemberLeft ? NetLine::Left
                        : e.kind == rooms::Event::Kind::MemberKicked ? NetLine::Kicked
                                                                     : NetLine::Banned;
            line.nick = e.nickname;
            add(line);
            break;
        default:
            break;
        }
    }
    if (left && !failed) {
        NetLine line;
        line.kind = NetLine::YouLeft;
        add(line);
    }
}

const std::vector<NetLine>& net_lines() {
    return g_lines;
}

bool net_parse_port(const std::string& text, std::uint16_t* port) {
    if (text.empty() || text.size() > 5 || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return false;
    const long n = std::strtol(text.c_str(), nullptr, 10);
    if (n < 1 || n > 65535) return false;
    *port = static_cast<std::uint16_t>(n);
    return true;
}

bool net_parse_address(const std::string& typed, std::string* host, std::uint16_t* port) {
    std::string text = typed;
    while (!text.empty() && text.back() == ' ') text.pop_back();
    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
    *port = rooms::kDefaultPort;
    const size_t colon = text.rfind(':');
    if (colon != std::string::npos) {
        if (!net_parse_port(text.substr(colon + 1), port)) return false;
        text.resize(colon);
    }
    if (text.empty() || text.size() > 253) return false;
    for (char c : text)  // (an IPv4 address or a host name: rooms are IPv4 only)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'))
            return false;
    *host = text;
    return true;
}

int net_play_with() {
    const std::string v = app_setting("Network", "PlayWith", "");
    if (v == "rooms") return PLAY_ROOMS;
    if (v == "mixed") return PLAY_MIXED;
    if (v.empty() && app_setting_bool("Network", "Crossplay", false)) return PLAY_MIXED;  // (an earlier build's choice)
    return PLAY_HUBS;
}
bool net_uses_hubs() { return net_play_with() != PLAY_ROOMS; }
bool net_uses_rooms() { return net_play_with() != PLAY_HUBS; }
bool net_crossplay() { return net_play_with() == PLAY_MIXED; }

void net_hub_apply() {
    if (!net_uses_hubs() || mh4n::hub::ParseServers(net_hub_servers_text()).empty()) {  // (Server 1 "off", Server 2 empty: none)
        if (rooms::GetHubStatus().active) net_leave_hubs();
        return;
    }
    // (hubs only, or rooms only: a room joined replaces the hubs and they are not started over one; Mixed keeps both)
    if (!rooms::GetStatus().room_session || net_crossplay()) {
        std::string error;
        net_hub_join(&error);
    }
}

void net_set_play_with(int mode) {
    mode = mode == PLAY_ROOMS ? PLAY_ROOMS : mode == PLAY_MIXED ? PLAY_MIXED : PLAY_HUBS;
    app_setting_set("Network", "PlayWith", mode == PLAY_ROOMS ? "rooms" : mode == PLAY_MIXED ? "mixed" : "hubs");
    app_setting_set("Network", "Crossplay", "0");  // (the earlier build's: no longer read once PlayWith is there)
    app_settings_save();
    const rooms::Status st = rooms::GetStatus();
    const bool hubs = rooms::GetHubStatus().active;
    if (mode == PLAY_HUBS) {
        if (st.room_session) {
            if (hubs) rooms::LeaveRoom();  // (hub servers only: the room goes, the hubs stay)
            else rooms::Leave();
        }
        if (!hubs) net_hub_apply();
    } else if (mode == PLAY_ROOMS) {
        if (hubs) net_leave_hubs();  // (rooms only: the hubs go, a room stays)
    } else if (!hubs) {
        net_hub_apply();
    }
}

namespace {
// Leaves the room; with crossplay the hub servers in use stay, as they do while a room is joined
void leave_room_only() {
    if (net_crossplay()) {
        rooms::LeaveRoom();
    } else {
        rooms::Leave();
    }
}
}  // namespace

NetProblem net_join(const std::string& address, const std::string& nickname, const std::string& password) {
    if (!rooms::ValidNickname(nickname)) return NetProblem::Nickname;
    rooms::JoinRequest request;
    if (!net_parse_address(address, &request.host, &request.port)) return NetProblem::Address;
    const rooms::Status status = rooms::GetStatus();
    bool own = request.host == "127.0.0.1" || request.host == "localhost";  // (this phone's room, at any of its addresses)
    for (const std::string& mine : net_local_addresses()) own = own || request.host == mine;
    own = own && request.port == status.hosted_port;
    if (status.hosting && !own) {  // (one room at a time)
        leave_room_only();
        rooms::CloseRoom();
    }
    request.keep_hubs = net_crossplay();
    request.nickname = nickname;
    request.password = password;
    // (no console ID: the running game's, which the runtime publishes at boot; a join before that waits for it)
    std::string error;
    if (!rooms::Join(request, &error)) {
        std::fprintf(stderr, "[Info] App: room: %s\n", error.c_str());
        return NetProblem::Address;
    }
    std::fprintf(stderr, "[Info] App: room: joining %s:%u as %s\n", request.host.c_str(), static_cast<unsigned>(request.port),
                 nickname.c_str());
    return NetProblem::None;
}

NetProblem net_host(const std::string& port_text, int slots, const std::string& nickname, const std::string& password) {
    if (!rooms::ValidNickname(nickname)) return NetProblem::Nickname;
    std::uint16_t port = 0;
    if (!net_parse_port(port_text, &port)) return NetProblem::Port;
    leave_room_only();
    rooms::CloseRoom();
    rooms::HostRequest request;
    request.port = port;
    request.name = "MH4U - " + nickname;
    request.password = password;
    request.slots = static_cast<std::uint32_t>(std::clamp(slots, 2, 16));
    request.host_username = nickname;  // (this phone joins as the room's moderator)
    std::string error;
    if (!rooms::Host(request, &error)) {
        std::fprintf(stderr, "[Info] App: room: could not host on UDP port %u: %s\n", static_cast<unsigned>(port), error.c_str());
        return NetProblem::PortBusy;
    }
    std::fprintf(stderr, "[Info] App: room: hosting on UDP port %u, %d hunters\n", static_cast<unsigned>(port), slots);
    return net_join("127.0.0.1:" + std::to_string(port), nickname, password);
}

std::vector<std::string> net_local_addresses() {
    static std::vector<std::string> cached;
    static Uint64 when = 0;
    const Uint64 now = SDL_GetTicks();
    if (when && now - when < 3000) return cached;  // (asked each frame the tab shows)
    when = now;
    cached.clear();
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return cached;
    std::vector<std::pair<int, std::string>> found;  // (Wi-Fi first, then the rest: mobile data, a hotspot)
    for (ifaddrs* a = list; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || (a->ifa_flags & IFF_LOOPBACK) || !(a->ifa_flags & IFF_UP))
            continue;
        char text[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr, text, sizeof text);
        const std::string name = a->ifa_name ? a->ifa_name : "";
        found.emplace_back(name.rfind("wlan", 0) == 0 ? 0 : name.rfind("ap", 0) == 0 || name.rfind("swlan", 0) == 0 ? 1 : 2, text);
    }
    freeifaddrs(list);
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& f : found) cached.push_back(f.second);
    return cached;
}

void net_leave() {
    if (net_crossplay() && rooms::GetStatus().room_session) {
        rooms::LeaveRoom();  // (the hub servers go on)
    } else {
        rooms::Leave();
    }
}

void net_leave_hubs() {
    if (rooms::GetStatus().room_session) {
        rooms::LeaveHubs();  // (the room stays)
    } else {
        rooms::Leave();
    }
}

void net_close_room() {
    leave_room_only();
    rooms::CloseRoom();
    std::fprintf(stderr, "[Info] App: room: closed\n");
}

bool net_say(const std::string& text) {
    if (text.empty() || !rooms::SendChat(text)) return false;
    NetLine line;  // (the room sends a chat line to everyone but its sender)
    line.kind = NetLine::Mine;
    line.nick = rooms::GetStatus().nickname;
    line.text = text.substr(0, rooms::kMaxMessageSize);
    add(line);
    return true;
}

// ---- the lobbies (Citra Web API URLs): a list in [Network] LobbyUrls, the URLs apart by spaces; empty or absent the two
// that come with the app, "off" none; up to 8. A file of the previous build has the one [Network] Lobby (empty: the default,
// "Off": none, else that one): read as the list it made.
namespace {
const char* const kDefaultLobbies = "http://88.198.47.46:5000 http://3ds.mgn.pub:3000";
}
std::vector<std::string> net_lobby_list() {
    const std::string list = app_setting("Network", "LobbyUrls", "\x01");
    std::vector<std::string> words;
    if (list != "\x01") {
        words = SplitServers(list);
        if (words.empty()) words = SplitServers(kDefaultLobbies);
        else if (IsOff(words)) words.clear();
        return words;
    }
    const std::string old = app_setting("Network", "Lobby", "");
    words = SplitServers(old);
    if (words.empty()) return SplitServers(kDefaultLobbies);
    if (IsOff(words)) return {};
    return words;
}
bool net_lobby_list_set(const std::vector<std::string>& list) {
    if (list.size() > 8) return false;
    std::string text;
    for (const std::string& one : list) text += (text.empty() ? "" : " ") + one;
    if (text.empty()) text = "off";
    else if (!strcasecmp(text.c_str(), kDefaultLobbies)) text.clear();
    app_setting_set("Network", "LobbyUrls", text);
    app_setting_set("Network", "Lobby", "");  // (the previous build's one: the list replaces it)
    return true;
}
std::string net_lobby_urls_text() {
    std::string text;
    for (const std::string& one : net_lobby_list()) text += (text.empty() ? "" : " ") + one;
    return text;
}
std::vector<std::string> net_lobby_default_list() { return SplitServers(kDefaultLobbies); }

// The rooms of every lobby in `urls` (apart by spaces), as one list: a room two lobbies list is there once
void net_lobby_fetch(const std::string& urls) {
    LobbyState& state = lobby_state();
    {
        std::lock_guard lock(state.mutex);
        if (state.lobby.busy) return;
        state.lobby.busy = true;
        state.lobby.done = false;
        state.lobby.url = urls;
    }
    std::thread([urls, &state] {
        rooms::LobbyList merged;
        for (const std::string& url : SplitServers(urls)) {
            rooms::LobbyList one = rooms::FetchLobby(url, rooms::kMh4uUsa, 6000);
            if (!one.ok) {
                if (merged.error.empty()) merged.error = one.error;
                continue;
            }
            merged.ok = true;
            for (rooms::LobbyRoom& room : one.rooms) {
                bool twice = false;
                for (const rooms::LobbyRoom& have : merged.rooms) twice |= have.address == room.address && have.port == room.port;
                if (!twice) merged.rooms.push_back(std::move(room));
            }
        }
        std::stable_sort(merged.rooms.begin(), merged.rooms.end(),
                         [](const rooms::LobbyRoom& a, const rooms::LobbyRoom& b) { return a.preferred && !b.preferred; });
        std::lock_guard lock(state.mutex);
        state.lobby.list = std::move(merged);
        state.lobby.busy = false;
        state.lobby.done = true;
    }).detach();
}

NetLobby net_lobby() {
    LobbyState& state = lobby_state();
    std::lock_guard lock(state.mutex);
    return state.lobby;
}

// ---- hub servers
namespace {
struct HubCheckState {
    std::mutex mutex;
    NetHubCheck check;
};
HubCheckState& hub_check_state() {
    static HubCheckState* state = new HubCheckState;  // (never freed: its thread may outlive the app's end)
    return *state;
}
// The hub servers, one text in [Network] HubServers (the servers apart by spaces or commas; empty: the default server,
// "off": none). A value that is not in the file is "\x01" here, to tell it from an empty one.
const char* const kAbsent = "\x01";
// The list as the file has it. A file of the previous build has two fields instead (HubServer1: empty the default, "off"
// none; HubServer2: more servers): read as the one list they made.
std::vector<std::string> HubListStored() {
    const std::string list = app_setting("Network", "HubServers", kAbsent);
    if (list != kAbsent) {
        const std::vector<std::string> words = SplitServers(list);
        if (words.empty()) return SplitServers(mh4n::hub::kDefaultServers);
        return IsOff(words) ? std::vector<std::string>() : words;
    }
    const std::string a = app_setting("Network", "HubServer1", kAbsent), b = app_setting("Network", "HubServer2", kAbsent);
    if (a == kAbsent && b == kAbsent) return SplitServers(mh4n::hub::kDefaultServers);
    std::vector<std::string> out;
    const std::vector<std::string> first = a == kAbsent ? std::vector<std::string>() : SplitServers(a);
    if (a == kAbsent || first.empty()) out = SplitServers(mh4n::hub::kDefaultServers);
    else if (!IsOff(first)) out = first;
    if (b != kAbsent)
        for (const std::string& w : SplitServers(b)) out.push_back(w);
    return out;
}

mh4n::hub::Family hub_check_family() {
    const auto choice = mh4n::hub::ParseFamilyChoice(app_setting("Network", "HubFamily", "auto"));
    if (choice == mh4n::hub::FamilyChoice::Mh4g) return mh4n::hub::Family::Mh4g;
    return mh4n::hub::Family::Mh4u;  // (auto: the game is not running here: MH4U's USA and EUR are what this app plays)
}
}  // namespace

std::vector<std::string> net_hub_server_list() { return HubListStored(); }

bool net_hub_server_list_set(const std::vector<std::string>& list) {
    if (list.size() > mh4n::hub::kMaxServers) return false;
    std::string text;
    for (const std::string& one : list) text += (text.empty() ? "" : " ") + one;
    if (text.empty()) text = "off";  // (none: not empty, which is the default)
    else if (!strcasecmp(text.c_str(), mh4n::hub::kDefaultServers)) text.clear();  // (the default stays the default)
    app_setting_set("Network", "HubServers", text);
    app_setting_remove("Network", "HubServer1");  // (the previous build's two fields: the list replaces them)
    app_setting_remove("Network", "HubServer2");
    return true;
}

std::string net_hub_servers_text() {
    std::string text;
    for (const std::string& one : HubListStored()) text += (text.empty() ? "" : " ") + one;
    return text.empty() ? "off" : text;
}

std::string net_hub_chat_scope() {
    const rooms::HubStatus hs = rooms::GetHubStatus();
    if (hs.hub_id.empty() || !(hs.hosting || hs.linked)) return "";
    const std::string text = hs.family + "/" + app_setting("Network", "HubKey", "") + "/" + hs.hub_id;
    uint64_t h = 1469598103934665603ull;  // (FNV-1a: the same on every device, as a relay's tag must be)
    for (const unsigned char c : text) h = (h ^ c) * 1099511628211ull;
    char out[17];
    snprintf(out, sizeof out, "%016llx", (unsigned long long)h);
    return out;
}

bool net_hub_join(std::string* error) {
    rooms::HubRequest request;
    request.keep_room = net_crossplay();
    request.servers = net_hub_servers_text();
    request.key = app_setting("Network", "HubKey", "");
    request.family = app_setting("Network", "HubFamily", "auto");
    if (request.family.empty()) request.family = "auto";
    if (!rooms::JoinHubs(request, error)) {
        std::fprintf(stderr, "[Info] App: hub servers: %s\n", error ? error->c_str() : "refused");
        return false;
    }
    std::fprintf(stderr, "[Info] App: hub servers: in use\n");
    return true;
}

void net_hub_check() {
    HubCheckState& state = hub_check_state();
    {
        std::lock_guard lock(state.mutex);
        if (state.check.busy) return;
        state.check.busy = true;
        state.check.done = false;
    }
    const std::vector<mh4n::hub::Endpoint> servers = mh4n::hub::ParseServers(net_hub_servers_text());
    const std::string key = app_setting("Network", "HubKey", "");
    const mh4n::hub::Family family = hub_check_family();
    std::thread([servers, key, family, &state] {
        std::vector<mh4n::hub::ServerCheck> out = mh4n::hub::CheckServers(servers, family, key, 5000);
        std::lock_guard lock(state.mutex);
        state.check.servers = std::move(out);
        state.check.busy = false;
        state.check.done = true;
    }).detach();
}

NetHubCheck net_hub_checked() {
    HubCheckState& state = hub_check_state();
    std::lock_guard lock(state.mutex);
    return state.check;
}
