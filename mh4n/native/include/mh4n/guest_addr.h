// Where the app's own fixed addresses are in the code the game runs.
//
// The port reads and calls the game at the USA v1.3.0 update's addresses (the frame counter, the
// quest HUD, the panel manager, the functions it calls...). A code mod that replaces the whole
// code.bin (another build of the game) has the same game at other addresses: functions moved, and
// parts of the data moved by a few bytes. The library carries, for each address of
// config/guest_addrs.txt, where it is in that code (tools/guest_map.py); Map() gives it. An address
// with no pair (the update's own code, or an address not on the list) is returned as it is.
#pragma once

#include <algorithm>
#include <cstdint>

namespace mh4n::guest {

struct Table {
    const std::uint32_t* from = nullptr;  // sorted
    const std::uint32_t* to = nullptr;
    std::uint32_t count = 0;
};

inline Table g_table;  // set once, when the code library is loaded, before any guest code runs

inline void Set(const std::uint32_t* from, const std::uint32_t* to, std::uint32_t count) {
    g_table = Table{from, to, count};
}

inline std::uint32_t Map(std::uint32_t stock) {
    const std::uint32_t* begin = g_table.from;
    const std::uint32_t* end = begin + g_table.count;
    const std::uint32_t* it = std::lower_bound(begin, end, stock);
    return (it != end && *it == stock) ? g_table.to[it - begin] : stock;
}

// The other way: the update's address of one the running code has, for a value read from the game's memory that
// is compared with the update's (a vtable: `switch (Stock(rd32(ctx, object)))` with the update's as the cases).
// One with no pair is returned as it is.
inline std::uint32_t Stock(std::uint32_t running) {
    for (std::uint32_t i = 0; i < g_table.count; ++i) {
        if (g_table.to[i] == running) {
            return g_table.from[i];
        }
    }
    return running;
}

// A fixed address of the update's, looked up where it is used: `constexpr Addr QUEST_HUD{0x0108260c};`
// reads as the u32 it stands for in the code the game runs.
struct Addr {
    std::uint32_t stock;
    operator std::uint32_t() const { return Map(stock); }
};

}  // namespace mh4n::guest
