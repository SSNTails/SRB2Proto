
//
// g_input.c : handle mouse/keyboard/joystick inputs,
//             maps inputs to game controls (forward,use,open...)
//
// by Denis Fabrice for Doom LEGACY project based on doom sources.
//

#include "doomdef.h"
#include "g_input.h"
#include "keys.h"
#include "hu_stuff.h"   //need HUFONT start & end
#include "keys.h"
#include "d_net.h"
#include "console.h"

CV_PossibleValue_t mousesens_cons_t[]={{1,"MIN"},{MAXMOUSESENSITIVITY,"MAXCURSOR"},{MAXINT,"MAX"},{0,NULL}};

// mouse values are used once
consvar_t  cv_mousesens    = {"mousesens","10",CV_SAVE,mousesens_cons_t};
consvar_t  cv_mlooksens    = {"mlooksens","10",CV_SAVE,mousesens_cons_t};
consvar_t  cv_allowjump    = {"allowjump","1",CV_NETVAR,CV_OnOff};
consvar_t  cv_allowautoaim = {"allowautoaim","1",CV_NETVAR,CV_OnOff};

int             mousex;
int             mousey;
int             mlooky;         //like mousey but with a custom sensitivity
                                //for mlook
// joystick values are repeated
int             joyxmove;
int             joyymove;

// current state of the keys : true if pushed
byte    gamekeydown[NUMINPUTS];

// two key codes (or virtual key) per game control
int     gamecontrol[num_gamecontrols][2];
int     gamecontrolbis[num_gamecontrols][2];        // secondary splitscreen player


typedef struct {
    int time;
    int state;
    int clicks;
} dclick_t;
static  dclick_t  mousedclicks[MOUSEBUTTONS];
static  dclick_t  joydclicks[JOYBUTTONS];



// protos
static boolean G_CheckDoubleClick (int state, dclick_t *dt);


//
//  Remaps the inputs to game controls.
//
//  A game control can be triggered by one or more keys/buttons.
//
//  Each key/mousebutton/joybutton triggers ONLY ONE game control.
//
//
void  G_MapEventsToControls (event_t *ev)
{
    int    i,flag;

    switch (ev->type)
    {
      case ev_keydown:
        if (ev->data1 <NUMINPUTS)
            gamekeydown[ev->data1] = 1;
        break;

      case ev_keyup:
        if (ev->data1 <NUMINPUTS)
            gamekeydown[ev->data1] = 0;
        break;

      case ev_mouse:           // buttons hare virtual keys
        mousex = ev->data2*(cv_mousesens.value+1)/10;
        mousey = ev->data3*(cv_mousesens.value+1)/10;

        //added:10-02-98:
        // for now I use the mlook sensitivity just for mlook,
        // instead of having a general mouse y sensitivity.
        mlooky = ev->data3*(cv_mlooksens.value+1)/10;
        break;

      case ev_joystick:        // buttons are virtual keys
        joyxmove = ev->data2;
        joyymove = ev->data3;
        break;

      default:
        break;

    }

    // ALWAYS check for mouse & joystick double-clicks
    // even if no mouse event
    for (i=0;i<MOUSEBUTTONS;i++)
    {
        flag = G_CheckDoubleClick (gamekeydown[KEY_MOUSE1+i], &mousedclicks[i]);
        gamekeydown[KEY_DBLMOUSE1+i] = flag;
    }

    for (i=0;i<JOYBUTTONS;i++)
    {
        flag = G_CheckDoubleClick (gamekeydown[KEY_JOY1+i], &joydclicks[i]);
        gamekeydown[KEY_DBLJOY1+i] = flag;
    }
}


//
//  General double-click detection routine for any kind of input.
//
static boolean G_CheckDoubleClick (int state, dclick_t *dt)
{
    if (state != dt->state && dt->time > 1 )
    {
        dt->state = state;
        if (state)
            dt->clicks++;
        if (dt->clicks == 2)
        {
            dt->clicks = 0;
            return true;
        }
        else
            dt->time = 0;
    }
    else
    {
        dt->time += ticdup;
        if (dt->time > 20)
        {
            dt->clicks = 0;
            dt->state = 0;
        }
    }
    return false;
}


typedef struct {
    int  keynum;
    char name[15];
} keyname_t;

static keyname_t keynames[] = {

    {KEY_SPACE     ,"SPACE"},
    {KEY_CAPSLOCK  ,"CAPS LOCK"},
    {KEY_ENTER     ,"ENTER"},
    {KEY_TAB       ,"TAB"},
    {KEY_ESCAPE    ,"ESCAPE"},
    {KEY_BACKSPACE ,"BACKSPACE"},

    {KEY_NUMLOCK   ,"NUMLOCK"},
    {KEY_SCROLLLOCK,"SCROLLLOCK"},

    // bill gates keys

    {KEY_LEFTWIN   ,"LEFTWIN"},
    {KEY_RIGHTWIN  ,"RIGHTWIN"},
    {KEY_MENU      ,"MENU"},

    // shift,ctrl,alt are not distinguished between left & right

    {KEY_SHIFT     ,"SHIFT"},
    {KEY_CTRL      ,"CTRL"},
    {KEY_ALT       ,"ALT"},

    // keypad keys

    {KEY_KPADSLASH,"KEYPAD /"},

    {KEY_KEYPAD7, "KEYPAD 7"},
    {KEY_KEYPAD8, "KEYPAD 8"},
    {KEY_KEYPAD9, "KEYPAD 9"},
    {KEY_MINUSPAD,"KEYPAD -"},
    {KEY_KEYPAD4, "KEYPAD 4"},
    {KEY_KEYPAD5, "KEYPAD 5"},
    {KEY_KEYPAD6, "KEYPAD 6"},
    {KEY_PLUSPAD, "KEYPAD +"},
    {KEY_KEYPAD1, "KEYPAD 1"},
    {KEY_KEYPAD2, "KEYPAD 2"},
    {KEY_KEYPAD3, "KEYPAD 3"},
    {KEY_KEYPAD0, "KEYPAD 0"},
    {KEY_KPADDEL, "KEYPAD ."},

    // extended keys (not keypad)

    {KEY_HOME,      "HOME"},
    {KEY_UPARROW,   "UP ARROW"},
    {KEY_PGUP,      "PGUP"},
    {KEY_LEFTARROW ,"LEFT ARROW"},
    {KEY_RIGHTARROW,"RIGHT ARROW"},
    {KEY_END,       "END"},
    {KEY_DOWNARROW, "DOWN ARROW"},
    {KEY_PGDN,      "PGDN"},
    {KEY_INS,       "INS"},
    {KEY_DEL,       "DEL"},

    // other keys

    {KEY_F1, "F1"},
    {KEY_F2, "F2"},
    {KEY_F3, "F3"},
    {KEY_F4, "F4"},
    {KEY_F5, "F5"},
    {KEY_F6, "F6"},
    {KEY_F7, "F7"},
    {KEY_F8, "F8"},
    {KEY_F9, "F9"},
    {KEY_F10,"F10"},
    {KEY_F11,"F11"},
    {KEY_F12,"F12"},

    // virtual keys for mouse buttons and joystick buttons

    {KEY_MOUSE1,  "MOUSE1"},
    {KEY_MOUSE1+1,"MOUSE2"},
    {KEY_MOUSE1+2,"MOUSE3"},

    {KEY_JOY1,  "JOY1"},
    {KEY_JOY1+1,"JOY2"},
    {KEY_JOY1+2,"JOY3"},
    {KEY_JOY1+3,"JOY4"},
    {KEY_JOY1+4,"JOY5"},
    {KEY_JOY1+5,"JOY6"},
    // we use up to 10 buttons in DirectInput
    {KEY_JOY1+6,"JOY7"},
    {KEY_JOY1+7,"JOY8"},
    {KEY_JOY1+8,"JOY9"},
    {KEY_JOY1+9,"JOY10"},
    // the DOS version uses Allegro's joystick support
    // the Hat is reported as extra buttons
    {KEY_JOY1+10,"HATUP"},
    {KEY_JOY1+11,"HATDOWN"},
    {KEY_JOY1+12,"HATLEFT"},
    {KEY_JOY1+13,"HATRIGHT"},

    {KEY_DBLMOUSE1,  "DBLMOUSE1"},
    {KEY_DBLMOUSE1+1,"DBLMOUSE2"},
    {KEY_DBLMOUSE1+2,"DBLMOUSE3"},

    {KEY_DBLJOY1,  "DBLJOY1"},
    {KEY_DBLJOY1+1,"DBLJOY2"},
    {KEY_DBLJOY1+2,"DBLJOY3"},
    {KEY_DBLJOY1+3,"DBLJOY4"},
    {KEY_DBLJOY1+4,"DBLJOY5"},
    {KEY_DBLJOY1+5,"DBLJOY6"},

};

char *gamecontrolname[num_gamecontrols] =
{
    "nothing",        //a key/button mapped to gc_null has no effect
    "forward",
    "backward",
    "strafe",
    "straferight",
    "strafeleft",
    "speed",
    "turnleft",
    "turnright",
    "fire",
    "use",
    "lookup",
    "lookdown",
    "centerview",
    "mouseaiming",
    "talkkey",
    "scores",
    "jump",
    "camleft", // camera left Tails 03-03-2000
    "camright", // camera right Tails 03-03-2000
    "console",
    "nextweapon",
    "prevweapon"
};

#define NUMKEYNAMES (sizeof(keynames)/sizeof(keyname_t))

//
//  Detach any keys associated to the given game control
//  - pass the pointer to the gamecontrol table for the player being edited
void  G_ClearControlKeys (int (*setupcontrols)[2], int control)
{
    setupcontrols[control][0] = KEY_NULL;
    setupcontrols[control][1] = KEY_NULL;
}

//
//  Returns the name of a key (or virtual key for mouse and joy)
//  the input value being an keynum
//
char* G_KeynumToString (int keynum)
{
static char keynamestr[8];

    int    j;

    // return a string with the ascii char if displayable
    if (keynum>' ' && keynum<='z' && keynum!=KEY_CONSOLE)
    {
        keynamestr[0] = keynum;
        keynamestr[1] = '\0';
        return keynamestr;
    }

    // find a description for special keys
    for (j=0;j<NUMKEYNAMES;j++)
        if (keynames[j].keynum==keynum)
            return keynames[j].name;

    // create a name for Unknown key
    sprintf (keynamestr,"KEY%d",keynum);
    return keynamestr;
}


int G_KeyStringtoNum(char *keystr)
{
    int j;

//    strupr(keystr);

    if(keystr[1]==0 && keystr[0]>' ' && keystr[0]<='z')
        return keystr[0];

    for (j=0;j<NUMKEYNAMES;j++)
        if (stricmp(keynames[j].name,keystr)==0)
            return keynames[j].keynum;

    if(strlen(keystr)>3)
        return atoi(&keystr[3]);

    return 0;
}

void G_Controldefault(void)
{
    gamecontrol[gc_forward    ][0]=KEY_UPARROW;
    gamecontrol[gc_forward    ][1]=KEY_MOUSE1+2;
    gamecontrol[gc_backward   ][0]=KEY_DOWNARROW;
    gamecontrol[gc_strafe     ][0]=KEY_ALT;
    gamecontrol[gc_strafe     ][1]=KEY_MOUSE1+1;
    gamecontrol[gc_straferight][0]='.';
    gamecontrol[gc_strafeleft ][0]=',';
    gamecontrol[gc_speed      ][0]=KEY_SHIFT;
    gamecontrol[gc_turnleft   ][0]=KEY_LEFTARROW;
    gamecontrol[gc_turnright  ][0]=KEY_RIGHTARROW;
    gamecontrol[gc_fire       ][0]=KEY_CTRL;
    gamecontrol[gc_fire       ][1]=KEY_MOUSE1;
    gamecontrol[gc_use        ][0]=KEY_SPACE;
    gamecontrol[gc_lookup     ][0]=KEY_PGUP;
    gamecontrol[gc_lookdown   ][0]=KEY_PGDN;
    gamecontrol[gc_centerview ][0]=KEY_END;
    gamecontrol[gc_mouseaiming][0]='s';
    gamecontrol[gc_talkkey    ][0]='t';
    gamecontrol[gc_scores     ][0]='f';
    gamecontrol[gc_jump       ][0]='/';
    gamecontrol[gc_camleft    ][0] = 'o'; // camera left Tails 03-03-2000
    gamecontrol[gc_camright   ][0] = 'p'; // camera right Tails 03-03-2000
    gamecontrol[gc_console    ][0]=KEY_CONSOLE;
    gamecontrol[gc_nextweapon ][0]=']';
    gamecontrol[gc_nextweapon ][1]=KEY_JOY1+4;
    gamecontrol[gc_prevweapon ][0]='[';
    gamecontrol[gc_prevweapon ][1]=KEY_JOY1+5;
}

void G_SaveKeySetting(FILE *f)
{
    int i;

    for(i=1;i<num_gamecontrols;i++)
       {
           fprintf(f,"setcontrol \"%s\" \"%s\""
                    ,gamecontrolname[i]
                    ,G_KeynumToString(gamecontrol[i][0]));

           if(gamecontrol[i][1])
               fprintf(f," \"%s\"\n"
                        ,G_KeynumToString(gamecontrol[i][1]));
           else
               fprintf(f,"\n");
       }

    for(i=1;i<num_gamecontrols;i++)
       {
           fprintf(f,"setcontrol2 \"%s\" \"%s\""
                    ,gamecontrolname[i]
                    ,G_KeynumToString(gamecontrolbis[i][0]));

           if(gamecontrolbis[i][1])
               fprintf(f," \"%s\"\n"
                        ,G_KeynumToString(gamecontrolbis[i][1]));
           else
               fprintf(f,"\n");
       }
}

void setcontrol(int (*gc)[2],int na)
{
    int numctrl;
    char *namectrl;

    namectrl=COM_Argv(1);
    for(numctrl=0;numctrl<num_gamecontrols
                  && stricmp(namectrl,gamecontrolname[numctrl])
                 ;numctrl++);
    if(numctrl==num_gamecontrols)
    {
        CONS_Printf("Control '%s' unknown\n",namectrl);
        return;
    }
    gc[numctrl][0]=G_KeyStringtoNum(COM_Argv(2));

    if(na==4)
        gc[numctrl][1]=G_KeyStringtoNum(COM_Argv(3));
    else
        gc[numctrl][1]=0;
}

void Command_Setcontrol_f(void)
{
    int na;

    na= COM_Argc();

    if ( na!= 3 && na!=4)
    {
        CONS_Printf ("setcontrol <controlname> <keyname> [<2nd keyname>]\n");
        return;
    }

    setcontrol(gamecontrol,na);
}

void Command_Setcontrol2_f(void)
{
    int na;

    na= COM_Argc();

    if ( na!= 3 && na!=4)
    {
        CONS_Printf ("setcontrol2 <controlname> <keyname> [<2nd keyname>]\n");
        return;
    }

    setcontrol(gamecontrolbis,na);
}
