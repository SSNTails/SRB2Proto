// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

#include "doomdef.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"

#include "d_net.h"
#include "g_game.h"

#include "endtxt.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"
#include "i_joy.h"

JoyType_t Joystick;

// dummy 19990119 by Kin
byte keyboard_started = 0;
void I_StartupKeyboard (void) {}
void I_StartupTimer (void) {}
void I_OutputMsg (char *error, ...) {}
int I_GetKey (void) { return 0; }
void I_InitJoystick (void) {}
void I_StartupMouse (void) {}
void I_GetFreeMem(void) {}


int	mb_used = 6;


void
I_Tactile
( int	on,
  int	off,
  int	total )
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t	emptycmd;
ticcmd_t*	I_BaseTiccmd(void)
{
    return &emptycmd;
}


int  I_GetHeapSize (void)
{
    return mb_used*1024*1024;
}

byte* I_ZoneBase (int*	size)
{
    *size = mb_used*1024*1024;
    return (byte *) malloc (*size);
}



//
// I_GetTime
// returns time in 1/TICRATE second tics
//
ULONG  I_GetTime (void)
{
    struct timeval	tp;
    struct timezone	tzp;
    int			newtics;
    static int		oldtics;
    static int		basetime=0;
  
    gettimeofday(&tp, &tzp);
    if (!basetime)
	basetime = tp.tv_sec;

    // On systems with RTC drift correction or NTP we need to take
    // care about the system clock running backwards sometimes. Make
    // sure the new tic is later then the last one.
again:
    newtics = (tp.tv_sec-basetime)*TICRATE + tp.tv_usec*TICRATE/1000000;
    if (!oldtics)
	oldtics = newtics;
    if (newtics < oldtics) {
	I_WaitVBL(1);
	goto again;
    }
    oldtics = newtics;
    return newtics;
}



//
// I_Init
//
void I_Init (void)
{
    I_StartupSound();
    I_InitMusic();
    //  I_InitGraphics();
}

//
// I_Quit
//
void I_Quit (void)
{
  //added:16-02-98: when recording a demo, should exit using 'q' key,
  //        but sometimes we forget and use 'F10'.. so save here too.
    if (demorecording)
        G_CheckDemoStatus();
    D_QuitNetGame ();
    I_ShutdownMusic();
    I_ShutdownSound();
   // use this for 1.27 19990220 by Kin
    M_SaveConfig (NULL);
    I_ShutdownGraphics();
    printf("\r");
    ShowEndTxt();
    exit(0);
}

void I_WaitVBL(int count)
{
#ifdef SGI
    sginap(1);                                           
#else
#ifdef SUN
    sleep(0);
#else
    usleep (count * (1000000/70) );                                
#endif
#endif
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte*	I_AllocLow(int length)
{
    byte*	mem;
        
    mem = (byte *)malloc (length);
    memset (mem,0,length);
    return mem;
}


//
// I_Error
//
extern boolean demorecording;

void I_Error (char *error, ...)
{
    va_list	argptr;

    // Message first.
    va_start (argptr,error);
    fprintf (stderr, "Error: ");
    vfprintf (stderr,error,argptr);
    fprintf (stderr, "\n");
    va_end (argptr);

    fflush( stderr );

    // Shutdown. Here might be other errors.
    if (demorecording)
	G_CheckDemoStatus();

    D_QuitNetGame ();
    I_ShutdownMusic();
    I_ShutdownSound();
    I_ShutdownGraphics();
    
    exit(-1);
}
