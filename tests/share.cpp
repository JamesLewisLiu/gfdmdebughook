extern "C" __declspec(dllexport) int DebugFixtureMarker(){return 1;}
extern "C" __declspec(dllexport) char __cdecl gd_dip_get_debug_dip_param(int index){return static_cast<char>(index+10);}
