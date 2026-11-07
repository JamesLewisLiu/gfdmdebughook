# Native Debug parameters

This page covers XG / XGCN and the historical V-series tables. See
[XG2 / XG3](xg23-debug.md) for their additional identities and consumers.

This plugin is an independent Windows x86 implementation. It has its own state,
input and renderer; it neither changes Spice source nor uses the Spice SDK.
The tables and effects below come from static examination of the original
modules, not from game testing of this plugin.

## Scope of the hook

The native interface is `char __cdecl gd_dip_get_debug_dip_param(int index)`.
Consumers use the low-byte return value. The plugin overrides the indexed
import in **game.dll**, with a profile-specific mapping. It does not install a
process-wide `sys_debug_dip_get_param(name)` hook. Another module that reads a
parameter by name or uses a separately cached pointer may therefore observe a
different value. An existing loader may also override native parameter values.

These switches do not restore the stripped retail Debug menu. The inspected XG
modules register empty Debug update/display callbacks; the separate native
select/decide input bits are not Auto Play or Neverend switches. Hardware DIP
switches are a different interface.

## Native index mapping

All listed native defaults are zero. A dash means that the parameter is absent
from that version's inspected table. In particular, the last three XG indices
must not be copied directly into V-series tables.

| Parameter | XG / XGCN | V4 | V5–V8 |
|---|---:|---:|---:|
| `gameinfo` | 0 | 0 | 0 |
| `neverend` | 1 | 1 | 1 |
| `autoplay` | 2 | 2 | 2 |
| `all_music` | 3 | 3 | 3 |
| `auto_select` | 4 | 4 | 4 |
| `netinfo` | 5 | 5 | 5 |
| `easyplay` | 6 | 6 | 6 |
| `lan_vs` | 7 | 7 | 7 |
| `help_cpu` | 8 | 8 | 8 |
| `packet_cnt` | 9 | 9 | 9 |
| `develop` | 10 | 10 | 10 |
| `user_offset` | 11 | 11 | 11 |
| `movie_only` | 12 | — | — |
| `scrshot` | 13 | 12 | 12 |
| `serial` | 14 | — | 13 |

V-series mappings are based on the inspected historical binaries. They do not
certify every V-series consumer or graphics backend. The current independent
D3D9 panel covers the XG family; V-series OpenGL integration needs separate work.

## Effects established in XG code

The following observations apply to the inspected XG/XGCN binaries. Function
locations are **RVAs in game.dll**, provided for audit only; the implementation
does not patch those addresses.

| Parameter | Observed behavior and limits |
|---|---|
| `gameinfo` | Enables native diagnostic output, including Session life/counters and CPU/status text. It also changes native flow: a Session-state branch returns early when enabled. It is not a display-only option. |
| `neverend` | Skips the menu timer decrement normally performed every 80 updates. Other flow, stage/counter and performance paths also read it; its scope extends beyond a single menu. |
| `autoplay` | Selects the native chart-event automatic judgment path and participates in judgment/statistics processing. Complete GF/DM note types, wailing/pedals and result/server flags still require play testing. |
| `all_music` | Affects native song-table searches/filtering. It also reaches judgment processing: a consumer changes the judgment category before counters, combo/life and related updates. Treat it as a native Debug mode, not a harmless song-unlock option. Full song availability is unverified. |
| `auto_select` | Advances the native song index and cycles part/difficulty selections. In a synchronization timeout branch it allows the counter to advance even with Neverend enabled. A complete unattended play loop is unverified. |
| `lan_vs` | Selects an AVS local-network address and fixed port 5700 for Session data. JP otherwise queries EA3 for the address/port; the corresponding CN branch uses zero. This does not establish working Session discovery, synchronization or online play. |
| `help_cpu` | Allows a native CPU-assistance state/update path to continue where it would otherwise return, and changes a separate input-assistance branch during play. It must not be assumed equivalent to Auto Play. |
| `user_offset` | With both `gameinfo` and this parameter enabled, a timing consumer returns an alternate native global value. Its units, valid range and a safe writable setting interface have not been established; this is not a calibrated timing-offset slider. |

Useful audit locations:

| Observation | JP RVA | CN RVA |
|---|---|---|
| Neverend menu timer | `0x6860` | `0x6850` |
| Auto Play / CPU-assistance chart processing | `0x48A10` | `0x482E0` |
| Auto Play judgment statistics | `0x49E50` | `0x49720` |
| Gameinfo Session flow | `0x13010` | `0x12CD0` |
| Gameinfo diagnostic life display | `0x2E770` | `0x2E1F0` |
| Gameinfo CPU/status text | `0x4ED30` | `0x4E600` |
| All Music judgment-category change | `0x30400` | `0x2FE80` |
| Auto Select song/difficulty cycling | `0x72740` | `0x71FE0` |
| Auto Select / Neverend timeout interaction | `0x69830` | `0x690B0` |
| LAN address/port selection | `0x14960`, `0x149C0` | `0x14490` |
| CPU-assistance update | `0x2F1B0` | `0x2EC30` |
| Gameinfo + User Offset timing source | `0x50F50` | `0x50820` |

## Registered parameters with unestablished effects

`netinfo`, `easyplay`, `packet_cnt`, `develop`, `movie_only`, `scrshot` and
`serial` are present in the XG native registration table. The bounded search of
the inspected game.dll's **direct constant-index calls** found no consumers for
these indices. This does not prove that the entire application never uses them:
other modules, dynamic indices and name-based reads are outside that search.

Providing their raw switches implements access to the native parameters, not
missing retail features. In particular, `movie_only` is not a verified video
repair, and `scrshot` is not a verified screenshot command. No new screenshot,
network or developer-menu feature is implied by a name alone.

## Evidence provenance

The 2026-11-07 investigation used Official IDA MCP on private copies, verified
module SHA256 identities, inspected the registration loops/getter ABI, and
recorded the actual game consumers. The matching XG game identities are:

- JP game.dll: `2966a51ed47ec9560925fa980b9a8931186ef792b4af6abad9d8ff9a566d1a68`
- CN game.dll: `1f6d3f1eab903377c4680f0bf4c449d6e81daf2bdddad38fa539db950e7e982a`

The corresponding libshare-pj.dll identities and the V4–V8 pairs are maintained
in the source profiles. XG table RVAs are `0x55D28` (JP) and `0x4CCD8` (CN);
each entry contains a name pointer and default value. Historical V4–V8 analysis
recorded 13/14 entries respectively. Investigation notes retain the table
outputs, consumer pseudocode, call-site assembly and module hashes; proprietary
game binaries and raw decompilation are not distributed in this repository.

Hook/renderer fixture tests establish plugin mechanics only. They do not
establish actual game effects, all-version compatibility, safe simultaneous use
with another tool's Debug controls, or complete Exit/Restart behavior.
