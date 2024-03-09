// g_state.h : Doom/Hexen game states

#ifndef __G_STATE__
#define __G_STATE__

#include "doomtype.h"

// skill levels
typedef enum
{
    sk_baby,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
} skill_t;

// the current state of the game
typedef enum
{
    GS_LEVEL,                   // we are playing
    GS_INTERMISSION,            // gazing at the intermission screen
    GS_FINALE,                  // game final animation
    GS_DEMOSCREEN,              // looking at a demo
    //legacy
    GS_DEDICATEDSERVER,         // added 27-4-98 : new state for dedicated server
    GS_WAITINGPLAYERS           // added 3-9-98 : waiting player in net game
} gamestate_t;

typedef enum
{
    ga_nothing,
    ga_loadlevel,
    ga_playdemo,
    ga_completed,
    ga_victory,
    ga_worlddone,
    ga_screenshot,
    //HeXen
/*
    ga_initnew,
    ga_newgame,
    ga_loadgame,
    ga_savegame,
    ga_leavemap,
    ga_singlereborn
*/
} gameaction_t;



extern  gamestate_t     gamestate;
extern  gameaction_t    gameaction;
extern  skill_t         gameskill;

extern  boolean         demoplayback;

#endif //__G_STATE__
