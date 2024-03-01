//  Intermission.

#ifndef __WI_STUFF__
#define __WI_STUFF__

//#include "v_video.h"

#include "doomdef.h"
#include "d_player.h"

// States for the intermission

typedef enum
{
    NoState = -1,
    StatCount,
    ShowNextLoc

} stateenum_t;

//added:05-02-98:
typedef struct {
    int  count;
    int  num;
    int  color;
    char *name;
} fragsort_t;

// Called by main loop, animate the intermission.
void WI_Ticker (void);

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer (void);

// Setup for an intermission screen.
void WI_Start(wbstartstruct_t*   wbstartstruct);

boolean teamingame(int teamnum);

// draw ranckings
void WI_drawRancking(char *title,int x,int y,fragsort_t *fragtable
                    , int scorelines, boolean large, int white);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
