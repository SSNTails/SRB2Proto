//
// Win32 Doom LEGACY
//

// To compile WINDOWS Legacy version : define a '__WIN32__' symbol.
// to do this go to Project/Settings/ menu, click C/C++ tab, in 'Preprocessor definitions:' add '__WIN32__'

#include <stdio.h>

#include "../doomdef.h"
#include "resource.h"

#include "../m_argv.h"
#include "../d_main.h"
#include "../i_system.h"

#include "../keys.h"    //hack quick test

#include "../console.h"

#include "fabdxlib.h"
#include "win_main.h"
#include "win_dbg.h"

HINSTANCE       myInstance=NULL;
HWND            hWndMain=NULL;
HCURSOR         windowCursor=NULL;                      // main window cursor

static  boolean         okToRender = true;
static  boolean         appActive = false;                      //app window is active

// this is were I log debug text, cons_printf, I_error ect for window port debugging
HANDLE  logstream;

// faB: the MIDI callback is another thread, and Midi volume is delayed here in window proc
extern void I_SetMidiChannelVolume( DWORD dwChannel, DWORD dwVolumePercent );
extern DWORD dwVolumePercent;

void I_LoadingScreen ( LPCSTR msg );
long FAR PASCAL  MainWndproc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    event_t ev;         //Doom input event
    
    switch( message )
    {
    /*case WM_SETCURSOR:
    if (Res != GR_RESOLUTION_NONE)
    {
    SetCursor(NULL);
    return 0;
    }
        break;*/
        
    case WM_CREATE:
        break;
        
    case WM_ACTIVATEAPP:           // Handle task switching
        appActive = wParam;
        InvalidateRect (hWnd, NULL, TRUE);
        break;
        
    //for MIDI music
    case WM_MSTREAM_UPDATEVOLUME:
        I_SetMidiChannelVolume( wParam, dwVolumePercent );
        break;

    case WM_PAINT:
        if (!appActive && !bAppFullScreen)
            // app becomes inactive (if windowed )
        {
            // Paint "Game Paused" in the middle of the screen
            PAINTSTRUCT ps;
            RECT        rect;
            HDC hdc = BeginPaint (hWnd, &ps);
            GetClientRect (hWnd, &rect);
            DrawText (hdc, "Game Paused", -1, &rect,
                DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            EndPaint (hWnd, &ps);
            return 0;
        }
        break;
        
    //case WM_RBUTTONDOWN:
    //case WM_LBUTTONDOWN:
        
    case WM_MOVE:
        if (bAppFullScreen) {
            SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
            return 0;
        }
        else {
            windowPosX = (SHORT) LOWORD(lParam);    // horizontal position
            windowPosY = (SHORT) HIWORD(lParam);    // vertical position
            break;
        }
        break;
        
        // This is where switching windowed/fullscreen is handled. DirectDraw
        // objects must be destroyed, recreated, and artwork reloaded.
        
    /*
    case WM_SYSKEYUP:
        if (wParam == VK_RETURN)
        {
            static int  lastPosX, lastPosY;             // last window position
            //toggleviewfullscreen:            
            
            CloseDirectDraw();
            bAppFullScreen = !bAppFullScreen;
            
            if (bAppFullScreen)
            {
                lastPosX = windowPosX;
                lastPosY = windowPosY;
                windowPosX = 0;
                windowPosY = 0;
            }
            else
            {
                windowPosX = lastPosX;
                windowPosY = lastPosY;
            }
            
            if (!InitDirectDrawe(hWnd, 320, 200, 1, //bpp
                    bAppFullScreen))
                I_Error ("Couldn't re-init DirectDraw after toggle between windowed/fullscreen mode\n");
            //PostMessage(hWnd, WM_CLOSE, 0, 0);
            
            //will set the current used palette or create it
            //SetPalette (main_pal);
            
            return 0;
        }
        break;*/
        
    case WM_DISPLAYCHANGE:
    case WM_SIZE:
        okToRender = true;
        break;
        
    case WM_SETCURSOR:
        if (bAppFullScreen)
            SetCursor(NULL);
        else
            SetCursor(windowCursor);
        return TRUE;
        
    case WM_KEYDOWN:
        //CONS_Printf ("yo touche %d\n", wParam);
        ev.type = ev_keydown;
        
        // allow to quit at any time
        // (use ALT-F4)
        /*if (wParam == VK_F12) {
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        }*/

handleKeyDoom:
        ev.data1 = 0;
        if (wParam == VK_PAUSE)
        // intercept PAUSE key
        {
            ev.data1 = KEY_PAUSE;
        }
        else if (!keyboard_started)
        // post some keys during the game startup
        // (allow escaping from network synchronization, or pressing enter after
        //  an error message in the console)
        {
            switch (wParam) {
            case VK_ESCAPE: ev.data1 = KEY_ESCAPE;  break;
            case VK_RETURN: ev.data1 = KEY_ENTER;   break;
            }
        }

        if (ev.data1) {
            D_PostEvent (&ev);
            return 0;
        }
        break;
        
    case WM_KEYUP:
        ev.type = ev_keyup;
        goto handleKeyDoom;
        break;
        
    case WM_CLOSE:
        PostQuitMessage(0);         //to quit while in-game
        ev.data1 = KEY_ESCAPE;      //to exit network synchronization
        ev.type = ev_keydown;
        D_PostEvent (&ev);
        return 0;
    case WM_DESTROY:
        //faB: main app loop will exit the loop and proceed with I_Quit()
        PostQuitMessage(0);
        break;
        
    default:
        break;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}


//
// Do that Windows initialization stuff...
//
HWND    OpenMainWindow (HINSTANCE hInstance, int nCmdShow, char* wTitle)
{
    HWND        hWnd;
    WNDCLASS    wc;
    BOOL        rc;
    
    // Set up and register window class
    wc.style = CS_HREDRAW | CS_VREDRAW /*| CS_DBLCLKS*/;
    wc.lpfnWndProc = MainWndproc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DLICON1));
    windowCursor = LoadCursor(NULL, IDC_WAIT); //LoadCursor(hInstance, MAKEINTRESOURCE(IDC_DLCURSOR1));
    wc.hCursor = windowCursor;
    wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "LegacyWC";
    rc = RegisterClass(&wc);
    if( !rc )
        return false;
    
    // Create a window
    // CreateWindowEx - seems to create just the interior, not the borders
    
    hWnd = CreateWindowEx(WS_EX_TOPMOST,    //ExStyle
        "LegacyWC",                         //Classname
        wTitle,                             //Windowname
        WS_CAPTION|WS_POPUP|WS_SYSMENU,     //dwStyle       //WS_VISIBLE|WS_POPUP for bAppFullScreen
        0,
        0,
        640,  //GetSystemMetrics(SM_CXSCREEN),
        400,  //GetSystemMetrics(SM_CYSCREEN),
        NULL,                               //hWnd Parent
        NULL,                               //hMenu Menu
        hInstance,
        NULL);
    
    /* faB: show window later when it is ready to draw startup screen
    if (hWnd!=NULL)
    {
        // show the Window
        SetFocus(hWnd);
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }*/
    
    return hWnd;
}


BOOL tlErrorMessage( char *err)
{
    /* make the cursor visible */
    SetCursor(LoadCursor( NULL, IDC_ARROW ));
    
    //
    // warn user if there is one 
    //
    printf("Error %s..\n", err);
    fflush(stdout);
    
    MessageBox( hWndMain, err, "ERROR", MB_OK );
    return FALSE;
}


// ------------------
// Command line stuff
// ------------------
#define         MAXCMDLINEARGS          64
static  char*   myWargv[MAXCMDLINEARGS+1];
static  char    myCmdline[512];

static void     GetArgcArgv (LPCSTR cmdline)
{
    char*   token;
    int     i, len;
    char    cSep;
    BOOL    bCvar = FALSE, prevCvar = FALSE;
    
    // split arguments of command line into argv
    strncpy (myCmdline, cmdline, 511);      // in case window's cmdline is in protected memory..for strtok
    len = lstrlen (myCmdline);
  
    myWargv[0] = "dummy.exe";
    myargc = 1;
    i = 0;
    cSep = ' ';
    while( myargc < MAXCMDLINEARGS )
    {
        // get token
        while ( myCmdline[i] == cSep )
            i++;
        if ( i >= len )
            break;
        token = myCmdline + i;
        if ( myCmdline[i] == '"' ) {
            cSep = '"';
            i++;
            if ( !prevCvar )    //cvar leave the "" in
                token++;
        }
        else
            cSep = ' ';

        //cvar
        if ( myCmdline[i] == '+' && cSep == ' ' )   //a + begins a cvarname, but not after quotes
            bCvar = TRUE;
        else
            bCvar = FALSE;

        while ( myCmdline[i] &&
                myCmdline[i] != cSep )
            i++;

        if ( myCmdline[i] == '"' ) {
             cSep = ' ';
             if ( prevCvar )
                 i++;       // get ending " quote in arg
        }
        
        prevCvar = bCvar;

        if ( myCmdline + i > token )
        {
            myWargv[myargc++] = token;
        }

        if ( !myCmdline[i] || i >= len )
            break;

        myCmdline[i++] = '\0';
    }
    myWargv[myargc] = NULL;
    
    // m_argv.c uses myargv[], we used myWargv because we fill the arguments ourselves
    // and myargv is just a pointer, so we set it to point myWargv
    myargv = myWargv;
}       


// -----------------------------------------------------------------------------
// HandledWinMain : called by exception handler
// -----------------------------------------------------------------------------
int WINAPI HandledWinMain(HINSTANCE hInstance,
                          HINSTANCE hPrevInstance,
                          LPSTR     lpCmdLine,
                          int       nCmdShow)
{
    MSG             msg;
    int             i;
    
    // DEBUG!!! - set logstream to NULL to disable debug log
    logstream = INVALID_HANDLE_VALUE;
    logstream = CreateFile ("log.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL /*file flag writethrough?*/, NULL);
    if (logstream == INVALID_HANDLE_VALUE)
        goto damnit;

    // fill myargc,myargv for m_argv.c retrieval of cmdline arguments
    CONS_Printf ("GetArgcArgv() ...\n");
    GetArgcArgv(lpCmdLine);
    
    CONS_Printf ("lpCmdLine is '%s'\n", lpCmdLine);
    CONS_Printf ("Myargc: %d\n", myargc);
    for (i=0;i<myargc;i++)
        CONS_Printf("myargv[%d] : '%s'\n", i, myargv[i]);
    
    
    // store for later use, will we need it ?
    myInstance = hInstance;
    
    // open a dummy window, both 3dfx Glide and DirectX need one.
    if ( (hWndMain = OpenMainWindow(hInstance,nCmdShow,
                va("Doom Legacy v%i.%i"VERSIONSTRING,VERSION/100,VERSION%100))) == NULL )
    {
        tlErrorMessage("Couldn't open window");
        return FALSE;
    }
    
    // put up a message in the window.
    /*
    {
    HDC hDC = GetDC(hWndMain);
    char *message = "loading Doom Legacy ...";
    RECT rect;
    
      GetClientRect(hWndMain, &rect);
      SetTextColor(hDC, RGB(0, 255, 255));
      SetBkColor(hDC, RGB(0, 0, 0));
      SetTextAlign(hDC, TA_CENTER);
      ExtTextOut(hDC, rect.right/2, rect.bottom/2, ETO_OPAQUE, &rect, 
      message, strlen(message), NULL);
      ReleaseDC(hWndMain, hDC);
      GdiFlush();
}*/
    
    // currently starts DirectInput keyboard and mouse
    CONS_Printf ("I_StartupSystem() ...\n");
    I_StartupSystem();

    // setup direct x ?
    
    // setup glide ?
    
    // startup Doom Legacy
    CONS_Printf ("D_DoomMain() ...\n");
    D_DoomMain ();
    
    //
    // da main Windoze loop
    //
    CONS_Printf ("Entering main app loop...\n");
    while (1)
    {
        if (PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage( &msg, NULL, 0, 0))
                break;
            TranslateMessage(&msg); 
            DispatchMessage(&msg);
        }
        else if (appActive)
        {
            // run the main Doom loop
            D_DoomInnerLoop();
        }
        else
        {
            WaitMessage();
        }
    }
    CONS_Printf ("Exited main app loop...\n");
    
    //faB: if it goes through here, we've probably send a CLOSE message to the main app
    //     window, in a situation where we couldn't I_Error() immediately, and left the
    //     app process the exit code when it was ready (eg: if we were in another thread)
    I_Quit ();
    
    if (logstream != INVALID_HANDLE_VALUE)
        CloseHandle (logstream);
damnit:
    
    // back to Windoze
    return msg.wParam;
}
                          
                          
// -----------------------------------------------------------------------------
// Exception handler calls WinMain for catching exceptions
// -----------------------------------------------------------------------------
int WINAPI WinMain (HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR        lpCmdLine,
                    int          nCmdShow)
{
    int Result = -1;
    __try
    {
        Result = HandledWinMain (hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    }
    
    __except ( RecordExceptionInfo( GetExceptionInformation(), "main thread", lpCmdLine) )
    {
        //Do nothing here.
    }
    
    /*__finally
    {
    if (logstream != INVALID_HANDLE_VALUE)
        CloseHandle (logstream);
}*/
    
    return Result;
}
