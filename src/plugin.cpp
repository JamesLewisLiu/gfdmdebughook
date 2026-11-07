#include "debug.h"
#include "edge.h"
#include "profiles.h"
#include <bcrypt.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace debughook {
std::atomic_bool active{false}, visible{false}, stopped{false};
Switches switches;
Layout layout=Layout::XG;
std::atomic<unsigned> reads[max_parameter_count]{};
static std::atomic<unsigned> bindings[max_parameter_count+1]{};
static std::atomic_bool bindings_ready{false};
static bool supports_panel=true;
std::atomic<unsigned> rendered{0};
std::atomic_bool panel_ready{false};
HMODULE self=nullptr;
std::wstring directory;
std::string profile;
static HANDLE worker=nullptr, stop_event=nullptr;
static std::mutex lifecycle, logging;
using Getter=char(__cdecl*)(int);
static Getter original=nullptr;
static void** slot=nullptr;

void log(const char* text) {
    std::lock_guard lock(logging);
    if(directory.empty()) return;
    FILE* f=_wfopen((directory+L"gfdmdebug.log").c_str(),L"ab");
    if(f){ SYSTEMTIME t; GetLocalTime(&t); fprintf(f,"%04u-%02u-%02u %02u:%02u:%02u %s\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,text); fclose(f); }
}
static void log_switch(int index) {
    const auto* parameter=parameter_at(layout,index);
    if(!parameter) return;
    std::string message=parameter->name;
    const int value=switches.get(index);
    message+=value<0?" Native (prior target)":value?" ON":" OFF";
    log(message.c_str());
}
bool set_switch(int index,bool enabled) {
    if(!active.load(std::memory_order_acquire)||!parameter_at(layout,index)) return false;
    switches.set(index,enabled);log_switch(index);return true;
}
bool restore_switch(int index) {
    if(!active.load(std::memory_order_acquire)||!parameter_at(layout,index)) return false;
    switches.restore(index);log_switch(index);return true;
}
void all_off() { if(active.load()){switches.all_off(layout);log("All native Debug switches explicitly OFF");} }
void restore_native() { if(active.load()){switches.restore_all();log("All Debug overrides released to prior target");} }
void change(int index) {
    if(active.load(std::memory_order_acquire)&&parameter_at(layout,index)) {
        switches.toggle(index);log_switch(index);
    }
}
static char __cdecl getter(int index) {
    if(active.load(std::memory_order_acquire)&&parameter_at(layout,index)) {
        reads[index].fetch_add(1,std::memory_order_relaxed);
        const int value=switches.get(index);
        if(value>=0) return static_cast<char>(value);
    }
    return original(index);
}
static std::string file_hash(HMODULE module) {
    wchar_t path[32768]; if(!GetModuleFileNameW(module,path,32768)) return {};
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE) return {};
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char result[32], buffer[16384]; DWORD read=0; bool ok=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 && BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0){
        bool good=true;
        for(;;){if(!ReadFile(f,buffer,sizeof(buffer),&read,nullptr)){good=false;break;}if(!read)break;if(BCryptHashData(hash,buffer,read,0)<0){good=false;break;}}
        ok=good && read==0 && BCryptFinishHash(hash,result,32,0)>=0;
    }
    if(hash) BCryptDestroyHash(hash); if(alg) BCryptCloseAlgorithmProvider(alg,0); CloseHandle(f);
    if(!ok) return {};
    char hex[65]; for(int i=0;i<32;i++) sprintf(hex+2*i,"%02x",result[i]); return hex;
}
static bool identify(HMODULE game, HMODULE share) {
#ifdef DEBUGHOOK_FIXTURE
    if(GetProcAddress(game,"DebugFixtureMarker") && GetProcAddress(share,"DebugFixtureMarker")){
        wchar_t requested[16]{};
        GetEnvironmentVariableW(L"GFDM_DEBUGHOOK_FIXTURE_LAYOUT",requested,16);
        if(!wcscmp(requested,L"XG2"))layout=Layout::XG2;
        else if(!wcscmp(requested,L"XG3"))layout=Layout::XG3;
        else if(!wcscmp(requested,L"V4"))layout=Layout::V4;
        else if(!wcscmp(requested,L"V5"))layout=Layout::V5ToV8;
        profile="TEST FIXTURE";
        supports_panel=layout!=Layout::V4&&layout!=Layout::V5ToV8;
        return true;
    }
#endif
    const auto gh=file_hash(game), sh=file_hash(share);
    if(const auto* known=find_profile(gh,sh)){
        profile=known->name;
        layout=known->layout;
        supports_panel=known->panel;
        return true;
    }
    log("Unsupported module hashes; Debug Hook remains inactive"); return false;
}
static void** find_slot(HMODULE module) {
    auto base=reinterpret_cast<unsigned char*>(module);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000) return nullptr;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC) return nullptr;
    const DWORD size=nt->OptionalHeader.SizeOfImage;
    auto fits=[size](DWORD rva,size_t length){return rva<size && length<=size-rva;};
    auto str=[&](DWORD rva)->const char* { return fits(rva,1)&&memchr(base+rva,0,size-rva)?reinterpret_cast<char*>(base+rva):nullptr; };
    auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!fits(dir.VirtualAddress,dir.Size)) return nullptr;
    for(DWORD off=0;off+sizeof(IMAGE_IMPORT_DESCRIPTOR)<=dir.Size;off+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){
        auto d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+dir.VirtualAddress+off);
        if(!d->Name) break;
        auto name=str(d->Name); if(!name||(_stricmp(name,"libshare-pj.dll")&&_stricmp(name,"libshare-pj"))) continue;
        if(!d->OriginalFirstThunk) return nullptr;
        for(DWORD i=0; i<size/4; ++i){
            DWORD n=d->OriginalFirstThunk+4*i, a=d->FirstThunk+4*i;
            if(n<d->OriginalFirstThunk||a<d->FirstThunk||!fits(n,4)||!fits(a,4)) return nullptr;
            DWORD value=*reinterpret_cast<DWORD*>(base+n); if(!value) break;
            if(IMAGE_SNAP_BY_ORDINAL32(value)) continue;
            if(value>0xfffffffd) return nullptr;
            auto symbol=str(value+2);
            if(symbol && !strcmp(symbol,"gd_dip_get_debug_dip_param")) return reinterpret_cast<void**>(base+a);
        }
    } return nullptr;
}
static bool attach(HMODULE game,HMODULE share) {
    if(!identify(game,share)) return false;
    slot=find_slot(game); if(!slot){log("Debug getter IAT slot missing");return false;}
    original=reinterpret_cast<Getter>(*slot);
    MEMORY_BASIC_INFORMATION mbi{};
    if(!original||!VirtualQuery(reinterpret_cast<void*>(original),&mbi,sizeof(mbi))||mbi.State!=MEM_COMMIT||!(mbi.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){log("Original IAT target is not executable");return false;}
    DWORD protection=0; if(!VirtualProtect(slot,4,PAGE_READWRITE,&protection)) return false;
    active.store(true,std::memory_order_release);
    const auto old=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(getter),reinterpret_cast<void*>(original));
    DWORD ignored; VirtualProtect(slot,4,protection,&ignored);
    if(old!=reinterpret_cast<void*>(original)){active.store(false);log("IAT target changed during installation");return false;}
    log(profile.c_str()); log("Debug getter installed; native parameters without an override chain to prior IAT target"); return true;
}
static DWORD WINAPI run(void*) {
    HMODULE pinned; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&run),&pinned);
    wchar_t path[32768]; if(GetModuleFileNameW(self,path,32768)){directory=path; directory.resize(directory.find_last_of(L"\\/")+1);}
    log("Independent Debug Hook starting (no Spice SDK)");
    HMODULE game=nullptr,share=nullptr;
    for(unsigned i=0;i<1200&&!stopped.load();i++){
        game=GetModuleHandleW(L"game.dll"); share=GetModuleHandleW(L"libshare-pj.dll");
        if(game&&share) break;
        if(WaitForSingleObject(stop_event,100)!=WAIT_TIMEOUT) return 0;
    }
    if(!game||!share||stopped.load()||!attach(game,share)) return 0;
    const auto ini=directory+L"gfdmdebug.ini";
    UINT keys[max_parameter_count+1]{};
    keys[0]=GetPrivateProfileIntW(L"Keys",L"Menu",VK_F10,ini.c_str());
    const int count=parameter_count(layout);
    for(int i=0;i<count;i++) keys[i+1]=GetPrivateProfileIntW(L"Keys",parameter_at(layout,i)->key,0,ini.c_str());
    for(auto& key:keys) if(!valid_key(key)){log("Invalid hotkey; that binding is disabled (valid range: 0..254)");key=0;}
    for(int i=0;i<=count;i++)bindings[i].store(keys[i]);
    bindings_ready.store(true);
    Edge edges[max_parameter_count+1];
    if(supports_panel){ if(!start_panel()) log("D3D9 Overlay unavailable; hotkeys still active"); }
    else log("V-series profile: hotkeys only; OpenGL Overlay is not implemented in this build");
    while(WaitForSingleObject(stop_event,8)==WAIT_TIMEOUT){
        DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid);
        for(int i=0;i<=count;i++){
            if(!bound_key(keys[i]))continue;
            bool down=(GetAsyncKeyState(keys[i])&0x8000)!=0;
            if(edges[i].update(down,pid==GetCurrentProcessId())){if(i==0)visible.store(!visible.load()); else change(i-1);}
        }
    } return 0;
}
}
extern "C" BOOL __cdecl DebugHookInitialize() {
    using namespace debughook; std::lock_guard lock(lifecycle);
    if(stopped.load()) return FALSE;
    if(worker) return !stopped.load();
    stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr); if(!stop_event)return FALSE;
    worker=CreateThread(nullptr,0,run,nullptr,0,nullptr);
    if(!worker){CloseHandle(stop_event);stop_event=nullptr;return FALSE;} return TRUE;
}
extern "C" void __cdecl DebugHookShutdown() {
    using namespace debughook; std::lock_guard lock(lifecycle);
    stopped.store(true); active.store(false); visible.store(false); switches.restore_all();
    if(stop_event) SetEvent(stop_event); if(worker) WaitForSingleObject(worker,INFINITE);
    // The worker may have finished installing while shutdown was requested.
    active.store(false); visible.store(false); switches.restore_all();
    stop_panel();
    if(slot){DWORD p; if(VirtualProtect(slot,4,PAGE_READWRITE,&p)){InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(original),reinterpret_cast<void*>(getter));DWORD ignored;VirtualProtect(slot,4,p,&ignored);}}
    if(worker){CloseHandle(worker);worker=nullptr;} if(stop_event){CloseHandle(stop_event);stop_event=nullptr;}
}
extern "C" DWORD __cdecl DebugHookGetStatus(){
    using namespace debughook;
    // Retain the prototype status ABI: active, Neverend, Auto Play, panel ready.
    return (active.load()?1u:0u)|(switches.get(1)==1?2u:0u)|(switches.get(2)==1?4u:0u)|(panel_ready.load()?8u:0u);
}
extern "C" DWORD __cdecl DebugHookGetFrames(){return debughook::rendered.load();}
extern "C" BOOL __cdecl DebugHookShowOverlay(BOOL show){if(!debughook::active.load())return FALSE;debughook::visible.store(show!=FALSE);return TRUE;}
extern "C" BOOL __cdecl DebugHookSetSwitch(int index,BOOL enabled){
    return debughook::set_switch(index,enabled!=FALSE);
}
extern "C" int __cdecl DebugHookGetSwitchCount(){return debughook::active.load()?debughook::parameter_count(debughook::layout):0;}
extern "C" const char* __cdecl DebugHookGetSwitchName(int index){
    using namespace debughook;if(!active.load())return nullptr;auto p=parameter_at(layout,index);return p?p->name:nullptr;
}
extern "C" int __cdecl DebugHookGetSwitchValue(int index){
    using namespace debughook;return active.load()&&parameter_at(layout,index)?switches.get(index):-2;
}
extern "C" DWORD __cdecl DebugHookGetSwitchReads(int index){
    using namespace debughook;return active.load()&&parameter_at(layout,index)?reads[index].load(std::memory_order_relaxed):0;
}
extern "C" int __cdecl DebugHookGetHotkey(int index){
    using namespace debughook;
    if(!active.load()||!bindings_ready.load()||index < -1||index>=parameter_count(layout))return -1;
    return int(bindings[index+1].load());
}
extern "C" BOOL __cdecl DebugHookRestoreSwitch(int index){return debughook::restore_switch(index);}
extern "C" void __cdecl DebugHookRestoreNative(){debughook::restore_native();}
extern "C" void __cdecl DebugHookAllOff(){debughook::all_off();}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){debughook::self=module;DisableThreadLibraryCalls(module);DebugHookInitialize();}return TRUE;}
