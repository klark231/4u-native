// Derived from Azahar src/network/room_member.cpp, src/network/room_member.h and src/network/network.cpp at
// 5f3306d13cab40bbd5cf38f2cf3ae8b6e6eedca6 (GPL-2.0-or-later: each says "GPLv2 or any later version"): the room
// member's connection (an ENet host of one peer and one channel, the 5 s wait for the connection), the join request,
// the handling of each message the room sends, the Wi-Fi packet, chat, game information and moderation messages, and
// the goodbye (a disconnect, then up to 5 s for the room to answer it).
// Copyright 2017 Citra Emulator Project (room_member.cpp, network.cpp)
// Copyright 2017-2024 Citra Emulator Project / Azahar Emulator Project (room_member.h)
//
// Departures from Azahar:
//  - Join returns at once: the connection is made on the client's network thread (Azahar's Join waits up to 5 s for
//    it on the caller's thread). Leave returns at once too: the goodbye finishes on that thread, and each join has a
//    thread of its own, so a new Join does not wait for the last goodbye.
//  - Received messages are checked: a short or malformed message is dropped (Azahar reads past its end).
//  - The room dropping the connection while the client is still Joining ends the join with LostConnection (Azahar
//    keeps the state Joining).
//  - A Join without a console ID waits, in Joining, for the game to publish the running console's (Azahar's
//    frontends need the system to exist before they can join).
//  - Outgoing messages wait at most 5 ms for the network thread (Azahar's loop waits up to 16 ms in enet_host_service
//    before it sends what was queued).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include <enet/enet.h>

#include "../services/cfg/sha256.h"
#include "hub_medium.h"
#include "mh4n/hub.h"
#include "mh4n/log.h"
#include "mh4n/rooms.h"
#include "net_internal.h"
#include "packet.h"

namespace mh4n::rooms {

namespace {

constexpr const char* kLog = "Network";
constexpr u32 kConnectionTimeoutMs = 5000;  // ConnectionTimeoutMs
constexpr std::size_t kMaxEvents = 256;
constexpr std::size_t kNumChannels = 1;     // NumChannels

}  // namespace

using detail::Packet;

namespace detail {

bool EnsureEnet() {
    static const bool ok = [] {
        if (enet_initialize() != 0) {
            MH4N_LOG(Error, kLog, "Error initalizing ENet");
            return false;
        }
        return true;
    }();
    return ok;
}

}  // namespace detail

// One join: its thread, its ENet host and what waits to be sent. The client keeps the current one; a session that was
// left finishes its goodbye on its own thread.
struct Session {
    u64 id = 0;
    JoinRequest request;
    std::string console_id_hash;
    std::atomic<bool> stop{false};
    std::atomic<bool> done{false};  // the thread has nothing left to do: join it
    std::mutex send_mutex;
    std::list<Packet> send_list;
    std::thread thread;
};

struct Client::Impl {
    mutable std::mutex mutex;
    std::condition_variable changed;

    State state = State::Idle;
    Error error = Error::None;
    bool waiting_for_console = false;
    std::string host;
    u16 port = 0;
    std::string nickname;
    std::string username;
    MacAddress mac{};
    RoomInformation room;
    std::vector<Member> members;
    u64 revision = 0;
    std::deque<Event> events;
    GameInfo game;
    std::string published_console;

    std::shared_ptr<Session> session;              // the current join
    std::shared_ptr<Session> pending;              // a join waiting for the console's ID
    std::vector<std::shared_ptr<Session>> leaving; // sessions saying goodbye
    u64 next_session = 1;

    std::mutex listener_mutex;
    std::map<u64, WifiListener> listeners;
    std::atomic<u64> next_listener{1};

    // hub servers instead of a room (JoinHubs): the medium carries the game's frames, and there is no session
    std::unique_ptr<hub::Medium> medium;
    std::optional<HubRequest> pending_hub;  // waiting for the game's console ID
    std::string hub_family;                 // the family in use ("MH4U" / "MH4G")
    std::string hub_problem;                // why hub servers cannot be used (a sentence)
    bool hub_mode = false;                  // JoinHubs was asked: Joining, Joined, or not startable
    MacAddress hub_mac{};                   // the console's address on the hubs (a room joined with keep_hubs is asked for it)
    std::string hub_host;                   // the servers, as the status shows them while hubs are all there is

    // ---- state, under `mutex` ----
    bool Current(const Session& s) const { return session != nullptr && session->id == s.id; }
    void Touch() {
        ++revision;
        changed.notify_all();
    }
    void PushEvent(Event event) {
        if (events.size() >= kMaxEvents) {
            events.pop_front();
        }
        events.push_back(std::move(event));
    }
    void SetState(State next) {
        if (state != next) {
            state = next;
            Event event;
            event.kind = Event::Kind::StateChanged;
            event.state = next;
            PushEvent(std::move(event));
        }
        Touch();
    }
    void Fail(Error what) {
        error = what;
        Event event;
        event.kind = Event::Kind::Failed;
        event.error = what;
        PushEvent(std::move(event));
        Touch();
    }
    void ClearRoom() {
        members.clear();
        room = {};
        mac = {};
        username.clear();
    }

    // The room's session is over (it ended, failed or was left). Without hub servers that is Idle; with them (crossplay) the
    // hubs go on, and what the status shows goes back to them.
    void RoomEnded() {
        ClearRoom();
        if (medium != nullptr) {
            if (session != nullptr) {
                session->stop = true;
                leaving.push_back(std::move(session));
                session.reset();
            }
            mac = hub_mac;
            room.name = "Hub servers";
            host = hub_host;
            port = 0;
            SetState(State::Joined);
        } else {
            SetState(State::Idle);
        }
    }

    // Joins the threads of the sessions that finished their goodbye (at once: they are done).
    void ReapLeaving() {
        for (auto it = leaving.begin(); it != leaving.end();) {
            if ((*it)->done.load()) {
                if ((*it)->thread.joinable()) {
                    (*it)->thread.join();
                }
                it = leaving.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---- on the session's thread ----
    void Run(const std::shared_ptr<Session>& s);
    void Loop(const std::shared_ptr<Session>& s, ENetHost* host, ENetPeer* peer);
    void Dispatch(const std::shared_ptr<Session>& s, const ENetPacket* packet);
    void Disconnect(ENetHost* host, ENetPeer* peer);
    void Invoke(const WifiPacket& packet);

    void StartSession(std::unique_lock<std::mutex>& lock, std::shared_ptr<Session> s);
    void StartHub(const HubRequest& request, const std::string& console_id_hash);  // under `mutex`
    void Queue(const std::shared_ptr<Session>& s, Packet&& packet) {
        std::lock_guard send_lock(s->send_mutex);
        s->send_list.push_back(std::move(packet));
    }
    std::shared_ptr<Session> JoinedSession() const {
        return (state == State::Joined || state == State::Moderator) ? session : nullptr;
    }
    std::shared_ptr<Session> ConnectedSession() const {
        return (state == State::Joining || state == State::Joined || state == State::Moderator) && session != nullptr &&
                       !waiting_for_console
                   ? session
                   : nullptr;
    }
};

// ---- the session's thread ------------------------------------------------------------------------------------------

void Client::Impl::Invoke(const WifiPacket& packet) {
    std::lock_guard lock(listener_mutex);
    for (const auto& [id, listener] : listeners) {
        listener(packet);
    }
}

void Client::Impl::Disconnect(ENetHost* host, ENetPeer* peer) {
    if (peer == nullptr) {
        return;
    }
    enet_peer_disconnect(peer, 0);
    ENetEvent event;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kConnectionTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const int got = enet_host_service(host, &event, 50);
        if (got < 0) {
            break;
        }
        if (got == 0) {
            continue;
        }
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            enet_packet_destroy(event.packet);  // ignore all incoming data
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            return;
        }
    }
    // didn't disconnect gracefully: force it
    enet_peer_reset(peer);
}

void Client::Impl::Dispatch(const std::shared_ptr<Session>& s, const ENetPacket* enet_packet) {
    if (enet_packet->dataLength == 0) {
        return;
    }
    Packet packet(enet_packet->data, enet_packet->dataLength);
    u8 type = 0;
    packet >> type;
    switch (type) {
    case kIdWifiPacket: {
        WifiPacket wifi;
        u8 frame_type = 0;
        packet >> frame_type >> wifi.channel >> wifi.transmitter >> wifi.destination >> wifi.data;
        if (!packet) {
            return;
        }
        wifi.type = static_cast<WifiPacket::Type>(frame_type);
        Invoke(wifi);
        return;
    }
    case kIdChatMessage: {
        Event event;
        event.kind = Event::Kind::Chat;
        packet >> event.nickname >> event.username >> event.message;
        if (!packet) {
            return;
        }
        event.message.resize(std::min(event.message.find('\0'), event.message.size()));
        std::lock_guard lock(mutex);
        if (Current(*s)) {
            PushEvent(std::move(event));
            Touch();
        }
        return;
    }
    case kIdStatusMessage: {
        u8 status = 0;
        Event event;
        packet >> status >> event.nickname >> event.username;
        if (!packet) {
            return;
        }
        switch (status) {
        case kStatusMemberJoin: event.kind = Event::Kind::MemberJoined; break;
        case kStatusMemberLeave: event.kind = Event::Kind::MemberLeft; break;
        case kStatusMemberKicked: event.kind = Event::Kind::MemberKicked; break;
        case kStatusMemberBanned: event.kind = Event::Kind::MemberBanned; break;
        case kStatusAddressUnbanned: event.kind = Event::Kind::AddressUnbanned; break;
        default: return;
        }
        std::lock_guard lock(mutex);
        if (Current(*s)) {
            PushEvent(std::move(event));
            Touch();
        }
        return;
    }
    case kIdRoomInformation: {
        RoomInformation info;
        packet >> info.name >> info.description >> info.member_slots >> info.port >> info.preferred_game >>
            info.host_username;
        u32 count = 0;
        packet >> count;
        // each member takes at least 6 + 4 * 5 + 8 bytes
        if (!packet || count > packet.Remaining() / 34) {
            return;
        }
        std::vector<Member> list(count);
        for (Member& m : list) {
            packet >> m.nickname >> m.mac >> m.game.name >> m.game.id >> m.username >> m.display_name >> m.avatar_url;
        }
        if (!packet) {
            return;
        }
        std::lock_guard lock(mutex);
        if (!Current(*s)) {
            return;
        }
        for (const Member& m : list) {
            if (m.nickname == nickname) {
                username = m.username;
            }
        }
        room = std::move(info);
        members = std::move(list);
        Event event;
        event.kind = Event::Kind::RoomChanged;
        PushEvent(std::move(event));
        Touch();
        return;
    }
    case kIdJoinSuccess:
    case kIdJoinSuccessAsMod: {
        MacAddress assigned{};
        packet >> assigned;
        std::lock_guard lock(mutex);
        if (!Current(*s)) {
            return;
        }
        mac = assigned;
        SetState(type == kIdJoinSuccessAsMod ? State::Moderator : State::Joined);
        MH4N_LOG(Info, kLog, "joined {}:{} as {}, MAC {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", host, port, nickname,
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return;
    }
    case kIdModBanListResponse: {
        std::vector<std::string> usernames, addresses;
        packet >> usernames >> addresses;
        if (!packet) {
            return;
        }
        Event event;
        event.kind = Event::Kind::BanList;
        event.ban_list = std::move(usernames);
        event.ban_list.insert(event.ban_list.end(), addresses.begin(), addresses.end());
        std::lock_guard lock(mutex);
        if (Current(*s)) {
            PushEvent(std::move(event));
            Touch();
        }
        return;
    }
    default:
        break;
    }
    // The answers that end the join (or are a moderation request's failure).
    Error what = Error::None;
    bool ends = true;
    switch (type) {
    case kIdRoomIsFull: what = Error::RoomIsFull; break;
    case kIdNameCollision: what = Error::NameCollision; break;
    case kIdMacCollision: what = Error::MacCollision; break;
    case kIdConsoleIdCollision: what = Error::ConsoleIdCollision; break;
    case kIdVersionMismatch: what = Error::WrongVersion; break;
    case kIdWrongPassword: what = Error::WrongPassword; break;
    case kIdCloseRoom: what = Error::LostConnection; break;
    case kIdHostKicked: what = Error::HostKicked; break;
    case kIdHostBanned: what = Error::HostBanned; break;
    case kIdModPermissionDenied: what = Error::PermissionDenied; ends = false; break;
    case kIdModNoSuchUser: what = Error::NoSuchUser; ends = false; break;
    default: return;  // a message this version does not know
    }
    std::lock_guard lock(mutex);
    if (!Current(*s)) {
        return;
    }
    if (ends) {
        const std::string was_host = host;
        const u16 was_port = port;
        RoomEnded();
        MH4N_LOG(Info, kLog, "room {}:{}: {}", was_host, was_port, ErrorName(what));
    }
    Fail(what);
}

void Client::Impl::Loop(const std::shared_ptr<Session>& s, ENetHost* host, ENetPeer* peer) {
    const auto connected = [this, &s] {
        std::lock_guard lock(mutex);
        return Current(*s) && state != State::Idle;
    };
    while (!s->stop.load() && connected()) {
        ENetEvent event;
        const int got = enet_host_service(host, &event, 5);
        if (got > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                Dispatch(s, event.packet);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT: {
                std::lock_guard lock(mutex);
                if (Current(*s) && state != State::Idle) {
                    const std::string was_host = this->host;
                    const u16 was_port = port;
                    RoomEnded();
                    Fail(Error::LostConnection);
                    MH4N_LOG(Info, kLog, "room {}:{}: the connection was lost", was_host, was_port);
                }
                return;  // the peer is gone: nothing to say goodbye to
            }
            case ENET_EVENT_TYPE_NONE:
            case ENET_EVENT_TYPE_CONNECT:
                break;
            }
        }
        std::list<Packet> packets;
        {
            std::lock_guard send_lock(s->send_mutex);
            packets.swap(s->send_list);
        }
        for (const Packet& packet : packets) {
            ENetPacket* out = enet_packet_create(packet.Data(), packet.Size(), ENET_PACKET_FLAG_RELIABLE);
            if (out != nullptr && enet_peer_send(peer, 0, out) != 0) {
                enet_packet_destroy(out);
            }
        }
        if (!packets.empty()) {
            enet_host_flush(host);
        }
    }
    // Flush what was queued last (a chat line sent just before leaving), then say goodbye.
    std::list<Packet> packets;
    {
        std::lock_guard send_lock(s->send_mutex);
        packets.swap(s->send_list);
    }
    for (const Packet& packet : packets) {
        ENetPacket* out = enet_packet_create(packet.Data(), packet.Size(), ENET_PACKET_FLAG_RELIABLE);
        if (out != nullptr && enet_peer_send(peer, 0, out) != 0) {
            enet_packet_destroy(out);
        }
    }
    Disconnect(host, peer);
}

void Client::Impl::Run(const std::shared_ptr<Session>& s) {
    const auto fail = [this, &s](Error what) {
        std::lock_guard lock(mutex);
        if (Current(*s)) {
            const std::string was_host = host;
            const u16 was_port = port;
            RoomEnded();
            Fail(what);
            MH4N_LOG(Info, kLog, "could not join {}:{}: {}", was_host, was_port, ErrorName(what));
        }
    };
    if (!detail::EnsureEnet()) {
        fail(Error::UnknownError);
        return;
    }
    ENetHost* enet_host = enet_host_create(nullptr, 1, kNumChannels, 0, 0);
    if (enet_host == nullptr) {
        MH4N_LOG(Error, kLog, "Could not create client");
        fail(Error::UnknownError);
        return;
    }
    ENetAddress address{};
    if (enet_address_set_host(&address, s->request.host.c_str()) != 0) {
        enet_host_destroy(enet_host);
        fail(Error::CouldNotConnect);
        return;
    }
    address.port = s->request.port;
    ENetPeer* peer = enet_host_connect(enet_host, &address, kNumChannels, 0);
    if (peer == nullptr) {
        enet_host_destroy(enet_host);
        fail(Error::UnknownError);
        return;
    }
    bool connected = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kConnectionTimeoutMs);
    while (!s->stop.load() && std::chrono::steady_clock::now() < deadline) {
        ENetEvent event;
        const int got = enet_host_service(enet_host, &event, 50);
        if (got > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            connected = true;
            break;
        }
        if (got > 0 && event.type == ENET_EVENT_TYPE_RECEIVE) {
            enet_packet_destroy(event.packet);
        }
        if (got < 0) {
            break;
        }
    }
    if (!connected) {
        enet_peer_reset(peer);
        enet_host_destroy(enet_host);
        if (!s->stop.load()) {
            fail(Error::CouldNotConnect);
        }
        return;
    }
    {
        // SendJoinRequest, then SendGameInfo (the game being played, kept from before the join)
        Packet join;
        join << static_cast<u8>(kIdJoinRequest) << s->request.nickname << s->console_id_hash
             << s->request.preferred_mac.value_or(kNoPreferredMac) << kNetworkVersion << s->request.password
             << s->request.token;
        Queue(s, std::move(join));
        GameInfo info;
        {
            std::lock_guard lock(mutex);
            info = game;
        }
        Packet game_packet;
        game_packet << static_cast<u8>(kIdSetGameInfo) << info.name << info.id;
        Queue(s, std::move(game_packet));
    }
    Loop(s, enet_host, peer);
    enet_host_destroy(enet_host);
}

void Client::Impl::StartSession(std::unique_lock<std::mutex>& lock, std::shared_ptr<Session> s) {
    (void)lock;
    session = s;
    waiting_for_console = false;
    s->thread = std::thread([this, s] {
        Run(s);
        s->done = true;
    });
}

// ---- Client ---------------------------------------------------------------------------------------------------------

Client::Client() : impl_(new Impl) {}

// A console's MAC address on the hub medium: from its console ID, so it is the same every time (a locally administered
// unicast address: bit 1 of the first byte set, bit 0 clear).
static MacAddress HubMac(const std::string& console_id_hash) {
    const std::string text = console_id_hash + "|mh4u-hub-mac";
    const std::array<u8, 32> hash = cfg::Sha256(reinterpret_cast<const u8*>(text.data()), text.size());
    MacAddress mac{};
    std::copy_n(hash.begin(), 6, mac.begin());
    mac[0] = static_cast<u8>((mac[0] | 0x02) & 0xFE);
    return mac;
}

void Client::Impl::StartHub(const HubRequest& request, const std::string& console_id_hash) {
    hub::Family family = hub::Family::Mh4u;
    const auto choice = hub::ParseFamilyChoice(request.family).value_or(hub::FamilyChoice::Auto);
    if (hub::kCrossRegion) {
        family = hub::Family::Mh4u;  // (every region on one link protocol; the family code below waits for kCrossRegion = false)
    } else if (choice == hub::FamilyChoice::Auto) {
        const auto found = hub::FamilyOfProgram(game.id);
        if (!found) {
            hub_problem = "This game cannot use hub servers: only MH4U and MH4G can. Set the family by hand if this is MH4G.";
            MH4N_LOG(Warning, kLog, "hub servers: program {:016X} has no family", game.id);
            Fail(Error::UnknownError);
            if (session == nullptr && pending == nullptr) {
                SetState(State::Idle);
            }
            hub_mode = false;
            return;
        }
        family = *found;
    } else {
        family = choice == hub::FamilyChoice::Mh4g ? hub::Family::Mh4g : hub::Family::Mh4u;
    }
    hub::MediumOptions options;
    options.family = family;
    options.servers = hub::ParseServers(request.servers);
    options.key = request.key;
    options.mac = JoinedSession() != nullptr ? mac : HubMac(console_id_hash);  // (a room already joined: its address)
    hub_mac = options.mac;
    hub_family = hub::FamilyName(family);
    hub_problem.clear();
    medium = std::make_unique<hub::Medium>(std::move(options), [this](const WifiPacket& packet) { Invoke(packet); });
    MH4N_LOG(Info, kLog, "using hub servers as {} ({})", hub_family, hub::FormatServers(hub::ParseServers(request.servers)));
    if (session == nullptr && pending == nullptr) {  // (no room beside them: the hubs are what the state shows)
        mac = hub_mac;
        SetState(State::Joined);
    } else {
        Touch();
    }
}

bool Client::JoinHubs(const HubRequest& request, std::string* error) {
    if (hub::ParseServers(request.servers).empty()) {
        if (error != nullptr) {
            *error = "No hub server.";
        }
        return false;
    }
    if (!hub::ParseFamilyChoice(request.family)) {
        if (error != nullptr) {
            *error = "The game family must be auto, 4U or 4G.";
        }
        return false;
    }
    bool keep = false;
    {
        std::lock_guard lock(impl_->mutex);
        keep = request.keep_room && (impl_->session != nullptr || impl_->pending != nullptr);
    }
    if (keep) {
        LeaveHubs();  // (crossplay: the room stays)
    } else {
        Leave();
    }
    std::unique_lock lock(impl_->mutex);
    impl_->ReapLeaving();
    const bool with_room = impl_->session != nullptr || impl_->pending != nullptr;
    impl_->hub_mode = true;
    impl_->hub_problem.clear();
    impl_->hub_host = hub::FormatServers(hub::ParseServers(request.servers));
    if (!with_room) {
        impl_->host = impl_->hub_host;
        impl_->port = 0;
        impl_->error = Error::None;
        impl_->ClearRoom();
        impl_->room.name = "Hub servers";
        impl_->SetState(State::Joining);
    }
    std::string console = request.console_id_hash;
    if (console.empty()) {
        console = impl_->published_console;
    }
    if (console.empty()) {
        impl_->pending_hub = request;  // (the game's console ID: PublishConsole)
        impl_->waiting_for_console = true;
        impl_->Touch();
        MH4N_LOG(Info, kLog, "using hub servers when the game starts");
        return true;
    }
    impl_->StartHub(request, console);
    return true;
}

HubStatus Client::GetHubStatus() const {
    std::lock_guard lock(impl_->mutex);
    HubStatus out;
    out.problem = impl_->hub_problem;
    out.active = impl_->hub_mode && (impl_->medium != nullptr || impl_->pending_hub.has_value());
    if (impl_->medium == nullptr) {
        return out;
    }
    out.family = impl_->hub_family;
    const hub::MediumStatus st = impl_->medium->Status();
    out.hosting = st.hosting;
    out.joining = st.joining;
    out.linked = st.linked;
    out.players = st.players;
    for (const auto& sv : st.servers) {
        out.servers.push_back({sv.server.Text(), sv.listed, sv.hosting, sv.problem});
    }
    for (const auto& h : st.hubs) {
        out.hubs.push_back({h.server.Text(), h.mac, h.nodes, h.max_nodes});
    }
    if (out.problem.empty()) {
        out.problem = st.problem;
    }
    return out;
}

Client::~Client() {
    Leave();
    std::vector<std::shared_ptr<Session>> all;
    {
        std::lock_guard lock(impl_->mutex);
        all.swap(impl_->leaving);
    }
    for (const auto& s : all) {
        if (s->thread.joinable()) {
            s->thread.join();
        }
    }
    delete impl_;
}

bool Client::Join(const JoinRequest& request, std::string* error) {
    if (!ValidNickname(request.nickname)) {
        if (error != nullptr) {
            *error = "The nickname must be 4 to 20 letters, digits, spaces, '.', '_' or '-'.";
        }
        return false;
    }
    if (request.host.empty() || request.port == 0) {
        if (error != nullptr) {
            *error = "No room address.";
        }
        return false;
    }
    if (request.keep_hubs) {
        LeaveRoom();  // (crossplay: hub servers in use stay)
    } else {
        Leave();
    }
    std::unique_lock lock(impl_->mutex);
    impl_->ReapLeaving();
    auto s = std::make_shared<Session>();
    s->id = impl_->next_session++;
    s->request = request;
    if (request.keep_hubs && impl_->medium != nullptr && !request.preferred_mac) {
        s->request.preferred_mac = impl_->hub_mac;  // (one address for the game on both: the room gives it if it is free)
    }
    s->console_id_hash = request.console_id_hash;
    impl_->host = request.host;
    impl_->port = request.port;
    impl_->nickname = request.nickname;
    impl_->error = Error::None;
    impl_->ClearRoom();
    impl_->SetState(State::Joining);
    if (s->console_id_hash.empty()) {
        if (impl_->published_console.empty()) {
            // wait for the game (PublishConsole)
            impl_->pending = s;
            impl_->waiting_for_console = true;
            impl_->Touch();
            MH4N_LOG(Info, kLog, "joining {}:{} as {} when the game starts", request.host, request.port,
                     request.nickname);
            return true;
        }
        s->console_id_hash = impl_->published_console;
    }
    MH4N_LOG(Info, kLog, "joining {}:{} as {}", request.host, request.port, request.nickname);
    impl_->StartSession(lock, std::move(s));
    return true;
}

void Client::Leave() {
    std::unique_ptr<hub::Medium> medium;  // (destroyed after the lock is let go)
    std::lock_guard lock(impl_->mutex);
    medium = std::move(impl_->medium);
    impl_->pending_hub.reset();
    impl_->hub_mode = false;
    impl_->pending.reset();
    impl_->waiting_for_console = false;
    if (impl_->session != nullptr) {
        impl_->session->stop = true;
        impl_->leaving.push_back(std::move(impl_->session));
        impl_->session.reset();
    }
    if (impl_->state != State::Idle) {
        impl_->SetState(State::Idle);
        impl_->ClearRoom();
    }
    impl_->ReapLeaving();
}

void Client::LeaveRoom() {
    std::lock_guard lock(impl_->mutex);
    const bool had_room = impl_->session != nullptr || impl_->pending != nullptr;
    impl_->pending.reset();
    if (!impl_->pending_hub.has_value()) {
        impl_->waiting_for_console = false;
    }
    if (impl_->session != nullptr) {
        impl_->session->stop = true;
        impl_->leaving.push_back(std::move(impl_->session));
        impl_->session.reset();
    }
    if (had_room && impl_->state != State::Idle) {
        impl_->RoomEnded();
    }
    impl_->ReapLeaving();
}

void Client::LeaveHubs() {
    std::unique_ptr<hub::Medium> medium;  // (destroyed after the lock is let go)
    std::lock_guard lock(impl_->mutex);
    medium = std::move(impl_->medium);
    impl_->pending_hub.reset();
    impl_->hub_mode = false;
    if (impl_->pending == nullptr) {
        impl_->waiting_for_console = false;
    }
    if (impl_->session == nullptr && impl_->pending == nullptr && impl_->state != State::Idle) {
        impl_->SetState(State::Idle);  // (no room: nothing is left)
        impl_->ClearRoom();
    }
    impl_->Touch();
}

bool Client::WaitJoined(int timeout_ms) {
    std::unique_lock lock(impl_->mutex);
    impl_->changed.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                            [this] { return impl_->state != State::Joining; });
    return impl_->state == State::Joined || impl_->state == State::Moderator;
}

Status Client::GetStatus() const {
    std::lock_guard lock(impl_->mutex);
    Status status;
    status.state = impl_->state;
    status.error = impl_->error;
    status.waiting_for_console = impl_->waiting_for_console;
    status.room_session = impl_->session != nullptr || impl_->pending != nullptr;
    status.host = impl_->host;
    status.port = impl_->port;
    status.nickname = impl_->nickname;
    status.username = impl_->username;
    status.mac = impl_->mac;
    status.room = impl_->room;
    status.members = impl_->members;
    status.revision = impl_->revision;
    return status;
}

State Client::GetState() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->state;
}

bool Client::IsConnected() const {
    const State state = GetState();
    return state == State::Joining || state == State::Joined || state == State::Moderator;
}

std::vector<Event> Client::TakeEvents() {
    std::lock_guard lock(impl_->mutex);
    std::vector<Event> out(std::make_move_iterator(impl_->events.begin()),
                           std::make_move_iterator(impl_->events.end()));
    impl_->events.clear();
    return out;
}

bool Client::SendChat(const std::string& message) {
    std::lock_guard lock(impl_->mutex);
    const auto s = impl_->JoinedSession();
    if (s == nullptr) {
        return false;
    }
    Packet packet;
    packet << static_cast<u8>(kIdChatMessage) << message;
    impl_->Queue(s, std::move(packet));
    return true;
}

void Client::SetGameInfo(const GameInfo& game) {
    std::lock_guard lock(impl_->mutex);
    impl_->game = game;
    const auto s = impl_->ConnectedSession();
    if (s == nullptr) {
        return;
    }
    Packet packet;
    packet << static_cast<u8>(kIdSetGameInfo) << game.name << game.id;
    impl_->Queue(s, std::move(packet));
}

namespace {

bool Moderation(Client::Impl& impl, u8 type, const std::string& text) {
    std::lock_guard lock(impl.mutex);
    const auto s = impl.ConnectedSession();
    if (s == nullptr) {
        return false;
    }
    Packet packet;
    packet << type;
    if (type != kIdModGetBanList) {
        packet << text;
    }
    impl.Queue(s, std::move(packet));
    return true;
}

}  // namespace

bool Client::Kick(const std::string& nickname) {
    return Moderation(*impl_, kIdModKick, nickname);
}

bool Client::Ban(const std::string& nickname) {
    return Moderation(*impl_, kIdModBan, nickname);
}

bool Client::Unban(const std::string& username_or_address) {
    return Moderation(*impl_, kIdModUnban, username_or_address);
}

bool Client::RequestBanList() {
    return Moderation(*impl_, kIdModGetBanList, {});
}

void Client::PublishConsole(const std::string& console_id_hash) {
    std::unique_lock lock(impl_->mutex);
    impl_->published_console = console_id_hash;
    if (impl_->pending_hub && !console_id_hash.empty()) {
        const HubRequest request = *impl_->pending_hub;
        impl_->pending_hub.reset();
        impl_->waiting_for_console = impl_->pending != nullptr;
        impl_->StartHub(request, console_id_hash);  // (a room waiting beside the hubs, crossplay, goes on below)
        impl_->Touch();
    }
    if (impl_->pending != nullptr && !console_id_hash.empty()) {
        auto s = std::move(impl_->pending);
        impl_->pending.reset();
        impl_->waiting_for_console = false;
        s->console_id_hash = console_id_hash;
        if (s->request.keep_hubs && impl_->medium != nullptr && !s->request.preferred_mac) {
            s->request.preferred_mac = impl_->hub_mac;
        }
        MH4N_LOG(Info, kLog, "joining {}:{} as {}", s->request.host, s->request.port, s->request.nickname);
        impl_->StartSession(lock, std::move(s));
        impl_->Touch();
    }
}

void Client::WithdrawConsole() {
    std::lock_guard lock(impl_->mutex);
    impl_->published_console.clear();
}

u64 Client::AddWifiListener(WifiListener listener) {
    const u64 id = impl_->next_listener++;
    std::lock_guard lock(impl_->listener_mutex);
    impl_->listeners.emplace(id, std::move(listener));
    return id;
}

void Client::RemoveWifiListener(u64 id) {
    std::lock_guard lock(impl_->listener_mutex);
    impl_->listeners.erase(id);
}

bool Client::SendWifi(WifiPacket wifi) {
    std::lock_guard lock(impl_->mutex);
    bool sent = false;
    if (impl_->medium != nullptr) {
        impl_->medium->Send(wifi);  // (returns at once: the medium's threads do the work)
        sent = true;
    }
    // (with hub servers and a room both in use, crossplay, the game's frames go to both: a hub it hosts is in the room too)
    if (const auto s = impl_->JoinedSession(); s != nullptr) {
        wifi.transmitter = impl_->mac;
        Packet packet;
        packet << static_cast<u8>(kIdWifiPacket) << static_cast<u8>(wifi.type) << wifi.channel << wifi.transmitter
               << wifi.destination << wifi.data;
        impl_->Queue(s, std::move(packet));
        sent = true;
    }
    return sent;
}

std::optional<MacAddress> Client::RoomMac() const {
    std::lock_guard lock(impl_->mutex);
    if (impl_->state == State::Joined || impl_->state == State::Moderator) {
        return impl_->mac;
    }
    if (impl_->medium != nullptr) {
        return impl_->hub_mac;  // (a room being joined beside the hubs: the game's address does not change meanwhile)
    }
    return std::nullopt;
}

// ---- the process's client and helpers -----------------------------------------------------------------------------

Client& Default() {
    static Client client;
    return client;
}

bool Join(const JoinRequest& request, std::string* error) {
    return Default().Join(request, error);
}

void Leave() {
    Default().Leave();
}

void LeaveRoom() {
    Default().LeaveRoom();
}

void LeaveHubs() {
    Default().LeaveHubs();
}

bool WaitJoined(int timeout_ms) {
    return Default().WaitJoined(timeout_ms);
}

Status GetStatus() {
    Status status = Default().GetStatus();
    Server& server = DefaultServer();
    if (server.IsOpen()) {
        const RoomInformation info = server.Information();
        status.hosting = true;
        status.hosted_port = info.port;
        status.hosted_name = info.name;
    }
    return status;
}

std::vector<Event> TakeEvents() {
    return Default().TakeEvents();
}

bool SendChat(const std::string& message) {
    return Default().SendChat(message);
}

bool JoinHubs(const HubRequest& request, std::string* error) {
    return Default().JoinHubs(request, error);
}

HubStatus GetHubStatus() {
    return Default().GetHubStatus();
}

std::string ConsoleIdHash(u64 console_unique_id) {
    u8 bytes[8];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<u8>(console_unique_id >> (8 * i));
    }
    const std::array<u8, 32> hash = cfg::Sha256(bytes, sizeof(bytes));
    static const char* const kHex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const u8 b : hash) {
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 15]);
    }
    return out;
}

bool ValidNickname(const std::string& nickname) {
    // Azahar's regex ^[ a-zA-Z0-9._-]{4,20}$
    if (nickname.size() < 4 || nickname.size() > 20) {
        return false;
    }
    return std::all_of(nickname.begin(), nickname.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' ||
               c == '_' || c == '-';
    });
}

const char* StateName(State state) {
    switch (state) {
    case State::Idle: return "Idle";
    case State::Joining: return "Joining";
    case State::Joined: return "Joined";
    case State::Moderator: return "Moderator";
    }
    return "Unknown";
}

const char* ErrorName(Error error) {
    switch (error) {
    case Error::None: return "None";
    case Error::LostConnection: return "LostConnection";
    case Error::HostKicked: return "HostKicked";
    case Error::UnknownError: return "UnknownError";
    case Error::NameCollision: return "NameCollision";
    case Error::MacCollision: return "MacCollision";
    case Error::ConsoleIdCollision: return "ConsoleIdCollision";
    case Error::WrongVersion: return "WrongVersion";
    case Error::WrongPassword: return "WrongPassword";
    case Error::CouldNotConnect: return "CouldNotConnect";
    case Error::RoomIsFull: return "RoomIsFull";
    case Error::HostBanned: return "HostBanned";
    case Error::PermissionDenied: return "PermissionDenied";
    case Error::NoSuchUser: return "NoSuchUser";
    }
    return "Unknown";
}

const char* ErrorMessage(Error error) {
    switch (error) {
    case Error::None: return "";
    case Error::LostConnection: return "The connection to the room was lost, or the room closed.";
    case Error::HostKicked: return "A moderator removed you from the room.";
    case Error::UnknownError: return "The network could not be used.";
    case Error::NameCollision:
        return "That nickname is taken in this room, or is not valid. Choose another one.";
    case Error::MacCollision: return "Another member of the room has this console's MAC address.";
    case Error::ConsoleIdCollision:
        return "A console with the same ID is already in this room (a copy of the same save folder?). Give this "
               "console a new ID.";
    case Error::WrongVersion: return "The room runs another version of the multiplayer protocol.";
    case Error::WrongPassword: return "Wrong password for this room.";
    case Error::CouldNotConnect:
        return "The room does not answer. Check the address and port, and that the host's UDP port is open.";
    case Error::RoomIsFull: return "The room is full.";
    case Error::HostBanned: return "You are banned from this room.";
    case Error::PermissionDenied: return "Only a moderator can do that.";
    case Error::NoSuchUser: return "There is no member of that name in the room.";
    }
    return "";
}

}  // namespace mh4n::rooms
