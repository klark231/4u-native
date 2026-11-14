// Hub servers: the local wireless play of MH4U (and MH4G) through an MHGURecomp hub server (gu-native's `server/`).
//
// A hub server is a lobby and a relay: a player who hosts a hub keeps one TCP connection to it, the server lists the hub
// for everyone who asks with the same *link protocol* number and the same *matchmaking key*, and a player who joins gets
// a TCP connection that the server splices to one the host opens for them. What passes through the splice is the two
// games' own link, which the server neither reads nor changes. The server needs no change to carry this port: it keeps
// hubs of different link protocols apart, and this port uses numbers of its own (kLinkMh4u, kLinkMh4g) that the
// MHGU and MHXX players' (2 and 3) never meet. That is how "only MH4U and MH4G players see these hubs" is made.
//
// The server's side of the protocol (control frames, "MHGS", LIST / HOST / JOIN / ATTACH) is the one in gu-native's
// server/server.go; the link that follows a splice is this port's (docs/hub-link.md): the local-wireless frames of the
// two consoles (the same WifiPacket the network rooms carry), so the game's UDS service does not know whether its
// frames travel through a room or a hub.
//
// Everything here is plain data and pure functions, so the tests can drive it without a socket. The sockets and threads
// are mh4n::hub::Medium (src/net/hub_medium.h), which the room client runs when the app asks for hub servers.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mh4n/rooms.h"
#include "mh4n/types.h"

namespace mh4n::hub {

// ---- the server's protocol ----------------------------------------------------------------------------------------

constexpr u16 kServerProtocol = 1;  ///< "MHGS" server protocol (server.go's serverProto)
constexpr u16 kDefaultPort = 39510;
constexpr std::size_t kHeaderSize = 24;     ///< "MHGS", u16 server protocol, u16 link protocol, room[16]
constexpr std::size_t kNetInfoSize = 0x480; ///< the NetworkInfo a hub is listed with
constexpr std::size_t kMaxFrame = 4096;     ///< the server's control frame limit (maxFrame)
constexpr std::size_t kMaxServers = 8;
/// The server used when none is typed (the app's first field; the second is blank). A server is not tied to a game: it
/// lists hubs by link protocol. Typed with or without "http://".
constexpr const char* kDefaultServers = "http://meoli.viewdns.net";

enum ControlType : u8 {
    kList = 1,
    kHost = 2,
    kAttach = 3,
    kJoin = 4,
    kUpdate = 5,
    kPing = 6,
    kHubs = 16,
    kHosted = 17,
    kIncoming = 18,
    kSpliced = 19,
    kError = 20,
};

enum ServerError : u32 {
    kErrNotFound = 1,  ///< no such hub (or not for this key / link protocol)
    kErrNoAnswer = 2,  ///< the host did not take the player
    kErrBusy = 3,      ///< a limit of the server was reached
    kErrVersion = 4,   ///< the server speaks another protocol
    kErrBadRequest = 5,
};

const char* ServerErrorText(u32 code);

// ---- which hubs a game sees -------------------------------------------------------------------------------------

/// The games that may use hubs, and the link protocol each lists its hubs under. MH4U's USA and EUR copies play with each
/// other, MH4G's JPN and KOR copies play with each other: those are the two families, and a hub of one is never listed
/// for the other (nor for MHGU, whose link protocols are 2 and 3).
enum class Family : u8 { Mh4u = 1, Mh4g = 2 };

/// Cross-region play: every copy (USA, EUR, JPN, KOR) lists and finds its hubs under the one link protocol kLinkMh4u, so
/// they all see each other. The two families above, their link protocols and the program id table are kept for the day
/// the regions have to be told apart again: set this to false (and show the app's "Game" row) to bring that back.
constexpr bool kCrossRegion = true;

constexpr u16 kLinkMh4u = 0x4D01;  ///< 'M', 1
constexpr u16 kLinkMh4g = 0x4D02;  ///< 'M', 2

const char* FamilyName(Family family);
u16 LinkProtocol(Family family);
/// "4u" / "4g" / "auto" (case does not matter); nullopt for anything else. "auto" is `Auto`.
enum class FamilyChoice : u8 { Auto, Mh4u, Mh4g };
std::optional<FamilyChoice> ParseFamilyChoice(std::string_view text);
/// The family of a game's program id: the USA and EUR copies of MH4U, and the copies of MH4G that are known. Anything
/// else (another game's local play) is nullopt: such a game never uses hubs.
std::optional<Family> FamilyOfProgram(u64 program_id);

// ---- servers and the matchmaking key ---------------------------------------------------------------------------

struct Endpoint {
    std::string host;
    u16 port = kDefaultPort;
    std::string Text() const;  ///< "host" or "host:port" (the default port is left out)
    bool operator==(const Endpoint& other) const { return host == other.host && port == other.port; }
};

/// "name", "name:port", "1.2.3.4" or "1.2.3.4:port"; a leading "http://" or "https://" and anything after a "/" are
/// ignored. False for anything else (an IPv6 literal included: hubs are IPv4).
bool ParseEndpoint(std::string_view text, Endpoint* out);
/// Servers separated by spaces, commas, semicolons or line breaks; a bad one is skipped, a double is kept once, at most
/// kMaxServers. An empty text is the default servers; "off" (alone) is none.
std::vector<Endpoint> ParseServers(std::string_view text);
std::string FormatServers(const std::vector<Endpoint>& servers);

/// room[16] of the header: the first 16 bytes of SHA-256 of the key; all zero without a key.
std::array<u8, 16> RoomOfKey(std::string_view key);

/// The request header of a control frame's payload.
std::vector<u8> MakeHeader(u16 link_protocol, const std::array<u8, 16>& room);

// ---- control frames --------------------------------------------------------------------------------------------

/// u32 size (of what follows), u8 type, payload; little endian.
std::vector<u8> MakeFrame(u8 type, const std::vector<u8>& payload);
/// Takes one frame off the front of `buffer` if it is whole: false while it is not. `*bad` is set (and false returned)
/// for a size the server would never send, which ends the connection.
bool TakeFrame(std::vector<u8>& buffer, std::size_t max_payload, u8* type, std::vector<u8>* payload, bool* bad);

struct ListedHub {
    u64 id = 0;
    std::array<u8, kNetInfoSize> info{};
};
/// The payload of HUBS: u32 count, count x (u64 hub id, NetworkInfo). Entries that do not fit are ignored.
std::vector<ListedHub> ParseHubs(const std::vector<u8>& payload);

// ---- a hub's NetworkInfo ---------------------------------------------------------------------------------------
//
// The server shows a hub's NetworkInfo (0x480 bytes) to every player who lists, and looks at five bytes of it only: the
// SSID length (0x26, at most 32), the network type (0x4B, 2), the node limit and count (0x66 and 0x67: 1 to 8, count at
// least 1 and at most the limit) and the advertise size (0x26A, at most 0x180). The rest is the game's: here it holds
// the beacon that the host's console sends, so a player's console sees the hub as the UDS network it is.
//
//   0x00 "M4HB"   0x04 u16 layout (1)   0x06 u16 link protocol   0x08 u16 beacon length   0x0A u8 channel
//   0x0C u8[6] the host console's MAC   0x12 u32 wlan comm id (big endian, as in the beacon)
//   0x26 0   0x4B 2   0x66 nodes at most   0x67 nodes now   0x26A u16 0
//   0x68..0x26A, then 0x26C..0x478: the beacon, in that order (514 + 524 bytes)

constexpr std::size_t kBeaconMax = 514 + 524;

struct HubBeacon {
    std::array<u8, 6> mac{};   ///< the host console's MAC: the beacon's transmitter
    u8 channel = 0;
    u32 comm_id = 0;           ///< the game's wlan comm id (what a scan asks for)
    u8 max_nodes = 0;
    u8 nodes = 0;              ///< the players now, the host included
    std::vector<u8> beacon;    ///< the frame body of a beacon (what the UDS service of the host sent)
};

/// What a beacon's network tag says (comm id, node count and limit); false for a frame that is not a UDS beacon.
bool SummarizeBeacon(const std::vector<u8>& beacon, u32* comm_id, u8* nodes, u8* max_nodes);
/// False when the beacon is too big for the field or its node counts are not what the server accepts.
bool PackInfo(const HubBeacon& hub, u16 link_protocol, std::array<u8, kNetInfoSize>* info);
std::optional<HubBeacon> UnpackInfo(const std::array<u8, kNetInfoSize>& info, u16 link_protocol);

// ---- the link after a splice -----------------------------------------------------------------------------------
//
// Frames of u16 size (of what follows), u8 kind, payload; little endian. Both ends send Hello first. The host answers a
// joining player's Hello with its own and then the frames flow; a refusal is a Reject and the host closes the link.

enum LinkKind : u8 { kLinkHello = 1, kLinkWifi = 2, kLinkPing = 3, kLinkReject = 4, kLinkBye = 5 };
enum RejectReason : u8 { kRejectFamily = 1, kRejectMac = 2, kRejectFull = 3, kRejectVersion = 4, kRejectNotHosting = 5 };
constexpr u16 kLinkVersion = 1;
constexpr std::size_t kLinkMaxFrame = 4096;

struct Hello {
    u16 version = kLinkVersion;
    u16 link_protocol = 0;
    std::array<u8, 6> mac{};
};

std::vector<u8> MakeLinkFrame(u8 kind, const std::vector<u8>& payload);
bool TakeLinkFrame(std::vector<u8>& buffer, u8* kind, std::vector<u8>* payload, bool* bad);
std::vector<u8> EncodeHello(const Hello& hello);
std::optional<Hello> DecodeHello(const std::vector<u8>& payload);

/// A console's frame on the link: u8 type, u8 channel, transmitter[6], destination[6], then the frame body.
std::vector<u8> EncodeWifi(const rooms::WifiPacket& packet);
std::optional<rooms::WifiPacket> DecodeWifi(const std::vector<u8>& payload);

// ---- checking a server ------------------------------------------------------------------------------------------

struct ServerCheck {
    Endpoint server;
    bool ok = false;
    int hubs = 0;      ///< the hubs it lists for this family and key
    int ms = 0;        ///< how long the answer took
    std::string detail;
};

/// Asks each server for its list, one after the other (the "Check hub server" button). Blocks: call it on a worker
/// thread. A hub of another game is not counted.
std::vector<ServerCheck> CheckServers(const std::vector<Endpoint>& servers, Family family, const std::string& key,
                                      int timeout_ms = 5000);

}  // namespace mh4n::hub
