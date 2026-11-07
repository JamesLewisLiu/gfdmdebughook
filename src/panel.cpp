#include "debug.h"
#include <d3d9.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <mutex>
#include <cfloat>
#include <chrono>
#include <cstdlib>
#include <algorithm>

namespace debughook {
namespace {
std::recursive_mutex graphics;
ImGuiContext* context=nullptr;
IDirect3DDevice9* current=nullptr;
void *present_target=nullptr,*reset_target=nullptr,*swap_target=nullptr;
bool enabled=false;
thread_local bool inside=false;
using Present=HRESULT(WINAPI*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using Swap=HRESULT(WINAPI*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
Present real_present=nullptr; Reset real_reset=nullptr; Swap real_swap=nullptr;
std::chrono::steady_clock::time_point last;

void release_context() {
    if(context){auto previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(context);ImGui_ImplDX9_Shutdown();ImGui::DestroyContext(context);context=nullptr;ImGui::SetCurrentContext(previous);}
    // The backend releases its own reference; never retain a second device reference.
    current=nullptr;
}
bool suitable(HWND window) {
    DWORD pid=0; RECT r{};GetWindowThreadProcessId(window,&pid);
    return window&&pid==GetCurrentProcessId()&&IsWindowVisible(window)&&GetClientRect(window,&r)&&r.right>=320&&r.bottom>=200;
}
void draw(IDirect3DDevice9* device,IDirect3DSurface9* backbuffer,HWND window) {
    std::lock_guard lock(graphics);
    if(!enabled||!visible.load()||!active.load()||!suitable(window)||FAILED(device->TestCooperativeLevel())) return;
    if(current&&current!=device)return;
    auto previous=ImGui::GetCurrentContext();
    if(!context){
        ImGui::SetAllocatorFunctions([](size_t size,void*)->void* {return std::malloc(size);},[](void* memory,void*) {std::free(memory);});
        context=ImGui::CreateContext();ImGui::SetCurrentContext(context);
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.MouseDrawCursor=true;
        ImGui::StyleColorsDark();
        if(!ImGui_ImplDX9_Init(device)){ImGui::DestroyContext(context);context=nullptr;ImGui::SetCurrentContext(previous);return;}
        current=device;last=std::chrono::steady_clock::now();
    }
    ImGui::SetCurrentContext(context);
    auto& io=ImGui::GetIO();D3DSURFACE_DESC desc{};
    if(FAILED(backbuffer->GetDesc(&desc))){ImGui::SetCurrentContext(previous);return;}
    io.DisplaySize=ImVec2(float(desc.Width),float(desc.Height));
    auto now=std::chrono::steady_clock::now();io.DeltaTime=std::chrono::duration<float>(now-last).count();last=now;
    if(io.DeltaTime<=0||io.DeltaTime>0.2f)io.DeltaTime=1.0f/60;
    bool focus=GetAncestor(GetForegroundWindow(),GA_ROOT)==GetAncestor(window,GA_ROOT);
    io.AddFocusEvent(focus);POINT point{};RECT client{};
    bool mouse=focus&&GetCursorPos(&point)&&ScreenToClient(window,&point)&&GetClientRect(window,&client)&&client.right>0&&client.bottom>0;
    io.AddMousePosEvent(mouse?float(point.x)*desc.Width/client.right:-FLT_MAX,mouse?float(point.y)*desc.Height/client.bottom:-FLT_MAX);
    bool swapped=GetSystemMetrics(SM_SWAPBUTTON)!=0;
    io.AddMouseButtonEvent(0,mouse&&(GetAsyncKeyState(swapped?VK_RBUTTON:VK_LBUTTON)&0x8000));
    io.AddMouseButtonEvent(1,mouse&&(GetAsyncKeyState(swapped?VK_LBUTTON:VK_RBUTTON)&0x8000));
    ImGui_ImplDX9_NewFrame();ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(24,24),ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(std::min(600.0f,io.DisplaySize.x-48),std::min(480.0f,io.DisplaySize.y-48)),ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.92f);
    bool open=true;
    if(ImGui::Begin("GFDM Debug Hook",&open,ImGuiWindowFlags_NoCollapse)){
        ImGui::TextUnformatted(profile.c_str());
        if(ImGui::Button("All OFF"))all_off();
        ImGui::SameLine();if(ImGui::Button("Restore Native"))restore_native();
        ImGui::BeginChild("parameters");
        if(ImGui::BeginTable("native-switches",3,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg)){
            ImGui::TableSetupColumn("Debug",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("State",ImGuiTableColumnFlags_WidthFixed,105);
            ImGui::TableSetupColumn("Reads",ImGuiTableColumnFlags_WidthFixed,60);
            ImGui::TableHeadersRow();
            for(int i=0;i<parameter_count(layout);i++){
                const auto* parameter=parameter_at(layout,i);
                ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();
                int value=switches.get(i);bool enabled=value==1;
                if(ImGui::Checkbox(parameter->label,&enabled))set_switch(i,enabled);
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",parameter->name);
                ImGui::TableNextColumn();
                value=switches.get(i);
                ImGui::TextUnformatted(value<0?"Native":value?"ON":"OFF");
                if(value>=0){ImGui::SameLine();if(ImGui::SmallButton("Reset"))restore_switch(i);}
                ImGui::TableNextColumn();ImGui::Text("%u",reads[i].load(std::memory_order_relaxed));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }
    ImGui::End();if(!open)visible.store(false);ImGui::Render();
    IDirect3DStateBlock9* state=nullptr; IDirect3DSurface9* targets[4]={},*depth=nullptr;
    D3DCAPS9 caps{};D3DVIEWPORT9 viewport{};
    bool ready=SUCCEEDED(device->GetDeviceCaps(&caps))&&SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&state))&&SUCCEEDED(state->Capture())&&SUCCEEDED(device->GetViewport(&viewport));
    unsigned count=std::min<unsigned>(caps.NumSimultaneousRTs,4);
    if(ready){
        for(unsigned i=0;i<count;i++){HRESULT hr=device->GetRenderTarget(i,&targets[i]);if(FAILED(hr)&&hr!=D3DERR_NOTFOUND)ready=false;}
        HRESULT hr=device->GetDepthStencilSurface(&depth);if(FAILED(hr)&&hr!=D3DERR_NOTFOUND)ready=false;
    }
    if(ready){
        device->SetDepthStencilSurface(nullptr);
        for(unsigned i=1;i<count;i++)device->SetRenderTarget(i,nullptr);
        if(SUCCEEDED(device->SetRenderTarget(0,backbuffer))){
            D3DVIEWPORT9 vp{0,0,desc.Width,desc.Height,0,1};device->SetViewport(&vp);
            if(SUCCEEDED(device->BeginScene())){ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());device->EndScene();rendered.fetch_add(1);}
        }
        for(unsigned i=0;i<count;i++)device->SetRenderTarget(i,targets[i]);
        device->SetDepthStencilSurface(depth);state->Apply();device->SetViewport(&viewport);
    }
    for(auto* t:targets)if(t)t->Release();if(depth)depth->Release();if(state)state->Release();
    ImGui::SetCurrentContext(previous);
}
HRESULT WINAPI hooked_present(IDirect3DDevice9* dev,const RECT* a,const RECT* b,HWND window,const RGNDATA* c){
    if(inside)return real_present(dev,a,b,window,c);inside=true;
    D3DDEVICE_CREATION_PARAMETERS cp{};IDirect3DSurface9* back=nullptr;
    if(SUCCEEDED(dev->GetCreationParameters(&cp))&&SUCCEEDED(dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))){draw(dev,back,window?window:cp.hFocusWindow);back->Release();}
    auto result=real_present(dev,a,b,window,c);inside=false;return result;
}
HRESULT WINAPI hooked_swap(IDirect3DSwapChain9* chain,const RECT* a,const RECT* b,HWND window,const RGNDATA* c,DWORD flags){
    if(inside)return real_swap(chain,a,b,window,c,flags);inside=true;
    IDirect3DDevice9* dev=nullptr;IDirect3DSurface9* back=nullptr;D3DPRESENT_PARAMETERS pp{};
    if(SUCCEEDED(chain->GetDevice(&dev))){
        if(SUCCEEDED(chain->GetPresentParameters(&pp))&&SUCCEEDED(chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back))){draw(dev,back,window?window:pp.hDeviceWindow);back->Release();} dev->Release();
    }
    auto result=real_swap(chain,a,b,window,c,flags);inside=false;return result;
}
HRESULT WINAPI hooked_reset(IDirect3DDevice9* dev,D3DPRESENT_PARAMETERS* pp){
    std::lock_guard lock(graphics);
    if(current==dev)release_context();
    return real_reset(dev,pp);
}
}
bool start_panel(){
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=self;wc.lpszClassName=L"GFDM_DEBUG_PROBE";
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    HWND window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,32,32,nullptr,nullptr,self,nullptr);
    auto d3d=Direct3DCreate9(D3D_SDK_VERSION);IDirect3DDevice9* dev=nullptr;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=32;pp.BackBufferHeight=32;
    HRESULT hr=window&&d3d?d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev):E_FAIL;
    bool success=false;
    if(SUCCEEDED(hr)){
        auto v=*reinterpret_cast<void***>(dev);present_target=v[17];reset_target=v[16];
        IDirect3DSwapChain9* swap=nullptr;if(SUCCEEDED(dev->GetSwapChain(0,&swap))){swap_target=(*reinterpret_cast<void***>(swap))[3];swap->Release();}
        if(MH_Initialize()==MH_OK && MH_CreateHook(present_target,reinterpret_cast<void*>(hooked_present),reinterpret_cast<void**>(&real_present))==MH_OK && MH_CreateHook(reset_target,reinterpret_cast<void*>(hooked_reset),reinterpret_cast<void**>(&real_reset))==MH_OK && swap_target && MH_CreateHook(swap_target,reinterpret_cast<void*>(hooked_swap),reinterpret_cast<void**>(&real_swap))==MH_OK){
            {std::lock_guard lock(graphics);enabled=true;}
            success=MH_EnableHook(present_target)==MH_OK&&MH_EnableHook(reset_target)==MH_OK&&MH_EnableHook(swap_target)==MH_OK;
        }
        dev->Release();
    }
    if(d3d)d3d->Release();if(window)DestroyWindow(window);UnregisterClassW(wc.lpszClassName,self);
    if(!success)stop_panel();else {panel_ready.store(true);log("Standalone D3D9 Overlay hooks ready");}return success;
}
void stop_panel(){
    // Disable detours without holding the render lock: suspended threads may own it.
    if(present_target)MH_DisableHook(present_target);if(reset_target)MH_DisableHook(reset_target);if(swap_target)MH_DisableHook(swap_target);
    std::lock_guard lock(graphics);enabled=false;panel_ready.store(false);release_context();
    // Keep trampoline allocations and this DLL alive until process exit for in-flight calls.
}
}
