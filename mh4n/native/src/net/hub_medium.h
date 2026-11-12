// The hub servers as a wireless medium: what the room client runs instead of a room's connection when the app asks for
// hub servers (rooms::Client::JoinHubs). Internal to native/src/net; the app talks to rooms.h.
//
// The UDS service hands the medium every frame it would send into a room and takes the frames it delivers as the
// room's. The medium makes a room's idea of a medium out of what a hub server offers:
//  - scanning: the hubs the servers list are delivered as the beacons of the consoles that host them, so the game's
//    search finds them as it finds a room member's Gathering Hall;
//  - hosting: the first beacon the game sends lists a hub (on every server) with that beacon as its description; a
//    player who joins it arrives as a link, and the host relays the frames among its links the way a room relays them
//    among its members;
//  - joining: the authentication frame the game sends to a listed host's MAC address opens the player's link to that
//    hub (through the server), and every frame after that goes down it.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mh4n/hub.h"
#include "mh4n/rooms.h"

namespace mh4n::hub {

struct MediumOptions {
    Family family = Family::Mh4u;
    std::vector<Endpoint> servers;
    std::string key;            ///< the matchmaking key (never logged, never sent: its hash is)
    rooms::MacAddress mac{};    ///< this console's address on the medium
    int list_every_ms = 3000;   ///< between two searches of the servers
    int beacon_every_ms = 300;  ///< between two deliveries of the listed hubs to the game
    int connect_timeout_ms = 5000;
    int handshake_timeout_ms = 10000;
    int max_players = 4;        ///< the most links a hosted hub takes (at most 7)
};

struct ServerState {
    Endpoint server;
    bool listed = false;    ///< the last search was answered
    bool hosting = false;   ///< the hub is listed on it
    std::string problem;    ///< what went wrong last, for the player
};

struct HubView {
    Endpoint server;
    u64 id = 0;
    rooms::MacAddress mac{};
    u8 nodes = 0;
    u8 max_nodes = 0;
    u32 comm_id = 0;
};

struct MediumStatus {
    bool hosting = false;
    bool joining = false;
    bool linked = false;   ///< this console is a player of a hub
    int players = 0;       ///< the links of a hosted hub
    std::vector<HubView> hubs;
    std::vector<ServerState> servers;
    std::string problem;   ///< the last thing that went wrong with a hub (a refusal, a lost link)
    u64 revision = 0;
};

class Medium {
public:
    using Deliver = std::function<void(const rooms::WifiPacket&)>;

    /// Starts the threads at once. `deliver` runs on the medium's threads.
    Medium(MediumOptions options, Deliver deliver);
    /// Stops everything. Returns at once; the threads end within a fraction of a second and touch nothing of the caller's
    /// after this returns.
    ~Medium();
    Medium(const Medium&) = delete;
    Medium& operator=(const Medium&) = delete;

    /// A frame of the UDS service (the transmitter is set to this console's MAC address).
    void Send(const rooms::WifiPacket& packet);
    rooms::MacAddress Mac() const;
    MediumStatus Status() const;

    struct Core;

private:
    std::shared_ptr<Core> core_;
};

}  // namespace mh4n::hub
