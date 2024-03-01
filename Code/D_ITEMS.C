
// d_items.h : holds the weapon info for now...


// We are referring to sprite numbers.
#include "info.h"
#include "d_items.h"


//
// PSPRITE ACTIONS for weapons.
// This struct controls the weapon animations.
//
// Each entry is:
//  ammo/amunition type
//  upstate
//  downstate
//  readystate
//  atkstate, i.e. attack/fire/hit frame
//  flashstate, muzzle flash
//
weaponinfo_t    weaponinfo[NUMWEAPONS] =
{
    {
        // fist
        am_noammo,
// no more punches Tails 11-06-99
        S_MISSILEUP,
        S_MISSILEDOWN,
        S_MISSILE,
        S_MISSILE1,
        S_MISSILEFLASH1
    },
    {
        // pistol
        am_clip,
// no more pistol Tails 11-06-99
        S_NULL
    },
    {
        // shotgun
        am_shell,
        S_SGUNUP,
        S_SGUNDOWN,
        S_SGUN,
        S_SGUN1,
        S_SGUNFLASH1
    },
    {
        // chaingun
        am_clip,
        S_CHAINUP,
        S_CHAINDOWN,
        S_CHAIN,
        S_CHAIN1,
        S_CHAINFLASH1
    },
    {
        // missile launcher
        am_misl,
        S_MISSILEUP,
        S_MISSILEDOWN,
        S_MISSILE,
        S_MISSILE1,
        S_MISSILEFLASH1
    },
    {
        // plasma rifle
        am_cell,
        S_PLASMAUP,
        S_PLASMADOWN,
        S_PLASMA,
        S_PLASMA1,
        S_PLASMAFLASH1
    },
    {
        // bfg 9000
        am_cell,
        S_BFGUP,
        S_BFGDOWN,
        S_BFG,
        S_BFG1,
        S_BFGFLASH1
    },
    {
        // chainsaw
        am_noammo,
        S_SAWUP,
        S_SAWDOWN,
        S_SAW,
        S_SAW1,
        S_NULL
    },
    {
        // super shotgun
        am_shell,
        S_DSGUNUP,
        S_DSGUNDOWN,
        S_DSGUN,
        S_DSGUN1,
        S_DSGUNFLASH1
    },
};
