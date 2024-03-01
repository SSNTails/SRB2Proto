
// doomstat.h :
//
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//

#ifndef __D_STATE__
#define __D_STATE__


// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"

// We need the player data structure as well.
#include "d_player.h"
#include "d_clisrv.h"


// Game mode handling - identify IWAD version,
//  handle IWAD dependend animations etc.
typedef enum
{
    shareware,    // DOOM 1 shareware, E1, M9
    registered,   // DOOM 1 registered, E3, M27
    commercial,   // DOOM 2 retail, E1 M34
    // DOOM 2 german edition not handled
    retail,       // DOOM 1 retail, E4, M36
    indetermined  // Well, no IWAD found.

} gamemode_t;


// Mission packs - might be useful for TC stuff?
typedef enum
{
    doom,         // DOOM 1
    doom2,        // DOOM 2
    pack_tnt,     // TNT mission pack
    pack_plut,    // Plutonia pack
    none

} gamemission_t;


// Identify language to use, software localization.
typedef enum
{
    english,
    french,
    german,
    unknown

} language_t;


// ===================================================
// Game Mode - identify IWAD as shareware, retail etc.
// ===================================================
//
extern gamemode_t       gamemode;
extern gamemission_t    gamemission;

// Set if homebrew PWAD stuff has been added.
extern  boolean modifiedgame;


// =========
// Language.
// =========
//
extern  language_t   language;



// =============================
// Selected skill type, map etc.
// =============================

// Selected by user.
extern  skill_t         gameskill;
extern  byte            gameepisode;
extern  byte            gamemap;

// Nightmare mode flag, single player.
// extern  boolean         respawnmonsters;

// Netgame? only true in a netgame
extern  boolean         netgame;
// Only true if >1 player. netgame => multiplayer but not (multiplayer=>netgame)
extern  boolean         multiplayer;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern  consvar_t       cv_deathmatch;


// ========================================
// Internal parameters for sound rendering.
// ========================================

extern boolean         nomusic; //defined in d_main.c
extern boolean         nosound;

// =========================
// Status flags for refresh.
// =========================
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern  boolean statusbaractive;

extern  boolean menuactive;     // Menu overlayed?
extern  boolean paused;         // Game Pause?


extern  boolean         viewactive;

extern  boolean         nodrawers;
extern  boolean         noblit;

extern  int             viewwindowx;
extern  int             viewwindowy;
extern  int             viewheight;
extern  int             viewwidth;
extern  int             scaledviewwidth;



// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern  int     viewangleoffset;

// Player taking events, and displaying.
extern  byte    consoleplayer;
extern  byte    displayplayer;
extern  byte    secondarydisplayplayer; // for splitscreen

//added:16-01-98: player from which the statusbar displays the infos.
extern  byte    statusbarplayer;


// ============================================
// Statistics on a given map, for intermission.
// ============================================
//
extern  int     totalkills;
extern  int     totalitems;
extern  int     totalsecret;


// ===========================
// Internal parameters, fixed.
// ===========================
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

extern  ULONG           gametic;

// Player spawn spots.
extern  mapthing_t      playerstarts[MAXPLAYERS];

// Intermission stats.
// Parameters for world map / intermission.
extern  wbstartstruct_t         wminfo;


// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern  int             maxammo[NUMAMMO];





// =====================================
// Internal parameters, used for engine.
// =====================================
//

// File handling stuff.
extern  char            basedefault[1024];
extern  FILE*           debugfile;

// if true, load all graphics at level load
extern  boolean         precache;


// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern  gamestate_t     wipegamestate;

//?
// debug flag to cancel adaptiveness
extern  boolean         singletics;

extern  int             bodyqueslot;


// =============
// Netgame stuff
// =============

// Points to the communication buffer between IPXSETUP and DOOM
// The address of this buffer is communicated to Doom via the -net parm.
//extern  doomcom_t*      doomcom;

// This points inside doomcom.
//extern  doomdata_t*     netbuffer;


//extern  ticcmd_t        localcmds[BACKUPTICS];

extern  ULONG           maketic;

extern  ticcmd_t        netcmds[BACKUPTICS][MAXPLAYERS];

#endif //__D_STATE__
