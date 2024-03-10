
// win_main.h

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

extern  HINSTANCE       myInstance;
extern  HWND            hWndMain;

// debugging CONS_Printf to file
extern  HANDLE          logstream;

//faB: midi channel Volume set is delayed by the MIDI stream callback thread, see win_snd.c
#define WM_MSTREAM_UPDATEVOLUME	(WM_USER + 101)

// defined in win_sys.c
void    I_BeginProfile (void);    //for timing code
DWORD   I_EndProfile (void);

void I_GetLastErrorMsgBox (void);

// output formatted string to file using win32 functions (win_dbg.c)
void FPrintf (HANDLE fileHandle, LPCTSTR lpFmt, ...);
