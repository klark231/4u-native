// Tests of the hub support (mh4n/hub.h, native/src/net/hub_medium.cpp and rooms::Client::JoinHubs).
//
// The pure parts are checked directly. The rest runs two or three room clients in one process through `TestServer`, a
// port of the hub server's LIST / HOST / JOIN / ATTACH and splice (gu-native's server/server.go: the same frames, the
// same checks of a NetworkInfo, the same keeping apart of hubs by link protocol and matchmaking key). It is a port,
// not the server: a run against the real server is a separate check (see docs/hub-link.md). Everything is synthetic.
#include <doctest/doctest.h>

#ifndef _WIN32

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "../src/net/hub_medium.h"
#include "../src/services/nwm/uds_frames.h"
#include "mh4n/hub.h"
#include "mh4n/rooms.h"

namespace {

using namespace mh4n;
using namespace std::chrono_literals;

template <class Pred>
bool WaitFor(Pred pred, int ms = 8000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return pred();
}

// ---- the server's port -----------------------------------------------------------------------------------------

class TestServer {
public:
    TestServer() {
        listen_ = socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(listen_, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0);
        listen(listen_, 32);
        socklen_t len = sizeof a;
        getsockname(listen_, reinterpret_cast<sockaddr*>(&a), &len);
        port_ = ntohs(a.sin_port);
        accept_thread_ = std::thread([this] { Accept(); });
    }
    ~TestServer() {
        stop_ = true;
        shutdown(listen_, SHUT_RDWR);
        close(listen_);
        accept_thread_.join();
        std::vector<std::thread> ts;
        {
            std::lock_guard lock(mu_);
            for (int fd : fds_) {
                shutdown(fd, SHUT_RDWR);
            }
            ts.swap(handlers_);
        }
        for (auto& t : ts) {
            t.join();
        }
    }
    u16 port() const { return port_; }
    int HubCount() {
        std::lock_guard lock(mu_);
        return static_cast<int>(hubs_.size());
    }

private:
    struct Hub {
        u64 id;
        u16 proto;
        std::array<u8, 16> room;
        std::array<u8, hub::kNetInfoSize> info;
        int fd;
    };
    struct Pending {
        u64 hub;
        std::mutex m;
        std::condition_variable cv;
        int host_fd = -1;
        int player_fd = -1;
    };

    void Accept() {
        while (!stop_) {
            const int fd = accept(listen_, nullptr, nullptr);
            if (fd < 0) {
                return;
            }
            std::lock_guard lock(mu_);
            fds_.push_back(fd);
            handlers_.emplace_back([this, fd] { Handle(fd); });
        }
    }

    static bool ReadN(int fd, u8* p, std::size_t n) {
        while (n > 0) {
            const auto r = recv(fd, p, n, 0);
            if (r <= 0) {
                return false;
            }
            p += r;
            n -= static_cast<std::size_t>(r);
        }
        return true;
    }
    static bool ReadFrame(int fd, u8* type, std::vector<u8>* payload) {
        u8 h[4];
        if (!ReadN(fd, h, 4)) {
            return false;
        }
        const u32 size = h[0] | (h[1] << 8) | (h[2] << 16) | (static_cast<u32>(h[3]) << 24);
        if (size < 1 || size > hub::kMaxFrame + 1) {
            return false;
        }
        std::vector<u8> p(size);
        if (!ReadN(fd, p.data(), size)) {
            return false;
        }
        *type = p[0];
        payload->assign(p.begin() + 1, p.end());
        return true;
    }
    static void Write(int fd, const std::vector<u8>& b) { send(fd, b.data(), b.size(), MSG_NOSIGNAL); }

    // infoOK of the server
    static bool InfoOk(const std::vector<u8>& i) {
        if (i.size() != hub::kNetInfoSize) {
            return false;
        }
        const u8 max = i[0x66], count = i[0x67];
        return i[0x4B] == 2 && i[0x26] <= 32 && max >= 1 && max <= 8 && count >= 1 && count <= max &&
               (i[0x26A] | (i[0x26B] << 8)) <= 0x180;
    }

    void Handle(int fd) {
        u8 type = 0;
        std::vector<u8> p;
        if (!ReadFrame(fd, &type, &p) || p.size() < hub::kHeaderSize || std::memcmp(p.data(), "MHGS", 4) != 0) {
            return;
        }
        if ((p[4] | (p[5] << 8)) != hub::kServerProtocol) {
            return;
        }
        const u16 proto = static_cast<u16>(p[6] | (p[7] << 8));
        std::array<u8, 16> room;
        std::memcpy(room.data(), p.data() + 8, 16);
        const std::vector<u8> body(p.begin() + hub::kHeaderSize, p.end());
        switch (type) {
        case hub::kList: {
            std::vector<u8> out(4, 0);
            u32 n = 0;
            {
                std::lock_guard lock(mu_);
                for (auto& [id, h] : hubs_) {
                    if (h.proto == proto && h.room == room) {
                        for (int i = 0; i < 8; ++i) out.push_back(static_cast<u8>(id >> (8 * i)));
                        out.insert(out.end(), h.info.begin(), h.info.end());
                        ++n;
                    }
                }
            }
            for (int i = 0; i < 4; ++i) out[i] = static_cast<u8>(n >> (8 * i));
            Write(fd, hub::MakeFrame(hub::kHubs, out));
            break;
        }
        case hub::kHost: {
            if (!InfoOk(body)) {
                Write(fd, hub::MakeFrame(hub::kError, {hub::kErrBadRequest, 0, 0, 0}));
                return;
            }
            Hub h;
            h.id = ++next_id_;
            h.proto = proto;
            h.room = room;
            std::memcpy(h.info.data(), body.data(), hub::kNetInfoSize);
            h.fd = fd;
            {
                std::lock_guard lock(mu_);
                hubs_[h.id] = h;
            }
            std::vector<u8> id8;
            for (int i = 0; i < 8; ++i) id8.push_back(static_cast<u8>(h.id >> (8 * i)));
            Write(fd, hub::MakeFrame(hub::kHosted, id8));
            for (;;) {  // UPDATE and PING from the host
                if (!ReadFrame(fd, &type, &p)) {
                    break;
                }
                if (type == hub::kUpdate) {
                    if (!InfoOk(p)) {
                        break;
                    }
                    std::lock_guard lock(mu_);
                    std::memcpy(hubs_[h.id].info.data(), p.data(), hub::kNetInfoSize);
                } else if (type != hub::kPing) {
                    break;
                }
            }
            std::lock_guard lock(mu_);
            hubs_.erase(h.id);
            break;
        }
        case hub::kJoin: {
            if (body.size() != 8) return;
            u64 id = 0;
            for (int i = 0; i < 8; ++i) id |= static_cast<u64>(body[i]) << (8 * i);
            int host_fd = -1;
            {
                std::lock_guard lock(mu_);
                const auto it = hubs_.find(id);
                if (it == hubs_.end() || it->second.proto != proto || it->second.room != room) {
                    Write(fd, hub::MakeFrame(hub::kError, {hub::kErrNotFound, 0, 0, 0}));
                    return;
                }
                host_fd = it->second.fd;
            }
            const u64 ticket = ++next_id_ + 1000;
            auto pend = std::make_shared<Pending>();
            pend->hub = id;
            {
                std::lock_guard lock(mu_);
                pending_[ticket] = pend;
            }
            std::vector<u8> t8;
            for (int i = 0; i < 8; ++i) t8.push_back(static_cast<u8>(ticket >> (8 * i)));
            Write(host_fd, hub::MakeFrame(hub::kIncoming, t8));
            std::unique_lock lock(pend->m);
            if (!pend->cv.wait_for(lock, 10s, [&] { return pend->host_fd >= 0; })) {
                Write(fd, hub::MakeFrame(hub::kError, {hub::kErrNoAnswer, 0, 0, 0}));
                return;
            }
            const int hf = pend->host_fd;
            lock.unlock();
            std::vector<u8> sp(12, 0);
            Write(hf, hub::MakeFrame(hub::kSpliced, sp));
            Write(fd, hub::MakeFrame(hub::kSpliced, sp));
            Splice(hf, fd);
            {
                std::lock_guard l2(pend->m);
                pend->player_fd = -2;
            }
            pend->cv.notify_all();
            break;
        }
        case hub::kAttach: {
            if (body.size() != 8) return;
            u64 ticket = 0;
            for (int i = 0; i < 8; ++i) ticket |= static_cast<u64>(body[i]) << (8 * i);
            std::shared_ptr<Pending> pend;
            {
                std::lock_guard lock(mu_);
                const auto it = pending_.find(ticket);
                if (it == pending_.end()) return;
                pend = it->second;
                pending_.erase(it);
            }
            std::unique_lock lock(pend->m);
            pend->host_fd = fd;
            pend->cv.notify_all();
            pend->cv.wait(lock, [&] { return pend->player_fd == -2; });  // (the splice ends)
            break;
        }
        default: break;
        }
    }

    // copies both ways until either end closes
    static void Splice(int a, int b) {
        pollfd fds[2] = {{a, POLLIN, 0}, {b, POLLIN, 0}};
        u8 buf[8192];
        for (;;) {
            if (poll(fds, 2, 200) < 0) return;
            for (int i = 0; i < 2; ++i) {
                if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                    const auto n = recv(fds[i].fd, buf, sizeof buf, 0);
                    if (n <= 0) {
                        shutdown(fds[1 - i].fd, SHUT_RDWR);
                        return;
                    }
                    send(fds[1 - i].fd, buf, static_cast<std::size_t>(n), MSG_NOSIGNAL);
                }
            }
        }
    }

    int listen_ = -1;
    u16 port_ = 0;
    std::atomic<bool> stop_{false};
    std::thread accept_thread_;
    std::mutex mu_;
    std::vector<int> fds_;
    std::vector<std::thread> handlers_;
    std::map<u64, Hub> hubs_;
    std::map<u64, std::shared_ptr<Pending>> pending_;
    std::atomic<u64> next_id_{100};
};

// ---- a console ---------------------------------------------------------------------------------------------------

struct Console {
    rooms::Client client;
    std::mutex m;
    std::vector<rooms::WifiPacket> got;
    u64 listener = 0;

    Console(u16 port, const std::string& name, const std::string& family = "4u", const std::string& key = "") {
        listener = client.AddWifiListener([this](const rooms::WifiPacket& p) {
            std::lock_guard lock(m);
            got.push_back(p);
        });
        rooms::HubRequest r;
        r.servers = "127.0.0.1:" + std::to_string(port);
        r.key = key;
        r.family = family;
        r.console_id_hash = rooms::ConsoleIdHash(std::hash<std::string>{}(name));
        std::string error;
        REQUIRE(client.JoinHubs(r, &error));
    }
    ~Console() {
        client.Leave();
        client.RemoveWifiListener(listener);
    }
    rooms::MacAddress Mac() { return *client.RoomMac(); }
    std::vector<rooms::WifiPacket> Take() {
        std::lock_guard lock(m);
        auto out = std::move(got);
        got.clear();
        return out;
    }
    bool Saw(rooms::WifiPacket::Type type, const rooms::MacAddress& from) {
        std::lock_guard lock(m);
        for (const auto& p : got) {
            if (p.type == type && p.transmitter == from) {
                return true;
            }
        }
        return false;
    }
};

// A beacon of a MH4U network (the frame body a UDS host sends) for the given host.
std::vector<u8> BeaconOf(const rooms::MacAddress& host, u8 players, u8 max_players) {
    nwm::NetworkInfo info{};
    std::memcpy(info.host_mac_address, host.data(), 6);
    info.channel = 11;
    info.oui_value[0] = 0x00;
    info.oui_value[1] = 0x1F;
    info.oui_value[2] = 0x32;
    info.oui_type = 21;
    const u8 comm[4] = {0x00, 0x12, 0x63, 0x00};
    std::memcpy(info.wlan_comm_id, comm, 4);
    info.id = 7;
    info.total_nodes = players;
    info.max_nodes = max_players;
    info.application_data_size = 3;
    std::vector<nwm::NodeInfo> nodes;
    for (u8 i = 0; i < players; ++i) {
        nwm::NodeInfo n{};
        n.friend_code_seed = 0x1000 + i;
        n.network_node_id = static_cast<u16>(i + 1);
        nodes.push_back(n);
    }
    return nwm::GenerateBeaconFrame(info, nodes);
}

rooms::WifiPacket Frame(rooms::WifiPacket::Type type, const rooms::MacAddress& to, std::vector<u8> data) {
    rooms::WifiPacket p;
    p.type = type;
    p.destination = to;
    p.channel = 11;
    p.data = std::move(data);
    return p;
}

}  // namespace

TEST_CASE("hub: servers, families and the matchmaking key") {
    CHECK(hub::ParseServers("").size() == 2);
    CHECK(hub::ParseServers("off").empty());
    const auto two = hub::ParseServers("a.example, b.example:1234;a.example bad:99999 ::1");
    REQUIRE(two.size() == 2);
    CHECK(two[0].Text() == "a.example");
    CHECK(two[1].port == 1234);
    CHECK(hub::FormatServers(two) == "a.example b.example:1234");

    CHECK(hub::FamilyOfProgram(rooms::kMh4uUsa) == hub::Family::Mh4u);
    CHECK(hub::FamilyOfProgram(rooms::kMh4uEurope) == hub::Family::Mh4u);
    CHECK(hub::FamilyOfProgram(0x0004000000155400ULL) == hub::Family::Mh4g);
    CHECK_FALSE(hub::FamilyOfProgram(0x0004000000197100ULL).has_value());  // another game
    CHECK(hub::LinkProtocol(hub::Family::Mh4u) != hub::LinkProtocol(hub::Family::Mh4g));
    // MHGU's link protocols are 2 and 3: never ours
    CHECK(hub::kLinkMh4u > 3);
    CHECK(hub::kLinkMh4g > 3);

    const auto none = hub::RoomOfKey("");
    CHECK(none == std::array<u8, 16>{});
    CHECK(hub::RoomOfKey("friends") != none);
    CHECK(hub::RoomOfKey("friends") == hub::RoomOfKey("friends"));
}

TEST_CASE("hub: a beacon fits a NetworkInfo the server accepts, and comes back whole") {
    const rooms::MacAddress mac = {0x02, 1, 2, 3, 4, 5};
    for (const u8 players : {1, 4, 8}) {
        const std::vector<u8> beacon = BeaconOf(mac, players, 8);
        hub::HubBeacon hb;
        hb.mac = mac;
        hb.channel = 11;
        hb.beacon = beacon;
        REQUIRE(hub::SummarizeBeacon(beacon, &hb.comm_id, &hb.nodes, &hb.max_nodes));
        CHECK(hb.comm_id == 0x00126300);
        CHECK(hb.nodes == players);
        CHECK(hb.max_nodes == 8);
        std::array<u8, hub::kNetInfoSize> info;
        REQUIRE(hub::PackInfo(hb, hub::kLinkMh4u, &info));
        // what the server looks at
        CHECK(info[0x4B] == 2);
        CHECK(info[0x26] <= 32);
        CHECK(info[0x66] == 8);
        CHECK(info[0x67] == players);
        CHECK((info[0x26A] | (info[0x26B] << 8)) == 0);
        const auto back = hub::UnpackInfo(info, hub::kLinkMh4u);
        REQUIRE(back.has_value());
        CHECK(back->beacon == beacon);
        CHECK(back->mac == mac);
        CHECK(back->comm_id == 0x00126300);
        CHECK_FALSE(hub::UnpackInfo(info, hub::kLinkMh4g).has_value());  // another family's
    }
    hub::HubBeacon too_big;
    too_big.beacon.assign(hub::kBeaconMax + 1, 1);
    too_big.nodes = 1;
    too_big.max_nodes = 4;
    std::array<u8, hub::kNetInfoSize> info;
    CHECK_FALSE(hub::PackInfo(too_big, hub::kLinkMh4u, &info));
}

TEST_CASE("hub: frames survive being cut into pieces") {
    std::vector<u8> stream;
    const auto a = hub::MakeFrame(hub::kHosted, {1, 2, 3});
    const auto b = hub::MakeLinkFrame(hub::kLinkPing, {});
    stream.insert(stream.end(), a.begin(), a.end());
    std::vector<u8> buf;
    u8 type = 0;
    std::vector<u8> payload;
    bool bad = false;
    for (std::size_t i = 0; i < stream.size(); ++i) {
        CHECK_FALSE(hub::TakeFrame(buf, 100, &type, &payload, &bad));
        buf.push_back(stream[i]);
    }
    REQUIRE(hub::TakeFrame(buf, 100, &type, &payload, &bad));
    CHECK(type == hub::kHosted);
    CHECK(payload == std::vector<u8>{1, 2, 3});
    std::vector<u8> huge = {0xFF, 0xFF, 0xFF, 0x7F, 1};
    CHECK_FALSE(hub::TakeFrame(huge, 100, &type, &payload, &bad));
    CHECK(bad);
    std::vector<u8> lb = b;
    u8 kind = 0;
    REQUIRE(hub::TakeLinkFrame(lb, &kind, &payload, &bad));
    CHECK(kind == hub::kLinkPing);
    hub::Hello hello;
    hello.link_protocol = hub::kLinkMh4u;
    hello.mac = {2, 3, 4, 5, 6, 7};
    const auto decoded = hub::DecodeHello(hub::EncodeHello(hello));
    REQUIRE(decoded.has_value());
    CHECK(decoded->mac == hello.mac);
    CHECK(decoded->link_protocol == hub::kLinkMh4u);
    CHECK_FALSE(hub::DecodeHello({1, 2, 3}).has_value());
    rooms::WifiPacket w;
    w.type = rooms::WifiPacket::Type::Data;
    w.channel = 3;
    w.transmitter = {1, 2, 3, 4, 5, 6};
    w.destination = {6, 5, 4, 3, 2, 1};
    w.data = {9, 8, 7};
    const auto w2 = hub::DecodeWifi(hub::EncodeWifi(w));
    REQUIRE(w2.has_value());
    CHECK(w2->data == w.data);
    CHECK(w2->transmitter == w.transmitter);
    CHECK(w2->destination == w.destination);
}

TEST_CASE("hub: a hosted network is found, joined and talked over, through a server") {
    TestServer server;
    Console host(server.port(), "host");
    Console player(server.port(), "player");
    REQUIRE(host.client.WaitJoined(2000));
    REQUIRE(player.client.WaitJoined(2000));
    const auto hm = host.Mac();
    const auto pm = player.Mac();
    CHECK(hm != pm);
    CHECK((hm[0] & 1) == 0);

    // the host's console sends beacons: the hub is listed, and the player's console receives them as the host's
    const auto beacon = BeaconOf(hm, 1, 4);
    REQUIRE(WaitFor([&] {
        host.client.SendWifi(Frame(rooms::WifiPacket::Type::Beacon, rooms::kBroadcastMac, beacon));
        return server.HubCount() == 1;
    }));
    REQUIRE(WaitFor([&] {
        host.client.SendWifi(Frame(rooms::WifiPacket::Type::Beacon, rooms::kBroadcastMac, beacon));
        return player.Saw(rooms::WifiPacket::Type::Beacon, hm);
    }));
    for (const auto& p : player.Take()) {
        if (p.type == rooms::WifiPacket::Type::Beacon) {
            CHECK(p.data == beacon);
            CHECK(p.transmitter == hm);
            CHECK(p.destination == rooms::kBroadcastMac);
            CHECK(p.channel == 11);
        }
    }
    // the host does not see its own hub
    CHECK_FALSE(host.Saw(rooms::WifiPacket::Type::Beacon, hm));
    CHECK_FALSE(host.Saw(rooms::WifiPacket::Type::Beacon, pm));

    // the player's authentication to the host's MAC opens the link; the host's console receives it from the player's
    const std::vector<u8> auth = {1, 2, 3, 4};
    player.client.SendWifi(Frame(rooms::WifiPacket::Type::Authentication, hm, auth));
    REQUIRE(WaitFor([&] { return host.Saw(rooms::WifiPacket::Type::Authentication, pm); }));
    // and the answer goes back, as a unicast and as a broadcast
    host.client.SendWifi(Frame(rooms::WifiPacket::Type::Authentication, pm, {9, 9}));
    host.client.SendWifi(Frame(rooms::WifiPacket::Type::Data, rooms::kBroadcastMac, {7, 7, 7}));
    REQUIRE(WaitFor([&] { return player.Saw(rooms::WifiPacket::Type::Authentication, hm); }));
    REQUIRE(WaitFor([&] { return player.Saw(rooms::WifiPacket::Type::Data, hm); }));
    // data both ways, with the bytes intact
    player.client.SendWifi(Frame(rooms::WifiPacket::Type::Data, hm, std::vector<u8>(1400, 0xAB)));
    REQUIRE(WaitFor([&] {
        for (const auto& p : host.Take()) {
            if (p.type == rooms::WifiPacket::Type::Data && p.data.size() == 1400 && p.data[7] == 0xAB) return true;
        }
        return false;
    }));
    // the status tells it
    const auto hs = host.client.GetHubStatus();
    CHECK(hs.active);
    CHECK(hs.hosting);
    CHECK(hs.players == 1);
    CHECK(hs.family == "MH4U");
    const auto ps = player.client.GetHubStatus();
    CHECK(ps.linked);

    // the player's console leaves: the host's console gets a deauthentication from it
    player.client.SendWifi(Frame(rooms::WifiPacket::Type::Deauthentication, hm, {}));
    REQUIRE(WaitFor([&] { return host.Saw(rooms::WifiPacket::Type::Deauthentication, pm); }));
    REQUIRE(WaitFor([&] { return host.client.GetHubStatus().players == 0; }));
}

TEST_CASE("hub: a player that drops is reported to the host's game") {
    TestServer server;
    Console host(server.port(), "h2");
    auto player = std::make_unique<Console>(server.port(), "p2");
    const auto hm = host.Mac();
    const auto pm = player->Mac();
    const auto beacon = BeaconOf(hm, 1, 4);
    REQUIRE(WaitFor([&] {
        host.client.SendWifi(Frame(rooms::WifiPacket::Type::Beacon, rooms::kBroadcastMac, beacon));
        return player->Saw(rooms::WifiPacket::Type::Beacon, hm);
    }));
    player->client.SendWifi(Frame(rooms::WifiPacket::Type::Authentication, hm, {1}));
    REQUIRE(WaitFor([&] { return host.Saw(rooms::WifiPacket::Type::Authentication, pm); }));
    player.reset();  // (the app is closed: no deauthentication was sent)
    REQUIRE(WaitFor([&] { return host.Saw(rooms::WifiPacket::Type::Deauthentication, pm); }));
}

TEST_CASE("hub: three players, the host relays between them") {
    TestServer server;
    Console host(server.port(), "h3");
    Console a(server.port(), "a3");
    Console b(server.port(), "b3");
    const auto hm = host.Mac(), am = a.Mac(), bm = b.Mac();
    const auto beacon = BeaconOf(hm, 1, 4);
    REQUIRE(WaitFor([&] {
        host.client.SendWifi(Frame(rooms::WifiPacket::Type::Beacon, rooms::kBroadcastMac, beacon));
        return a.Saw(rooms::WifiPacket::Type::Beacon, hm) && b.Saw(rooms::WifiPacket::Type::Beacon, hm);
    }));
    a.client.SendWifi(Frame(rooms::WifiPacket::Type::Authentication, hm, {1}));
    b.client.SendWifi(Frame(rooms::WifiPacket::Type::Authentication, hm, {1}));
    REQUIRE(WaitFor([&] {
        return host.Saw(rooms::WifiPacket::Type::Authentication, am) && host.Saw(rooms::WifiPacket::Type::Authentication, bm);
    }));
    // a's broadcast reaches the host and b; a frame to b alone reaches b only
    a.client.SendWifi(Frame(rooms::WifiPacket::Type::Data, rooms::kBroadcastMac, {5}));
    REQUIRE(WaitFor([&] { return b.Saw(rooms::WifiPacket::Type::Data, am) && host.Saw(rooms::WifiPacket::Type::Data, am); }));
    host.Take();
    b.Take();
    a.client.SendWifi(Frame(rooms::WifiPacket::Type::Data, bm, {6}));
    REQUIRE(WaitFor([&] { return b.Saw(rooms::WifiPacket::Type::Data, am); }));
    std::this_thread::sleep_for(300ms);
    CHECK_FALSE(host.Saw(rooms::WifiPacket::Type::Data, am));
    // the host's game rebroadcasts what is for another node: the broadcast reaches both
    host.client.SendWifi(Frame(rooms::WifiPacket::Type::Data, rooms::kBroadcastMac, {8}));
    REQUIRE(WaitFor([&] { return a.Saw(rooms::WifiPacket::Type::Data, hm) && b.Saw(rooms::WifiPacket::Type::Data, hm); }));
}

TEST_CASE("hub: hubs are kept apart by family and by matchmaking key") {
    TestServer server;
    Console host(server.port(), "h4", "4u", "group-one");
    Console same(server.port(), "s4", "4u", "group-one");
    Console other_key(server.port(), "k4", "4u", "group-two");
    Console no_key(server.port(), "n4", "4u", "");
    Console other_game(server.port(), "g4", "4g", "group-one");
    const auto hm = host.Mac();
    const auto beacon = BeaconOf(hm, 1, 4);
    REQUIRE(WaitFor([&] {
        host.client.SendWifi(Frame(rooms::WifiPacket::Type::Beacon, rooms::kBroadcastMac, beacon));
        return same.Saw(rooms::WifiPacket::Type::Beacon, hm);
    }));
    std::this_thread::sleep_for(1200ms);  // (more than a search has taken)
    CHECK_FALSE(other_key.Saw(rooms::WifiPacket::Type::Beacon, hm));
    CHECK_FALSE(no_key.Saw(rooms::WifiPacket::Type::Beacon, hm));
    CHECK_FALSE(other_game.Saw(rooms::WifiPacket::Type::Beacon, hm));
}

TEST_CASE("hub: a game that is not MH4U or MH4G does not use hubs") {
    rooms::Client client;
    client.SetGameInfo({"Some other game", 0x0004000000197100ULL});
    rooms::HubRequest r;
    r.servers = "127.0.0.1:1";
    r.console_id_hash = rooms::ConsoleIdHash(5);
    REQUIRE(client.JoinHubs(r));
    CHECK(client.GetState() == rooms::State::Idle);
    CHECK_FALSE(client.GetHubStatus().problem.empty());
    CHECK_FALSE(client.RoomMac().has_value());
    // MH4U's own ids are fine (auto)
    client.SetGameInfo({"Monster Hunter 4 Ultimate", rooms::kMh4uUsa});
    REQUIRE(client.JoinHubs(r));
    CHECK(client.WaitJoined(2000));
    CHECK(client.GetHubStatus().family == "MH4U");
    client.Leave();
}

TEST_CASE("hub: a server that is down does not stop the game") {
    Console lonely(1, "x");  // (nothing listens on port 1)
    REQUIRE(lonely.client.WaitJoined(2000));
    CHECK(lonely.client.SendWifi(Frame(rooms::WifiPacket::Type::Data, rooms::kBroadcastMac, {1})));
    REQUIRE(WaitFor([&] {
        const auto st = lonely.client.GetHubStatus();
        return !st.servers.empty() && !st.servers[0].problem.empty();
    }));
    CHECK_FALSE(lonely.client.GetHubStatus().servers[0].listed);
}

TEST_CASE("hub: Check hub server") {
    TestServer server;
    const auto ep = hub::ParseServers("127.0.0.1:" + std::to_string(server.port()));
    const auto ok = hub::CheckServers(ep, hub::Family::Mh4u, "", 2000);
    REQUIRE(ok.size() == 1);
    CHECK(ok[0].ok);
    CHECK(ok[0].hubs == 0);
    const auto down = hub::CheckServers(hub::ParseServers("127.0.0.1:1"), hub::Family::Mh4u, "", 1000);
    REQUIRE(down.size() == 1);
    CHECK_FALSE(down[0].ok);
    CHECK_FALSE(down[0].detail.empty());
}

#endif  // !_WIN32
