// g_game.h :

#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"

//added:11-02-98: yeah now you can change it!
// changed to 2d array 19990220 by Kin
extern char          player_names[MAXPLAYERS][MAXPLAYERNAME];
extern char*                    team_names[];

extern  boolean nomonsters;             // checkparm of -nomonsters
extern  boolean respawnparm;    // checkparm of -respawn
extern  boolean fastparm;               // checkparm of -fast

extern  player_t        players[MAXPLAYERS];
extern  boolean   playeringame[MAXPLAYERS];

// ======================================
// DEMO playback/recording related stuff.
// ======================================
extern  boolean usergame;

// demoplaying back and demo recording
extern  boolean demoplayback;
extern  boolean demorecording;
extern  boolean timingdemo;       // timedemo

// Quit after playing a demo from cmdline.
extern  boolean         singledemo;

extern  ULONG           levelstarttic;  // gametic at level start

// used in game menu
extern consvar_t  cv_crosshair;
extern consvar_t  cv_autorun;
extern consvar_t  cv_invertmouse;
extern consvar_t  cv_alwaysfreelook;
extern consvar_t  cv_showmessages;
extern consvar_t  cv_fastmonsters;

// build an internal map name ExMx MAPxx from episode,map numbers
char* G_BuildMapName (int episode, int map);
void G_BuildTiccmd (ticcmd_t* cmd, int realtics);
void G_BuildTiccmd2(ticcmd_t* cmd, int realtics);

//added:22-02-98: clip the console player aiming to the view
short G_ClipAimingPitch (int* aiming);

extern angle_t localangle,localangle2;
extern int     localaiming,localaiming2; // should be a angle_t but signed


//
// GAME
//
void    G_DoReborn (int playernum);
boolean G_DeathMatchSpawnPlayer (int playernum);
void G_CoopSpawnPlayer (int playernum);
void    G_PlayerReborn (int player);

void G_InitNew (skill_t skill, char* mapname);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (skill_t skill, char* mapname);

void G_DeferedPlayDemo (char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_LoadGame (int slot);
void G_DoLoadGame (int slot);

// Called by M_Responder.
void G_DoSaveGame(int slot, char* description);
void G_SaveGame  (int slot, char* description);

// Only called by startup code.
void G_RecordDemo (char* name);

void G_BeginRecording (void);

void G_PlayDemo (char* name);
void G_TimeDemo (char* name);
void G_StopDemo(void);
boolean G_CheckDemoStatus (void);

void G_ExitLevel (void);
void G_SecretExitLevel (void);

void G_WorldDone (void);

//added:12-02-98: init new features of Doom LEGACY by default
void G_InitNewFeatures (void);

void G_Ticker (void);
boolean G_Responder (event_t*   ev);

void G_ScreenShot (void);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
