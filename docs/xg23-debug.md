# XG2 / XG3 native Debug support

K32 here means **XG2**, not the V8 binary that also uses K32. L32 means XG3.
The module hashes, rather than the model string alone, select a profile.
This document describes static native evidence. It does not claim game-tested
Auto Play, menus, results, networking or renderer coexistence.

## Interface and layout

Both versions retain the indexed import
`char __cdecl gd_dip_get_debug_dip_param(int)` from `libshare-pj`.
The XG3 PE import descriptor omits `.dll`; loaders resolve it to
`libshare-pj.dll`. Matching the import must accept that extensionless name.
Consumers use AL. XG3's host `gdxg.dll` has no second Debug getter export.

XG2 has the 15 XG parameters at indices 0..14. XG3 has the same first 15,
followed by two additional native flags. All registered defaults are zero.

| Native index | Parameter | XG2 | XG3 |
|---:|---|:---:|:---:|
| 0 | `gameinfo` | yes | yes |
| 1 | `neverend` | yes | yes |
| 2 | `autoplay` | yes | yes |
| 3 | `all_music` | yes | yes |
| 4 | `auto_select` | yes | yes |
| 5 | `netinfo` | yes | yes |
| 6 | `easyplay` | yes | yes |
| 7 | `lan_vs` | yes | yes |
| 8 | `help_cpu` | yes | yes |
| 9 | `packet_cnt` | yes | yes |
| 10 | `develop` | yes | yes |
| 11 | `user_offset` | yes | yes |
| 12 | `movie_only` | yes | yes |
| 13 | `scrshot` | yes | yes |
| 14 | `serial` | yes | yes |
| 15 | `autodbg` | absent | yes |
| 16 | `benchmark` | absent | yes |

A 15-bit implementation cannot represent XG3. State/ownership storage, read
counters, bindings, API bounds and the panel must all accommodate 17 parameters.
None of these indices is an address to patch in the original DLL.

## XG2 evidence

The inspected K32 pair is:

- game.dll: `fa7ed8b7897e6d082610cffd73881e2ab1fee5d04ced6232842a2d8efebf597e`
- libshare-pj.dll: `b80f36eeafb2f0b1987b2d13551453cd05f82fa1b8407099474f8191c354e097`

Official IDA on private copies confirms libshare initialization RVA `0x34C20`,
getter `0x34CB0`, and the 15-entry table `0x67A48`. The getter returns char in
AL using cdecl and forwards the table's parameter names to the native system
getter. All defaults are zero. game.dll's getter IAT is RVA `0x137710`.

Game RVA `0x6460` skips the 80-update countdown when Neverend or native event
mode is enabled. The chart handler at `0x4AE40` uses Auto Play to select its
automatic-input path; `0x4C8E0` uses it in stage-flow conditions. Other observed
indexed branches include song selection, LAN address selection and CPU help.
The bounded candidate search is not a proof that every other registered flag
has or lacks a consumer. Full chart coverage, results and all other effects
still need play testing.

The frontend directly imports `d3d9!Direct3DCreate9`, providing static support
for the independent D3D9 renderer choice. Rendering with the real loader has
not been tested.

## XG3 consumers established statically

Locations below are game.dll RVAs for the supported main XG3 pair:

- `0x9590`: Neverend or native event mode skips the countdown normally updated
  every 80 calls.
- `0x44C50`: Auto Play selects native automatic chart handling and bypasses
  selected physical-input checks; `0x45FF0` also uses it in judgment/statistics.
- `0x6C6A0`: `autodbg` clears a native selection-flow state in case 4, before
  the existing 80/170-update thresholds. This is an experimental native flow
  switch, not proof of a complete unattended play/test loop.
- `0x3BBD0`, `0x3EAA0`, `0x43EB0`, `0x44C50`, `0x462F0`: `benchmark` changes
  native gameplay/effect handling. Observed branches extend a chain timer by
  8568, play extra effects/sound, trigger assistance periodically after 120
  updates, and alter flow conditions involving Auto Play/Neverend. It is not
  merely an FPS counter; exact complete benchmark behavior is unverified.
- `netinfo` has actual chain/network diagnostic display consumers, including
  `0x43EB0`; `develop` has a consumer at `0x8B460`. The bounded absence of
  direct consumers reported for the older XG game must not be generalized to
  XG3.

Registered flags without a verified effect remain raw native controls. Their
names do not implement missing screenshots, video decoders, development menus
or Session synchronization. Overrides affect the intercepted indexed game
import, not every module's name-based `sys_debug_dip_get_param` calls.

The XG3 host imports `d3d9!Direct3DCreate9`; game/shared modules also reference
native D3D9 device accessors in `libgdme`. This supports the D3D9 integration
choice but is not a runtime renderer or graphics-hook coexistence test.

## Exact XG3 identities and audit locations

| Pair | Module | SHA256 |
|---|---|---|
| Main | game.dll | `3a3a4835d7c43b4cf7d38a0c93a97281346ac5dc27d92450e5237e3506e132b1` |
| Main | libshare-pj.dll | `780f413e8f93a1056239f791516e64ee5aa61069b3715c1d12d4c63af6abd3cd` |
| Backup variant | game.dll | `28d0185e610b169d14e0a7a26956020aa1c445294466798f9145e35310de95c1` |
| Backup variant | libshare-pj.dll | `c5f890c648fdd49ded9318ca2008858c88d20cbcbdc15555382ebcf58863285f` |

In main libshare-pj.dll, initialization is RVA `0x381D0`, the getter is
`0x38270`, and the 17-entry registration table is `0x6F488`. The backup variant
uses initialization `0x38230`, getter `0x382D0`, and the same table RVA and
contents. Main game.dll's indexed import slot is RVA `0x112F78`.

These locations are audit metadata; implementation resolves imports/exports
and validates identities rather than patching game addresses. Static analysis
used Official IDA MCP on private copies, retaining full query/protocol results
and closing each owned database. Proprietary binaries and raw decompilation are
not included in this source repository.

The backup variant was also independently checked: game import slot RVA
`0x112F7C`, Neverend timer `0x9590`, Auto Play chart handler `0x44C80` and
Auto Play/Neverend/Benchmark flow consumer `0x46320`. Their cdecl/low-byte
indexed calls and the 17-entry shared table match the same integration
contract; their internal addresses differ and are not used by the hook.
