# Hub servers for MH4U / MH4G local play

The game's local play can go through an MHGURecomp hub server (gu-native's `server/`) instead of a network room.
Gear, **Multiplayer**, **Hub servers**, **Use hub servers**. The server needs no change.

## Who sees whom

A server lists hubs by *link protocol* and *matchmaking key*. This port uses one link protocol of its own, `0x4D01`, for
every region: **USA, EUR, JPN and KOR copies all see each other's hubs** (`hub::kCrossRegion`). MHGU uses 2 and 3 and MHXX
players reach MHGU hubs through those, so they never see these hubs and these never see theirs.

The two families the port had before (MH4U `0x4D01` for USA and EUR, MH4G `0x4D02` for JPN and KOR), the program id table
(`hub::FamilyOfProgram`) and the `[Network] HubFamily` setting are still in the code, but unused while `kCrossRegion` is
true; the app's "Game" row is hidden (`kHubFamilyPicker` in `settings_panel.cpp`). Set `kCrossRegion = false` to tell the
regions apart again. That the games themselves accept each other's sessions is up to the games.

## What goes over the server

- **Listing.** The first beacon the game sends makes a hub: a HOST on every configured server, whose NetworkInfo (0x480
  bytes) holds the beacon (layout in `mh4n/hub.h`). The server only reads five bytes of it (SSID length 0, network type 2,
  node limit and count, advertise size 0); `PackInfo` keeps them valid. The listing is refreshed (UPDATE) when the beacon
  changes.
- **Searching.** Each server is asked (LIST) every 3 s while the console is neither hosting nor playing; the hubs found are
  delivered to the game as the beacons of their host's console, every 300 ms.
- **Joining.** The game's first authentication frame to a listed host's MAC opens a JOIN; the host gets INCOMING and opens
  an ATTACH; the server splices the two TCP connections.
- **The link** (after SPLICED): frames of `u16 size, u8 kind, payload`. Both ends send `Hello` (`"M4HL"`, u16 version,
  u16 link protocol, MAC[6]) first; the host may answer with `Reject`. `Wifi` frames carry the console's frame
  (`u8 type, u8 channel, transmitter[6], destination[6], body`): the same frame a network room carries. The host relays
  between its links as a room does (a broadcast reaches every other link, a unicast its link). A link that drops is
  reported to the host's game as a deauthentication from that console. `Ping` every 5 s; 30 s of silence ends a link.
- All of it is TCP: no UDP relay and no direct connections yet (the server's UDP path and hole punching are unused).

A console's MAC on the medium is derived from its console ID. The matchmaking key never leaves the phone: the server gets
the first 16 bytes of its SHA-256.

## Settings (`mh4u-app.ini`, `[Network]`)

`HubServer1` (the first server: empty is the default, `http://meoli.viewdns.net`; `off` is none) and `HubServer2` (the second
server: empty is none). A server is a name or an IP, with or without `http://`, and `:port` if it is not 39510. They are
the two fields under **Hub servers** in the app. An older `HubServers` list (servers separated by spaces) gives its first
word to Server 1 and the rest to Server 2. `HubKey` is the matchmaking key. Everyone who plays together uses the same servers
and key.

## Crossplay: hubs and a room together

With `[Network] Crossplay = 1` (Settings, Multiplayer, **Crossplay**) the client keeps hub servers and a Citra/Azahar room
at the same time (`HubRequest::keep_room`, `JoinRequest::keep_hubs` in `rooms.h`). A room joined does not end the hubs (and the
hubs do not end a room); the game's frames go to both, so a hub the game creates is in the room too, and its beacons reach
both kinds of player. Two things are shared: the game's MAC address (the room is asked for the hubs' one, as
`preferred_mac`), and the console ID. Each side stays what it was: hub players and room players reach the host, not each
other. With it off (the default) a room replaces the hubs and the hubs replace a room.

## Tests

`mh4n/native/tests/hub_test.cpp` runs two or three room clients through `TestServer`, a port of the hub server's LIST /
HOST / JOIN / ATTACH and splice. It is a port, not the real server: also try the real one (Go 1.22,
`cd server && go build && ./mhgurecomp-hubserver -addr :39510 -v`, then `HubServers = 127.0.0.1`).
