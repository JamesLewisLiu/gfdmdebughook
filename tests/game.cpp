extern "C" __declspec(dllimport) char __cdecl gd_dip_get_debug_dip_param(int);
extern "C" __declspec(dllexport) int DebugFixtureMarker(){return 1;}
extern "C" __declspec(dllexport) char __cdecl ReadDebug(int index){return gd_dip_get_debug_dip_param(index);}
