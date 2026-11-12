# Hub servers for MH4U / MH4G local play

The game's local play can go through an MHGURecomp hub server (gu-native's `server/`) instead of a network room.
Gear, **Multiplayer**, **Hub servers**, **Use hub servers**. The server needs no change.

## Who sees whom

A server lists hubs by *link protocol* and *matchmaking key*. This port uses link protocols of its own:

| Family | Games | Link protocol |
|---|---|---|
| MH4U | USA, EUR | `0x4D01` |
| MH4G | JPN, KOR | `0x4D02` |

MHGU uses 2 and 3 and MHXX players reach MHGU hubs through those, so they never see these hubs and these never see theirs.
"The running game" picks the family from the game's program id (`hub::FamilyOfProgram`): USA and EUR MH4U, and the
Japanese MH4G. A game that is in no family (another game's id) does not use hubs. **The Korean MH4G program id is not in
the table**: choose "MH4G" by hand for it (`[Network] HubFamily = 4G`), or add its id to `FamilyOfProgram`.

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

`HubServers` (names or IPs, `:port` if not 39510, separated by spaces; empty = `hunterhubid-gu-hub.duckdns.org` and
`morerokk-mhgurecomp-ldnserver.duckdns.org`, the ones MHGU uses), `HubKey`, `HubFamily` (`auto`, `4U`, `4G`).
Everyone who plays together uses the same servers, key and family.

## Tests

`mh4n/native/tests/hub_test.cpp` runs two or three room clients through `TestServer`, a port of the hub server's LIST /
HOST / JOIN / ATTACH and splice. It is a port, not the real server: also try the real one (Go 1.22,
`cd server && go build && ./mhgurecomp-hubserver -addr :39510 -v`, then `HubServers = 127.0.0.1`).
