# Licensing and component notices

## Project license

Project-authored code, tests, build scripts and documentation in the current
version are distributed under **GNU GPL version 3 only (GPL-3.0-only)**. The
complete license is in [LICENSE](LICENSE). Third-party components listed below
remain available under their respective licenses.

The original independent DebugHook prototype was offered under MIT. Its notice
is retained in [licenses/DebugHook-original-MIT.txt](licenses/DebugHook-original-MIT.txt)
for material originating in that prototype. Previously granted MIT permissions
remain in effect for that material. This historical notice does not make all
subsequent changes or the complete current version available under MIT.

## Third-party components

| Component | License | Copyright / notice |
| --- | --- | --- |
| Dear ImGui, including the DX9 backend | MIT | Omar Cornut; `vendor/imgui/LICENSE.txt` |
| Embedded STB rectpack, textedit and truetype | MIT option selected for this distribution | Sean Barrett; `licenses/stb-MIT.txt`; original MIT / public-domain alternatives remain in the headers |
| Embedded ProggyClean font | MIT | Tristan Grimmer; `licenses/ProggyClean-MIT.txt` |
| Embedded ProggyForever font subset | MIT | Disco Hello and Tristan Grimmer; `licenses/ProggyForever-MIT.txt` |
| MinHook, including HDE32 and HDE64 | BSD-2-Clause | Tsuda Kageyu and Vyacheslav Patkov; `vendor/minhook/LICENSE.txt` |

The fonts are embedded in `vendor/imgui/imgui_draw.cpp`, so their notices also
apply to binary distributions even when no separate font files are shipped.
Font upstreams:

- ProggyClean: <https://github.com/bluescan/proggyfonts>
- ProggyForever: <https://github.com/ocornut/proggyforever>

`DEPENDENCIES.json` records the exact vendored file hashes and donor revision.
The project uses its own renderer and does not link the Spice SDK. Copying these
third-party components from a Spice checkout does not replace their licenses.
Original vendor files and their embedded notices are preserved.

## Binary and source distribution

Binary packages include this document, GPLv3, the historical MIT notice and all
component notices above. `SOURCES.txt` identifies the packaged source revision,
provides fixed download URLs and names the companion source archive. The build
artifact includes that source archive, all tracked build scripts and vendored
dependencies, plus checksums for both archives. Redistribution should preserve
these materials and keep the corresponding source accessible to recipients.
