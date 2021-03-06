/*  ZeroGS KOSMOS
 *  Copyright (C) 2005-2006 zerofrog@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#if defined(_WIN32)
#include <windows.h>
#include "Win32.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <list>
#include <vector>
#include <map>
#include <string>
using namespace std;

#include "GS.h"
#include "Mem.h"
#include "Regs.h"

#include "zerogs.h"
#include "targets.h"
#include "ZeroGSShaders/zerogsshaders.h"

#ifdef __MSCW32__
#pragma warning(disable:4244)
#endif

GSinternal gs;
char GStitle[256];
GSconf conf;
int ppf;
primInfo *prim;
FILE *gsLog;
int g_GSMultiThreaded = 0;
void (*GSirq)();
u8* g_pBasePS2Mem = NULL;
int g_TransferredToGPU = 0;
string s_strIniPath="inis/zerogs.ini";

static BOOL g_bHidden = 0;
int g_GameSettings = 0;

// statistics
u32 g_nGenVars = 0, g_nTexVars = 0, g_nAlphaVars = 0, g_nResolve = 0;

#define VER 96
const unsigned char zgsversion  = PS2E_GS_VERSION;
unsigned char zgsrevision = 0; // revision and build gives plugin version
unsigned char zgsbuild    = VER;
unsigned char zgsminor = 7;

#ifdef _DEBUG
char *libraryName     = "ZeroGS OpenGL (Debug) ";
#elif defined(RELEASE_TO_PUBLIC)
char *libraryName     = "ZeroGS KOSMOS OpenGL ";
#else
char *libraryName     = "ZeroGS OpenGL (Dev) ";
#endif

static const char* s_aa[3] = { "AA none |", "AA 2x |", "AA 4x |" };
static const char* pbilinear[] = { "off", "normal", "forced" };

extern GIFRegHandler g_GIFPackedRegHandlers[];
extern GIFRegHandler g_GIFRegHandlers[];
GIFRegHandler g_GIFTempRegHandlers[16] = {0};
extern int g_nPixelShaderVer;
extern int g_nFrameRender;
extern int g_nFramesSkipped;

#ifdef RELEASE_TO_PUBLIC
#define g_bWriteProfile 0
#else
BOOL g_bWriteProfile = 0;
#endif

int s_frameskipping = 0;
u32 CALLBACK PS2EgetLibType() {
    return PS2E_LT_GS;
}

char* CALLBACK PS2EgetLibName() {
    return libraryName;
}

u32 CALLBACK PS2EgetLibVersion2(u32 type) {
    return (zgsversion<<16) | (zgsrevision<<8) | zgsbuild | (zgsminor << 24);
}

static u64 luPerfFreq;

#ifdef _WIN32

HWND GShwnd = NULL;

void SysMessage(char *fmt, ...) {
    va_list list;
    char tmp[512];

    va_start(list,fmt);
    vsprintf(tmp,fmt,list);
    va_end(list);
    MessageBox(0, tmp, "GSsoftdx Msg", 0);
}
#else

GLWindow GLWin;

#endif

#ifdef GS_LOG
void __Log(char *fmt, ...) {
    va_list list;

    if (!conf.log) return;

    va_start(list, fmt);
    vfprintf(gsLog, fmt, list);
    va_end(list);
}
#endif

void ERROR_LOG(char *fmt, ...) {
    va_list list;

    va_start(list, fmt);
    fprintf(gsLog, "ZeroGS: ");
    vfprintf(gsLog, fmt, list);
    va_end(list);

    va_start(list, fmt);
    printf("ZeroGS: ");
    vprintf(fmt, list);
    va_end(list);
}

void CALLBACK GSsetBaseMem(void* pmem) {
    g_pBasePS2Mem = (u8*)pmem;
}

extern int VALIDATE_THRESH;
extern u32 TEXDESTROY_THRESH;
int g_LastCRC = 0;
void CALLBACK GSsetGameCRC(int crc, int options)
{
    VALIDATE_THRESH = 8;
    g_GameSettings = conf.gamesettings|options;
    conf.mrtdepth = 0;//!(conf.gamesettings&GAME_DISABLEMRTDEPTH);

    if( !conf.mrtdepth ) {
        ERROR_LOG("Disabling MRT depth writing\n");
    }

    g_GameSettings |= GAME_PATH3HACK;
    g_LastCRC = crc;

    switch(crc) {
        case 0x54A548B4: // crash n burn
            // overbright
            break;

        case 0xA3D63039: // xenosaga(j)
        case 0x0E7807B2: // xenosaga(u)
            g_GameSettings |= GAME_DOPARALLELCTX;
            VALIDATE_THRESH = 64;
            TEXDESTROY_THRESH = 32;
            break;

        case 0x7D2FE035: // espgaluda (j)
            VALIDATE_THRESH = 24;
            //g_GameSettings |= GAME_BIGVALIDATE;
            break;
    }
}

void CALLBACK GSsetFrameSkip(int frameskip)
{
    s_frameskipping |= frameskip;
    if( frameskip && g_nFrameRender > 1 ) {
        
        for(int i = 0; i < 16; ++i) {
            g_GIFPackedRegHandlers[i] = GIFPackedRegHandlerNOP;
        }

        // still keep certain handlers
        g_GIFPackedRegHandlers[6] = GIFRegHandlerTEX0_1;
        g_GIFPackedRegHandlers[7] = GIFRegHandlerTEX0_2;
        g_GIFPackedRegHandlers[14] = GIFPackedRegHandlerA_D;

        g_GIFRegHandlers[0] = GIFRegHandlerNOP;
        g_GIFRegHandlers[1] = GIFRegHandlerNOP;
        g_GIFRegHandlers[2] = GIFRegHandlerNOP;
        g_GIFRegHandlers[3] = GIFRegHandlerNOP;
        g_GIFRegHandlers[4] = GIFRegHandlerNOP;
        g_GIFRegHandlers[5] = GIFRegHandlerNOP;
        g_GIFRegHandlers[12] = GIFRegHandlerNOP;
        g_GIFRegHandlers[13] = GIFRegHandlerNOP;
        g_GIFRegHandlers[26] = GIFRegHandlerNOP;
        g_GIFRegHandlers[27] = GIFRegHandlerNOP;
        g_nFrameRender = 0;
    }
    else if( !frameskip && g_nFrameRender <= 0 ) {
        g_nFrameRender = 1;

        if( g_GIFTempRegHandlers[0] == NULL ) return; // not init yet

        // restore
        memcpy(g_GIFPackedRegHandlers, g_GIFTempRegHandlers, sizeof(g_GIFTempRegHandlers));

        g_GIFRegHandlers[0] = GIFRegHandlerPRIM;
        g_GIFRegHandlers[1] = GIFRegHandlerRGBAQ;
        g_GIFRegHandlers[2] = GIFRegHandlerST;
        g_GIFRegHandlers[3] = GIFRegHandlerUV;
        g_GIFRegHandlers[4] = GIFRegHandlerXYZF2;
        g_GIFRegHandlers[5] = GIFRegHandlerXYZ2;
        g_GIFRegHandlers[12] = GIFRegHandlerXYZF3;
        g_GIFRegHandlers[13] = GIFRegHandlerXYZ2;
        g_GIFRegHandlers[26] = GIFRegHandlerPRMODECONT;
        g_GIFRegHandlers[27] = GIFRegHandlerPRMODE;
    }
}

void CALLBACK GSreset() {

    memset(&gs, 0, sizeof(gs));

    ZeroGS::GSStateReset();
    
    gs.prac = 1;
    prim = &gs._prim[0];
    gs.nTriFanVert = -1;
    gs.imageTransfer = -1;
    gs.q = 1;
}

void CALLBACK GSgifSoftReset(int mask)
{
    if( mask & 1 ) memset(&gs.path1, 0, sizeof(gs.path1));
    if( mask & 2 ) memset(&gs.path2, 0, sizeof(gs.path2));
    if( mask & 4 ) memset(&gs.path3, 0, sizeof(gs.path3));
    gs.imageTransfer = -1;
    gs.q = 1;
    gs.nTriFanVert = -1;
}

s32 CALLBACK GSinit()
{
    memcpy(g_GIFTempRegHandlers, g_GIFPackedRegHandlers, sizeof(g_GIFTempRegHandlers));

#ifdef GS_LOG
    gsLog = fopen("logs/gsLog.txt", "w");
    if (gsLog == NULL) {
        gsLog = fopen("gsLog.txt", "w");
        if (gsLog == NULL) {
            SysMessage("Can't create gsLog.txt"); return -1;
        }
    }
    setvbuf(gsLog, NULL,  _IONBF, 0);
    GS_LOG("GSinit\n");
#endif

#ifdef __LINUX__
    char strcurdir[256];
    getcwd(strcurdir, 256);
    s_strIniPath = strcurdir;
    s_strIniPath += "/inis/zerogs.ini";
#endif

    GSreset();

#ifdef GS_LOG
    GS_LOG("GSinit ok\n");
#endif

    return 0;
}

void CALLBACK GSshutdown()
{
#ifdef GS_LOG
    fclose(gsLog);
#endif
}

// keyboard functions
void OnKeyboardF5(int shift)
{
    char strtitle[256];
    if( shift ) {
        if( g_nPixelShaderVer == SHADER_REDUCED ) {
            conf.bilinear = 0;
            sprintf(strtitle, "reduced shaders don't support bilinear filtering");
        }
        else {
            conf.bilinear = (conf.bilinear+1)%3;
            sprintf(strtitle, "bilinear filtering - %s", pbilinear[conf.bilinear]);
        }
    }
    else {
        conf.interlace++;
        if( conf.interlace > 2 ) conf.interlace = 0;
        if( conf.interlace < 2 ) sprintf(strtitle, "interlace on - mode %d", conf.interlace);
        else sprintf(strtitle, "interlace off");
    }
    
    ZeroGS::AddMessage(strtitle);
    SaveConfig();
}

void OnKeyboardF6(int shift)
{
    char strtitle[256];
    if( shift ) {
        conf.aa--; // -1
        if( conf.aa < 0 ) conf.aa = 4;
        sprintf(strtitle, "anti-aliasing - %s", s_aa[conf.aa]);
        ZeroGS::SetAA(conf.aa);
    }
    else {
        conf.aa++;
        if( conf.aa > 4 ) conf.aa = 0;
        sprintf(strtitle, "anti-aliasing - %s", s_aa[conf.aa]);
        ZeroGS::SetAA(conf.aa);
    }
    
    ZeroGS::AddMessage(strtitle);
    SaveConfig();
}

void OnKeyboardF7(int shift)
{
    char strtitle[256];
    if( shift ) {
        extern BOOL g_bDisplayFPS;
        g_bDisplayFPS ^= 1;
    }
    else {
        conf.options ^= GSOPTION_WIREFRAME;
        glPolygonMode(GL_FRONT_AND_BACK, (conf.options&GSOPTION_WIREFRAME)?GL_LINE:GL_FILL);
        sprintf(strtitle, "wireframe rendering - %s", (conf.options&GSOPTION_WIREFRAME)?"on":"off");
    }
}

void OnKeyboardF9(int shift)
{
    char strtitle[256];
    g_GameSettings ^= GAME_PATH3HACK;
    sprintf(strtitle, "path3 hack - %s", (g_GameSettings&GAME_PATH3HACK) ? "on" : "off");
    ZeroGS::AddMessage(strtitle);
    //SaveConfig();
}

#ifdef _WIN32

#ifdef _DEBUG
HANDLE g_hCurrentThread = NULL;
#endif

LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static int nWindowWidth = 0, nWindowHeight = 0;

    switch( msg ) {
        case WM_DESTROY:
            PostQuitMessage( 0 );
            return 0;

        case WM_KEYDOWN:
//          switch(wParam) {
//              case VK_ESCAPE:
//                  SendMessage(hWnd, WM_DESTROY, 0L, 0L);
//                  break;
//          }
            break;

        case WM_ACTIVATE:

            if( wParam != WA_INACTIVE ) {
                //printf("restoring device\n");
                ZeroGS::Restore();
            }

            break;

        case WM_SIZE:
            nWindowWidth = lParam&0xffff;
            nWindowHeight = lParam>>16;
            ZeroGS::ChangeWindowSize(nWindowWidth, nWindowHeight);

            break;

        case WM_SIZING:
            // if button is 0, then just released so can resize
            if( GetSystemMetrics(SM_SWAPBUTTON) ? !GetAsyncKeyState(VK_RBUTTON) : !GetAsyncKeyState(VK_LBUTTON) ) {
                ZeroGS::SetChangeDeviceSize(nWindowWidth, nWindowHeight);
            }
            break;

        case WM_SETCURSOR:
            SetCursor(NULL);
            break;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}

extern HINSTANCE hInst;
void CALLBACK GSconfigure() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_CONFIG),
              GetActiveWindow(),  
              (DLGPROC)ConfigureDlgProc);

    if( g_nPixelShaderVer == SHADER_REDUCED )
        conf.bilinear = 0;
}


s32 CALLBACK GSopen(void *pDsp, char *Title, int multithread) {

    g_GSMultiThreaded = multithread;

#ifdef GS_LOG
    GS_LOG("GSopen\n");
#endif

#ifdef _DEBUG
    g_hCurrentThread = GetCurrentThread();
#endif

    assert( GSirq != NULL );
    LoadConfig();

    strcpy(GStitle, Title);
    
    RECT rc, rcdesktop;
    rc.left = 0; rc.top = 0;
    rc.right = conf.width; rc.bottom = conf.height;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, 
                    GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                    "PS2EMU_ZEROGS", NULL };
    RegisterClassEx( &wc );

    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    GetWindowRect(GetDesktopWindow(), &rcdesktop);

    GShwnd = CreateWindow( "PS2EMU_ZEROGS", "ZeroGS", WS_OVERLAPPEDWINDOW,
        (rcdesktop.right-(rc.right-rc.left))/2, (rcdesktop.bottom-(rc.bottom-rc.top))/2,
        rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, wc.hInstance, NULL );

    if(GShwnd == NULL) {
        GS_LOG("Failed to create window. Exiting...");
        return -1;
    }

    if( pDsp != NULL )
        *(HWND*)pDsp = GShwnd;

    ERROR_LOG("creating\n");
    //if (conf.record) recOpen();
    if( !ZeroGS::Create(conf.width, conf.height) )
        return -1;

    ERROR_LOG("initialization successful\n");

    if( conf.bilinear == 2 ) {
        ZeroGS::AddMessage("bilinear filtering - forced", 1000);
    }
    else if( conf.bilinear == 1 ) {
        ZeroGS::AddMessage("bilinear filtering - normal", 1000);
    }
    if( conf.aa ) {
        char strtitle[64];
        sprintf(strtitle, "anti-aliasing - %s", s_aa[conf.aa], 1000);
        ZeroGS::AddMessage(strtitle);
    }

    // set just in case
    SetWindowLongPtr(GShwnd, GWLP_WNDPROC, (LPARAM)(WNDPROC)MsgProc);

    ShowWindow( GShwnd, SW_SHOWDEFAULT );
    UpdateWindow( GShwnd );
    //SetForegroundWindow(GShwnd);
    SetFocus(GShwnd);

    conf.winstyle = GetWindowLong( GShwnd, GWL_STYLE );
    conf.winstyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style

#ifdef GS_LOG
    GS_LOG("GSopen ok\n");
#endif

    LARGE_INTEGER temp;
    QueryPerformanceFrequency(&temp);
    luPerfFreq = temp.QuadPart;

    gs.path1.mode = 0;
    gs.path2.mode = 0;
    gs.path3.mode = 0;

    return 0;
}

void ProcessMessages()
{
    MSG msg; 
    ZeroMemory( &msg, sizeof(msg) );
    while( 1 ) {
        if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
        {
            switch( msg.message ) {
                case WM_KEYDOWN :
                    if( msg.wParam == VK_F5 ) {
                        OnKeyboardF5(GetKeyState(VK_SHIFT)&0x8000);
                    }
                    else if( msg.wParam == VK_F6 ) {
                        OnKeyboardF6(GetKeyState(VK_SHIFT)&0x8000);
                    }
                    else if( msg.wParam == VK_F7 ) {
                        OnKeyboardF7(GetKeyState(VK_SHIFT)&0x8000);
                    }
                    else if( msg.wParam == VK_F9 ) {
                        OnKeyboardF9(GetKeyState(VK_SHIFT)&0x8000);
                    }
                    else if( msg.wParam == VK_ESCAPE ) {

                        if( conf.options & GSOPTION_FULLSCREEN ) {
                            // destroy that msg                 
                            conf.options &= ~GSOPTION_FULLSCREEN;
                            conf.winstyle = GetWindowLong( GShwnd, GWL_STYLE );
                            conf.winstyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style
                            ZeroGS::ChangeDeviceSize(conf.width, conf.height);
                            UpdateWindow(GShwnd);
                            continue; // so that msg doesn't get sent
                        }
                        else {
                            SendMessage(GShwnd, WM_DESTROY, 0, 0);
                            g_bHidden = 1;
                            return;
                        }
                    }
            
                    break;
            }

            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
            break;
    }

    if( (GetKeyState(VK_MENU)&0x8000) && (GetKeyState(VK_RETURN)&0x8000) ) {
        conf.options ^= GSOPTION_FULLSCREEN;

        if( conf.options & GSOPTION_FULLSCREEN ) {
            conf.winstyle = GetWindowLong( GShwnd, GWL_STYLE );
            conf.winstyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style
        }

        ZeroGS::SetChangeDeviceSize(
            (conf.options&GSOPTION_FULLSCREEN) ? 1280 : conf.width,
            (conf.options&GSOPTION_FULLSCREEN) ? 960 : conf.height);
    }

//  if( conf.fullscreen && (GetKeyState(VK_ESCAPE)&0x8000)) {
//      conf.fullscreen &= ~GSOPTION_FULLSCREEN;
//      ZeroGS::SetChangeDeviceSize(conf.width, conf.height);
//  }

    //if( conf.interlace && g_nGenVars + g_nTexVars + g_nAlphaVars + g_nResolve == 0 )
    //  CSR->FIELD = 0; // 0 should always be the repeating at 0
}

#else // linux

s32 CALLBACK GSopen(void *pDsp, char *Title, int multithread)
{
#ifdef GS_LOG
    GS_LOG("GSopen\n");
#endif

    assert( GSirq != NULL );
    LoadConfig();

    strcpy(GStitle, Title);

    GLWin.dpy = XOpenDisplay(0);
    GLWin.screen = DefaultScreen(GLWin.dpy);

    if( pDsp != NULL )
        *(Display**)pDsp = GLWin.dpy;

    ERROR_LOG("creating\n");
    //if (conf.record) recOpen();
    if( !ZeroGS::Create(conf.width, conf.height) )
        return -1;

    ERROR_LOG("initialization successful\n");

    if( conf.bilinear == 2 ) {
        ZeroGS::AddMessage("bilinear filtering - forced", 1000);
    }
    else if( conf.bilinear == 1 ) {
        ZeroGS::AddMessage("bilinear filtering - normal", 1000);
    }
    if( conf.aa ) {
        char strtitle[64];
        sprintf(strtitle, "anti-aliasing - %s", s_aa[conf.aa], 1000);
        ZeroGS::AddMessage(strtitle);
    }

#ifdef GS_LOG
    GS_LOG("GSopen ok\n");
#endif

    gs.path1.mode = 0;
    gs.path2.mode = 0;
    gs.path3.mode = 0;
    luPerfFreq = 1;

    return 0;
}

void ProcessMessages()
{
    XEvent event;
//    KeySym key;
//    vector<XEvent> keyevents;

    // check resizing
    while(XCheckTypedEvent(GLWin.dpy, ConfigureNotify, &event)) {
        if ((event.xconfigure.width != GLWin.width) || (event.xconfigure.height != GLWin.height)) {
            ZeroGS::ChangeWindowSize(event.xconfigure.width, event.xconfigure.height);
            GLWin.width = event.xconfigure.width;
            GLWin.height = event.xconfigure.height;
        }
    }

//    while (XPending(GLWin.dpy) > 0) {
//        XNextEvent(GLWin.dpy, &event);
//        keyevents.push_back(event);
//
//        switch (event.type) {
//        case ConfigureNotify:
//            if ((event.xconfigure.width != GLWin.width) || 
//                (event.xconfigure.height != GLWin.height))
//                {
//                    ZeroGS::ChangeWindowSize(event.xconfigure.width, event.xconfigure.height);
//                    GLWin.width = event.xconfigure.width;
//                    GLWin.height = event.xconfigure.height;
//                }
//            break;
//        default:
//            break;
//        }
//    }
//
//    // push back all the key events for the PAD plugins, etc
//    for(vector<XEvent>::iterator it = keyevents.begin(); it != keyevents.end(); ++it)
//        XPutBackEvent(GLWin.dpy, &(*it));
}

#endif // linux

void CALLBACK GSclose() {
    ZeroGS::Destroy(1);

#ifdef _WIN32
    if( GShwnd != NULL ) {
        DestroyWindow(GShwnd);
        GShwnd = NULL;
    }
#else
    if( GLWin.dpy != NULL ) {
        XCloseDisplay(GLWin.dpy);
        GLWin.dpy = NULL;
    }
#endif
}

void CALLBACK GSirqCallback(void (*callback)()) {
    GSirq = callback;
}

void CALLBACK GSwriteCSR(u32 write)
{
    gs.CSRw = write;
}

void CALLBACK GSchangeSaveState(int newstate, const char* filename)
{
    char str[255];
    sprintf(str, "save state %d", newstate);
    ZeroGS::AddMessage(str);
}

void CALLBACK GSmakeSnapshot(char *path)
{
    FILE *bmpfile;
    char filename[256];     
    u32 snapshotnr = 0;
    
    // increment snapshot value & try to get filename
    for (;;) {
        snapshotnr++;

        sprintf(filename,"%ssnap%03ld.%s", path, snapshotnr, (conf.options&GSOPTION_TGASNAP)?"bmp":"jpg");

        bmpfile=fopen(filename,"rb");
        if (bmpfile == NULL) break;
        fclose(bmpfile);
    }

    // try opening new snapshot file
    if((bmpfile=fopen(filename,"wb"))==NULL) {
        char strdir[255];
        
#ifdef _WIN32
        sprintf(strdir, "%s", path);
        CreateDirectory(strdir, NULL);
#else
        sprintf(strdir, "mkdir %s", path);
        system(strdir);
#endif

        if((bmpfile=fopen(filename,"wb"))==NULL) return;
    }

    fclose(bmpfile);

    // get the bits
    ZeroGS::SaveSnapshot(filename);
}

int UPDATE_FRAMES = 16;
int g_nFrame = 0;
int g_nRealFrame = 0;

float fFPS = 0;

void CALLBACK GSvsync(int interlace)
{
#ifdef GS_LOG
    GS_LOG("\nGSvsync\n\n");
#endif

    static u32 dwTime = timeGetTime();
    static int nToNextUpdate = 1;
    char strtitle[256];

    GL_REPORT_ERRORD();
    
    g_nRealFrame++;

    ZeroGS::RenderCRTC(!interlace);

#ifndef _WIN32
    //XLockDisplay(GLWin.dpy);
#endif
    
    ProcessMessages();

    if( --nToNextUpdate <= 0 ) {

        u32 d = timeGetTime();
        fFPS = UPDATE_FRAMES * 1000.0f / (float)max(d-dwTime,1);
        dwTime = d;
        g_nFrame += UPDATE_FRAMES;

#ifdef RELEASE_TO_PUBLIC
        const char* g_pShaders[4] = { "full", "reduced", "accurate", "accurate-reduced" };

        sprintf(strtitle, "ZeroGS KOSMOS 0.%d.%d %.1f fps | %s%s%s%s %s (%.1f)", zgsbuild, zgsminor, fFPS,
            (conf.interlace < 2) ? "interlace | " : "",
            conf.bilinear ? (conf.bilinear==2?"forced bilinear | ":"bilinear | ") : "",
            conf.aa ? s_aa[conf.aa] : "",
            (g_GameSettings&GAME_FFXHACK) ? "ffxhack | " : "",
            g_pShaders[g_nPixelShaderVer], (ppf&0xfffff)/(float)UPDATE_FRAMES);
#else
        sprintf(strtitle, "%d | %.1f fps (sk:%d%%) | g: %.1f, t: %.1f, a: %.1f, r: %.1f | p: %.1f | tex: %d %d (%d kbpf)", g_nFrame, fFPS, 
            100*g_nFramesSkipped/g_nFrame,
            g_nGenVars/(float)UPDATE_FRAMES, g_nTexVars/(float)UPDATE_FRAMES, g_nAlphaVars/(float)UPDATE_FRAMES,
            g_nResolve/(float)UPDATE_FRAMES, (ppf&0xfffff)/(float)UPDATE_FRAMES,
            ZeroGS::g_MemTargs.listTargets.size(), ZeroGS::g_MemTargs.listClearedTargets.size(), g_TransferredToGPU>>10);
        //_snprintf(strtitle, 512, "%x %x", *(int*)(g_pbyGSMemory + 256 * 0x3e0c + 4), *(int*)(g_pbyGSMemory + 256 * 0x3e04 + 4));

#endif

//        if( g_nFrame > 100 && fFPS > 60.0f ) {
//            printf("set profile\n");
//            g_bWriteProfile = 1;
//        }

#ifdef _WIN32
        if( !(conf.options&GSOPTION_FULLSCREEN) )
            SetWindowText(GShwnd, strtitle);
#else // linux
        XTextProperty prop;
        memset(&prop, 0, sizeof(prop));
        char* ptitle = strtitle;
        if( XStringListToTextProperty(&ptitle, 1, &prop) )
            XSetWMName(GLWin.dpy, GLWin.win, &prop);
        XFree(prop.value);
#endif

        if( fFPS < 16 ) UPDATE_FRAMES = 4;
        else if( fFPS < 32 ) UPDATE_FRAMES = 8;
        else UPDATE_FRAMES = 16;

        nToNextUpdate = UPDATE_FRAMES;

        g_TransferredToGPU = 0;
        g_nGenVars = 0;
        g_nTexVars = 0;
        g_nAlphaVars = 0;
        g_nResolve = 0;
        ppf = 0;
        g_nFramesSkipped = 0;
    }

#ifndef RELEASE_TO_PUBLIC
    if( g_bWriteProfile ) {
        //g_bWriteProfile = 0;
        DVProfWrite("prof.txt", UPDATE_FRAMES);
        DVProfClear();
    }
#endif
    GL_REPORT_ERRORD();

#ifndef _WIN32
    //XUnlockDisplay(GLWin.dpy);
#endif
}

void GIFtag(pathInfo *path, u32 *data) {

    path->tag.nloop = data[0] & 0x7fff;
    path->tag.eop   = (data[0] >> 15) & 0x1;
    u32 tagpre   = (data[1] >> 14) & 0x1;
    u32 tagprim  = (data[1] >> 15) & 0x7ff;
    u32 tagflg   = (data[1] >> 26) & 0x3;
    path->tag.nreg  = (data[1] >> 28)<<2;
    if (path->tag.nreg == 0) path->tag.nreg = 64;

    gs.q = 1;

#ifdef GS_LOG
//  GS_LOG("GIFtag: %8.8lx_%8.8lx_%8.8lx_%8.8lx: EOP=%d, NLOOP=%x, FLG=%x, NREG=%d, PRE=%d\n",
//          data[3], data[2], data[1], data[0],
//          path->tag.eop, path->tag.nloop, tagflg, path->tag.nreg, tagpre);
#endif

    path->mode = tagflg+1;

    switch (tagflg) {
        case 0x0:
            path->regs = *(u64 *)(data+2);
            path->regn = 0;
            if (tagpre)
                GIFRegHandlerPRIM((u32*)&tagprim);

            break;

        case 0x1:
            path->regs = *(u64 *)(data+2);
            path->regn = 0;
            break;
    }
}

void _GSgifPacket(pathInfo *path, u32 *pMem) { // 128bit

    int reg = (int)((path->regs >> path->regn) & 0xf);
    g_GIFPackedRegHandlers[reg](pMem);

    path->regn += 4;
    if (path->tag.nreg == path->regn) {
        path->regn = 0;
        path->tag.nloop--;
    }
}

void _GSgifRegList(pathInfo *path, u32 *pMem) { // 64bit
    int reg;

    reg = (int)((path->regs >> path->regn) & 0xf);

    g_GIFRegHandlers[reg](pMem);
    path->regn += 4;
    if (path->tag.nreg == path->regn) {
        path->regn = 0;
        path->tag.nloop--;
    }
}

static int nPath3Hack = 0;

void CALLBACK GSgetLastTag(u64* ptag)
{
//    int mode = s_pLastPath->mode > 0 ? s_pLastPath->mode-1 : 0;
//    *(u32*)ptag = s_pLastPath->tag.nloop|(s_pLastPath->tag.eop<<15);
//    *((u32*)ptag+1) = (mode<<26)|(s_pLastPath->regn<<28);
    *(u32*)ptag = nPath3Hack;
    nPath3Hack = 0;
}

void _GSgifTransfer(pathInfo *path, u32 *pMem, u32 size)
{
#ifdef _WIN32
    assert( g_hCurrentThread == GetCurrentThread() );
#endif

#ifdef _DEBUG
    if( conf.log & 0x20 ) {
        static int nSaveIndex=0;
        __Log("%d: p:%d %x\n", nSaveIndex++, (path==&gs.path3)?3:(path==&gs.path2?2:1), size);
        int vals[4] = {0};
        for(int i = 0; i < size; i++) {
            for(int j = 0; j < 4; ++j )
                vals[j] ^= pMem[4*i+j];
        }
        __Log("%x %x %x %x\n", vals[0], vals[1], vals[2], vals[3]);
    }
#endif
    //s_pLastPath = path;
#ifdef _DEBUG
    if( conf.log & 0x20 ) {
        static int nSaveIndex=0;
        __Log("%d: p:%d %x\n", nSaveIndex++, (path==&gs.path3)?3:(path==&gs.path2?2:1), size);
        int vals[4] = {0};
        for(int i = 0; i < size; ++i) {
            for(int j = 0; j < 4; ++j )
                vals[j] ^= pMem[4*i+j];
        }

        __Log("%x %x %x %x\n", vals[0], vals[1], vals[2], vals[3]);
    }
#endif

    while(size > 0)
    {
        //LOG(_T("Transfer(%08x, %d) START\n"), pMem, size);
        if(path->tag.nloop == 0) {
            GIFtag(path, pMem);
            pMem+= 4;
            size--;

            if( (g_GameSettings & GAME_PATH3HACK) && path == &gs.path3 && gs.path3.tag.eop )
                nPath3Hack = 1;

            if( path == &gs.path1 ) {

                // if too much data, just ignore
                if( path->tag.nloop * (path->tag.nreg / 4) > (int)size * (path->mode==2?2:1)) {
                    static int lasttime = 0;
                    if( timeGetTime() - lasttime > 5000 ) {
                        ERROR_LOG("VU1 too much data, ignore if gfx are fine\n");
                        lasttime = timeGetTime();
                    }
                    path->tag.nloop = 0;
                    return;
                }

                if( path->mode == 1 ) {

                    // check if 0xb is in any reg, if yes, exit (kh2)
                    for(int i = 0; i < path->tag.nreg; i += 4) {
                        if( ((path->regs >> i)&0xf) == 11 ) {
                            static int lasttime = 0;
                            if( timeGetTime() - lasttime > 5000 ) {
                                ERROR_LOG("Invalid unpack type\n");
                                lasttime = timeGetTime();
                            }
                            path->tag.nloop = 0;
                            return;
                        }
                    }
                }
            }

            if(path->tag.nloop == 0 ) {
                
                if( path == &gs.path1 ) {
                    // ffx hack
                    if( g_GameSettings & GAME_FFXHACK ) {
                        if( path->tag.eop )
                            return;
                        continue;
                    }

                    return;
                }

                if( !path->tag.eop ) {
                    //printf("contuing from eop\n");
                    continue;
                }

                break;
            }
        }

        switch(path->mode) {
        case 1: // PACKED
        {
            assert( path->tag.nloop > 0 );
            for(; size > 0; size--, pMem += 4)
            {
                int reg = (int)((path->regs >> path->regn) & 0xf);
                g_GIFPackedRegHandlers[reg](pMem);

                path->regn += 4;
                if (path->tag.nreg == path->regn) {
                    path->regn = 0;
                    if( path->tag.nloop-- <= 1 ) {
                        size--;
                        pMem += 4;
                        break;
                    }
                }
            }
            break;
        }
        case 2: // REGLIST
        {
            //__Log("%8.8x%8.8x %d L\n", ((u32*)&gs.regs)[1], *(u32*)&gs.regs, path->tag.nreg/4);
            assert( path->tag.nloop > 0 );
            size *= 2;
            for(; size > 0; pMem+= 2, size--)
            {
                int reg = (int)((path->regs >> path->regn) & 0xf);
                g_GIFRegHandlers[reg](pMem);
                path->regn += 4;
                if (path->tag.nreg == path->regn) {
                    path->regn = 0;
                    if( path->tag.nloop-- <= 1 ) {
                        size--;
                        pMem += 2;
                        break;
                    }
                }
            }

            if( size & 1 ) pMem += 2;
            size /= 2;
            break;
        }
        case 3: // GIF_IMAGE (FROM_VFRAM)
        {
            if(gs.imageTransfer >= 0 && gs.imageTransfer <= 1)
            {
                int process = min((int)size, path->tag.nloop);

                if( process > 0 ) {
                    if( gs.imageTransfer ) ZeroGS::TransferLocalHost(pMem, process);
                    else ZeroGS::TransferHostLocal(pMem, process*4);

                    path->tag.nloop -= process;
                    pMem += process*4; size -= process;

                    assert( size == 0 || path->tag.nloop == 0 );
                }
                break;
            }
            else {
                // simulate
                int process = min((int)size, path->tag.nloop);
                path->tag.nloop -= process;
                pMem += process*4; size -= process;
            }

            break;
        }
        default:
            GS_LOG("*** WARNING **** Unexpected GIFTag flag\n");
            assert(0);
//          ZeroGS::TransferLocalHost(pMem, size);
//          pMem+= size*4; path->tag.nloop-= size;
//          size = 0; path->mode = 0;
            path->tag.nloop = 0;
            break;
        }

        if( path == &gs.path1 && path->tag.eop )
            return;
    }
}

void CALLBACK GSgifTransfer2(u32 *pMem, u32 size)
{
#ifdef GS_LOG
    //GS_LOG("GSgifTransfer2 size = %lx (mode %d, gs.path2.tag.nloop = %d)\n", size, gs.path2.mode, gs.path2.tag.nloop);
#endif
    
    //  if(!g_GSMultiThreaded)
//      CSR->FINISH = 0;
    //DVProfileFunc _pf("Transf2");

    //assert( ((u32)pMem & 0xf) == 0 );
    _GSgifTransfer(&gs.path2, pMem, size);
}

void CALLBACK GSgifTransfer3(u32 *pMem, u32 size)
{
#ifdef GS_LOG
    //GS_LOG("GSgifTransfer3 size = %lx (mode %d, gs.path3.tag.nloop = %d)\n", size, gs.path3.mode, gs.path3.tag.nloop);
#endif

    nPath3Hack = 0;
    _GSgifTransfer(&gs.path3, pMem, size);
}

static int s_trcount = 0;
void CALLBACK GSgifTransfer1(u32 *pMem, u32 addr)
{
    pathInfo *path = &gs.path1;

#ifdef GS_LOG
    //GS_LOG("GSgifTransfer1 0x%x (mode %d)\n", addr, path->mode);
#endif

    addr &= 0x3fff;

#ifdef _DEBUG
    PRIM_LOG("count :%d\n", s_trcount);
    s_trcount++;

//  for(int i = addr; i < 0x4000; i += 16 ) {
//  u32* mem = (u32*)((u8*)pMem+i);
//      PRIM_LOG("%x: %x %x %x %x\n", i, mem[0], mem[1], mem[2], mem[3]);
//  }
#endif

    gs.path1.tag.nloop = 0;
    gs.path1.tag.eop = 0;
    _GSgifTransfer(&gs.path1, (u32*)((u8*)pMem+addr), (0x4000-addr)/16);

    if( !gs.path1.tag.eop && gs.path1.tag.nloop > 0 ) {
        assert( (addr&0xf) == 0 ); //BUG
        gs.path1.tag.nloop = 0;
        ERROR_LOG("Transfer1 - 2\n");
        //_GSgifTransfer(&gs.path1, (u32*)((u8*)pMem+0x4000-addr), addr/16);
        return;
    }
}

void CALLBACK GSreadFIFO(u64 *pMem)
{
#ifdef GS_LOG
    //GS_LOG("GSreadFIFO\n");
#endif

    ZeroGS::TransferLocalHost((u32*)pMem, 1);
}

void CALLBACK GSreadFIFO2(u64 *pMem, int qwc)
{
#ifdef GS_LOG
    //GS_LOG("GSreadFIFO2\n");
#endif

    ZeroGS::TransferLocalHost((u32*)pMem, qwc);
}

int CALLBACK GSsetupRecording(int start, void* pData)
{
    if( start ) {
        if( conf.options & GSOPTION_CAPTUREAVI )
            return 1;
        ZeroGS::StartCapture();
        conf.options |= GSOPTION_CAPTUREAVI;
        printf("ZeroGS: started recording at zerogs.avi\n");
    }
    else {
        if( !(conf.options & GSOPTION_CAPTUREAVI) )
            return 1;
        conf.options &= ~GSOPTION_CAPTUREAVI;
        ZeroGS::StopCapture();
        printf("ZeroGS: stopped recording\n");
    }

    return 1;
}

s32 CALLBACK GSfreeze(int mode, freezeData *data) {
    if (mode == FREEZE_LOAD) {
        if( !ZeroGS::Load(data->data) )
            ERROR_LOG("Bad load format!");
        g_nRealFrame += 100;

    } else if (mode == FREEZE_SAVE) {
        ZeroGS::Save(data->data);
    }
    if (mode == FREEZE_SIZE) {
        data->size = ZeroGS::Save(NULL);
    }

    return 0;
}

////////////////////
// Small profiler //
////////////////////
#include <list>
#include <string>
#include <map>
using namespace std;

#ifdef _WIN32

inline u64 GET_PROFILE_TIME()
{
    LARGE_INTEGER lu;
    QueryPerformanceCounter(&lu);
    return lu.QuadPart;
}
#else
#define GET_PROFILE_TIME() //GetCpuTick()
#endif


struct DVPROFSTRUCT;

struct DVPROFSTRUCT
{
    struct DATA
    {
        DATA(u64 time, u32 user = 0) : dwTime(time), dwUserData(user) {}
        DATA() : dwTime(0), dwUserData(0) {}
        
        u64 dwTime;
        u32 dwUserData;
    };

    ~DVPROFSTRUCT() {
        list<DVPROFSTRUCT*>::iterator it = listpChild.begin();
        while(it != listpChild.end() ) {
            SAFE_DELETE(*it);
            ++it;
        }
    }

    list<DATA> listTimes;       // before DVProfEnd is called, contains the global time it started
                                // after DVProfEnd is called, contains the time it lasted
                                // the list contains all the tracked times 
    char pname[256];

    list<DVPROFSTRUCT*> listpChild;     // other profilers called during this profiler period
};

struct DVPROFTRACK
{
    u32 dwUserData;
    DVPROFSTRUCT::DATA* pdwTime;
    DVPROFSTRUCT* pprof;
};

list<DVPROFTRACK> g_listCurTracking;    // the current profiling functions, the back element is the
                                        // one that will first get popped off the list when DVProfEnd is called
                                        // the pointer is an element in DVPROFSTRUCT::listTimes
list<DVPROFSTRUCT> g_listProfilers;         // the current profilers, note that these are the parents
                                            // any profiler started during the time of another is held in
                                            // DVPROFSTRUCT::listpChild
list<DVPROFSTRUCT*> g_listAllProfilers;     // ignores the hierarchy, pointer to elements in g_listProfilers

void DVProfRegister(char* pname)
{
    if( !g_bWriteProfile )
        return;

    list<DVPROFSTRUCT*>::iterator it = g_listAllProfilers.begin();
    
//  while(it != g_listAllProfilers.end() ) {
//
//      if( _tcscmp(pname, (*it)->pname) == 0 ) {
//          (*it)->listTimes.push_back(timeGetTime());
//          DVPROFTRACK dvtrack;
//          dvtrack.pdwTime = &(*it)->listTimes.back();
//          dvtrack.pprof = *it;
//          g_listCurTracking.push_back(dvtrack);
//          return;
//      }
//
//      ++it;
//  }

    // else add in a new profiler to the appropriate parent profiler
    DVPROFSTRUCT* pprof = NULL;
    
    if( g_listCurTracking.size() > 0 ) {
        assert( g_listCurTracking.back().pprof != NULL );
        g_listCurTracking.back().pprof->listpChild.push_back(new DVPROFSTRUCT());
        pprof = g_listCurTracking.back().pprof->listpChild.back();
    }
    else {
        g_listProfilers.push_back(DVPROFSTRUCT());
        pprof = &g_listProfilers.back();
    }

    strncpy(pprof->pname, pname, 256);

    // setup the profiler for tracking
    pprof->listTimes.push_back(DVPROFSTRUCT::DATA(GET_PROFILE_TIME()));

    DVPROFTRACK dvtrack;
    dvtrack.pdwTime = &pprof->listTimes.back();
    dvtrack.pprof = pprof;
    dvtrack.dwUserData = 0;

    g_listCurTracking.push_back(dvtrack);

    // add to all profiler list
    g_listAllProfilers.push_back(pprof);
}

void DVProfEnd(u32 dwUserData)
{
    if( !g_bWriteProfile )
        return;
    B_RETURN( g_listCurTracking.size() > 0 );

    DVPROFTRACK dvtrack = g_listCurTracking.back();

    assert( dvtrack.pdwTime != NULL && dvtrack.pprof != NULL );

    dvtrack.pdwTime->dwTime = 1000000 * (GET_PROFILE_TIME()- dvtrack.pdwTime->dwTime) / luPerfFreq;
    dvtrack.pdwTime->dwUserData= dwUserData;

    g_listCurTracking.pop_back();
}

struct DVTIMEINFO
{
    DVTIMEINFO() : uInclusive(0), uExclusive(0) {}
    u64 uInclusive, uExclusive;
};

map<string, DVTIMEINFO> mapAggregateTimes;

u64 DVProfWriteStruct(FILE* f, DVPROFSTRUCT* p, int ident)
{
    fprintf(f, "%*s%s - ", ident, "", p->pname);

    list<DVPROFSTRUCT::DATA>::iterator ittime = p->listTimes.begin();

    u32 utime = 0;

    while(ittime != p->listTimes.end() ) {
        utime += (u32)ittime->dwTime;
        
        if( ittime->dwUserData ) 
            fprintf(f, "time: %d, user: 0x%8.8x", (u32)ittime->dwTime, ittime->dwUserData);
        else
            fprintf(f, "time: %d", (u32)ittime->dwTime);
        ++ittime;
    }

    mapAggregateTimes[p->pname].uInclusive += utime;

    fprintf(f, "\n");

    list<DVPROFSTRUCT*>::iterator itprof = p->listpChild.begin();

    u32 uex = utime;
    while(itprof != p->listpChild.end() ) {

        uex -= DVProfWriteStruct(f, *itprof, ident+4);
        ++itprof;
    }

    mapAggregateTimes[p->pname].uExclusive += uex;
    return utime;
}

void DVProfWrite(char* pfilename, u32 frames)
{
    assert( pfilename != NULL );
    FILE* f = fopen(pfilename, "wb");

    mapAggregateTimes.clear();
    list<DVPROFSTRUCT>::iterator it = g_listProfilers.begin();

    while(it != g_listProfilers.end() ) {
        DVProfWriteStruct(f, &(*it), 0);
        ++it;
    }

    {
        map<string, DVTIMEINFO>::iterator it;
        fprintf(f, "\n\n-------------------------------------------------------------------\n\n");

        u64 uTotal[2] = {0};
        double fiTotalTime[2];

        for(it = mapAggregateTimes.begin(); it != mapAggregateTimes.end(); ++it) {
            uTotal[0] += it->second.uExclusive;
            uTotal[1] += it->second.uInclusive;
        }

        fprintf(f, "total times (%d): ex: %Lu ", frames, uTotal[0]/frames);
        fprintf(f, "inc: %Lu\n", uTotal[1]/frames);

        fiTotalTime[0] = 1.0 / (double)uTotal[0];
        fiTotalTime[1] = 1.0 / (double)uTotal[1];

        // output the combined times
        for(it = mapAggregateTimes.begin(); it != mapAggregateTimes.end(); ++it) {
            fprintf(f, "%s - ex: %f inc: %f\n", it->first.c_str(), (double)it->second.uExclusive * fiTotalTime[0],
                (double)it->second.uInclusive * fiTotalTime[1]);
        }
    }


    fclose(f);
}

void DVProfClear()
{
    g_listCurTracking.clear();
    g_listProfilers.clear();
    g_listAllProfilers.clear();
}
