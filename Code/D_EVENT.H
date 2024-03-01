// d_event.h : event handling.

#ifndef __D_EVENT__
#define __D_EVENT__

#include "doomtype.h"
#include "g_state.h"

// Input event types.
typedef enum
{
    ev_keydown,
    ev_keyup,
    ev_mouse,
    ev_joystick
} evtype_t;

// Event structure.
typedef struct
{
    evtype_t    type;
    int         data1;          // keys / mouse/joystick buttons
    int         data2;          // mouse/joystick x move
    int         data3;          // mouse/joystick y move
} event_t;



//
// Button/action code definitions.
//

//added:16-02-98: bit of value 64 doesnt seem to be used,
//                now its used to jump

typedef enum
{
    // Press "Fire".
    BT_ATTACK           = 1,
    // Use button, to open doors, activate switches.
    BT_USE              = 2,

    // Flag, weapon change pending.
    // If true, the next 3 bits hold weapon num.
    BT_CHANGE           = 4,
    // The 3bit weapon mask and shift, convenience.
    BT_WEAPONMASK       = (8+16+32),
    BT_WEAPONSHIFT      = 3,

    // Jump button.
    BT_JUMP             = 64,
    BT_EXTRAWEAPON      = 128, // Tails 03-03-2000
    BT_CAMLEFT          = 256, // Move cammy wammy left Tails 03-03-2000
    BT_CAMRIGHT         = 512, // Move cammy wammy right Tails 03-03-2000
} buttoncode_t;




//
// GLOBAL VARIABLES
//
#define MAXEVENTS               64

extern  event_t         events[MAXEVENTS];
extern  int             eventhead;
extern  int             eventtail;

extern  gameaction_t    gameaction;


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
