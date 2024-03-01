
// p_fab.h
//

#ifndef __P_FAB__
#define __P_FAB__

#include "doomtype.h"
#include "command.h"

extern consvar_t cv_solidcorpse;        //p_enemy
extern consvar_t cv_bloodtime;

// spawn smoke trails behind rockets and skull head attacks
void A_SmokeTrailer (mobj_t* actor);

// hack the states table to set Doom Legacy's default translucency on sprites
void P_SetTranslucencies (void);

// add commands for deathmatch rules and style (like more blood) :)
void D_AddDeathmatchCommands (void);

#endif
