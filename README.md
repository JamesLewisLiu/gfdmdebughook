# GFDM DebugHook

An independent **Windows x86 DLL** with its own Dear ImGui context, D3D9
renderer, input, INI configuration and logging. It does not use the Spice SDK
or link against Spice or gfdmtools5. No launcher source changes or game binary
patches are needed. Dependencies are vendored; compilation downloads nothing.

This is a development build. The isolated tests pass; actual game behavior,
launcher coexistence and normal game Exit/Restart still need testing.

## Load

Keep `gfdmdebug.dll` and `gfdmdebug.ini` together. With a Spice version that
supports loading external DLLs:

```bat
spice.exe -gdxg GF -k gfdmdebug.dll
spice.exe -gdxg DM -k gfdmdebug.dll
```

Those commands are for XG / XGCN. For XG2 (K32) / XG3 (L32), append
`-k gfdmdebug.dll` to your normal game launch command. The profile is detected
from loaded modules; no separate DebugHook game-selection argument is needed.

`-k` is only a loading mechanism; the plugin does not call Spice SDK functions.
Another launcher needs its own supported x86 DLL loading mechanism. Placing
the file beside the original `gdxg.exe` does not make that EXE load it. There is
no universal injector in this project. Do not replace a game library with it.

Bootstrap waits up to 120 seconds for `game.dll` and `libshare-pj.dll`, validates
both on-disk SHA256 identities, and replaces only the named Debug getter import
in `game.dll`. Unknown module pairs stay inactive. Existing cached function
pointers and late injection are not certified. The DLL logs to
`gfdmdebug.log` beside itself; this directory must be writable.

## Controls

**F10** opens or hides the XG D3D9 panel. **All Debug actions, including Auto Play
and Neverend, are unbound by default. F3/F4 do nothing unless you bind them.**

Use the checkboxes to change a native Debug parameter:

- **ON / OFF** explicitly overrides the game getter with one or zero.
- **Native** chains to the prior getter, including an existing tools hook. An
  unchecked checkbox with Native status does not assert that the native value is
  zero. Its value is not sampled by the UI.
- **Reset** releases one override; **Restore Native** releases all overrides.
- **All OFF** explicitly disables every parameter in the detected profile.

Each new process starts Auto Play and Neverend explicitly OFF. Other parameters
start Native. Switch states are not saved. Hiding the panel keeps overrides.

XG / XGCN and XG2 have **15 native Debug parameters**: `gameinfo`, `neverend`,
`autoplay`, `all_music`, `auto_select`, `netinfo`, `easyplay`, `lan_vs`, `help_cpu`,
`packet_cnt`, `develop`, `user_offset`, `movie_only`, `scrshot`, and `serial`.
XG3 adds `autodbg` and `benchmark`, for **17 parameters**. Both are native
flow/gameplay controls; Benchmark is not a simple FPS display. See the
[XG2 / XG3 notes](docs/xg23-debug.md) for version-specific effects and identities.
Detailed effects and limitations are documented below. Registered parameters
without a verified consumer are exposed as native flags; this does not recreate
removed developer code, implement a screenshot service, or repair video/networking.

Some flags affect gameplay and flow: `all_music` also changes judgment paths;
`auto_select` can advance a flow even with Neverend enabled; `user_offset` uses
an alternate native timing source together with `gameinfo`. See
[native Debug notes](docs/native-debug.md) in the source tree. Test their effects
in the intended scene rather than assuming they are display-only controls.

**Reads** counts calls through this game's intercepted import. Zero only
means that this import has not consumed the parameter so far, not that there are
no consumers anywhere else. Other modules calling `sys_debug_dip_get_param`
directly are not changed by this plugin.

## Optional hotkeys

Edit the `[Keys]` section of `gfdmdebug.ini` beside the DLL, then restart:

```ini
[Keys]
Menu=121
AutoPlay=0
Neverend=0
AllMusic=0
```

Values are decimal Windows virtual-key codes; **0 means unbound**, 121 is F10.
All remaining action names are listed in the supplied INI. Missing action keys
also default to zero; invalid values disable only that binding. `Menu=0`
disables the menu hotkey as well. Existing INI files with explicit F3/F4 values
retain those bindings; change them to zero to remove them.

Only a window in the current process being foreground permits hotkeys. A held
key must be released before it can trigger; long holds do not toggle repeatedly.
No controller binding or text-entry UI is included. The panel does not consume
game or Spice input. Avoid overlapping gameplay bindings or another interactive
overlay; mouse input can still reach the game.

## Versions and independence

- **XG / XGCN:** known paired module profiles, all 15 native parameters and
  standalone D3D9 panel implemented; actual game testing pending.
- **XG2 (K32):** known paired profile, 15 parameters and D3D9 panel implemented.
- **XG3 (L32):** two independently inspected module pairs, 17 parameters and
  D3D9 panel implemented. Import names with and without `.dll` are accepted.
- **V4:** experimental core/hotkeys only, 13 native parameters.
- **V5-V8:** experimental core/hotkeys only, 14 native parameters.

Version tails are mapped separately: XG index12 is `movie_only`, whereas V4-V8
index12 is `scrshot`. V4 has no `serial`; V5-V8 index13 is `serial`.
XG2 retains the XG layout; XG3 adds indices15/16. K32 alone is ambiguous with
V8, so only exact game/libshare hash pairs select a profile. XG2/XG3 have static
ABI/consumer evidence and fixture coverage, but no actual game testing yet.
There is no OpenGL renderer in this build. Old-version support is not certified.
V-series users need to configure optional hotkeys or use the exported API until
an appropriate renderer is implemented and validated.

Only the selected game import is modified; untouched parameters chain to its
saved target. Avoid having gfdmtools5 bindings and this DLL control the same
parameter simultaneously. Coexistence with existing graphics hooks, multiple
device replacement, GF/DM full-song behavior, TEST, results, Alt-Tab and
Exit/Restart require game regression tests.

The DLL is pinned for process lifetime. `DebugHookShutdown` stops its worker,
disables drawing, releases its renderer resources and restores the IAT slot only
if this plugin still owns it. Shutdown is terminal; restarting requires a new
process. Do not use FreeLibrary to unload it during play. This UCRT/bcrypt build
does not support Windows XP.

## Build

Use CMake >=3.20 and a Windows x86 C++17 toolchain. x64 builds are intentionally
rejected. For MSVC:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
./scripts/package.ps1 -BinaryPath build/Release/gfdmdebug.dll
```

MinGW requires an i686 toolchain and the corresponding CMake toolchain file.
The project links the C++ runtime statically; MSVC uses `/MT`. Local validation
used LLVM/MinGW. MSVC and hosted Actions execution have not yet been verified.

The [build workflow](.github/workflows/build.yml) supports push, pull request
and manual dispatch. It builds the production DLL and fixtures on Windows 2022
with MSVC Win32, runs the six core tests, and uploads a production-only ZIP,
SHA256 list and test logs. It compiles the two renderer tests but deliberately
does not run interactive D3D9 tests on the hosted runner. No publish/release step
is configured.

Packaging requires a fresh output directory. Override it with
`-OutputDirectory <new-directory>` for subsequent builds. The package whitelist
contains the production DLL, INI, guide, native Debug notes, licenses and digest
manifest. It excludes all fixture DLLs and test EXEs.

## Tests

```powershell
# Without an interactive D3D9 device:
ctest --test-dir build -C Release --output-on-failure --label-exclude interactive
# On an interactive Windows desktop:
ctest --test-dir build -C Release --output-on-failure -L interactive
```

`core` tests actual x86 IAT replacement, production rejection of fixture
identities, Native/OFF/ON, disabled default hotkeys, read telemetry, invalid
indices, press/focus edges, strict native profile pairs and shutdown. The
`core-xg2`, `core-xg3`, `core-v4` and `core-v5` cases exercise all five layouts
(13/14/15/17 controls), including real extensionless import-name matching and
XG3's new high-index state/ownership boundaries.
`core-chain` also checks that shutdown preserves a later hook's IAT takeover.
`renderer` and `renderer-xg3` open their own test windows, render 120 D3D9 frames
each and check state
restoration, device Reset, hiding and orderly shutdown. It writes a fixture
screenshot; it does not launch a game.

The separately named `gfdmdebug_test.dll` accepts synthetic test modules. Never
distribute or load it, fixture `game.dll`, or fixture `libshare-pj.dll` in a game.
The production DLL rejects these fixtures. `-DBUILD_TESTING=OFF` omits them.

## Exported API

All exports use x86 `__cdecl`. Loaders must keep the DLL loaded. Parameter
indices are **native indices for the detected version**, not XG indices for
every version; query names/count instead of assuming the layout.

| Function | Result / behavior |
|---|---|
| `BOOL DebugHookInitialize()` | Asynchronous, idempotent; false after terminal shutdown. |
| `void DebugHookShutdown()` | Stop worker/renderer and relinquish an owned IAT slot. |
| `DWORD DebugHookGetStatus()` | Bits0..3: active, Neverend override ON, Auto Play override ON, panel hooks ready. |
| `BOOL DebugHookShowOverlay(BOOL)` | Request panel visibility. V-series has no renderer yet. |
| `DWORD DebugHookGetFrames()` | Number of plugin draw calls. |
| `int DebugHookGetSwitchCount()` | 0 when inactive; otherwise 13, 14, 15 or 17. |
| `const char* DebugHookGetSwitchName(int)` | Native name; null for invalid/inactive. |
| `int DebugHookGetSwitchValue(int)` | -2 invalid/inactive, -1 Native, 0 OFF, 1 ON. |
| `BOOL DebugHookSetSwitch(int, BOOL)` | Explicit override; false for invalid/inactive. |
| `BOOL DebugHookRestoreSwitch(int)` | Release one override. |
| `void DebugHookRestoreNative()` | Release all overrides. |
| `void DebugHookAllOff()` | Explicitly override all profile parameters with zero. |
| `DWORD DebugHookGetSwitchReads(int)` | Intercepted getter read count, or 0 for invalid/inactive. |
| `int DebugHookGetHotkey(int)` | VK, 0 unbound, -1 unavailable; index -1 queries Menu. |

## Rollback and licensing

Exit the process and remove the DLL loading argument from the next launch.
No game or Spice binary needs restoring. Plugin INI/log may be retained.

Project-authored code and documentation in this version are distributed under
**GPL-3.0-only**, with the full terms in `LICENSE`. Third-party components retain
their own licenses. The historical prototype's MIT notice is preserved in
`licenses/DebugHook-original-MIT.txt`; it does not offer the entire current
version under MIT or revoke MIT permissions already granted for that prototype.
See [LICENSES.md](LICENSES.md) for the component scope, embedded fonts and notices.

Dear ImGui 1.92.9 WIP and MinHook keep their separate notices under `vendor/`.
They were copied from the existing Spice checkout at commit
`b7d33db9b7a8f2b523cf8667ac03b8882045ec34`; no Spice implementation or SDK is
linked. `DEPENDENCIES.json` records exact vendor file hashes. The donor ImGui
configuration disables default allocators; this DLL explicitly supplies its own
malloc/free callbacks before creating an ImGui context.

Each packaged build includes `SOURCES.txt` with its source commit and fixed
source download links. The same build artifact also contains the corresponding
`GFDM-DebugHook-source-<commit>.zip`, including vendored dependencies and build
scripts. Keep that source archive, the notices and `SHA256SUMS.txt` available
alongside the binary when redistributing it. Source is available at no extra
charge; downloading the binary does not require downloading the source.
