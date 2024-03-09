//      System specific interface stuff.


#ifndef __I_VIDEO__
#define __I_VIDEO__


#include "doomtype.h"

#ifdef __GNUG__
#pragma interface
#endif

typedef enum {
    render_soft = 1,
    render_glide = 2,
    render_d3d = 3
} rendermode_t;

extern rendermode_t    rendermode;

// use highcolor modes if true
extern boolean highcolor;

void I_StartupGraphics (void);          //setup video mode
void I_ShutdownGraphics(void);          //restore old video mode

// Takes full 8 bit values.
void I_SetPalette (byte* palette);

void I_UpdateNoBlit (void);
void I_FinishUpdate (void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

void I_ReadScreen (byte* scr);

void I_BeginRead (void);
void I_EndRead (void);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
