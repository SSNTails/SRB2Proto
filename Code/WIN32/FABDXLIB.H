// faB's DirectX library v1.0
//

#ifndef _H_FABDXLIB_
#define _H_FABDXLIB_

#define SAFE_RELEASE(x) if(x != NULL) { x->lpVtbl->Release(x); x = NULL; }
#define SAFE_DELETE(x) if(x != NULL) { delete x; x = NULL; }

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

// format of function in app called with width,height
typedef BOOL (*APPENUMMODESCALLBACK)(int width, int height, int bpp);


// globals
extern IDirectDraw*                                     DDr;
extern IDirectDrawSurface*                      ScreenReal;
extern IDirectDrawSurface*                      ScreenVirtual;
extern IDirectDrawPalette*                      DDPalette;

extern BOOL                                     bAppFullScreen;                             // main code might need this to know the current
                                                                                                        // fullscreen or windowed state

extern int                                      windowPosX;                             // current position in windowed mode
extern int                                      windowPosY;

extern int                                      ScreenWidth;    
extern int                                      ScreenHeight;
extern BOOL                                     ScreenLocked;                   // Screen surface is being locked
extern int                                      ScreenPitch;                    // offset from one line to the next
extern unsigned char*           ScreenPtr;                              // memory of the surface


BOOL    EnumDirectDrawDisplayModes (APPENUMMODESCALLBACK appFunc);
BOOL    CreateDirectDrawInstance (void);

int     InitDirectDrawe (HWND appWin, int width, int height, int bpp, int fullScr);
void    CloseDirectDraw (void);

void    ReleaseChtuff (void);

void    ClearSurface (IDirectDrawSurface* surface, int color);
BOOL    ScreenFlip (int wait);
void    TextPrint (int x, int y, char* message);

void    CreateDDPalette (PALETTEENTRY* colorTable);
void    DestroyDDPalette (void);
void    SetDDPalette (PALETTEENTRY* pal);

void    WaitVbl (void);

void    LockScreen (void);
void    UnlockScreen (void);


#endif /* _H_FABDXLIB_ */