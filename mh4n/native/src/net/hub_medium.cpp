// The hub servers as a wireless medium (hub_medium.h). The wire protocol toward the server is the one in gu-native's
// server/server.go; the link between two players is this port's (mh4n/hub.h).
#include "hub_medium.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

#include "mh4n/log.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace mh4n::hub {

namespace {

constexpr const char* kLog = "Hub";
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using rooms::MacAddress;
using rooms::WifiPacket;

// ---- sockets (IPv4 TCP, non-blocking, every wait a poll that looks at `stop`) ------------------------------------

namespace sock {

#ifdef _WIN32
using Handle = SOCKET;
constexpr Handle kBad = INVALID_SOCKET;
int LastError() { return WSAGetLastError(); }
bool WouldBlock(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
int Poll(Handle h, short events, int ms) {
    WSAPOLLFD p{h, events, 0};
    const int r = WSAPoll(&p, 1, ms);
    return r < 0 ? -1 : r == 0 ? 0 : p.revents;
}
void CloseHandle_(Handle h) { closesocket(h); }
void SetNonBlocking(Handle h) { u_long on = 1; ioctlsocket(h, FIONBIO, &on); }
#else
using Handle = int;
constexpr Handle kBad = -1;
int LastError() { return errno; }
bool WouldBlock(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS || e == EINTR; }
int Poll(Handle h, short events, int ms) {
    pollfd p{h, events, 0};
    const int r = ::poll(&p, 1, ms);
    return r < 0 ? (errno == EINTR ? 0 : -1) : r == 0 ? 0 : p.revents;
}
void CloseHandle_(Handle h) { ::close(h); }
void SetNonBlocking(Handle h) {
    const int flags = fcntl(h, F_GETFL, 0);
    fcntl(h, F_SETFL, flags | O_NONBLOCK);
}
#endif

bool Init() {
#ifdef _WIN32
    static const bool ok = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ok;
#else
    return true;
#endif
}

void Close(Handle h) {
    if (h != kBad) {
        CloseHandle_(h);
    }
}

constexpr int kSlice = 200;  // a wait is cut into slices of this many ms so `stop` is looked at

// Connects to host:port; kBad (with `*error`) when it cannot.
Handle Connect(const Endpoint& e, int timeout_ms, const std::atomic<bool>& stop, std::string* error) {
    if (!Init()) {
        *error = "the network could not start";
        return kBad;
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(e.host.c_str(), std::to_string(e.port).c_str(), &hints, &res) != 0 || res == nullptr) {
        *error = "the name " + e.host + " was not found";
        return kBad;
    }
    Handle out = kBad;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    for (addrinfo* ai = res; ai != nullptr && out == kBad && !stop; ai = ai->ai_next) {
        const Handle s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == kBad) {
            continue;
        }
        SetNonBlocking(s);
        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
        setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&one), sizeof one);
#ifdef SO_NOSIGPIPE
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&one), sizeof one);
#endif
        const int rc = ::connect(s, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
        bool ok = rc == 0;
        if (!ok && WouldBlock(LastError())) {
            while (!stop && Clock::now() < deadline) {
                const int ev = Poll(s, POLLOUT, kSlice);
                if (ev < 0) {
                    break;
                }
                if (ev != 0) {
                    int err = 0;
                    socklen_t len = sizeof err;
                    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
                    ok = err == 0;
                    break;
                }
            }
        }
        if (ok) {
            out = s;
        } else {
            Close(s);
        }
    }
    freeaddrinfo(res);
    if (out == kBad && error->empty()) {
        *error = stop ? "stopped" : "no answer from " + e.Text();
    }
    return out;
}

// Sends it all; false when the connection is gone, the time ran out or `stop` was set.
bool SendAll(Handle h, const u8* data, std::size_t size, int timeout_ms, const std::atomic<bool>& stop) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    std::size_t sent = 0;
    while (sent < size) {
        if (stop || Clock::now() > deadline) {
            return false;
        }
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const auto n = ::send(h, reinterpret_cast<const char*>(data + sent), static_cast<int>(size - sent), flags);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && WouldBlock(LastError())) {
            if (Poll(h, POLLOUT, kSlice) < 0) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

enum class Got { Data, Closed, Timeout, Stopped };

// Waits up to `timeout_ms` for bytes and appends what arrived to `buffer`.
Got Receive(Handle h, std::vector<u8>& buffer, int timeout_ms, const std::atomic<bool>& stop) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    u8 chunk[8192];
    for (;;) {
        if (stop) {
            return Got::Stopped;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (left <= 0) {
            return Got::Timeout;
        }
        const int ev = Poll(h, POLLIN, static_cast<int>(std::min<long long>(left, kSlice)));
        if (ev < 0) {
            return Got::Closed;
        }
        if (ev == 0) {
            continue;
        }
        const auto n = ::recv(h, reinterpret_cast<char*>(chunk), sizeof chunk, 0);
        if (n > 0) {
            buffer.insert(buffer.end(), chunk, chunk + n);
            return Got::Data;
        }
        if (n < 0 && WouldBlock(LastError())) {
            continue;
        }
        return Got::Closed;
    }
}

}  // namespace sock

// One control frame from the server (the first answer of a request).
enum class Read { Ok, Closed, Timeout, Stopped, Bad };

Read ReadControl(sock::Handle h, std::vector<u8>& buffer, std::size_t max_payload, int timeout_ms,
                 const std::atomic<bool>& stop, u8* type, std::vector<u8>* payload) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        bool bad = false;
        if (TakeFrame(buffer, max_payload, type, payload, &bad)) {
            return Read::Ok;
        }
        if (bad) {
            return Read::Bad;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (left <= 0) {
            return Read::Timeout;
        }
        switch (sock::Receive(h, buffer, static_cast<int>(left), stop)) {
        case sock::Got::Data: break;
        case sock::Got::Closed: return Read::Closed;
        case sock::Got::Timeout: return Read::Timeout;
        case sock::Got::Stopped: return Read::Stopped;
        }
    }
}

bool SendControl(sock::Handle h, u8 type, const std::vector<u8>& payload, const std::atomic<bool>& stop) {
    const std::vector<u8> f = MakeFrame(type, payload);
    return sock::SendAll(h, f.data(), f.size(), 10000, stop);
}

std::vector<u8> Request(u16 link, const std::array<u8, 16>& room, const std::vector<u8>& body) {
    std::vector<u8> p = MakeHeader(link, room);
    p.insert(p.end(), body.begin(), body.end());
    return p;
}

std::vector<u8> U64(u64 v) {
    std::vector<u8> out;
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<u8>(v >> (8 * i)));
    }
    return out;
}

std::string MacText(const MacAddress& m) {
    char b[24];
    std::snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return b;
}

constexpr std::size_t kMaxHubsPayload = 1 << 17;

// Asks one server for its hubs: false (with `*why`) when it cannot be asked.
bool ListOne(const Endpoint& server, u16 link, const std::array<u8, 16>& room, int timeout_ms,
             const std::atomic<bool>& stop, std::vector<ListedHub>* hubs, std::string* why) {
    std::string error;
    const sock::Handle h = sock::Connect(server, timeout_ms, stop, &error);
    if (h == sock::kBad) {
        *why = error;
        return false;
    }
    bool ok = false;
    if (!SendControl(h, kList, Request(link, room, {}), stop)) {
        *why = "the server closed the connection";
    } else {
        std::vector<u8> buffer, payload;
        u8 type = 0;
        switch (ReadControl(h, buffer, kMaxHubsPayload, timeout_ms, stop, &type, &payload)) {
        case Read::Ok:
            if (type == kHubs) {
                *hubs = ParseHubs(payload);
                ok = true;
            } else if (type == kError && payload.size() >= 4) {
                const u32 code = static_cast<u32>(payload[0]) | (static_cast<u32>(payload[1]) << 8);
                *why = ServerErrorText(code);
            } else {
                *why = "the server answered something else";
            }
            break;
        case Read::Timeout: *why = "the server did not answer in time"; break;
        case Read::Stopped: *why = "stopped"; break;
        default: *why = "the server closed the connection"; break;
        }
    }
    sock::Close(h);
    return ok;
}

// ---- a link between two players ------------------------------------------------------------------------------------

class Link {
public:
    /// `leftover`: bytes that came behind the server's SPLICED frame: they are the peer's first link frames.
    Link(sock::Handle h, const std::atomic<bool>& stop, std::vector<u8> leftover = {})
        : fd_(h), stop_(stop), buffer_(std::move(leftover)) {
        writer_ = std::thread([this] { WriterLoop(); });
    }
    ~Link() {
        Close();
        if (writer_.joinable()) {
            writer_.join();
        }
        sock::Close(fd_);
    }
    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;

    /// A whole link frame (MakeLinkFrame). Dropped when the queue is long (the medium is unreliable like the air).
    void Queue(std::vector<u8> bytes) {
        std::lock_guard lock(m_);
        if (closed_ || out_.size() > 512) {
            return;
        }
        out_.push_back(std::move(bytes));
        cv_.notify_one();
    }
    /// Closes once what is queued is sent.
    void CloseAfterFlush() {
        std::lock_guard lock(m_);
        flush_close_ = true;
        cv_.notify_all();
    }
    void Close() {
        std::lock_guard lock(m_);
        closed_ = true;
        cv_.notify_all();
    }
    bool Closed() const {
        std::lock_guard lock(m_);
        return closed_;
    }

    enum class Frame { Ok, Closed, Timeout };
    /// One link frame, waiting at most `timeout_ms`.
    Frame Read(int timeout_ms, u8* kind, std::vector<u8>* payload) {
        const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            bool bad = false;
            if (TakeLinkFrame(buffer_, kind, payload, &bad)) {
                return Frame::Ok;
            }
            if (bad || Closed()) {
                return Frame::Closed;
            }
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (left <= 0) {
                return Frame::Timeout;
            }
            switch (sock::Receive(fd_, buffer_, static_cast<int>(left), stop_)) {
            case sock::Got::Data: break;
            case sock::Got::Timeout: return Frame::Timeout;
            default: return Frame::Closed;
            }
        }
    }

private:
    void WriterLoop() {
        std::unique_lock lock(m_);
        auto next_ping = Clock::now() + 5s;
        for (;;) {
            while (!out_.empty()) {
                std::vector<u8> bytes = std::move(out_.front());
                out_.pop_front();
                lock.unlock();
                const bool ok = sock::SendAll(fd_, bytes.data(), bytes.size(), 15000, stop_);
                lock.lock();
                if (!ok) {
                    closed_ = true;
                    return;
                }
                next_ping = Clock::now() + 5s;
            }
            if (closed_ || stop_) {
                return;
            }
            if (flush_close_) {
                closed_ = true;
                return;
            }
            if (Clock::now() >= next_ping) {
                out_.push_back(MakeLinkFrame(kLinkPing, {}));
                continue;
            }
            cv_.wait_for(lock, 250ms);
        }
    }

    sock::Handle fd_;
    const std::atomic<bool>& stop_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::vector<u8>> out_;
    bool closed_ = false;
    bool flush_close_ = false;
    std::vector<u8> buffer_;
    std::thread writer_;
};

struct Pair {
    std::shared_ptr<Link> link;
    MacAddress mac{};
    bool got_deauth = false;
};

}  // namespace

// ---- the medium ------------------------------------------------------------------------------------------------

struct Medium::Core : std::enable_shared_from_this<Medium::Core> {
    MediumOptions opt;
    Deliver deliver;
    std::mutex deliver_mutex;  // a call of `deliver` runs under it; Shutdown takes it, so none runs after Shutdown
    bool shut = false;         // under deliver_mutex
    std::atomic<bool> stop{false};
    std::array<u8, 16> room{};
    u16 link_protocol = 0;

    mutable std::mutex mu;
    std::condition_variable cv;
    u64 revision = 1;
    std::string problem;

    struct Entry {
        Endpoint server;
        u64 id = 0;
        HubBeacon hub;
        Clock::time_point seen;
    };
    std::vector<Entry> entries;
    std::vector<ServerState> servers;

    // hosting
    bool hosting = false;
    u64 host_gen = 0;
    HubBeacon host_hub;
    u64 host_version = 0;  // changes when the hub's listing changes
    Clock::time_point last_beacon;
    std::vector<std::shared_ptr<Pair>> pairs;

    // playing
    bool joining = false;
    std::shared_ptr<Link> client_link;
    MacAddress client_host{};
    std::deque<std::vector<u8>> join_queue;

    std::vector<std::thread> threads;

    // ---- plumbing ----
    void Touch() {
        ++revision;
        cv.notify_all();
    }
    void SetProblem(std::string text) {
        problem = std::move(text);
        Touch();
    }
    void Out(const WifiPacket& p) {
        std::lock_guard lock(deliver_mutex);
        if (!shut && deliver) {
            deliver(p);
        }
    }
    ServerState* StateOf(const Endpoint& e) {
        for (ServerState& s : servers) {
            if (s.server == e) {
                return &s;
            }
        }
        return nullptr;
    }
    bool Sleep(std::chrono::milliseconds d) {  // false when stopped
        std::unique_lock lock(mu);
        cv.wait_for(lock, d, [this] { return stop.load(); });
        return !stop;
    }

    void Start();
    void Shutdown();

    // ---- threads ----
    void ListerLoop();
    void TickerLoop();
    void HostConnLoop(Endpoint server, u64 gen);
    void PairLoop(Endpoint server, u64 ticket, u64 gen);
    void JoinLoop(std::vector<Entry> candidates);

    // ---- from the UDS service ----
    void Send(WifiPacket p);
    void HostBeacon(const WifiPacket& p);
    void StopHosting(const char* why);
    void RouteHostFrame(const WifiPacket& p, const std::shared_ptr<Pair>& from);
    void Detach(std::thread t) { threads.push_back(std::move(t)); }  // under mu
};

void Medium::Core::Start() {
    stop = false;
    room = RoomOfKey(opt.key);
    link_protocol = LinkProtocol(opt.family);
    opt.max_players = std::clamp(opt.max_players, 1, 7);
    for (const Endpoint& e : opt.servers) {
        ServerState s;
        s.server = e;
        servers.push_back(s);
    }
    const auto self = shared_from_this();
    std::lock_guard lock(mu);
    threads.emplace_back([self] { self->ListerLoop(); });
    threads.emplace_back([self] { self->TickerLoop(); });
}

void Medium::Core::Shutdown() {
    {
        std::lock_guard lock(deliver_mutex);
        shut = true;
        deliver = nullptr;
    }
    std::vector<std::thread> ts;
    std::vector<std::shared_ptr<Pair>> ps;
    std::shared_ptr<Link> cl;
    {
        std::lock_guard lock(mu);
        stop = true;
        ps = pairs;
        cl = client_link;
        ts.swap(threads);
        cv.notify_all();
    }
    for (auto& p : ps) {
        p->link->Close();
    }
    if (cl) {
        cl->Close();
    }
    // The threads hold this core, and end within a few poll slices: they are left to finish on their own, so a name
    // that takes long to resolve cannot hold the caller.
    for (std::thread& t : ts) {
        if (t.joinable()) {
            t.detach();
        }
    }
}

// ---- listing and delivering the hubs to the game ------------------------------------------------------------------

void Medium::Core::ListerLoop() {
    bool first = true;
    while (!stop) {
        bool busy;
        {
            std::lock_guard lock(mu);
            busy = hosting || client_link != nullptr;
        }
        if (!busy || first) {
            first = false;
            for (const Endpoint& server : opt.servers) {
                if (stop) {
                    return;
                }
                std::vector<ListedHub> hubs;
                std::string why;
                const bool ok = ListOne(server, link_protocol, room, opt.connect_timeout_ms, stop, &hubs, &why);
                std::lock_guard lock(mu);
                ServerState* st = StateOf(server);
                entries.erase(std::remove_if(entries.begin(), entries.end(),
                                             [&](const Entry& e) { return ok && e.server == server; }),
                              entries.end());
                if (ok) {
                    const auto now = Clock::now();
                    for (const ListedHub& h : hubs) {
                        if (const auto b = UnpackInfo(h.info, link_protocol)) {
                            entries.push_back({server, h.id, *b, now});
                        }
                    }
                    if (st != nullptr && (!st->listed || !st->problem.empty())) {
                        st->listed = true;
                        st->problem.clear();
                    }
                    Touch();
                } else if (st != nullptr && !stop) {
                    if (st->listed || st->problem != why) {
                        MH4N_LOG(Warning, kLog, "{}: {}", server.Text(), why);
                    }
                    st->listed = false;
                    st->problem = why;
                    Touch();
                }
            }
        }
        if (!Sleep(std::chrono::milliseconds(opt.list_every_ms))) {
            return;
        }
    }
}

void Medium::Core::TickerLoop() {
    auto next_beacons = Clock::now();
    while (!stop) {
        if (!Sleep(50ms)) {
            return;
        }
        const auto now = Clock::now();
        std::vector<Entry> deliver_now;
        {
            std::lock_guard lock(mu);
            if (hosting && now - last_beacon > 3s) {
                StopHosting("the game stopped its beacons");
            }
            if (now >= next_beacons) {
                next_beacons = now + std::chrono::milliseconds(opt.beacon_every_ms);
                if (!hosting && !client_link && !joining) {
                    const auto fresh = std::chrono::milliseconds(opt.list_every_ms * 3 + opt.connect_timeout_ms);
                    std::vector<MacAddress> seen;
                    for (const Entry& e : entries) {
                        if (now - e.seen > fresh || e.hub.mac == opt.mac ||
                            std::find(seen.begin(), seen.end(), e.hub.mac) != seen.end()) {
                            continue;
                        }
                        seen.push_back(e.hub.mac);
                        deliver_now.push_back(e);
                    }
                }
            }
        }
        for (const Entry& e : deliver_now) {
            WifiPacket p;
            p.type = WifiPacket::Type::Beacon;
            p.channel = e.hub.channel;
            p.transmitter = e.hub.mac;
            p.destination = rooms::kBroadcastMac;
            p.data = e.hub.beacon;
            Out(p);
        }
    }
}

// ---- hosting -----------------------------------------------------------------------------------------------------------

void Medium::Core::HostBeacon(const WifiPacket& p) {
    HubBeacon hub;
    hub.mac = opt.mac;
    hub.channel = p.channel;
    hub.beacon = p.data;
    u32 comm = 0;
    u8 nodes = 1, max = 4;
    if (!SummarizeBeacon(p.data, &comm, &nodes, &max)) {
        std::lock_guard lock(mu);
        if (problem.empty()) {
            MH4N_LOG(Warning, kLog, "the game's beacon is not one this port can list ({} bytes)", p.data.size());
            SetProblem("the hub could not be listed: the game's beacon is not one the hub servers can carry");
        }
        return;
    }
    hub.comm_id = comm;
    hub.nodes = std::clamp<u8>(nodes, 1, 8);
    hub.max_nodes = std::clamp<u8>(max, hub.nodes, 8);
    std::array<u8, kNetInfoSize> probe;
    if (!PackInfo(hub, link_protocol, &probe)) {
        std::lock_guard lock(mu);
        SetProblem("the hub could not be listed: its description is too big for the hub servers");
        return;
    }
    const auto self = shared_from_this();
    std::lock_guard lock(mu);
    last_beacon = Clock::now();
    if (stop || client_link || joining) {
        return;  // (a console that plays on another's hub does not host)
    }
    const bool changed = !hosting || hub.beacon != host_hub.beacon || hub.nodes != host_hub.nodes ||
                         hub.max_nodes != host_hub.max_nodes;
    host_hub = hub;
    if (!hosting) {
        hosting = true;
        ++host_gen;
        MH4N_LOG(Info, kLog, "hosting a {} hub on {} server(s), {} of {} players", FamilyName(opt.family),
                 opt.servers.size(), static_cast<int>(hub.nodes), static_cast<int>(hub.max_nodes));
        for (const Endpoint& e : opt.servers) {
            const u64 gen = host_gen;
            Detach(std::thread([self, e, gen] { self->HostConnLoop(e, gen); }));
        }
        Touch();
    }
    if (changed) {
        ++host_version;
        Touch();
    }
}

void Medium::Core::StopHosting(const char* why) {  // under mu
    if (!hosting) {
        return;
    }
    MH4N_LOG(Info, kLog, "the hub is closed: {}", why);
    hosting = false;
    ++host_gen;
    for (auto& p : pairs) {
        p->link->Close();
    }
    for (ServerState& s : servers) {
        s.hosting = false;
    }
    Touch();
}

void Medium::Core::HostConnLoop(Endpoint server, u64 gen) {
    const auto self = shared_from_this();
    const auto active = [&] {
        std::lock_guard lock(mu);
        return hosting && host_gen == gen && !stop;
    };
    while (active()) {
        std::string error;
        sock::Handle h = sock::Connect(server, opt.connect_timeout_ms, stop, &error);
        const auto fail = [&](const std::string& why) {
            std::lock_guard lock(mu);
            if (ServerState* s = StateOf(server)) {
                s->hosting = false;
                if (s->problem != why) {
                    MH4N_LOG(Warning, kLog, "{}: {}", server.Text(), why);
                }
                s->problem = why;
            }
            Touch();
        };
        if (h == sock::kBad) {
            fail(error);
            if (!Sleep(3s)) {
                return;
            }
            continue;
        }
        u64 sent_version = 0;
        std::array<u8, kNetInfoSize> info;
        {
            std::lock_guard lock(mu);
            sent_version = host_version;
            PackInfo(host_hub, link_protocol, &info);
        }
        std::vector<u8> buffer, payload;
        u8 type = 0;
        bool hosted = false;
        if (SendControl(h, kHost, Request(link_protocol, room, std::vector<u8>(info.begin(), info.end())), stop) &&
            ReadControl(h, buffer, kMaxFrame, opt.handshake_timeout_ms, stop, &type, &payload) == Read::Ok) {
            if (type == kHosted) {
                hosted = true;
            } else if (type == kError && payload.size() >= 4) {
                fail(ServerErrorText(static_cast<u32>(payload[0]) | (static_cast<u32>(payload[1]) << 8)));
            } else {
                fail("the server answered something else");
            }
        } else if (!stop) {
            fail("the server did not take the hub");
        }
        if (hosted) {
            {
                std::lock_guard lock(mu);
                if (ServerState* s = StateOf(server)) {
                    s->hosting = true;
                    s->problem.clear();
                }
                Touch();
            }
            auto last_sent = Clock::now();
            auto last_ping = Clock::now();
            while (active()) {
                bool bad = false;
                if (!TakeFrame(buffer, kMaxFrame, &type, &payload, &bad)) {
                    if (bad) {
                        break;
                    }
                    const auto got = sock::Receive(h, buffer, 100, stop);
                    if (got == sock::Got::Closed) {
                        fail("the server closed the hub's connection");
                        break;
                    }
                } else if (type == kIncoming && payload.size() >= 8) {
                    u64 ticket = 0;
                    for (int i = 0; i < 8; ++i) {
                        ticket |= static_cast<u64>(payload[i]) << (8 * i);
                    }
                    std::lock_guard lock(mu);
                    Detach(std::thread([self, server, ticket, gen] { self->PairLoop(server, ticket, gen); }));
                } else if (type == kError) {
                    break;
                }
                const auto now = Clock::now();
                bool update = false;
                {
                    std::lock_guard lock(mu);
                    if (host_version != sent_version && now - last_sent > 500ms) {
                        sent_version = host_version;
                        PackInfo(host_hub, link_protocol, &info);
                        update = true;
                    }
                }
                if (update) {
                    last_sent = now;
                    if (!SendControl(h, kUpdate, std::vector<u8>(info.begin(), info.end()), stop)) {
                        break;
                    }
                } else if (now - last_ping > 10s) {
                    last_ping = now;
                    if (!SendControl(h, kPing, {}, stop)) {
                        break;
                    }
                }
            }
        }
        sock::Close(h);
        {
            std::lock_guard lock(mu);
            if (ServerState* s = StateOf(server)) {
                s->hosting = false;
            }
            Touch();
        }
        if (!active() || !Sleep(3s)) {
            return;
        }
    }
}

// The host's side of one player: ATTACH with the ticket, the link's Hello, then the frames.
void Medium::Core::PairLoop(Endpoint server, u64 ticket, u64 gen) {
    std::string error;
    sock::Handle h = sock::Connect(server, opt.connect_timeout_ms, stop, &error);
    if (h == sock::kBad) {
        return;
    }
    std::vector<u8> buffer, payload;
    u8 type = 0;
    if (!SendControl(h, kAttach, Request(link_protocol, room, U64(ticket)), stop) ||
        ReadControl(h, buffer, kMaxFrame, opt.handshake_timeout_ms, stop, &type, &payload) != Read::Ok ||
        type != kSpliced) {
        sock::Close(h);
        return;
    }
    auto link = std::make_shared<Link>(h, stop, std::move(buffer));  // (the peer's Hello may be behind SPLICED already)
    link->Queue(MakeLinkFrame(kLinkHello, EncodeHello({kLinkVersion, link_protocol, opt.mac})));
    u8 kind = 0;
    std::vector<u8> body;
    if (link->Read(opt.handshake_timeout_ms, &kind, &body) != Link::Frame::Ok || kind != kLinkHello) {
        MH4N_LOG(Info, kLog, "a player's link did not say hello");
        return;
    }
    const auto hello = DecodeHello(body);
    const auto reject = [&](u8 reason) {
        MH4N_LOG(Info, kLog, "a player was refused (reason {})", static_cast<int>(reason));
        link->Queue(MakeLinkFrame(kLinkReject, {reason}));
        link->CloseAfterFlush();
        std::this_thread::sleep_for(200ms);
    };
    if (!hello) {
        return;
    }
    if (hello->version != kLinkVersion) {
        return reject(kRejectVersion);
    }
    if (hello->link_protocol != link_protocol) {
        return reject(kRejectFamily);
    }
    auto pair = std::make_shared<Pair>();
    pair->link = link;
    pair->mac = hello->mac;
    {
        std::lock_guard lock(mu);
        if (!hosting || host_gen != gen) {
            return reject(kRejectNotHosting);
        }
        const bool taken = hello->mac == opt.mac || hello->mac == rooms::kBroadcastMac || (hello->mac[0] & 1) != 0 ||
                           std::any_of(pairs.begin(), pairs.end(), [&](const auto& p) { return p->mac == hello->mac; });
        if (taken) {
            return reject(kRejectMac);
        }
        if (static_cast<int>(pairs.size()) >= opt.max_players - 1 || static_cast<int>(pairs.size()) >= host_hub.max_nodes - 1) {
            return reject(kRejectFull);
        }
        pairs.push_back(pair);
        Touch();
    }
    MH4N_LOG(Info, kLog, "a player joined the hub ({} on {})", MacText(pair->mac), server.Text());
    for (;;) {
        const auto r = link->Read(30000, &kind, &body);
        if (r != Link::Frame::Ok) {
            break;
        }
        if (kind == kLinkBye) {
            break;
        }
        if (kind != kLinkWifi) {
            continue;
        }
        auto p = DecodeWifi(body);
        if (!p || p->transmitter != pair->mac) {
            continue;  // (a frame that is not from this player's console)
        }
        if (p->type == WifiPacket::Type::Deauthentication) {
            pair->got_deauth = true;
        }
        RouteHostFrame(*p, pair);
    }
    link->Close();
    {
        std::lock_guard lock(mu);
        pairs.erase(std::remove(pairs.begin(), pairs.end(), pair), pairs.end());
        Touch();
    }
    MH4N_LOG(Info, kLog, "a player left the hub ({})", MacText(pair->mac));
    if (!pair->got_deauth && !stop) {
        // The game's UDS service learns of a console that left by a deauthentication frame.
        WifiPacket p;
        p.type = WifiPacket::Type::Deauthentication;
        p.transmitter = pair->mac;
        p.destination = opt.mac;
        p.channel = host_hub.channel;
        Out(p);
    }
}

// A frame from a player's link, on its way: to this console (the game), to another link, or to every other one.
void Medium::Core::RouteHostFrame(const WifiPacket& p, const std::shared_ptr<Pair>& from) {
    const bool broadcast = p.destination == rooms::kBroadcastMac;
    bool mine = broadcast || p.destination == opt.mac;
    std::vector<std::shared_ptr<Pair>> to;
    {
        std::lock_guard lock(mu);
        for (const auto& other : pairs) {
            if (other != from && (broadcast || other->mac == p.destination)) {
                to.push_back(other);
            }
        }
        mine = mine && hosting;
    }
    if (!to.empty()) {
        const std::vector<u8> bytes = MakeLinkFrame(kLinkWifi, EncodeWifi(p));
        for (const auto& other : to) {
            other->link->Queue(bytes);
        }
    }
    if (mine) {
        Out(p);
    }
}

// ---- playing on another console's hub --------------------------------------------------------------------------

void Medium::Core::JoinLoop(std::vector<Entry> candidates) {
    std::string why = "no hub answered";
    for (const Entry& entry : candidates) {
        if (stop) {
            break;
        }
        std::string error;
        sock::Handle h = sock::Connect(entry.server, opt.connect_timeout_ms, stop, &error);
        if (h == sock::kBad) {
            why = error;
            continue;
        }
        std::vector<u8> buffer, payload;
        u8 type = 0;
        const bool sent = SendControl(h, kJoin, Request(link_protocol, room, U64(entry.id)), stop);
        const Read r = sent ? ReadControl(h, buffer, kMaxFrame, opt.handshake_timeout_ms + 2000, stop, &type, &payload)
                            : Read::Closed;
        if (r != Read::Ok || type != kSpliced) {
            if (r == Read::Ok && type == kError && payload.size() >= 4) {
                why = ServerErrorText(static_cast<u32>(payload[0]) | (static_cast<u32>(payload[1]) << 8));
            } else if (r == Read::Timeout) {
                why = "the host did not answer";
            }
            sock::Close(h);
            continue;
        }
        auto link = std::make_shared<Link>(h, stop, std::move(buffer));
        link->Queue(MakeLinkFrame(kLinkHello, EncodeHello({kLinkVersion, link_protocol, opt.mac})));
        u8 kind = 0;
        std::vector<u8> body;
        if (link->Read(opt.handshake_timeout_ms, &kind, &body) != Link::Frame::Ok || kind != kLinkHello) {
            why = kind == kLinkReject ? "the host refused" : "the host did not answer";
            continue;
        }
        const auto hello = DecodeHello(body);
        if (!hello || hello->link_protocol != link_protocol || hello->mac != entry.hub.mac) {
            why = "that is not the hub that was listed";
            continue;
        }
        std::deque<std::vector<u8>> queued;
        {
            std::lock_guard lock(mu);
            if (stop) {
                break;
            }
            client_link = link;
            client_host = entry.hub.mac;
            queued.swap(join_queue);
            Touch();
        }
        MH4N_LOG(Info, kLog, "joined the hub of {} through {}", MacText(entry.hub.mac), entry.server.Text());
        for (auto& q : queued) {
            link->Queue(std::move(q));
        }
        bool refused = false;
        for (;;) {
            const auto got = link->Read(30000, &kind, &body);
            if (got != Link::Frame::Ok) {
                break;
            }
            if (kind == kLinkReject) {
                refused = true;
                break;
            }
            if (kind == kLinkBye) {
                break;
            }
            if (kind != kLinkWifi) {
                continue;
            }
            auto p = DecodeWifi(body);
            if (p && (p->destination == opt.mac || p->destination == rooms::kBroadcastMac)) {
                Out(*p);
            }
        }
        link->Close();
        {
            std::lock_guard lock(mu);
            if (client_link == link) {
                client_link.reset();
            }
            joining = false;
            join_queue.clear();
            if (refused) {
                const u8 reason = body.empty() ? 0 : body[0];
                SetProblem(reason == kRejectFull ? "the hub is full"
                           : reason == kRejectMac ? "the hub already has a console with that address"
                           : reason == kRejectFamily ? "that hub is for another game"
                                                     : "the host refused");
            } else {
                Touch();
            }
        }
        return;
    }
    std::lock_guard lock(mu);
    joining = false;
    join_queue.clear();
    if (!stop) {
        MH4N_LOG(Warning, kLog, "could not join the hub: {}", why);
        SetProblem("could not join the hub: " + why);
    }
}

// ---- the UDS service's frames -----------------------------------------------------------------------------------

void Medium::Core::Send(WifiPacket p) {
    p.transmitter = opt.mac;
    if (p.type == WifiPacket::Type::Beacon) {
        HostBeacon(p);  // (only the console that hosts a network sends beacons)
        return;
    }
    const auto self = shared_from_this();
    std::unique_lock lock(mu);
    if (stop) {
        return;
    }
    if (hosting) {
        std::vector<std::shared_ptr<Pair>> to;
        for (const auto& pair : pairs) {
            if (p.destination == rooms::kBroadcastMac || pair->mac == p.destination) {
                to.push_back(pair);
            }
        }
        lock.unlock();
        if (!to.empty()) {
            const std::vector<u8> bytes = MakeLinkFrame(kLinkWifi, EncodeWifi(p));
            for (const auto& pair : to) {
                pair->link->Queue(bytes);
            }
        }
        return;
    }
    const std::vector<u8> bytes = MakeLinkFrame(kLinkWifi, EncodeWifi(p));
    if (client_link) {
        const auto link = client_link;
        if (p.type == WifiPacket::Type::Deauthentication) {
            client_link.reset();  // (the game leaves the network: the link goes once this frame is sent)
            lock.unlock();
            link->Queue(bytes);
            link->Queue(MakeLinkFrame(kLinkBye, {}));
            link->CloseAfterFlush();
        } else {
            lock.unlock();
            link->Queue(bytes);
        }
        return;
    }
    if (joining) {
        if (join_queue.size() < 8) {
            join_queue.push_back(bytes);
        }
        return;
    }
    if (p.type != WifiPacket::Type::Authentication) {
        return;  // (nothing to send it to)
    }
    // The first frame to a listed host's MAC address: join its hub (every server that lists it, the freshest first).
    std::vector<Entry> candidates;
    for (const Entry& e : entries) {
        if (e.hub.mac == p.destination) {
            candidates.push_back(e);
        }
    }
    if (candidates.empty()) {
        SetProblem("that hub is not listed any more");
        return;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Entry& a, const Entry& b) { return a.seen > b.seen; });
    joining = true;
    join_queue.clear();
    join_queue.push_back(bytes);
    problem.clear();
    Touch();
    Detach(std::thread([self, candidates] { self->JoinLoop(candidates); }));
}

// ---- the public class --------------------------------------------------------------------------------------------------

Medium::Medium(MediumOptions options, Deliver deliver) : core_(std::make_shared<Core>()) {
    core_->opt = std::move(options);
    core_->deliver = std::move(deliver);
    core_->Start();
}

Medium::~Medium() {
    core_->Shutdown();
}

void Medium::Send(const rooms::WifiPacket& packet) {
    core_->Send(packet);
}

rooms::MacAddress Medium::Mac() const {
    return core_->opt.mac;
}

MediumStatus Medium::Status() const {
    std::lock_guard lock(core_->mu);
    MediumStatus st;
    st.hosting = core_->hosting;
    st.joining = core_->joining;
    st.linked = core_->client_link != nullptr;
    st.players = static_cast<int>(core_->pairs.size());
    st.servers = core_->servers;
    st.problem = core_->problem;
    st.revision = core_->revision;
    const auto now = Clock::now();
    const auto fresh = std::chrono::milliseconds(core_->opt.list_every_ms * 3 + core_->opt.connect_timeout_ms);
    for (const auto& e : core_->entries) {
        if (now - e.seen > fresh || e.hub.mac == core_->opt.mac) {
            continue;
        }
        HubView v;
        v.server = e.server;
        v.id = e.id;
        v.mac = e.hub.mac;
        v.nodes = e.hub.nodes;
        v.max_nodes = e.hub.max_nodes;
        v.comm_id = e.hub.comm_id;
        st.hubs.push_back(v);
    }
    return st;
}

std::vector<ServerCheck> CheckServers(const std::vector<Endpoint>& servers, Family family, const std::string& key,
                                      int timeout_ms) {
    std::vector<ServerCheck> out;
    const std::atomic<bool> never{false};
    const auto room = RoomOfKey(key);
    for (const Endpoint& server : servers) {
        ServerCheck c;
        c.server = server;
        std::vector<ListedHub> hubs;
        const auto start = Clock::now();
        c.ok = ListOne(server, LinkProtocol(family), room, timeout_ms, never, &hubs, &c.detail);
        c.ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
        if (c.ok) {
            for (const ListedHub& h : hubs) {
                if (UnpackInfo(h.info, LinkProtocol(family))) {
                    ++c.hubs;
                }
            }
        }
        out.push_back(std::move(c));
    }
    return out;
}

}  // namespace mh4n::hub
