# Fork Roadmap

This roadmap orders the work for this fork (ateterycz/GeneralsGameCode) toward the goals in the README: fix critical
bugs, modernize the codebase and improve the engine. Multiplayer must keep working both peer-to-peer (LAN and direct
connect) and on the existing online infrastructure (community servers that emulate GameSpy, such as C&C:Online),
alongside retail Zero Hour 1.04 and Generals 1.08 clients.

It is based on a review of the code on 29/10/2026 and on the open upstream issues. Items are numbered by area (N, L, M,
C, O, D, K, T, P, X). Upstream issues are linked in full.

## Guardrails

- **Retail compatibility.** A change that alters the simulation, the CRC, the lobby protocol or the wire format must be
  gated (`RETAIL_COMPATIBLE_CRC`, `RETAIL_COMPATIBLE_NETWORKING` and similar). Otherwise fork players can't play with
  retail players online.
- **Verification.** Every fix comes with a check that would catch its regression:
  - a unit test that fails before the fix and passes after it, or
  - a headless replay run, or
  - a documented manual repro when neither is possible.
- **Simulation changes.** A change that could touch the simulation is checked with the VC6 replay test in `TESTING.md`
  before merging. VS2022 builds always mismatch retail replays at frame 110, so a VS2022 run only proves that a map
  loads.
- **Upstream.** The fork stays close to upstream main. Work that an open upstream pull request already covers is
  reviewed and brought in, not duplicated. Anything sent upstream must follow `AI_POLICY.md`: a human who understands
  every line submits it, and the use of AI is disclosed. A branch for upstream must start from `upstream/main`, so
  that it doesn't carry fork-only commits such as the GeneralsGamePatch submodule.

## Phase 1: Harden every untrusted input (first batch done on local main)

In peer-to-peer play every other player, and every shared map or replay, is untrusted input. These fixes are small, do
not affect compatibility (an honest retail client never sends the rejected values) and can be unit tested, so they come
first.

**In-game network commands**

| Item | Fix | Trigger |
|---|---|---|
| N1 | Disconnect vote slot is used as an array index without a bounds check (out-of-bounds write) | Any peer |
| N2 | Player leave trusts the leaving player ID: out-of-bounds read, and one player can make another player leave | Any peer |
| N3 | Run-ahead command: frame rate 0 divides by zero, and the run-ahead value is not clamped | Any peer |
| N4 | `NetPacket` keeps a packet length larger than its buffer in retail-compatible builds | Any peer |

**Lobby and NAT**

| Item | Fix | Trigger |
|---|---|---|
| L1 | A LAN game announce can delete (use-after-free) or overwrite the current game | Anyone on the LAN |
| L2 | LAN packets have no length check or NUL termination, and join accepts are not checked against the host | Anyone on the LAN |
| L3 | NAT negotiation messages index arrays before the bounds check (off by one) and scan past the string end | Staging room member |
| L4 | A CRC interval of 0 divides by zero on every client and in replays; negative values disable desync detection | Host, crafted replay |
| L5 | An empty or failed MOTD reply writes before its buffer | Server |
| L6 | Direct connect leaves the IP octets uninitialized for malformed input | Local |

**Maps and map chunks** ([upstream issue 3319](https://github.com/TheSuperHackers/GeneralsGameCode/issues/3319))

| Item | Fix | Trigger |
|---|---|---|
| M1 | Chunk reads past the end of a chunk are only checked by debug asserts (root fix for the map reader) | Map from a peer |
| M2 | A script's parameter count overflows `m_parms[MAX_PARMS]` | Map or .scb |
| M3 | Height map texture class, blend tile and cliff counts overflow fixed arrays | Map |
| M4 | The time of day and weather index lighting arrays without a check | Map |
| M5 | The polygon trigger point count overflows | Map |
| M6 | The side list build list and team counts have no ceiling | Map |

**Compression, replays and map.ini**

| Item | Fix | Trigger |
|---|---|---|
| C1 | The RefPack, BTREE and HUFF decoders have no output bound (heap overflow) | Map, transferred .tga |
| C2 | Replay header strings write one element past a stack buffer | Downloaded replay |
| C3 | Long command set, module and model names from `map.ini` overflow stack buffers ([upstream issue 1518](https://github.com/TheSuperHackers/GeneralsGameCode/issues/1518)) | Map from a peer |

Already done on main: wrapped command reassembly hardening, the simulation math CRC in the EXE CRC, the script engine
state in the CRC, logic random for topple bursts, stack dump fixes and the session log. N1 to N4, L1 to L6, M1 to M5
and C1 to C3 are done as well (M6 is covered by M1's chunk bounds), each with unit tests.

**Follow-ups found while fixing Phase 1**

| Item | Work |
|---|---|
| N5 | Command player IDs aren't bound to the connection they arrive on, so a peer can claim another player's ID |
| L7 | `handleJoinDeny` has no sender check, NAT messages aren't tied to their sender, and `configHeadCallback` has the same zero-length write as the MOTD callback |
| L8 | The fork's `RequestGameLeave` fills a different field than retail does (since commit 3b78b63d4), so lobbies only drop a departed host's game after the timeout |
| M7 | Catch the corrupt-map error where maps load (`MapCache::addMap`, game start), so that a bad map is skipped instead of ending the game |
| M8 | Draw-time tile, blend and cliff indices from the map are read without bounds checks |
| M9 | Out-of-range script action and condition types survive when the parameter count matches, and Generals lacks the template parameter count check |
| C4 | Audit the LZHL decompressor, and fix `getMaxCompressedSize`, which is too small for small incompressible input |

**Disclosure.** Many of these bugs can be triggered remotely, and they also exist in the client that most online players
use today (see Phase 2). `SECURITY.md` asks for vulnerabilities to be reported privately to TheSuperHackers before they
are made public. Report them that way before pushing these commits or this roadmap to the public fork.

## Phase 2: Online on the existing infrastructure

The online infrastructure for Zero Hour has moved:

- **C&C:Online** stopped serving Generals and Zero Hour in 2026. Its FAQ sends Zero Hour players to Generals Online.
- **[Generals Online](https://www.playgenerals.online/)** is where Zero Hour is played online now: about 1400 players
  were online on 29/10/2026.
  - It is not GameSpy based. It has its own client, a public GPL fork of TheSuperHackers/GeneralsGameCode
    ([GeneralsOnlineDevelopmentTeam/GameClient](https://github.com/GeneralsOnlineDevelopmentTeam/GameClient)).
  - The client talks to REST services
    ([Services](https://github.com/GeneralsOnlineDevelopmentTeam/Services), AGPL, also forked by TheSuperHackers).
  - It does its own NAT traversal (UPnP, NAT-PMP, libplum) and has an anti-cheat plugin.
  - Its client still has the same lockstep network code as this fork, including N1 and N3.

A client built from this repo can't go online through GameSpy either:

- The GameSpy SDK's `peerInitialize` returns null until an availability check has run, and the game never runs it.
- The game code still hard codes `servserv.generals.ea.com`, `peerchat.gamespy.com` and other dead hosts.

| Item | Work | Verify |
|---|---|---|
| O1 | Get the Phase 1 fixes to the players who are online today: report them privately to TheSuperHackers, whose code Generals Online merges, and to the Generals Online team. A human has to do this; `AI_POLICY.md` doesn't let agents submit. | Fixes land upstream or in Generals Online |
| O2 | Decide the fork's own online strategy (see below) | Decision |
| O3 | For GameSpy emulating servers, including self-hosted ones: bring in [upstream PR 3166](https://github.com/TheSuperHackers/GeneralsGameCode/pull/3166) with fixes. It runs the availability check, adds a configurable server host, replaces the SNMP lookup that frees stack memory (the likely cause of [upstream issue 91](https://github.com/TheSuperHackers/GeneralsGameCode/issues/91)) and fixes the host name lifetime. It still needs these fixes: a servserv 404 must not block going online, the host name must be validated with `inet_addr` instead of "starts with a digit", and host names longer than 63 characters must be rejected. | Build, unit tests, log in against a GameSpy emulator |

**Strategy options for O2**

- **(a) Peer-to-peer plus GameSpy emulators.** The fork plays over LAN, direct connect (Phase 3) and any GameSpy
  emulating server (O3). Cheap, and keeps retail compatibility.
- **(b) Generals Online compatible client.** Bring Generals Online's network layer into the fork, which the GPL allows,
  and connect to their services. This only makes sense with the Generals Online team's agreement, because their
  service authenticates clients and runs anti-cheat.
- **(c) Self-hosted Generals Online services.** Run the AGPL services for a private community, with (b)'s client
  layer.

Recommendation: (a) now, together with O1, so that the fixes reach the live player base through upstream. Start (b)
only after talking with the Generals Online team.

## Phase 3: Peer-to-peer connectivity

| Item | Work | Compat |
|---|---|---|
| P1 | Direct connect sends the host's private adapter IP to joiners, so it only works on a LAN, a VPN or a forwarded public IP. Let the joiner use the address it connected to. | Lobby only, needs review |
| P2 | LAN and direct connect use fixed UDP ports 8086 and 8088, so only one player per public IP. Make the game port configurable. | Gated, retail peers expect 8088 |
| P3 | Same-NAT peers online depend on an internal IP lookup that returns 0 on community servers. This may cause [upstream issue 68](https://github.com/TheSuperHackers/GeneralsGameCode/issues/68). It's fixed by O3's lookup change; retest after O3. | None |
| P4 | IPv6 ([upstream issue 64](https://github.com/TheSuperHackers/GeneralsGameCode/issues/64)). The IPs are written as hex IPv4 in the options string, so this is a lobby protocol change for non-retail builds only. | Breaks retail |

## Phase 4: Desync detection and diagnostics

[Upstream issue 289](https://github.com/TheSuperHackers/GeneralsGameCode/issues/289) is critical. The session log
already records mismatches, but not what differed.

| Item | Work |
|---|---|
| D1 | On a mismatch, dump the CRC of each subsystem (objects, players, AI, scripts, partitions, random seeds) for the mismatch frame to the session log, so that two players' logs can be compared. Also log the INI CRC at game start: players with and without the community patch differ only in it. |
| D2 | Add the remaining script engine state to the CRC (it's only partly covered now) and add the math CRC to the VC6 lobby |
| D3 | `map.ini` is not in the INI CRC, which is computed once at startup, and the community patch edits the map.ini of 57 official maps. Recheck the map.ini mismatches ([upstream issue 2016](https://github.com/TheSuperHackers/GeneralsGameCode/issues/2016) and its related issues) once D1 can show what differs. |
| D4 | W3D models are in no CRC ([upstream issue 84](https://github.com/TheSuperHackers/GeneralsGameCode/issues/84)). The GeneralsGamePatch submodule has 55 bug repro maps for these cases (for example `Maps/Any/!TestGermanTerroristMismatch`). |

## Phase 5: Engine crashes

| Item | Work | Verify |
|---|---|---|
| K1 | The sorting renderer still overflows on the vertex side above 65535 sorted vertices ([upstream issue 2007](https://github.com/TheSuperHackers/GeneralsGameCode/issues/2007)). Flush the pool before it would go past the limit. | Manual repro from the issue |
| K2 | `normalizeAngle` never returns for infinite or huge input. Return 0 for non-finite input. | Unit test |
| K3 | Terrain tracks write through a null vertex pointer when a buffer lock fails ([upstream issue 2827](https://github.com/TheSuperHackers/GeneralsGameCode/issues/2827)) | Fault injection |
| K4 | The audio file cache keeps a dangling event info pointer after a map reset. Rebase onto upstream PR 3623 first. | ASAN |
| K5 | A weapon is freed while its own fire call is still running, when a unit gets a veteran weapon set from a kill ([upstream issue 941](https://github.com/TheSuperHackers/GeneralsGameCode/issues/941)). The fix changes the simulation, so it must be gated. | ASAN replay run, VC6 replays |
| K6 | The WorldBuilder passable area overlay reads out of bounds ([upstream issue 254](https://github.com/TheSuperHackers/GeneralsGameCode/issues/254)) | Manual |
| K7 | [Upstream issue 2263](https://github.com/TheSuperHackers/GeneralsGameCode/issues/2263) looks fixed by an earlier upstream change. Retest it. | Manual |

## Phase 6: Modernization with safety nets first

Golden tests come before refactors, so that a refactor or a new toolchain (MinGW, x64, Linux) that changes simulation
results fails a test.

| Item | Work |
|---|---|
| T1 | Golden tests for the game logic random generator, XferCRC, the NameKeyGenerator and UnicodeString |
| T2 | `static_assert`s for type sizes where they matter (`uint32` in `w3d_file.h`, `WideChar` in GameText and LANAPI, `sizeof(LANMessage)`). These turn the 64-bit Linux and macOS blockers into compile errors at their location. |
| T3 | Further `strcpy` and `sprintf` batches ([upstream issue 1518](https://github.com/TheSuperHackers/GeneralsGameCode/issues/1518)), ordered by whether untrusted input reaches them |
| T4 | A Linux CI job that builds and runs the Dependencies tests |

## Phase 7: Performance

| Item | Work | Gate |
|---|---|---|
| X1 | Look up command sets by name through an index instead of a list of string compares. The lookup is called from AI code too. | None, same pointer |
| X2 | The special power shortcut bar scans all objects twice per button every frame | None, client only |
| X3 | Pathfinder open list ([upstream issue 100](https://github.com/TheSuperHackers/GeneralsGameCode/issues/100)). Contribute a measurement harness to the upstream skip list work instead of a competing algorithm. | `RETAIL_COMPATIBLE_PATHFINDING` |

## Tooling and CI for the fork

- GitHub Actions has never run on the fork. The workflows must be enabled in the fork's Actions tab.
- The replay check needs upstream's storage secrets, so it can't run in the fork. Until a self-hosted runner with game
  data exists, run the VC6 replay check from `TESTING.md` locally before merging simulation changes.
- Add an ASAN build that runs the unit tests, and an advisory clang-tidy job on changed files.

## Open questions

- Map transfer overwrites an existing file with the same name. This may be intended, so that a client with an older
  version of a map gets the host's version. Decide before changing it.
- The duplicate start position check for AI slots compares a slot with itself (`GameInfo.cpp`). Retail hosts may rely
  on this, so only change it behind a gate.
