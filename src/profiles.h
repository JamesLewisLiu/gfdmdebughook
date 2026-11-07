#pragma once
#include <string_view>
#include "parameters.h"

namespace debughook {
struct Profile {
    const char* name;
    const char* game;
    const char* share;
    Layout layout;
    bool panel;
};
inline constexpr Profile profiles[]={
    {"XG","2966a51ed47ec9560925fa980b9a8931186ef792b4af6abad9d8ff9a566d1a68","2b6f48f5e6198e9a6922a0a23d412b551e58110c8a3cdb88d7c1f4d69b013df6",Layout::XG,true},
    {"XGCN","1f6d3f1eab903377c4680f0bf4c449d6e81daf2bdddad38fa539db950e7e982a","a33361fa55ec7299f501c54299c6dc41e748160b5386153ae0fdba08f97d6c61",Layout::XG,true},
    {"XG2","fa7ed8b7897e6d082610cffd73881e2ab1fee5d04ced6232842a2d8efebf597e","b80f36eeafb2f0b1987b2d13551453cd05f82fa1b8407099474f8191c354e097",Layout::XG2,true},
    {"XG3","3a3a4835d7c43b4cf7d38a0c93a97281346ac5dc27d92450e5237e3506e132b1","780f413e8f93a1056239f791516e64ee5aa61069b3715c1d12d4c63af6abd3cd",Layout::XG3,true},
    {"XG3","28d0185e610b169d14e0a7a26956020aa1c445294466798f9145e35310de95c1","c5f890c648fdd49ded9318ca2008858c88d20cbcbdc15555382ebcf58863285f",Layout::XG3,true},
    {"V4 (core only)","8cfdcf100530e1b9d8592cc4404d705b48ece5fd631e32965ec66a40c9eed389","e355693bf2341ae3dd69b11911abeec6208e1afe5eb6e36b3e11c484af2c9918",Layout::V4,false},
    {"V5 (core only)","1dd799d0283363e3887e62ed10d6add9da15d17b79ee093d29b420e22229b398","ab06c88e9347bbfe862e02c983f7b0ede80c77dd1c5a241d128eeb0069f18ca1",Layout::V5ToV8,false},
    {"V6 (core only)","c2dfccfbbfc56cf4aceee84e10bd5522473275ed36668b2ae1f742b1b500f5b8","3391c0a3d9ea7a66bbb542636f637ddad64d8f0470938b6add1420f52b8cee44",Layout::V5ToV8,false},
    {"V7 (core only)","110112ff4c82d8f1d4958bee2d8ac0ecbb6824790d79524a4925abc29759397e","2a801d1b9434d73bae462729df9cd03ec13fc196ed890745bca3f0911164f277",Layout::V5ToV8,false},
    {"V8 (core only)","91867e4e23fe6f06d77a87840c2559d7e2272ffbc5339f9ab4da6302dc9f3cf8","87ef44eb6161ba166a8aa997a409763986e8d5b9f7af4b688a25ef3490f01729",Layout::V5ToV8,false}
};
inline const Profile* find_profile(std::string_view game,std::string_view share) {
    for(const auto& profile:profiles)if(game==profile.game&&share==profile.share)return &profile;
    return nullptr;
}
}
