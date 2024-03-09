
// win_joy.h : share joystick information with game control code

#ifndef __I_JOY_H__
#define __I_JOY_H__

#include "g_input.h"

#define JOYAXISRANGE     1023   //faB: (1024-1) so we can do a right shift instead of division
                                //     (doesnt matter anyway, just give enough precision)
                                // a gamepad will return -1, 0, or 1 in the event data
                                // an analog type joystick will return a value
                                // from -JOYAXISRANGE to +JOYAXISRANGE for each axis

// detect a bug if we increase JOYBUTTONS above DIJOYSTATE's number of buttons
#if (JOYBUTTONS > 32)
#error "JOYBUTTONS is greater than DIJOYSTATE number of buttons"
#endif

// share some joystick information (maybe 2 for splitscreen), to the game input code,
// actually, we need to know if it is a gamepad or analog controls

struct JoyType_s {
    int     bJoyNeedPoll;       // if true, we MUST Poll() to get new joystick data,
                                // that is: we NEED the DIRECTINPUTDEVICE2 ! (watchout NT compatibility)
    int     bGamepadStyle;      // this joystick is a gamepad, read: digital axes
                                // if FALSE, interpret the joystick event data as JOYAXISRANGE
                                // (see above)
};
typedef struct JoyType_s JoyType_t;

extern JoyType_t   Joystick;    //faB: may become an array (2 for splitscreen), I said: MAY BE...

#endif // __I_JOY_H__
