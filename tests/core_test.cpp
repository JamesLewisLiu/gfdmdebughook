#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "../src/edge.h"
#include "../src/parameters.h"
#include "../src/profiles.h"
extern "C" __declspec(dllimport) char __cdecl ReadDebug(int);
void require(bool ok,const char* text){if(!ok){fprintf(stderr,"FAIL: %s\n",text);ExitProcess(1);}}
static char __cdecl later_hook(int index){return static_cast<char>(index+30);}
static void replace_fixture_import(bool extensionless=false){
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"game.dll"));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    auto descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;descriptor->Name;descriptor++){
        auto module_name=reinterpret_cast<char*>(base+descriptor->Name);
        if(_stricmp(module_name,"libshare-pj.dll")&&_stricmp(module_name,"libshare-pj"))continue;
        if(extensionless){
            DWORD protection=0,ignored=0;
            require(VirtualProtect(module_name,16,PAGE_READWRITE,&protection)!=FALSE,"fixture import name writable");
            strcpy(module_name,"libshare-pj");
            VirtualProtect(module_name,16,protection,&ignored);return;
        }
        auto names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+descriptor->OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+descriptor->FirstThunk);
        for(;names->u1.AddressOfData;names++,slots++){
            if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))continue;
            auto symbol=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(strcmp(reinterpret_cast<char*>(symbol->Name),"gd_dip_get_debug_dip_param"))continue;
            DWORD protection=0,ignored=0;
            require(VirtualProtect(&slots->u1.Function,4,PAGE_READWRITE,&protection)!=FALSE,"fixture takeover writable");
            InterlockedExchangePointer(reinterpret_cast<void**>(&slots->u1.Function),reinterpret_cast<void*>(later_hook));
            VirtualProtect(&slots->u1.Function,4,protection,&ignored);return;
        }
    }
    require(false,"fixture takeover slot");
}
static void check_model(){
    using namespace debughook;
    require(parameter_count(Layout::XG)==15&&parameter_count(Layout::XG2)==15&&parameter_count(Layout::XG3)==17&&parameter_count(Layout::V4)==13&&parameter_count(Layout::V5ToV8)==14,"profile counts");
    for(auto layout:{Layout::XG,Layout::XG2,Layout::XG3,Layout::V4,Layout::V5ToV8}){
        require(!strcmp(parameter_at(layout,1)->name,"neverend")&&!strcmp(parameter_at(layout,2)->name,"autoplay"),"common indices");
        require(!parameter_at(layout,-1)&&!parameter_at(layout,parameter_count(layout)),"profile boundaries");
    }
    require(!strcmp(parameter_at(Layout::XG,12)->name,"movie_only")&&!strcmp(parameter_at(Layout::XG,13)->name,"scrshot")&&!strcmp(parameter_at(Layout::XG,14)->name,"serial"),"XG tail");
    require(!strcmp(parameter_at(Layout::XG2,12)->name,"movie_only")&&!strcmp(parameter_at(Layout::XG2,14)->name,"serial"),"XG2 tail retains XG mapping");
    require(!strcmp(parameter_at(Layout::XG3,15)->name,"autodbg")&&!strcmp(parameter_at(Layout::XG3,16)->name,"benchmark"),"XG3 new parameters");
    require(!strcmp(parameter_at(Layout::V4,12)->name,"scrshot")&&!parameter_at(Layout::V4,13),"V4 tail");
    require(!strcmp(parameter_at(Layout::V5ToV8,12)->name,"scrshot")&&!strcmp(parameter_at(Layout::V5ToV8,13)->name,"serial"),"V5-V8 tail");
    require(valid_key(0)&&!bound_key(0)&&bound_key(121)&&!valid_key(255)&&!valid_key(~0u),"disabled and invalid bindings");
    Switches state;
    require(state.get(1)==0&&state.get(2)==0&&state.get(0)==-1,"default ownership");
    for(int i=0;i<max_parameter_count;i++){state.set(i,true);require(state.get(i)==1,"packed ON");}
    for(int i=0;i<max_parameter_count;i++){state.toggle(i);require(state.get(i)==0,"packed toggle OFF");}
    state.restore_all();for(int i=0;i<max_parameter_count;i++)require(state.get(i)==-1,"packed Native");
    state.set(0,true);state.set(16,true);state.restore(16);
    require(state.get(0)==1&&state.get(16)==-1,"index16 ownership never overlaps index0");
    state.all_off(Layout::XG3);for(int i=0;i<17;i++)require(state.get(i)==0,"XG3 all OFF covers both new bits");
    state.all_off(Layout::V4);for(int i=0;i<13;i++)require(state.get(i)==0,"V4 all OFF");
    require(state.get(13)==-1&&state.get(14)==-1,"V4 mask bounds");
    for(const auto& profile:profiles){
        require(find_profile(profile.game,profile.share)==&profile,"strict native SHA pair lookup");
        require(!find_profile(profile.game,"unknown")&&!find_profile("unknown",profile.share),"mixed or unknown profile rejected");
    }
    const auto* xg2=find_profile("fa7ed8b7897e6d082610cffd73881e2ab1fee5d04ced6232842a2d8efebf597e","b80f36eeafb2f0b1987b2d13551453cd05f82fa1b8407099474f8191c354e097");
    const auto* xg3=find_profile("3a3a4835d7c43b4cf7d38a0c93a97281346ac5dc27d92450e5237e3506e132b1","780f413e8f93a1056239f791516e64ee5aa61069b3715c1d12d4c63af6abd3cd");
    require(xg2&&xg2->layout==Layout::XG2&&xg2->panel&&xg3&&xg3->layout==Layout::XG3&&xg3->panel,"K32/L32 native identities enable correct layout and renderer");
}
int main(int argc,char** argv){
    check_model();
    wchar_t requested[16]{};GetEnvironmentVariableW(L"GFDM_DEBUGHOOK_FIXTURE_LAYOUT",requested,16);
    auto expected_layout=!wcscmp(requested,L"XG2")?debughook::Layout::XG2:!wcscmp(requested,L"XG3")?debughook::Layout::XG3:!wcscmp(requested,L"V4")?debughook::Layout::V4:!wcscmp(requested,L"V5")?debughook::Layout::V5ToV8:debughook::Layout::XG;
    const bool takeover=argc>1&&!strcmp(argv[1],"later-hook");
    debughook::Edge edge;
    require(!edge.update(true,true),"initial held key ignored");
    require(!edge.update(false,true)&&edge.update(true,true),"press edge toggles");
    for(int i=0;i<10000;i++)require(!edge.update(true,true),"long hold has one edge");
    require(!edge.update(false,false)&&!edge.update(true,true),"refocus held key ignored");
    require(!edge.update(false,true)&&edge.update(true,true),"release and repress after focus");
    require(ReadDebug(1)==11,"native baseline");
    auto production=LoadLibraryW(L"gfdmdebug.dll");require(production!=nullptr,"load production");
    auto status=reinterpret_cast<DWORD(__cdecl*)()>(GetProcAddress(production,"DebugHookGetStatus"));
    auto shutdown=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(production,"DebugHookShutdown"));
    require(status&&shutdown,"exports");Sleep(400);require(status()==0&&ReadDebug(1)==11,"production rejects fixture");shutdown();
    if(argc>1&&!strcmp(argv[1],"extensionless"))replace_fixture_import(true);
    auto plugin=LoadLibraryW(L"gfdmdebug_test.dll");require(plugin!=nullptr,"load test");
    status=reinterpret_cast<DWORD(__cdecl*)()>(GetProcAddress(plugin,"DebugHookGetStatus"));
    auto set=reinterpret_cast<BOOL(__cdecl*)(int,BOOL)>(GetProcAddress(plugin,"DebugHookSetSwitch"));
    shutdown=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(plugin,"DebugHookShutdown"));
    auto count=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(plugin,"DebugHookGetSwitchCount"));
    auto name=reinterpret_cast<const char*(__cdecl*)(int)>(GetProcAddress(plugin,"DebugHookGetSwitchName"));
    auto value=reinterpret_cast<int(__cdecl*)(int)>(GetProcAddress(plugin,"DebugHookGetSwitchValue"));
    auto reads=reinterpret_cast<DWORD(__cdecl*)(int)>(GetProcAddress(plugin,"DebugHookGetSwitchReads"));
    auto key=reinterpret_cast<int(__cdecl*)(int)>(GetProcAddress(plugin,"DebugHookGetHotkey"));
    auto restore=reinterpret_cast<BOOL(__cdecl*)(int)>(GetProcAddress(plugin,"DebugHookRestoreSwitch"));
    auto native=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(plugin,"DebugHookRestoreNative"));
    auto off=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(plugin,"DebugHookAllOff"));
    require(status&&set&&shutdown&&count&&name&&value&&reads&&key&&restore&&native&&off,"test exports");
    for(int i=0;i<250&&(!(status()&1)||key(1)<0);i++)Sleep(20);
    require(status()&1,"fixture installed");
    require(count()==debughook::parameter_count(expected_layout)&&key(-1)==121,"profile count and menu binding");
    for(int i=0;i<count();i++)require(key(i)==0,"all Debug actions unbound by default");
    require(ReadDebug(1)==0&&ReadDebug(2)==0,"defaults disabled");
    require(ReadDebug(0)==10&&ReadDebug(3)==13&&ReadDebug(count()-1)==count()+9,"other parameters passthrough");
    for(int i=0;i<count();i++){
        require(!strcmp(name(i),debughook::parameter_at(expected_layout,i)->name),"native parameter names");
        require(set(i,TRUE)&&ReadDebug(i)==1&&value(i)==1,"each native parameter ON");
        require(set(i,FALSE)&&ReadDebug(i)==0&&value(i)==0,"each native parameter explicitly OFF");
        require(restore(i)&&ReadDebug(i)==i+10&&value(i)==-1,"each native parameter Native");
    }
    require(set(2,FALSE),"reset independent switch state");
    require(set(1,TRUE)&&ReadDebug(1)==1&&ReadDebug(2)==0,"neverend independent");
    require(set(2,TRUE)&&ReadDebug(1)==1&&ReadDebug(2)==1,"both active");
    auto before=reads(2);
    for(int i=0;i<10000;i++)require(ReadDebug(2)==1,"getter retains level");
    require(reads(2)==before+10000,"telemetry counts actual IAT reads");
    const int invalid=count();
    require(!set(-1,TRUE)&&!set(invalid,TRUE)&&!restore(invalid)&&!name(invalid)&&value(invalid)==-2,"unknown switch rejected");
    require(set(2,FALSE)&&ReadDebug(2)==0&&ReadDebug(1)==1,"disable one");
    off();for(int i=0;i<count();i++)require(ReadDebug(i)==0,"all OFF");
    require(ReadDebug(invalid)==invalid+10,"out-of-profile calls still passthrough");
    native();for(int i=0;i<count();i++)require(ReadDebug(i)==i+10,"restore all Native");
    if(takeover){replace_fixture_import();require(ReadDebug(2)==32,"later hook owns IAT");}
    shutdown();require(ReadDebug(1)==(takeover?31:11)&&ReadDebug(2)==(takeover?32:12),"shutdown respects IAT ownership");
    shutdown();require(status()==0,"idempotent shutdown");
    require(count()==0&&!set(1,TRUE),"terminal shutdown");
    puts("PASS: strict native profiles, all layouts, unbound defaults, production rejection, x86 IAT, native parameters, Native/OFF/ON, telemetry, boundaries, shutdown ownership");
}
