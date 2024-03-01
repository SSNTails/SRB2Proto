//-----------------------------------------------------------------------------
//
// g_input.h
//
//-----------------------------------------------------------------------------


#ifndef __G_INPUT__
#define __G_INPUT__

#include "d_event.h"
#include "keys.h"
#include "command.h"

#define MAXMOUSESENSITIVITY   40        // sensitivity steps

// number of total 'button' inputs, include keyboard keys, plus virtual
// keys (mousebuttons and joybuttons becomes keys)
#define NUMKEYS         256

#define MOUSEBUTTONS    3
#define MOUSE2BUTTONS   2
#define JOYBUTTONS      14  // 10 bases + 4 hat

//
// mouse and joystick buttons are handled as 'virtual' keys
//
#define KEY_MOUSE1      (NUMKEYS)                  // 256
#define KEY_JOY1        (KEY_MOUSE1   +MOUSEBUTTONS)   // 259
#define KEY_DBLMOUSE1   (KEY_JOY1     +JOYBUTTONS)       // 263  // double clicks
#define KEY_DBLJOY1     (KEY_DBLMOUSE1+MOUSEBUTTONS) // 266
#define KEY_2MOUSE1     (KEY_DBLJOY1  +JOYBUTTONS)

#define NUMINPUTS       (KEY_2MOUSE1  +MOUSE2BUTTONS)   // 270

enum
{
    gc_null = 0,        //a key/button mapped to gc_null has no effect
    gc_forward,
    gc_backward,
    gc_strafe,
    gc_straferight,
    gc_strafeleft,
    gc_speed,
    gc_turnleft,
    gc_turnright,
    gc_fire,
    gc_use,
    gc_lookup,
    gc_lookdown,
    gc_centerview,
    gc_mouseaiming,     // mouse aiming is momentary (toggleable in the menu)
    gc_talkkey,
    gc_scores,
    gc_jump,
    gc_camleft, // camera left Tails 03-03-2000
    gc_camright, // camera right Tails 03-03-2000
    gc_console,
    gc_nextweapon,
    gc_prevweapon,
    num_gamecontrols
} gamecontrols_e;


// mouse values are used once
extern consvar_t       cv_mousesens;
extern consvar_t       cv_mlooksens;
extern consvar_t       cv_allowjump;
extern consvar_t       cv_allowautoaim;
extern int             mousex;
extern int             mousey;
extern int             mlooky;  //mousey with mlookSensitivity

extern int             dclicktime;
extern int             dclickstate;
extern int             dclicks;
extern int             dclicktime2;
extern int             dclickstate2;
extern int             dclicks2;

extern int             joyxmove;
extern int             joyymove;

// current state of the keys : true if pushed
extern  byte    gamekeydown[NUMINPUTS];

// two key codes (or virtual key) per game control
extern  int     gamecontrol[num_gamecontrols][2];
extern  int     gamecontrolbis[num_gamecontrols][2];    // secondary splitscreen player

// peace to my little coder fingers!
// check a gamecontrol being active or not

// remaps the input event to a game control.
void  G_MapEventsToControls (event_t *ev);

// returns the name of a key
char* G_KeynumToString (int keynum);
int   G_KeyStringtoNum(char *keystr);

// detach any keys associated to the given game control
void  G_ClearControlKeys (int (*setupcontrols)[2], int control);
void  Command_Setcontrol_f(void);
void  Command_Setcontrol2_f(void);
void  G_Controldefault(void);
void  G_SaveKeySetting(FILE *f);

#endif
