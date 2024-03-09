//      Status bar code.
//      Does the face/direction indicator animatin.
//      Does palette indicators as well (red pain/berserk, bright pickup)

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomtype.h"
#include "d_event.h"
#include "d_player.h"

                     //software mode : position according to resolution, not scaled
                     //hardware mode : original coords, scaled to current resolution, correct aspect
#define ST_Y         (rendermode==render_soft ? vid.height - ST_HEIGHT : BASEVIDHEIGHT - ST_HEIGHT)


//
// STATUS BAR
//

// Called by main loop.
boolean ST_Responder (event_t* ev);

// Called by main loop.
void ST_Ticker (void);

// Called by main loop.
void ST_Drawer (boolean fullscreen, boolean refresh);

// Called when the console player is spawned on each level.
void ST_Start (void);

// Called by startup code.
void ST_Init (void);

// Called by G_Responder() when pressing F12 while viewing a demo.
void ST_changeDemoView (void);

// Add status bar related commands & vars
void ST_AddCommands (void);



// need this for SCR_Recalc() coz widgets coords change with resolutions
extern boolean   st_recalc;

// States for status bar code.
typedef enum
{
    AutomapState,
    FirstPersonState

} st_stateenum_t;


// States for the chat code.
typedef enum
{
    StartChatState,
    WaitDestState,
    GetChatState

} st_chatstateenum_t;


boolean ST_Responder(event_t* ev);

// face load/unload graphics, called when skin changes
void ST_loadFaceGraphics (char *facestr);
void ST_unloadFaceGraphics (void);

// return if player a is in the same team of the player b
boolean ST_SameTeam(player_t *a,player_t *b);

// get the frags of the player
// only one function for calculation : more simple code
int  ST_PlayerFrags (int playernum);

//--------------------
// status bar overlay
//--------------------
extern boolean    st_overlay;   // sb overlay on or off when fullscreen

void ST_overlayDrawer (void);   // draw vital info overlay when fullscreen


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
