#pragma once
#include <atomic>
#include <cstdint>

namespace debughook {
enum class Layout { XG, XG2, XG3, V4, V5ToV8 };
inline constexpr int max_parameter_count=17;
struct Parameter {
    const char* name;
    const char* label;
    const wchar_t* key;
    const char* note;
};

// Canonical metadata. Native indices 12+ vary between the XG and V layouts.
inline constexpr Parameter parameters[] = {
    {"gameinfo", "Game Info", L"GameInfo", "Native diagnostics AND flow changes, including a Session update branch. Not a display-only switch."},
    {"neverend", "Neverend / Disable Menu Timer", L"Neverend", "Native Neverend affects more than menu timers. Auto Select can still advance some flows."},
    {"autoplay", "Auto Play", L"AutoPlay", "Native automatic judgment path. Full GF/DM input coverage and result flags need game testing."},
    {"all_music", "All Music", L"AllMusic", "Experimental: changes song filtering AND gameplay judgment/score paths. Not a visibility-only unlock."},
    {"auto_select", "Auto Select", L"AutoSelect", "Experimental: selection cycling and automatic progression. Can override Neverend in some flows."},
    {"netinfo", "Net Info", L"NetInfo", "Registered native flag. No direct constant-index consumer found in the inspected XG game.dll."},
    {"easyplay", "Easy Play", L"EasyPlay", "Registered native flag. No direct constant-index consumer found in the inspected XG game.dll."},
    {"lan_vs", "LAN VS", L"LanVs", "Native Session address selection: local AVS address and port 5700. Does not implement or repair Session networking."},
    {"help_cpu", "Help CPU", L"HelpCpu", "Experimental native CPU/help input paths. Exact GF/DM behavior is not fully established."},
    {"packet_cnt", "Packet Count", L"PacketCount", "Registered native flag. No direct constant-index consumer found in the inspected XG game.dll."},
    {"develop", "Develop", L"Develop", "Registered native flag; no verified XG game consumer. Cannot restore removed retail developer code."},
    {"user_offset", "User Offset", L"UserOffset", "Native alternate timing source; requires Game Info. Not a numeric timing-offset editor; units/range are unverified."},
    {"movie_only", "Movie Only", L"MovieOnly", "XG only. Registered native flag; effect not established. This is not a video-decoder repair."},
    {"scrshot", "Screenshot (native flag)", L"Screenshot", "Registered native flag; screenshot output behavior is not established. No custom capture is implemented."},
    {"serial", "Serial", L"Serial", "Absent in V4. Registered native flag; no verified XG game consumer or serial-device implementation."},
    {"autodbg", "Auto Debug", L"AutoDebug", "XG3 native automatic-flow Debug branch. Full behavior requires game testing."},
    {"benchmark", "Benchmark", L"Benchmark", "XG3 native gameplay/timer/effect/assistance branches. Not an FPS display; full effects require game testing."}
};
inline constexpr int parameter_count(Layout layout) {
    return layout == Layout::XG3 ? 17 : layout == Layout::V4 ? 13 : layout == Layout::V5ToV8 ? 14 : 15;
}
inline constexpr const Parameter* parameter_at(Layout layout, int index) {
    if (index < 0 || index >= parameter_count(layout)) return nullptr;
    return &parameters[(layout == Layout::V4 || layout == Layout::V5ToV8) && index >= 12 ? index + 1 : index];
}
inline constexpr unsigned parameter_mask(Layout layout) {
    return (1u << parameter_count(layout)) - 1;
}
inline constexpr bool valid_key(unsigned key) { return key <= 254; }
inline constexpr bool bound_key(unsigned key) { return key >= 1 && valid_key(key); }

// Low 32 bits are values; high 32 bits are ownership. A single 64-bit atomic
// publishes both, including XG3's indices 15 and 16, without overlapping bits.
// A Native value chains
// to the pre-existing IAT target; explicitly setting OFF overrides it with zero.
class Switches {
    std::atomic<std::uint64_t> packed{(std::uint64_t{1} << 33) | (std::uint64_t{1} << 34)};
public:
    int get(int index) const {
        if (index < 0 || index >= max_parameter_count) return -2;
        auto value = packed.load();
        return value & (std::uint64_t{1} << (index + 32)) ? int((value >> index) & 1u) : -1;
    }
    void set(int index, bool enabled) {
        auto old = packed.load();
        const auto owned = std::uint64_t{1} << (index + 32), bit = std::uint64_t{1} << index;
        while (!packed.compare_exchange_weak(old, enabled ? old | owned | bit : (old | owned) & ~bit)) {}
    }
    void toggle(int index) {
        auto old = packed.load();
        const auto owned = std::uint64_t{1} << (index + 32), bit = std::uint64_t{1} << index;
        while (!packed.compare_exchange_weak(old, (old | owned) ^ bit)) {}
    }
    void restore(int index) { packed.fetch_and(~((std::uint64_t{1} << (index + 32)) | (std::uint64_t{1} << index))); }
    void restore_all() { packed.store(0); }
    void all_off(Layout layout) { packed.store(std::uint64_t{parameter_mask(layout)} << 32); }
};
}
