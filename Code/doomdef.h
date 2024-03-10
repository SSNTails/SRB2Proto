
// doomdef.h : general header

#ifndef __DOOMDEF__
#define __DOOMDEF__


#include "doomtype.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <ctype.h>

#ifndef LINUX
// only dos need this 19990203 by Kin
#include <io.h>
#endif

#ifdef PC_DOS
#include <conio.h>
#endif

// Uncheck this to compile debugging code
//#define RANGECHECK
//#define PARANOIA                // do some test that never happens but maybe
#define VERSION        128      // Game version
#define VERSIONSTRING  ""

// some tests, enable or desable it if it run or not
//#define HORIZONTALDRAW        // abandoned : too slow
//#define CLIENTPREDICTION      // not finished
//#define TILTVIEW              // not finished
//#define PERSPCORRECT          // not finished
//#define SPLITSCREEN             // not finished
#define ABSOLUTEANGLE           // work fine, soon #ifdef and old code remove
//#define JOININGAME              // next incoming feature to do


// =========================================================================
#ifdef __WIN32__
#define ASMCALL __cdecl
#pragma warning (disable :  4244 4146 4761) // 4244 4146 4761 4018
// warning C4146: unary minus operator applied to unsigned type, result still unsigned
// warning C4761: integral size mismatch in argument; conversion supplied
// warning C4244: 'initializing' : conversion from 'const double ' to 'int ', possible loss of data
// warning C4244: '=' : conversion from 'double ' to 'int ', possible loss of data
// warning C4018: '<' : signed/unsigned mismatch

#else
#define ASMCALL
#endif



// demo version when playback demo, or the current VERSION
// used to enable/disable selected features for backward compatibility
// (where possible)
extern byte     demoversion;


// The maximum number of players, multiplayer/networking.
// NOTE: it needs more than this to increase the number of players...

#define MAXPLAYERS              32      // TODO: ... more!!!
#define MAXSKINS                MAXPLAYERS
#define PLAYERSMASK             (MAXPLAYERS-1)
#define MAXPLAYERNAME           21
#define MAXSKINCOLORS           11

#define SAVESTRINGSIZE          24

// State updates, number of tics / second.
// NOTE: used to setup the timer rate, see I_StartupTimer().
#define TICRATE         35


// ==================================
// Difficulty/skill settings/filters.
// ==================================

// Skill flags.
#define MTF_EASY                1
#define MTF_NORMAL              2
#define MTF_HARD                4

// Deaf monsters/do not react to sound.
#define MTF_AMBUSH              8

#include "g_state.h"

//
// Key cards.
//
typedef enum
{
    it_bluecard,
    it_yellowcard,
    it_redcard,
    it_blueskull,
    it_yellowskull,
    it_redskull,

    NUMCARDS

} card_t;



// The defined weapons,
//  including a marker indicating
//  user has not changed weapon.
typedef enum
{
    wp_fist,
    wp_pistol,
    wp_shotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
    wp_chainsaw,
    wp_supershotgun,

    NUMWEAPONS,

    // No pending weapon change.
    wp_nochange

} weapontype_t;


// Ammunition types defined.
typedef enum
{
    am_clip,    // Pistol / chaingun ammo.
    am_shell,   // Shotgun / double barreled shotgun.
    am_cell,    // Plasma rifle, BFG.
    am_misl,    // Missile launcher.
    NUMAMMO,
    am_noammo   // Unlimited for chainsaw / fist.

} ammotype_t;


// Power up artifacts.
typedef enum
{
    pw_invulnerability,
    pw_strength,
    pw_invisibility,
    pw_ironfeet,
    pw_allmap,
    pw_infrared,
    pw_blueshield, // blue shield Tails 03-04-2000
    pw_tailsfly, // tails flying Tails 03-05-2000
    NUMPOWERS

} powertype_t;



//
// Power up durations,
//  how many seconds till expiration,
//  assuming TICRATE is 35 ticks/second.
//
typedef enum
{
    INVULNTICS  = (30*TICRATE),
    INVISTICS   = (3*TICRATE), // just hit Tails 2-26-2000
    INFRATICS   = (120*TICRATE),
    IRONTICS    = (60*TICRATE), // Tails 02-28-2000
    SPEEDTICS   = (30*TICRATE), // for super sneaker timer Tails 02-28-2000
    TAILSTICS   = (10*TICRATE), // tails flying Tails 03-05-2000

} powerduration_t;


// commonly used routines - moved here for include convenience

// i_system.h
void I_Error (char *error, ...);

// console.h
void    CONS_Printf (char *fmt, ...);

#include "m_swap.h"

// m_misc.h
char *va(char *format, ...);
char *DupString (char *in);

// g_game.h
extern  boolean devparm;                // development mode (-devparm)

// =======================
// Misc stuff for later...
// =======================

// if we ever make our alloc stuff...
#define ZZ_Alloc(x) Z_Malloc(x,PU_STATIC,NULL)

// debug me in color (v_video.c)
void IO_Color( unsigned char color, unsigned char r, unsigned char g, unsigned char b );

// i_system.c, replace getchar() once the keyboard has been appropriated
int I_GetKey (void);


#endif          // __DOOMDEF__
