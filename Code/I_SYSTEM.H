//      System specific interface stuff.


#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include "d_ticcmd.h"
#include "d_event.h"

#ifdef __GNUG__
#pragma interface
#endif

// See Shutdown_xxx() routines.
extern byte graphics_started;
extern byte keyboard_started;
extern byte sound_started;
//extern byte music_installed;

/* flag for 'win-friendly' mode used by interface code */
extern int i_love_bill;
extern volatile ULONG ticcount;

// Called by DoomMain.
void I_InitJoystick (void);

// Called by startup code
// to get the ammount of memory to malloc
// for the zone management.
byte*   I_ZoneBase (int *size);

void I_GetFreeMem(void);

// Called by D_DoomLoop,
// returns current time in tics.
ULONG I_GetTime (void);


void I_GetEvent (void);


//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//
void I_StartFrame (void);


//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic (void);

// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.
ticcmd_t* I_BaseTiccmd (void);


// Called by M_Responder when quit is selected, return code 0.
void I_Quit (void);

void I_Error (char *error, ...);

// Allocates from low memory under dos,
// just mallocs under unix
byte* I_AllocLow (int length);

void I_Tactile (int on, int off, int total);

//added:18-02-98: write a message to stderr (use before I_Quit)
//                for when you need to quit with a msg, but need
//                the return code 0 of I_Quit();
void I_OutputMsg (char *error, ...);

void I_StartupMouse (void);

// keyboard startup,shutdown,handler
void I_StartupKeyboard (void);

// setup timer irq and user timer routine.
void I_TimerISR (void);      //timer callback routine.
void I_StartupTimer (void);

/* list of functions to call at program cleanup */
void I_AddExitFunc (void (*func)());
void I_RemoveExitFunc (void (*func)());

// Setup signal handler, plus stuff for trapping errors and cleanly exit.
int  I_StartupSystem (void);
void I_ShutdownSystem (void);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
