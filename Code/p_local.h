//      Play functions, animation, global header.


#ifndef __P_LOCAL__
#define __P_LOCAL__

#include "command.h"
#include "d_player.h"
#include "d_think.h"
#include "m_fixed.h"
#include "m_bbox.h"
#include "p_tick.h"
#include "r_defs.h"
#include "p_maputl.h"

#define FLOATSPEED              (FRACUNIT*4)

// added by Boris : for dehacked patches, replaced #define by int
extern int MAXHEALTH;   // 100

#define VIEWHEIGHT               41

// default viewheight is changeable at console
extern consvar_t cv_viewheight; // p_mobj.c

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)


// player radius for movement checking
#define PLAYERRADIUS    (16*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)

#define MAXMOVE         (30*FRACUNIT)

//added:26-02-98: max Z move up or down without jumping
//      above this, a heigth difference is considered as a 'dropoff'
#define MAXSTEPMOVE     (24*FRACUNIT)

//added:22-02-98: initial momz when player jumps (moves up)
#define JUMPGRAVITY     (6*FRACUNIT)

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD   100

#//define AIMINGTOSLOPE(aiming)   finetangent[(2048+(aiming>>ANGLETOFINESHIFT)) & FINEMASK]
#define AIMINGTOSLOPE(aiming)   finesine[(aiming>>ANGLETOFINESHIFT) & FINEMASK]

//26-07-98: p_mobj.c
extern  consvar_t cv_gravity;


//
// P_TICK
//

// both the head and tail of the thinker list
extern  thinker_t       thinkercap;


void P_InitThinkers (void);
void P_AddThinker (thinker_t* thinker);
void P_RemoveThinker (thinker_t* thinker);


//
// P_PSPR
//
void P_SetupPsprites (player_t* curplayer);
void P_MovePsprites (player_t* curplayer);
void P_DropWeapon (player_t* player);


//
// P_USER
//
typedef struct camera_s
{
    boolean     chase;
    angle_t     aiming;
    int         fixedcolormap;

    mobj_t*     mo;
} camera_t;

extern camera_t camera;

void   P_ResetCamera (player_t *player);
void   P_PlayerThink (player_t* player);


//
// P_MOBJ
//
#define ONFLOORZ                MININT
#define ONCEILINGZ              MAXINT

// Time interval for item respawning.
// WARING MUST be a power of 2
#define ITEMQUESIZE             128

extern mapthing_t       itemrespawnque[ITEMQUESIZE];
extern int              itemrespawntime[ITEMQUESIZE];
extern int              iquehead;
extern int              iquetail;


void P_RespawnSpecials (void);
void P_RespawnWeapons(void);

mobj_t*
P_SpawnMobj
( fixed_t       x,
  fixed_t       y,
  fixed_t       z,
  mobjtype_t    type );

void    P_RemoveMobj (mobj_t* th);
boolean P_SetMobjState (mobj_t* mobj, statenum_t state);
void    P_MobjThinker (mobj_t* mobj);

//spawn splash at surface of water in sector where the mobj resides
void    P_SpawnSplash (mobj_t* mo, boolean oldwater);
//Fab: when fried in in lava/slime, spawn some smoke
void    P_SpawnSmoke (fixed_t x, fixed_t y, fixed_t z);

void    P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z);
void    P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);
void    P_SpawnBloodSplats (fixed_t x, fixed_t y, fixed_t z, int damage, divline_t* damagedir);
mobj_t* P_SpawnMissile (mobj_t* source, mobj_t* dest, mobjtype_t type);

//added:16-02-98: added arg3 player_t* since its only used for player its ok
void    P_SpawnPlayerMissile (mobj_t* source, mobjtype_t type, player_t* pl);


//
// P_ENEMY
//
void P_NoiseAlert (mobj_t* target, mobj_t* emmiter);



boolean
P_PathTraverse
( fixed_t       x1,
  fixed_t       y1,
  fixed_t       x2,
  fixed_t       y2,
  int           flags,
  boolean       (*trav) (intercept_t *));

void P_UnsetThingPosition (mobj_t* thing);
void P_SetThingPosition (mobj_t* thing);


//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern boolean          floatok;
extern fixed_t          tmfloorz;
extern fixed_t          tmceilingz;
extern fixed_t          tmsectorceilingz;      //added:28-02-98: p_spawnmobj
extern mobj_t*          tmfloorthing;

extern  line_t*         ceilingline;
extern  line_t*         blockingline;

boolean P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (mobj_t* thing, fixed_t x, fixed_t y);
boolean P_TeleportMove (mobj_t* thing, fixed_t x, fixed_t y);
void    P_SlideMove (mobj_t* mo);
boolean P_CheckSight (mobj_t* t1, mobj_t* t2);
void    P_UseLines (player_t* player);

boolean P_ChangeSector (sector_t* sector, boolean crunch);

extern mobj_t*  linetarget;     // who got hit (or NULL)

fixed_t
P_AimLineAttack
( mobj_t*       t1,
  angle_t       angle,
  fixed_t       distance );

void
P_LineAttack
( mobj_t*       t1,
  angle_t       angle,
  fixed_t       distance,
  fixed_t       slope,
  int           damage );

void
P_RadiusAttack
( mobj_t*       spot,
  mobj_t*       source,
  int           damage );



//
// P_SETUP
//
extern byte*            rejectmatrix;   // for fast sight rejection
extern short*           blockmaplump;   // offsets in blockmap are from here
extern short*           blockmap;
extern int              bmapwidth;
extern int              bmapheight;     // in mapblocks
extern fixed_t          bmaporgx;
extern fixed_t          bmaporgy;       // origin of block map
extern mobj_t**         blocklinks;     // for thing chains


//
// P_INTER
//
extern int              maxammo[NUMAMMO];
extern int              clipammo[NUMAMMO];

void
P_TouchSpecialThing
( mobj_t*       special,
  mobj_t*       toucher );

boolean
P_DamageMobj
( mobj_t*       target,
  mobj_t*       inflictor,
  mobj_t*       source,
  int           damage );


//
// P_SPEC
//
#include "p_spec.h"


#endif  // __P_LOCAL__
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
