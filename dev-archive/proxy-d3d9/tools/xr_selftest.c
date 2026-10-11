/* xr_selftest.c - no-game test of the headset output (2026-10-11). Loads ../build/d3d9.dll (the one beside this exe)
 * by full path, makes a windowed 1280x720 D3D9 device through it, clears the back buffer to a slowly changing colour
 * and presents for SECS seconds, with fc3bd_vr_openxr.txt and fc3bd_vr.ini ([vr] RuntimeJson=<arg 1>) written beside
 * the exe. Then reads fc3bd_vr_log.txt and passes when the headset thread submitted frames.
 *
 *   build/xr_selftest.exe <32-bit runtime json> [secs] [pattern]
 *
 * Needs the 32-bit openxr_loader.dll in build/. Opens the runtime's preview window (the simulator's), so do not run
 * it while a game is being driven on the same PC. With no camera shifts the proxy labels every frame eye 0, so both
 * eyes get the same picture; `pattern` ([vr] TestPattern=1) checks the left / right routing instead (red | blue). */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

#define TEST_W          1280
#define TEST_H          720
#define DEFAULT_SECS    10
#define FRAME_SLEEP_MS  16
#define LOG_LINE_MAX    1024

typedef IDirect3D9 *(WINAPI *Create9_t)(UINT);

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }

static void beside_exe(char *out, const char *name) {
    char *sl;
    GetModuleFileNameA(NULL, out, MAX_PATH);
    sl = strrchr(out, '\\');
    if (sl) sl[1] = 0;
    strcat(out, name);
}

int main(int argc, char **argv) {
    char path[MAX_PATH], line[LOG_LINE_MAX];
    HMODULE dll;
    Create9_t create;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *dev = NULL;
    D3DPRESENT_PARAMETERS pp;
    WNDCLASSA wc;
    HWND wnd;
    FILE *f;
    DWORD end;
    int secs = argc > 2 ? atoi(argv[2]) : DEFAULT_SECS, frame = 0, good = 0;
    if (argc < 2) { printf("usage: xr_selftest <32-bit runtime json> [secs] [pattern]\n"); return 2; }
    if (secs <= 0) secs = DEFAULT_SECS;

    beside_exe(path, "fc3bd_vr_openxr.txt");
    f = fopen(path, "w"); if (f) fclose(f);
    beside_exe(path, "fc3bd_vr.ini");
    f = fopen(path, "w");
    if (f) { fprintf(f, "[vr]\nRuntimeJson=%s\nTestPattern=%d\n", argv[1], argc > 3); fclose(f); }

    beside_exe(path, "d3d9.dll");
    dll = LoadLibraryA(path);
    create = dll ? (Create9_t)(void *)GetProcAddress(dll, "Direct3DCreate9") : NULL;
    if (!create) { printf("FAIL: cannot load %s\n", path); return 1; }
    d3d = create(D3D_SDK_VERSION);
    if (!d3d) { printf("FAIL: Direct3DCreate9\n"); return 1; }

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "fc3bd_xr_selftest";
    RegisterClassA(&wc);
    wnd = CreateWindowA(wc.lpszClassName, "fc3bd xr selftest", WS_OVERLAPPEDWINDOW, 0, 0, TEST_W, TEST_H,
                        NULL, NULL, wc.hInstance, NULL);
    memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = TEST_W;
    pp.BackBufferHeight = TEST_H;
    pp.hDeviceWindow = wnd;
    if (FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
                                       D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev))) {
        printf("FAIL: CreateDevice\n");
        return 1;
    }
    end = GetTickCount() + (DWORD)secs * 1000u;
    while (GetTickCount() < end) {
        MSG m;
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&m);
        IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(frame & 255, 128, 255 - (frame & 255)),
                               1.0f, 0);
        IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
        frame++;
        Sleep(FRAME_SLEEP_MS);
    }
    IDirect3DDevice9_Release(dev);
    IDirect3D9_Release(d3d);

    beside_exe(path, "fc3bd_vr_log.txt");
    f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f)) {
        const char *p = strstr(line, "frames submitted ");
        if (strstr(line, "xr:")) fputs(line, stdout);
        if (p && atoi(p + strlen("frames submitted ")) > 0) good = 1;
    }
    if (f) fclose(f);
    printf("%s: %d game frames presented; headset frames submitted: %s\n", good ? "PASS" : "FAIL", frame,
           good ? "yes" : "none");
    return good ? 0 : 1;
}
