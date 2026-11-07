#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <vector>
extern "C" __declspec(dllimport) char __cdecl ReadDebug(int);
static void check(bool value,const char* message){if(!value){fprintf(stderr,"FAIL: %s (%lu)\n",message,GetLastError());ExitProcess(1);}}
static void capture(IDirect3DDevice9* device){
    IDirect3DSurface9 *back=nullptr,*copy=nullptr;D3DSURFACE_DESC desc{};
    check(SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)),"capture backbuffer");
    check(SUCCEEDED(back->GetDesc(&desc)),"capture description");
    check(desc.Format==D3DFMT_X8R8G8B8||desc.Format==D3DFMT_A8R8G8B8,"capture pixel format");
    check(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr)),"capture surface");
    check(SUCCEEDED(device->GetRenderTargetData(back,copy)),"GPU readback");
    D3DLOCKED_RECT locked{};check(SUCCEEDED(copy->LockRect(&locked,nullptr,D3DLOCK_READONLY)),"lock capture");
    int w=int(desc.Width),h=int(desc.Height);std::vector<unsigned char> pixels(size_t(w)*h*4);unsigned changed=0;
    for(int y=0;y<h;y++){
        auto row=static_cast<const unsigned char*>(locked.pBits)+y*locked.Pitch;
        for(int x=0;x<w;x++){
            if(row[x*4]!=80||row[x*4+1]!=50||row[x*4+2]!=20)++changed;
            for(int c=0;c<4;c++)pixels[(size_t(h-1-y)*w+x)*4+c]=row[x*4+c];
        }
    }
    copy->UnlockRect();copy->Release();back->Release();
    check(changed>50000&&changed<unsigned(w*h-10000),"panel pixels and background both present");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+DWORD(pixels.size());
    FILE* file=fopen("overlay-preview.bmp","wb");check(file!=nullptr,"open capture");fwrite(&header,1,sizeof(header),file);fwrite(&info.bmiHeader,1,sizeof(info.bmiHeader),file);fwrite(pixels.data(),1,pixels.size(),file);fclose(file);
}
static LONG WINAPI report_exception(EXCEPTION_POINTERS* p) {HMODULE module=nullptr;char name[32768]{};auto addr=p->ExceptionRecord->ExceptionAddress;GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(addr),&module);GetModuleFileNameA(module,name,32768);fprintf(stderr,"EXCEPTION %lx at %s+%lx EIP=%lx ESP=%lx\n",p->ExceptionRecord->ExceptionCode,name,DWORD(reinterpret_cast<uintptr_t>(addr)-reinterpret_cast<uintptr_t>(module)),p->ContextRecord->Eip,p->ContextRecord->Esp);return EXCEPTION_EXECUTE_HANDLER;}
int main(){
    setvbuf(stdout,nullptr,_IONBF,0);
    SetUnhandledExceptionFilter(report_exception);
    check(ReadDebug(1)==11,"native fixture");
    auto plugin=LoadLibraryW(L"gfdmdebug_test.dll");check(plugin!=nullptr,"plugin load");
    auto status=reinterpret_cast<DWORD(__cdecl*)()>(GetProcAddress(plugin,"DebugHookGetStatus"));
    auto frames=reinterpret_cast<DWORD(__cdecl*)()>(GetProcAddress(plugin,"DebugHookGetFrames"));
    auto show=reinterpret_cast<BOOL(__cdecl*)(BOOL)>(GetProcAddress(plugin,"DebugHookShowOverlay"));
    auto stop=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(plugin,"DebugHookShutdown"));
    for(int i=0;i<250&&!(status()&8);i++)Sleep(20);
    check(status()&8,"D3D9 hooks ready");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"GFDM_DebugHook_Render_Test";
    RegisterClassW(&wc);auto window=CreateWindowW(wc.lpszClassName,L"GFDM DebugHook isolated renderer test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,120,120,820,580,nullptr,nullptr,wc.hInstance,nullptr);
    check(window!=nullptr,"window");
    auto d3d=Direct3DCreate9(D3D_SDK_VERSION);check(d3d!=nullptr,"d3d9");IDirect3DDevice9* dev=nullptr;
    // COPY preserves the rendered buffer for GPU readback after the hooked Present.
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;pp.BackBufferWidth=800;pp.BackBufferHeight=540;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    check(SUCCEEDED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev)),"create device");
    show(TRUE);
    for(int i=0;i<120;i++){
        MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        check(SUCCEEDED(dev->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(20,50,80),1,0)),"clear");
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);dev->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
        check(SUCCEEDED(dev->Present(nullptr,nullptr,nullptr,nullptr)),"present");
        DWORD value=1;dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&value);check(value==FALSE,"render-state restored");
        if(i==30){puts("reset begin");auto before=frames();check(SUCCEEDED(dev->Reset(&pp)),"reset");check(frames()==before,"reset does not render");puts("reset done");}
        if(i==90)capture(dev);
        Sleep(15);
    }
    check(frames()>90,"overlay rendered before and after reset");
    show(FALSE);auto before=frames();dev->Present(nullptr,nullptr,nullptr,nullptr);check(frames()==before,"hidden panel does not draw");
    puts("shutdown begin");stop();check(ReadDebug(2)==12,"shutdown restores IAT");puts("shutdown done");
    dev->Release();d3d->Release();DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    printf("PASS: %lu frames, state restoration, reset, hide and orderly shutdown\n",before);
}

