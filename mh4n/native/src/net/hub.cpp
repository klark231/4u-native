// The pure parts of the hub support (mh4n/hub.h): families, server lists, the server's frames, a hub's NetworkInfo and
// the link frames. No sockets here.
#include "mh4n/hub.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "../services/cfg/sha256.h"

namespace mh4n::hub {

namespace {

void PutLe16(std::vector<u8>& out, u16 v) {
    out.push_back(static_cast<u8>(v));
    out.push_back(static_cast<u8>(v >> 8));
}
void PutLe32(std::vector<u8>& out, u32 v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<u8>(v >> (8 * i)));
    }
}
u16 GetLe16(const u8* p) {
    return static_cast<u16>(p[0] | (p[1] << 8));
}
u32 GetLe32(const u8* p) {
    return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) |
           (static_cast<u32>(p[3]) << 24);
}
u64 GetLe64(const u8* p) {
    return static_cast<u64>(GetLe32(p)) | (static_cast<u64>(GetLe32(p + 4)) << 32);
}

bool IEquals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// NetworkInfo offsets (the server's checks and this port's fields)
constexpr std::size_t kNiMagic = 0x00;
constexpr std::size_t kNiLayout = 0x04;
constexpr std::size_t kNiLink = 0x06;
constexpr std::size_t kNiBeaconLen = 0x08;
constexpr std::size_t kNiChannel = 0x0A;
constexpr std::size_t kNiMac = 0x0C;
constexpr std::size_t kNiCommId = 0x12;
constexpr std::size_t kNiSsidLen = 0x26;
constexpr std::size_t kNiNetType = 0x4B;
constexpr std::size_t kNiNodeMax = 0x66;
constexpr std::size_t kNiNodeCount = 0x67;
constexpr std::size_t kNiBeaconA = 0x68;
constexpr std::size_t kNiBeaconALen = 0x26A - 0x68;  // 514
constexpr std::size_t kNiAdvSize = 0x26A;
constexpr std::size_t kNiBeaconB = 0x26C;
constexpr std::size_t kNiBeaconBLen = 0x478 - 0x26C;  // 524

}  // namespace

// ---- the server's errors ---------------------------------------------------------------------------------------

const char* ServerErrorText(u32 code) {
    switch (code) {
    case kErrNotFound: return "that hub is gone, or it is not for your matchmaking key";
    case kErrNoAnswer: return "the host did not answer";
    case kErrBusy: return "the hub server is busy or its limits were reached";
    case kErrVersion: return "the hub server speaks another protocol";
    case kErrBadRequest: return "the hub server did not accept the request";
    default: return "the hub server answered with an error";
    }
}

// ---- families ----------------------------------------------------------------------------------------------------

const char* FamilyName(Family family) {
    return family == Family::Mh4g ? "MH4G" : "MH4U";
}

u16 LinkProtocol(Family family) {
    if (kCrossRegion) {
        return kLinkMh4u;  // (all regions together: see kCrossRegion)
    }
    return family == Family::Mh4g ? kLinkMh4g : kLinkMh4u;
}

std::optional<FamilyChoice> ParseFamilyChoice(std::string_view text) {
    if (text.empty() || IEquals(text, "auto")) {
        return FamilyChoice::Auto;
    }
    if (IEquals(text, "4u") || IEquals(text, "mh4u")) {
        return FamilyChoice::Mh4u;
    }
    if (IEquals(text, "4g") || IEquals(text, "mh4g")) {
        return FamilyChoice::Mh4g;
    }
    return std::nullopt;
}

std::optional<Family> FamilyOfProgram(u64 program_id) {
    switch (program_id) {
    case rooms::kMh4uUsa:     // 0004000000126300
    case rooms::kMh4uEurope:  // 0004000000126100
        return Family::Mh4u;
    case 0x0004000000155400ULL:  // MH4G, the Japanese copy
        return Family::Mh4g;
    // MH4G's Korean copy: its program id is not in this table yet. Until it is, a player of that copy sets the family
    // to 4G by hand ([Network] HubFamily = 4G in mh4u-app.ini).
    default:
        return std::nullopt;
    }
}

// ---- servers ---------------------------------------------------------------------------------------------------

std::string Endpoint::Text() const {
    return port == kDefaultPort ? host : host + ":" + std::to_string(port);
}

bool ParseEndpoint(std::string_view text, Endpoint* out) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    // (an address as it is pasted from a browser: http://name, https://name:port/)
    for (const std::string_view scheme : {std::string_view("http://"), std::string_view("https://")}) {
        if (text.size() >= scheme.size() && IEquals(text.substr(0, scheme.size()), scheme)) {
            text.remove_prefix(scheme.size());
            break;
        }
    }
    if (const std::size_t slash = text.find('/'); slash != std::string_view::npos) {
        text = text.substr(0, slash);
    }
    Endpoint e;
    const std::size_t colon = text.rfind(':');
    if (colon != std::string_view::npos) {
        const std::string_view digits = text.substr(colon + 1);
        if (digits.empty() || digits.size() > 5 ||
            !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            return false;
        }
        const long n = std::strtol(std::string(digits).c_str(), nullptr, 10);
        if (n < 1 || n > 65535) {
            return false;
        }
        e.port = static_cast<u16>(n);
        text = text.substr(0, colon);
    }
    if (text.empty() || text.size() > 253) {
        return false;
    }
    for (const char c : text) {  // (an IPv4 address or a host name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return false;
        }
    }
    e.host = std::string(text);
    *out = std::move(e);
    return true;
}

std::vector<Endpoint> ParseServers(std::string_view text) {
    std::vector<Endpoint> out;
    const auto separator = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';';
    };
    // an empty text is the defaults; "off" alone is none
    bool blank = true;
    for (const char c : text) {
        blank = blank && separator(c);
    }
    if (blank) {
        return ParseServers(kDefaultServers);
    }
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && separator(text[i])) {
            ++i;
        }
        const std::size_t start = i;
        while (i < text.size() && !separator(text[i])) {
            ++i;
        }
        const std::string_view word = text.substr(start, i - start);
        if (word.empty() || IEquals(word, "off")) {
            continue;
        }
        Endpoint e;
        if (ParseEndpoint(word, &e) && std::find(out.begin(), out.end(), e) == out.end() && out.size() < kMaxServers) {
            out.push_back(std::move(e));
        }
    }
    return out;
}

std::string FormatServers(const std::vector<Endpoint>& servers) {
    std::string out;
    for (const Endpoint& e : servers) {
        out += (out.empty() ? "" : " ") + e.Text();
    }
    return out;
}

std::array<u8, 16> RoomOfKey(std::string_view key) {
    std::array<u8, 16> room{};
    if (key.empty()) {
        return room;
    }
    const std::array<u8, 32> hash = cfg::Sha256(reinterpret_cast<const u8*>(key.data()), key.size());
    std::copy_n(hash.begin(), 16, room.begin());
    return room;
}

std::vector<u8> MakeHeader(u16 link_protocol, const std::array<u8, 16>& room) {
    std::vector<u8> h = {'M', 'H', 'G', 'S'};
    PutLe16(h, kServerProtocol);
    PutLe16(h, link_protocol);
    h.insert(h.end(), room.begin(), room.end());
    return h;
}

// ---- control frames ------------------------------------------------------------------------------------------------

std::vector<u8> MakeFrame(u8 type, const std::vector<u8>& payload) {
    std::vector<u8> out;
    out.reserve(5 + payload.size());
    PutLe32(out, static_cast<u32>(1 + payload.size()));
    out.push_back(type);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool TakeFrame(std::vector<u8>& buffer, std::size_t max_payload, u8* type, std::vector<u8>* payload, bool* bad) {
    if (bad != nullptr) {
        *bad = false;
    }
    if (buffer.size() < 4) {
        return false;
    }
    const u32 size = GetLe32(buffer.data());
    if (size < 1 || size > max_payload + 1) {
        if (bad != nullptr) {
            *bad = true;
        }
        return false;
    }
    if (buffer.size() < 4 + static_cast<std::size_t>(size)) {
        return false;
    }
    *type = buffer[4];
    payload->assign(buffer.begin() + 5, buffer.begin() + 4 + static_cast<std::ptrdiff_t>(size));
    buffer.erase(buffer.begin(), buffer.begin() + 4 + static_cast<std::ptrdiff_t>(size));
    return true;
}

std::vector<ListedHub> ParseHubs(const std::vector<u8>& payload) {
    std::vector<ListedHub> out;
    if (payload.size() < 4) {
        return out;
    }
    const u32 count = GetLe32(payload.data());
    constexpr std::size_t kEntry = 8 + kNetInfoSize;
    for (u32 i = 0; i < count; ++i) {
        const std::size_t at = 4 + static_cast<std::size_t>(i) * kEntry;
        if (at + kEntry > payload.size()) {
            break;
        }
        ListedHub hub;
        hub.id = GetLe64(payload.data() + at);
        std::memcpy(hub.info.data(), payload.data() + at + 8, kNetInfoSize);
        out.push_back(hub);
    }
    return out;
}

// ---- a hub's NetworkInfo ----------------------------------------------------------------------------------------

bool SummarizeBeacon(const std::vector<u8>& beacon, u32* comm_id, u8* nodes, u8* max_nodes) {
    // fixed parameters (12 bytes), then tags: [id, length, body]. The network tag is the vendor tag (221) of the Great N's
    // OUI whose type is not the dummy (20) or encrypted data (24, 25); its body starts at the network structure's OUI
    // (0xC): the comm id is at +4 (big endian), the node count at +16 and the node limit at +17.
    std::size_t at = 12;
    while (at + 2 <= beacon.size()) {
        const u8 id = beacon[at];
        const std::size_t len = beacon[at + 1];
        if (at + 2 + len > beacon.size()) {
            return false;
        }
        const u8* body = beacon.data() + at + 2;
        if (id == 221 && len >= 18 && body[0] == 0x00 && body[1] == 0x1F && body[2] == 0x32 && body[3] != 20 &&
            body[3] != 24 && body[3] != 25) {
            *comm_id = (static_cast<u32>(body[4]) << 24) | (static_cast<u32>(body[5]) << 16) |
                       (static_cast<u32>(body[6]) << 8) | body[7];
            *nodes = body[16];
            *max_nodes = body[17];
            return true;
        }
        at += 2 + len;
    }
    return false;
}

bool PackInfo(const HubBeacon& hub, u16 link_protocol, std::array<u8, kNetInfoSize>* info) {
    if (hub.beacon.empty() || hub.beacon.size() > kBeaconMax || hub.max_nodes < 1 || hub.max_nodes > 8 ||
        hub.nodes < 1 || hub.nodes > hub.max_nodes) {
        return false;
    }
    info->fill(0);
    u8* p = info->data();
    std::memcpy(p + kNiMagic, "M4HB", 4);
    p[kNiLayout] = 1;
    p[kNiLink] = static_cast<u8>(link_protocol);
    p[kNiLink + 1] = static_cast<u8>(link_protocol >> 8);
    p[kNiBeaconLen] = static_cast<u8>(hub.beacon.size());
    p[kNiBeaconLen + 1] = static_cast<u8>(hub.beacon.size() >> 8);
    p[kNiChannel] = hub.channel;
    std::memcpy(p + kNiMac, hub.mac.data(), 6);
    p[kNiCommId] = static_cast<u8>(hub.comm_id >> 24);
    p[kNiCommId + 1] = static_cast<u8>(hub.comm_id >> 16);
    p[kNiCommId + 2] = static_cast<u8>(hub.comm_id >> 8);
    p[kNiCommId + 3] = static_cast<u8>(hub.comm_id);
    p[kNiSsidLen] = 0;
    p[kNiNetType] = 2;
    p[kNiNodeMax] = hub.max_nodes;
    p[kNiNodeCount] = hub.nodes;
    p[kNiAdvSize] = p[kNiAdvSize + 1] = 0;
    const std::size_t first = std::min(hub.beacon.size(), kNiBeaconALen);
    std::memcpy(p + kNiBeaconA, hub.beacon.data(), first);
    if (hub.beacon.size() > first) {
        std::memcpy(p + kNiBeaconB, hub.beacon.data() + first, hub.beacon.size() - first);
    }
    return true;
}

std::optional<HubBeacon> UnpackInfo(const std::array<u8, kNetInfoSize>& info, u16 link_protocol) {
    const u8* p = info.data();
    if (std::memcmp(p + kNiMagic, "M4HB", 4) != 0 || p[kNiLayout] != 1 || GetLe16(p + kNiLink) != link_protocol) {
        return std::nullopt;  // another game's hub, or another layout
    }
    HubBeacon hub;
    const std::size_t len = GetLe16(p + kNiBeaconLen);
    if (len == 0 || len > kBeaconMax) {
        return std::nullopt;
    }
    hub.channel = p[kNiChannel];
    std::memcpy(hub.mac.data(), p + kNiMac, 6);
    hub.comm_id = (static_cast<u32>(p[kNiCommId]) << 24) | (static_cast<u32>(p[kNiCommId + 1]) << 16) |
                  (static_cast<u32>(p[kNiCommId + 2]) << 8) | p[kNiCommId + 3];
    hub.max_nodes = p[kNiNodeMax];
    hub.nodes = p[kNiNodeCount];
    hub.beacon.resize(len);
    const std::size_t first = std::min(len, kNiBeaconALen);
    std::memcpy(hub.beacon.data(), p + kNiBeaconA, first);
    if (len > first) {
        std::memcpy(hub.beacon.data() + first, p + kNiBeaconB, len - first);
    }
    return hub;
}

// ---- the link ------------------------------------------------------------------------------------------------------

std::vector<u8> MakeLinkFrame(u8 kind, const std::vector<u8>& payload) {
    std::vector<u8> out;
    out.reserve(3 + payload.size());
    PutLe16(out, static_cast<u16>(1 + payload.size()));
    out.push_back(kind);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool TakeLinkFrame(std::vector<u8>& buffer, u8* kind, std::vector<u8>* payload, bool* bad) {
    if (bad != nullptr) {
        *bad = false;
    }
    if (buffer.size() < 2) {
        return false;
    }
    const std::size_t size = GetLe16(buffer.data());
    if (size < 1 || size > kLinkMaxFrame + 1) {
        if (bad != nullptr) {
            *bad = true;
        }
        return false;
    }
    if (buffer.size() < 2 + size) {
        return false;
    }
    *kind = buffer[2];
    payload->assign(buffer.begin() + 3, buffer.begin() + 2 + static_cast<std::ptrdiff_t>(size));
    buffer.erase(buffer.begin(), buffer.begin() + 2 + static_cast<std::ptrdiff_t>(size));
    return true;
}

std::vector<u8> EncodeHello(const Hello& hello) {
    std::vector<u8> out = {'M', '4', 'H', 'L'};
    PutLe16(out, hello.version);
    PutLe16(out, hello.link_protocol);
    out.insert(out.end(), hello.mac.begin(), hello.mac.end());
    return out;
}

std::optional<Hello> DecodeHello(const std::vector<u8>& payload) {
    if (payload.size() < 14 || std::memcmp(payload.data(), "M4HL", 4) != 0) {
        return std::nullopt;
    }
    Hello h;
    h.version = GetLe16(payload.data() + 4);
    h.link_protocol = GetLe16(payload.data() + 6);
    std::memcpy(h.mac.data(), payload.data() + 8, 6);
    return h;
}

std::vector<u8> EncodeWifi(const rooms::WifiPacket& packet) {
    std::vector<u8> out;
    out.reserve(14 + packet.data.size());
    out.push_back(static_cast<u8>(packet.type));
    out.push_back(packet.channel);
    out.insert(out.end(), packet.transmitter.begin(), packet.transmitter.end());
    out.insert(out.end(), packet.destination.begin(), packet.destination.end());
    out.insert(out.end(), packet.data.begin(), packet.data.end());
    return out;
}

std::optional<rooms::WifiPacket> DecodeWifi(const std::vector<u8>& payload) {
    if (payload.size() < 14) {
        return std::nullopt;
    }
    rooms::WifiPacket p;
    p.type = static_cast<rooms::WifiPacket::Type>(payload[0]);
    p.channel = payload[1];
    std::memcpy(p.transmitter.data(), payload.data() + 2, 6);
    std::memcpy(p.destination.data(), payload.data() + 8, 6);
    p.data.assign(payload.begin() + 14, payload.end());
    return p;
}

}  // namespace mh4n::hub
