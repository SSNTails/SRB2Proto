
// d_netcmd.h : host/client network commands
//              commands are executed through the command buffer
//              like console commands

#ifndef __D_NETCMD__
#define __D_NETCMD__

#include "command.h"

// console vars
extern consvar_t   cv_playername;
extern consvar_t   cv_playercolor;
extern consvar_t   cv_usemouse;
extern consvar_t   cv_usejoystick;
extern consvar_t   cv_autoaim;

// normaly in p_mobj but the .h in not read !
extern consvar_t   cv_itemrespawntime;
extern consvar_t   cv_itemrespawn;
extern consvar_t   cv_respawnmonsters;
extern consvar_t   cv_respawnmonsterstime;

// added 16-6-98 : splitscreen
extern consvar_t   cv_splitscreen;

// 02-08-98      : r_things.c
extern consvar_t   cv_skin;

// secondary splitscreen player
extern consvar_t   cv_playername2;
extern consvar_t   cv_playercolor2;
extern consvar_t   cv_skin2;

extern consvar_t   cv_teamplay;
extern consvar_t   cv_teamdamage;
extern consvar_t   cv_fraglimit;
extern consvar_t   cv_timelimit;

typedef enum {
 XD_NAMEANDCOLOR=1,
 XD_WEAPONPREF,
 XD_EXIT,
 XD_QUIT,
 XD_KICK,
 XD_NETVAR,
 XD_SAY,
 XD_MAP,
 XD_EXITLEVEL,
 XD_LOADGAME,
 XD_SAVEGAME,
 XD_PAUSE,
 MAXNETXCMD
} netxcmd_t;

// add game commands, needs cleanup
void D_RegisterClientCommands (void);
void D_SendPlayerConfig(void);

#endif
