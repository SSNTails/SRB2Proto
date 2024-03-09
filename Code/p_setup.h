//   Setup a game, startup stuff.


#ifndef __P_SETUP__
#define __P_SETUP__

#include "doomdata.h"

// Player spawn spots for deathmatch.
#define MAX_DM_STARTS   64
extern  mapthing_t      deathmatchstarts[MAX_DM_STARTS];
extern  mapthing_t*     deathmatch_p;

extern int        lastloadedmaplumpnum; // for comparative savegame
//
// MAP used flats lookup table
//
typedef struct
{
    char        name[8];        // resource name from wad
    int         lumpnum;        // lump number of the flat

    // for flat animation
    int         baselumpnum;
    int         animseq;        // start pos. in the anim sequence
    int         numpics;
    int         speed;

    //hack add water splash for old water texture
    boolean     iswater;        // water texture

} levelflat_t;

extern int             numlevelflats;
extern levelflat_t*    levelflats;
int P_AddLevelFlat (char* flatname, levelflat_t* levelflat);

// NOT called by W_Ticker. Fixme.
boolean P_SetupLevel
( int           episode,
  int           map,
  int           playermask,
  skill_t       skill,
  char*         mapname);

boolean P_AddWadFile (char* wadfilename);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
