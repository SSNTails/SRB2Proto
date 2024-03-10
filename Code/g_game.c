
// g_game.c : game loop functions, events handling

#include "doomdef.h"
#include "command.h"
#include "console.h"
#include "dstrings.h"

#include "d_main.h"
#include "d_net.h"
#include "d_netcmd.h"
#include "f_finale.h"
#include "p_setup.h"
#include "p_saveg.h"

#include "i_system.h"

#include "wi_stuff.h"
#include "am_map.h"
#include "m_random.h"
#include "p_local.h"
#include "p_tick.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_sky.h"

#include "s_sound.h"

#include "g_game.h"
#include "g_state.h"
#include "g_input.h"

//added:16-01-98:quick hack test of rocket trails
#include "p_fab.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_argv.h"

#include "hu_stuff.h"

#include "st_stuff.h"

#include "keys.h"
#include "w_wad.h"
#include "z_zone.h"

#include "i_video.h"
#include "p_inter.h"
#include "byteptr.h"

#include "i_joy.h"

// added 8-3-98 increse savegame size from 0x2c000 to 512*1024
#define SAVEGAMESIZE    (512*1024)
#define SAVESTRINGSIZE  24



boolean G_Downgrade(int version);

boolean G_CheckDemoStatus (void);
void    G_ReadDemoTiccmd (ticcmd_t* cmd,int playernum);
void    G_WriteDemoTiccmd (ticcmd_t* cmd,int playernum);
void    G_InitNew (skill_t skill, char* mapname);

void    G_DoLoadLevel (void);
void    G_DoPlayDemo (void);
void    G_DoCompleted (void);
void    G_DoVictory (void);
void    G_DoWorldDone (void);


// demoversion the 'dynamic' version number, this should be == game VERSION
// when playing back demos, 'demoversion' receives the version number of the
// demo. At each change to the game play, demoversion is compared to
// the game version, if it's older, the changes are not done, and the older
// code is used for compatibility.
//
byte            demoversion=VERSION;

//boolean         respawnmonsters;

byte            gameepisode;
byte            gamemap;
char            gamemapname[MAX_WADPATH];      // an external wad filename


gamemode_t      gamemode = indetermined;       // Game Mode - identify IWAD as shareware, retail etc.
gamemission_t   gamemission = doom;
language_t      language = english;            // Language.
boolean         modifiedgame;                  // Set if homebrew PWAD stuff has been added.


boolean         paused;
boolean         usergame;               // ok to save / end game

boolean         timingdemo;             // if true, exit with report on completion
boolean         nodrawers;              // for comparative timing purposes
boolean         noblit;                 // for comparative timing purposes
ULONG           starttime;              // for comparative timing purposes

boolean         viewactive;

boolean         netgame;                // only true if packets are broadcast
boolean         multiplayer;
boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];

byte            consoleplayer;          // player taking events and displaying
byte            displayplayer;          // view being displayed
byte            secondarydisplayplayer; // for splitscreen
byte            statusbarplayer;        // player who's statusbar is displayed
                                        // (for spying with F12)

ULONG           gametic;
ULONG           levelstarttic;          // gametic at level start
int             totalkills, totalitems, totalsecret;    // for intermission

char            demoname[32];
boolean         demorecording;
boolean         demoplayback;
byte*           demobuffer;
byte*           demo_p;
byte*           demoend;
boolean         singledemo;             // quit after playing a demo from cmdline

boolean         precache = true;        // if true, load all graphics at start

wbstartstruct_t wminfo;                 // parms for world map / intermission

byte*           savebuffer;

void ShowMessage_OnChange(void);

CV_PossibleValue_t showmessages_cons_t[]={{0,"Off"},{1,"On"},{2,"Not All"},{0,NULL}};
CV_PossibleValue_t crosshair_cons_t[]   ={{0,"Off"},{1,"Cross"},{2,"Angle"},{3,"Point"},{0,NULL}};

consvar_t cv_crosshair      = {"crosshair"   ,"0",CV_SAVE,crosshair_cons_t};
consvar_t cv_autorun        = {"autorun"     ,"0",CV_SAVE,CV_OnOff};
consvar_t cv_invertmouse    = {"invertmouse" ,"0",CV_SAVE,CV_OnOff};
consvar_t cv_alwaysfreelook = {"alwaysmlook" ,"0",CV_SAVE,CV_OnOff};
consvar_t cv_showmessages   = {"showmessages","1",CV_SAVE | CV_CALL | CV_NOINIT,showmessages_cons_t,ShowMessage_OnChange};

#if MAXPLAYERS>32
#error please update "player_name" table using the new value for MAXPLAYERS
#endif
#if MAXPLAYERNAME!=21
#error please update "player_name" table using the new value for MAXPLAYERNAME
#endif
// changed to 2d array 19990220 by Kin
char    player_names[MAXPLAYERS][MAXPLAYERNAME] =
{
    // THESE SHOULD BE AT LEAST MAXPLAYERNAME CHARS
    "Player 1\0a123456789a\0",
    "Player 2\0a123456789a\0",
    "Player 3\0a123456789a\0",
    "Player 4\0a123456789a\0",
    "Player 5\0a123456789a\0",        // added 14-1-98 for support 8 players
    "Player 6\0a123456789a\0",        // added 14-1-98 for support 8 players
    "Player 7\0a123456789a\0",        // added 14-1-98 for support 8 players
    "Player 8\0a123456789a\0",        // added 14-1-98 for support 8 players
    "Player 9\0a123456789a\0",
    "Player 10\0a123456789\0",
    "Player 11\0a123456789\0",
    "Player 12\0a123456789\0",
    "Player 13\0a123456789\0",
    "Player 14\0a123456789\0",
    "Player 15\0a123456789\0",
    "Player 16\0a123456789\0",
    "Player 17\0a123456789\0",
    "Player 18\0a123456789\0",
    "Player 19\0a123456789\0",
    "Player 20\0a123456789\0",
    "Player 21\0a123456789\0",
    "Player 22\0a123456789\0",
    "Player 23\0a123456789\0",
    "Player 24\0a123456789\0",
    "Player 25\0a123456789\0",
    "Player 26\0a123456789\0",
    "Player 27\0a123456789\0",
    "Player 28\0a123456789\0",
    "Player 29\0a123456789\0",
    "Player 30\0a123456789\0",
    "Player 31\0a123456789\0",
    "Player 32\0a123456789\0"
};

char *team_names[MAXPLAYERS] =
{
    "Team 1\0a123456789bc\0",
    "Team 2\0a123456789bc\0",
    "Team 3\0a123456789bc\0",
    "Team 4\0a123456789bc\0",
    "Team 5\0a123456789bc\0",
    "Team 6\0a123456789bc\0",
    "Team 7\0a123456789bc\0",
    "Team 8\0a123456789bc\0",
    "Team 9\0a123456789bc\0",
    "Team 10\0a123456789bc\0",
    "Team 11\0a123456789bc\0",
    "Team 12\0a123456789bc\0",      // the other name hare not used because no colors
    "Team 13\0a123456789bc\0",      // but who know ?
    "Team 14\0a123456789bc\0",
    "Team 15\0a123456789bc\0",
    "Team 16\0a123456789bc\0",
    "Team 17\0a123456789bc\0",
    "Team 18\0a123456789bc\0",
    "Team 19\0a123456789bc\0",
    "Team 20\0a123456789bc\0",
    "Team 21\0a123456789bc\0",
    "Team 22\0a123456789bc\0",
    "Team 23\0a123456789bc\0",
    "Team 24\0a123456789bc\0",
    "Team 25\0a123456789bc\0",
    "Team 26\0a123456789bc\0",
    "Team 27\0a123456789bc\0",
    "Team 28\0a123456789bc\0",
    "Team 29\0a123456789bc\0",
    "Team 30\0a123456789bc\0",
    "Team 31\0a123456789bc\0",
    "Team 32\0a123456789bc\0"
};


#define   BODYQUESIZE     32

mobj_t*   bodyque[BODYQUESIZE];
int       bodyqueslot;

void*     statcopy;                      // for statistics driver

void ShowMessage_OnChange(void)
{
    if (!cv_showmessages.value)
        CONS_Printf(MSGOFF);
    else
        CONS_Printf(MSGON);
}

//added:16-02-98: unused ?
//int G_CmdChecksum (ticcmd_t* cmd)
//{
//    unsigned int i;
//    int          sum = 0;
//
//    for (i=0 ; i< sizeof(*cmd)/4 - 1 ; i++)
//        sum += ((int *)cmd)[i];
//
//    return sum;
//}


//  Build an original game map name from episode and map number,
//  based on the game mode (doom1, doom2...)
//
char* G_BuildMapName (int episode, int map)
{
    static char  mapname[9];    // internal map name (wad resource name)

    if (gamemode==commercial)
        strcpy (mapname, va("MAP%#02d",map));
    else
    {
        mapname[0] = 'E';
        mapname[1] = '0' + episode;
        mapname[2] = 'M';
        mapname[3] = '0' + map;
        mapname[4] = 0;
    }
    return mapname;
}


//
//  Clip the console player mouse aiming to the current view,
//  also returns a signed char for the player ticcmd if needed.
//  Used whenever the player view pitch is changed manually
//
//added:22-02-98:
//changed:3-3-98: do a angle limitation now
short G_ClipAimingPitch (int* aiming)
{
    int limitangle;

    //note: the current software mode implementation doesn't have true perspective
    if ( rendermode == render_soft )
        limitangle = ANG90 - 10; // Tails 10-31-99 Some viewing fun, but not too far down...
    //    limitangle = 732<<ANGLETOFINESHIFT;
    else
        limitangle = ANG90 - 1;

    if (*aiming > limitangle )
        *aiming = limitangle;
    else
    if (*aiming < -limitangle)
        *aiming = -limitangle;

    return (*aiming)>>16;
}


//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
// set secondaryplayer true to build player 2's ticcmd in splitscreen mode
//
int     localaiming,localaiming2;
angle_t localangle,localangle2;

//added:06-02-98: mouseaiming (looking up/down with the mouse or keyboard)
#define KB_LOOKSPEED    (1<<25)
#define MAXPLMOVE       (forwardmove[1])
#define TURBOTHRESHOLD  0x32
#define SLOWTURNTICS    6

fixed_t forwardmove[2] = {25, 50};
fixed_t sidemove[2]    = {24, 40};
fixed_t angleturn[3]   = {640, 1280, 320};        // + slow turn


// for change this table change also nextweapon func and P_PlayerThink
char extraweapons[8]={wp_chainsaw,-1,wp_supershotgun,-1,-1,-1,-1,-1};
byte nextweaponorder[NUMWEAPONS]={wp_fist,wp_chainsaw,wp_pistol,
     wp_shotgun,wp_supershotgun,wp_chaingun,wp_missile,wp_plasma,wp_bfg};

byte NextWeapon(player_t *player,int step)
{
    byte   w;
    int    i;
    for (i=0;i<NUMWEAPONS;i++)
        if( player->readyweapon == nextweaponorder[i] )
        {
            i = (i+NUMWEAPONS+step)%NUMWEAPONS;
            break;
        }
    for (;nextweaponorder[i]!=player->readyweapon; i=(i+NUMWEAPONS+step)%NUMWEAPONS)
    {
        w = nextweaponorder[i];
        // skip super shotgun for non-Doom2
        if (gamemode!=commercial && w==wp_supershotgun)
            continue;

        if ( player->weaponowned[w] &&
             player->ammo[weaponinfo[w].ammo] >= ammopershoot[w] )
        {
            if(w==wp_chainsaw)
                return (BT_CHANGE | BT_EXTRAWEAPON | (wp_fist<<BT_WEAPONSHIFT));
            if(w==wp_supershotgun)
                return (BT_CHANGE | BT_EXTRAWEAPON | (wp_shotgun<<BT_WEAPONSHIFT));
            return (BT_CHANGE | (w<<BT_WEAPONSHIFT));
        }
    }
    return 0;
}


void G_BuildTiccmd (ticcmd_t* cmd, int realtics)
{
//    int         i;
    boolean     strafe;
    int         speed;
    int         tspeed;
    int         forward;
    int         side;
    ticcmd_t*   base;
    //added:14-02-98: these ones used for multiple conditions
    boolean     turnleft,turnright,mouseaiming;
    static int  turnheld;               // for accelerative turning
    static boolean  keyboard_look;      // true if lookup/down using keyboard

    base = I_BaseTiccmd ();             // empty, or external driver
    memcpy (cmd,base,sizeof(*cmd));

    // a little clumsy, but then the g_input.c became a lot simpler!
    strafe = gamekeydown[gamecontrol[gc_strafe][0]] ||
             gamekeydown[gamecontrol[gc_strafe][1]];
    speed  = gamekeydown[gamecontrol[gc_speed][0]] ||
             gamekeydown[gamecontrol[gc_speed][1]] ||
             cv_autorun.value;

    turnright = gamekeydown[gamecontrol[gc_turnright][0]]
              ||gamekeydown[gamecontrol[gc_turnright][1]];
    turnleft  = gamekeydown[gamecontrol[gc_turnleft][0]]
              ||gamekeydown[gamecontrol[gc_turnleft][1]];
    mouseaiming = gamekeydown[gamecontrol[gc_mouseaiming][0]]
                ||gamekeydown[gamecontrol[gc_mouseaiming][1]]
                || cv_alwaysfreelook.value;

    if( !cv_splitscreen.value && Joystick.bGamepadStyle)
    {
        turnright = turnright || joyxmove > 0;
        turnleft  = turnleft  || joyxmove < 0;
    }
    forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
    if (turnleft || turnright)
        turnheld += realtics;
    else
        turnheld = 0;

    if (turnheld < SLOWTURNTICS)
        tspeed = 2;             // slow turn
    else
        tspeed = speed;

    // let movement keys cancel each other out
    if (strafe)
    {
        if (turnright)
            side += sidemove[speed];
        if (turnleft)
            side -= sidemove[speed];
        
        if (!Joystick.bGamepadStyle && !cv_splitscreen.value) 
        {
            //faB: JOYAXISRANGE is supposed to be 1023 ( divide by 1024 )
            side += ( (joyxmove * sidemove[1]) >> 10 );
        }
    }
    else
    {
        if ( turnright )
            cmd->angleturn -= angleturn[tspeed];
        else
        if ( turnleft  )
            cmd->angleturn += angleturn[tspeed];
        if ( joyxmove && !Joystick.bGamepadStyle && !cv_splitscreen.value) {
            //faB: JOYAXISRANGE should be 1023 ( divide by 1024 )
            cmd->angleturn -= ( (joyxmove * angleturn[1]) >> 10 );        // ANALOG!
            //CONS_Printf ("joyxmove %d  angleturn %d\n", joyxmove, cmd->angleturn);
        }

    }

    //added:07-02-98: forward with key or button
    if (gamekeydown[gamecontrol[gc_forward][0]] ||
        gamekeydown[gamecontrol[gc_forward][1]] ||
        ( joyymove < 0 && Joystick.bGamepadStyle && !cv_splitscreen.value) )
    {
        forward += forwardmove[speed];
    }
    if (gamekeydown[gamecontrol[gc_backward][0]] ||
        gamekeydown[gamecontrol[gc_backward][1]] ||
        ( joyymove > 0 && Joystick.bGamepadStyle && !cv_splitscreen.value) )
    {
        forward -= forwardmove[speed];
    }

    if ( joyymove && !Joystick.bGamepadStyle && !cv_splitscreen.value) {
        forward -= ( (joyymove * forwardmove[1]) >> 10 );               // ANALOG!
    }


    //added:07-02-98: some people strafe left & right with mouse buttons
    if (gamekeydown[gamecontrol[gc_straferight][0]] ||
        gamekeydown[gamecontrol[gc_straferight][1]])
        cmd->buttons |= BT_CAMRIGHT; // Tails 03-04-2000
    if (gamekeydown[gamecontrol[gc_strafeleft][0]] ||
        gamekeydown[gamecontrol[gc_strafeleft][1]])
        cmd->buttons |= BT_CAMLEFT; // Tails 03-04-2000

    //added:07-02-98: fire with any button/key
    if (gamekeydown[gamecontrol[gc_fire][0]] ||
        gamekeydown[gamecontrol[gc_fire][1]])
        cmd->buttons |= BT_ATTACK;

    //added:07-02-98: use with any button/key
    if (gamekeydown[gamecontrol[gc_use][0]] ||
        gamekeydown[gamecontrol[gc_use][1]])
        cmd->buttons |= BT_USE;

    //added:22-02-98: jump button
    if (cv_allowjump.value && (gamekeydown[gamecontrol[gc_jump][0]] ||
                               gamekeydown[gamecontrol[gc_jump][1]]))
        cmd->buttons |= BT_JUMP;

    //added:07-02-98: any key / button can trigger a weapon
    // chainsaw overrides
    if (gamekeydown[gamecontrol[gc_nextweapon][0]] ||
        gamekeydown[gamecontrol[gc_nextweapon][1]])
        cmd->buttons |= NextWeapon(&players[consoleplayer],1);
    else
    if (gamekeydown[gamecontrol[gc_prevweapon][0]] ||
        gamekeydown[gamecontrol[gc_prevweapon][1]])
        cmd->buttons |= NextWeapon(&players[consoleplayer],-1);

    // mouse look stuff (mouse look is not the same as mouse aim)
    if (mouseaiming)
    {
        keyboard_look = false;

        // looking up/down
        if (cv_invertmouse.value)
            localaiming -= mlooky<<19;
        else
            localaiming += mlooky<<19;
    }
    else
      // spring back if not using keyboard neither mouselookin'
        if (!keyboard_look )
             localaiming = 0;

    if (gamekeydown[gamecontrol[gc_lookup][0]] ||
        gamekeydown[gamecontrol[gc_lookup][1]])
    {
        localaiming += KB_LOOKSPEED;
        keyboard_look = true;
    }
    else
    if (gamekeydown[gamecontrol[gc_lookdown][0]] ||
        gamekeydown[gamecontrol[gc_lookdown][1]])
    {
        localaiming -= KB_LOOKSPEED;
        keyboard_look = true;
    }
    else
    if (gamekeydown[gamecontrol[gc_centerview][0]] ||
        gamekeydown[gamecontrol[gc_centerview][1]])
        localaiming = 0;

    cmd->aiming = G_ClipAimingPitch (&localaiming);

    if (!mouseaiming)
        forward += mousey;

    if (strafe)
        side += mousex*2;
    else
        cmd->angleturn -= mousex*0x8;

    mousex = mousey = mlooky = 0;

    if (forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if (forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if (side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if (side < -MAXPLMOVE)
        side = -MAXPLMOVE;

    cmd->forwardmove += forward;
    cmd->sidemove += side;

#ifdef ABSOLUTEANGLE
    localangle += (cmd->angleturn<<16);
    cmd->angleturn = localangle >> 16;
//    localaiming = cmd->aiming;
#endif
}
// like the g_buildticcmd 1 but lite version witout mouse...
void G_BuildTiccmd2 (ticcmd_t* cmd, int realtics)
{
//    int         i;
    boolean     strafe;
    int         speed;
    int         tspeed;
    int         forward;
    int         side;
    ticcmd_t*   base;
    //added:14-02-98: these ones used for multiple conditions
    boolean     turnleft,turnright;
    static int  turnheld;                    // for accelerative turning

    base = I_BaseTiccmd ();             // empty, or external driver
    memcpy (cmd,base,sizeof(*cmd));

    // a little clumsy, but then the g_input.c became a lot simpler!
    strafe = gamekeydown[gamecontrolbis[gc_strafe][0]] ||
             gamekeydown[gamecontrolbis[gc_strafe][1]];
    speed  = gamekeydown[gamecontrolbis[gc_speed][0]] ||
             gamekeydown[gamecontrolbis[gc_speed][1]] ||
             cv_autorun.value;

    turnright = gamekeydown[gamecontrolbis[gc_turnright][0]]
              ||gamekeydown[gamecontrolbis[gc_turnright][1]];
    turnleft  = gamekeydown[gamecontrolbis[gc_turnleft][0]]
              ||gamekeydown[gamecontrolbis[gc_turnleft][1]];

    if(Joystick.bGamepadStyle)
    {
        turnright = turnright || joyxmove > 0;
        turnleft  = turnleft  || joyxmove < 0;
    }

    forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
    if (turnleft || turnright)
        turnheld += realtics;
    else
        turnheld = 0;

    if (turnheld < SLOWTURNTICS)
        tspeed = 2;             // slow turn
    else
        tspeed = speed;

    // let movement keys cancel each other out
    if (strafe)
    {
        if (turnright)
            side += sidemove[speed];
        if (turnleft)
            side -= sidemove[speed];

        if (!Joystick.bGamepadStyle) 
        {
            //faB: JOYAXISRANGE is supposed to be 1023 ( divide by 1024 )
            side += ( (joyxmove * sidemove[1]) >> 10 );
        }
    }
    else
    {
        if (turnright )
            cmd->angleturn -= angleturn[tspeed];
        if (turnleft  )
            cmd->angleturn += angleturn[tspeed];
        
        if ( joyxmove && !Joystick.bGamepadStyle ) 
        {
            //faB: JOYAXISRANGE should be 1023 ( divide by 1024 )
            cmd->angleturn -= ( (joyxmove * angleturn[1]) >> 10 );        // ANALOG!
            //CONS_Printf ("joyxmove %d  angleturn %d\n", joyxmove, cmd->angleturn);
        }
    }

    //added:07-02-98: forward with key or button
    if (gamekeydown[gamecontrolbis[gc_forward][0]] ||
        gamekeydown[gamecontrolbis[gc_forward][1]] ||
        (joyymove < 0 && Joystick.bGamepadStyle))
    {
        forward += forwardmove[speed];
    }

    if (gamekeydown[gamecontrolbis[gc_backward][0]] ||
        gamekeydown[gamecontrolbis[gc_backward][1]] ||
        (joyymove > 0 && Joystick.bGamepadStyle))
    {
        forward -= forwardmove[speed];
    }

    if ( joyymove && !Joystick.bGamepadStyle ) {
        forward -= ( (joyymove * forwardmove[1]) >> 10 );               // ANALOG!
    }

    //added:07-02-98: some people strafe left & right with mouse buttons
    if (gamekeydown[gamecontrolbis[gc_straferight][0]] ||
        gamekeydown[gamecontrolbis[gc_straferight][1]])
        side += sidemove[speed];
    if (gamekeydown[gamecontrolbis[gc_strafeleft][0]] ||
        gamekeydown[gamecontrolbis[gc_strafeleft][1]])
        side -= sidemove[speed];

    // buttons
//    cmd->chatchar = HU_dequeueChatChar();

    //added:07-02-98: fire with any button/key
    if (gamekeydown[gamecontrolbis[gc_fire][0]] ||
        gamekeydown[gamecontrolbis[gc_fire][1]])
        cmd->buttons |= BT_ATTACK;

    //added:07-02-98: use with any button/key
    if (gamekeydown[gamecontrolbis[gc_use][0]] ||
        gamekeydown[gamecontrolbis[gc_use][1]])
        cmd->buttons |= BT_USE;

    //added:22-02-98: jump button
    if (cv_allowjump.value && (gamekeydown[gamecontrolbis[gc_jump][0]] ||
                               gamekeydown[gamecontrolbis[gc_jump][1]]))
        cmd->buttons |= BT_JUMP;

    //added:07-02-98: any key / button can trigger a weapon
    // chainsaw overrides
    if (gamekeydown[gamecontrolbis[gc_nextweapon][0]] ||
        gamekeydown[gamecontrolbis[gc_nextweapon][1]])
        cmd->buttons |= NextWeapon(&players[secondarydisplayplayer],1);
    else
    if (gamekeydown[gamecontrolbis[gc_prevweapon][0]] ||
        gamekeydown[gamecontrolbis[gc_prevweapon][1]])
        cmd->buttons |= NextWeapon(&players[secondarydisplayplayer],-1);

    if (gamekeydown[gamecontrolbis[gc_lookup][0]] ||
        gamekeydown[gamecontrolbis[gc_lookup][1]])
    {
        localaiming2 += KB_LOOKSPEED;
    }
    else
    if (gamekeydown[gamecontrolbis[gc_lookdown][0]] ||
        gamekeydown[gamecontrolbis[gc_lookdown][1]])
    {
        localaiming2 -= KB_LOOKSPEED;
    }
    else
    if (gamekeydown[gamecontrolbis[gc_centerview][0]] ||
        gamekeydown[gamecontrolbis[gc_centerview][1]])
        localaiming2 = 0;

    // look up max (viewheight/2) look down min -(viewheight/2)
    cmd->aiming = G_ClipAimingPitch (&localaiming2);;

    if (strafe)
        side += mousex*2;
    else
        cmd->angleturn -= mousex*0x8;

    if (forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if (forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if (side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if (side < -MAXPLMOVE)
        side = -MAXPLMOVE;

    cmd->forwardmove += forward;
    cmd->sidemove += side;

#ifdef ABSOLUTEANGLE
    localangle2 += (cmd->angleturn<<16);
    cmd->angleturn = localangle2 >> 16;
//    localaiming2 = cmd->aiming;
#endif
}


//
// G_DoLoadLevel
//
extern  gamestate_t     wipegamestate;

void G_DoLoadLevel (void)
{
    int             i;

    levelstarttic = gametic;        // for time calculation

    if (wipegamestate == GS_LEVEL)
        wipegamestate = -1;             // force a wipe

    gamestate = GS_LEVEL;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if (playeringame[i] && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
        memset (players[i].frags,0,sizeof(players[i].frags));
    }

    if (!P_SetupLevel (gameepisode, gamemap, 0, gameskill, gamemapname[0] ? gamemapname:NULL) )
        return;

    //BOT_InitLevelBots ();

    displayplayer = consoleplayer;          // view the guy you are playing
    if(!cv_splitscreen.value)
        secondarydisplayplayer = consoleplayer;
    starttime = I_GetTime ();
    gameaction = ga_nothing;
#ifdef PARANOIA
    Z_CheckHeap (-2);
#endif

    if (camera.chase)
        P_ResetCamera (&players[displayplayer]);

    // clear cmd building stuff
    memset (gamekeydown, 0, sizeof(gamekeydown));
    joyxmove = joyymove = 0;
    mousex = mousey = 0;

    // clear hud messages remains (usually from game startup)
    CON_ClearHUD ();

}

//
// G_Responder
//  Get info needed to make ticcmd_ts for the players.
//
boolean G_Responder (event_t* ev)
{
    // allow spy mode changes even during the demo
    if (gamestate == GS_LEVEL && ev->type == ev_keydown
        && ev->data1 == KEY_F12 && (singledemo || !cv_deathmatch.value) )
    {
        // spy mode
        do
        {
            displayplayer++;
            if (displayplayer == MAXPLAYERS)
                displayplayer = 0;
        } while (!playeringame[displayplayer] && displayplayer != consoleplayer);

        //added:16-01-98:change statusbar also if playingback demo
        if( singledemo )
            ST_changeDemoView ();

        //added:11-04-98: tell who's the view
        CONS_Printf("Viewpoint : %s\n", player_names[displayplayer]);

        return true;
    }

#ifdef __WIN32_OLDSTUFF__
    // console of 3dfx version on/off
    if (rendermode==render_glide &&
        ev->type==ev_keydown &&
        ev->data1=='g')
        glide_console ^= 1;
#endif

    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo &&
        (demoplayback || gamestate == GS_DEMOSCREEN) )
    {
        if (ev->type == ev_keydown)
        {
            M_StartControlPanel ();
            return true;
        }
        return false;
    }

    if (gamestate == GS_LEVEL)
    {
#if 0
        if (devparm && ev->type == ev_keydown && ev->data1 == ';')
        {
            // added Boris : test different player colors
            players[consoleplayer].skincolor = (players[consoleplayer].skincolor+1) %MAXSKINCOLORS;
            players[consoleplayer].mo->flags |= (players[consoleplayer].skincolor)<<MF_TRANSSHIFT;
            G_DeathMatchSpawnPlayer (0);
            return true;
        }
#endif
        if(!multiplayer)
           cht_Responder (ev); // never eat
        if (HU_Responder (ev))
            return true;        // chat ate the event
        if (ST_Responder (ev))
            return true;        // status window ate it
        if (AM_Responder (ev))
            return true;        // automap ate it
        //added:07-02-98: map the event (key/mouse/joy) to a gamecontrol
    }

    if (gamestate == GS_FINALE)
    {
        if (F_Responder (ev))
            return true;        // finale ate the event
    }

    // update keys current state
    G_MapEventsToControls (ev);

    switch (ev->type)
    {
      case ev_keydown:
        if (ev->data1 == KEY_PAUSE)
        {
            COM_BufAddText("pause\n");
            return true;
        }
        return true;

      case ev_keyup:
        return false;   // always let key up events filter down

      case ev_mouse:
        return true;    // eat events

      case ev_joystick:
        return true;    // eat events

      default:
        break;
    }

    return false;
}


//
//  Activates the new features of Doom LEGACY, by default
//  This is done at the start of D_DoomLoop, before a demo
//  starts playing, because for older demos, some features
//  are disabled.
//

void G_InitNewFeatures (void)
{
    //activate rocket trails by default
    states[S_ROCKET].action.acv = A_SmokeTrailer;

    // smoke trails behind the skull heads
    states[S_SKULL_ATK3].action.acv = A_SmokeTrailer;
    states[S_SKULL_ATK4].action.acv = A_SmokeTrailer;

     //added:02-01-98: enable transparency effect to selected sprites.
    //if (transparency_enable)
    //{
    //added:03-02-98: activates all new features by default.
    if (!fuzzymode)  //only for translucency
    {
        //hack the states and set translucency for standard doom sprites
        P_SetTranslucencies ();
    }
}


//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker (void)
{
    ULONG       i;
    int         buf;
    ticcmd_t*   cmd;

    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++)
        if (playeringame[i] && players[i].playerstate == PST_REBORN)
            G_DoReborn (i);

    // do things to change the game state
    while (gameaction != ga_nothing)
    {
        switch (gameaction)
        {
          case ga_loadlevel :  G_DoLoadLevel ();        break;
          case ga_playdemo  :  G_DoPlayDemo ();         break;
          case ga_completed :  G_DoCompleted ();        break;
          case ga_victory   :  F_StartFinale ();        break;
          case ga_worlddone :  G_DoWorldDone ();        break;
          case ga_screenshot:  M_ScreenShot ();
                               gameaction = ga_nothing;
                               break;
          case ga_nothing:
            break;
        }
    }

    // get commands, check consistancy,
    // and build new consistancy check
    buf = (gametic/ticdup)%BACKUPTICS;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if (playeringame[i])
        {
            cmd = &players[i].cmd;

            if (demoplayback)
                G_ReadDemoTiccmd (cmd,i);
            else
                memcpy (cmd, &netcmds[buf][i], sizeof(ticcmd_t));

            if (demorecording)
                G_WriteDemoTiccmd (cmd,i);

            // check for turbo cheats
            if (cmd->forwardmove > TURBOTHRESHOLD
                && !(gametic&31) && ((gametic>>5)&3) == i )
            {
                static char turbomessage[80];
                sprintf (turbomessage, "%s is turbo!",player_names[i]);
                players[consoleplayer].message = turbomessage;
            }
        }
    }

    // do main actions
    switch (gamestate)
    {
      case GS_LEVEL:
        //IO_Color(0,255,0,0);
        P_Ticker ();             // tic the game
        //IO_Color(0,0,255,0);
        ST_Ticker ();
        AM_Ticker ();
        HU_Ticker ();
        break;

      case GS_INTERMISSION:
        WI_Ticker ();
        break;

      case GS_FINALE:
        F_Ticker ();
        break;

      case GS_DEMOSCREEN:
        D_PageTicker ();
        break;
      case GS_WAITINGPLAYERS:
      case GS_DEDICATEDSERVER:
      // do nothing
        break;
    }
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player)
{
    player_t*   p;

    // set up the saved info
    p = &players[player];

    // clear everything else to defaults
    G_PlayerReborn (player);

}



//
// G_PlayerFinishLevel
//  Can when a player completes a level.
//
void G_PlayerFinishLevel (int player)
{
    player_t*   p;

    p = &players[player];

    memset (p->powers, 0, sizeof (p->powers));
    memset (p->cards, 0, sizeof (p->cards));
    p->mo->flags &= ~MF_SHADOW;         // cancel invisibility
    p->extralight = 0;                  // cancel gun flashes
    p->fixedcolormap = 0;               // cancel ir gogles
    p->damagecount = 0;                 // no palette changes
    p->bonuscount = 0;

    //added:16-02-98: hmm.. I should reset the aiming stuff between each
    //                level and each life... maybe that sould be done
    //                somewhere else.
    //p->aiming = 0;

}


// added 2-2-98 for hacking with dehacked patch
//SOM: Sonic starts with one health
int initial_health=1; //MAXHEALTH;
int initial_bullets=0; // Tails 10-24-99

void VerifFavoritWeapon (player_t *player);

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
void G_PlayerReborn (int player)
{
    player_t*   p;
    int         i;
    int         frags[MAXPLAYERS];
    int         killcount;
    int         itemcount;
    int         secretcount;

    //from Boris
    int         skincolor;
    char        favoritweapon[NUMWEAPONS];
    boolean     originalweaponswitch;
    boolean     autoaim;
    int         skin;                           //Fab: keep same skin

    memcpy (frags,players[player].frags,sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;

    //from Boris
    skincolor = players[player].skincolor;
    originalweaponswitch = players[player].originalweaponswitch;
    memcpy (favoritweapon,players[player].favoritweapon,NUMWEAPONS);
    autoaim   = players[player].autoaim_toggle;
    skin = players[player].skin;

    p = &players[player];
    memset (p, 0, sizeof(*p));

    memcpy (players[player].frags, frags, sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;

    // save player config truth reborn
    players[player].skincolor = skincolor;
    players[player].originalweaponswitch = originalweaponswitch;
    memcpy (players[player].favoritweapon,favoritweapon,NUMWEAPONS);
    players[player].autoaim_toggle = autoaim;
    players[player].skin = skin;

    p->usedown = p->attackdown = true;  // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = initial_health;
    p->readyweapon = p->pendingweapon = wp_fist;
    p->weaponowned[wp_fist] = true; // Tails 11-06-99
    p->weaponowned[wp_pistol] = false; // Tails 11-06-99
    p->ammo[am_clip] = initial_bullets;

    // Boris stuff
    if(!p->originalweaponswitch)
        VerifFavoritWeapon(p);
    //eof Boris

    for (i=0 ; i<NUMAMMO ; i++)
        p->maxammo[i] = maxammo[i];

}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
void P_SpawnPlayer (mapthing_t* mthing);

boolean G_CheckSpot ( int           playernum,
                      mapthing_t*   mthing )
{
    fixed_t             x;
    fixed_t             y;
    subsector_t*        ss;
    unsigned            an;
    mobj_t*             mo;
    int                 i;

    // added 25-4-98 : maybe there is no player start
    if(!mthing || mthing->type<0)
        return false;

    if (!players[playernum].mo)
    {
        // first spawn of level, before corpses
        for (i=0 ; i<playernum ; i++)
            // added 15-1-98 check if player is in game (mistake from id)
            if (playeringame[i]
                && players[i].mo->x == mthing->x << FRACBITS
                && players[i].mo->y == mthing->y << FRACBITS)
                return false;
        return true;
    }

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (!P_CheckPosition (players[playernum].mo, x, y) )
        return false;

    // flush an old corpse if needed
    if (bodyqueslot >= BODYQUESIZE)
        P_RemoveMobj (bodyque[bodyqueslot%BODYQUESIZE]);
    bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo;
    bodyqueslot++;

    // spawn a teleport fog
    ss = R_PointInSubsector (x,y);
    an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT;

    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
                      , ss->sector->floorheight
                      , MT_TFOG);

    //added:16-01-98:consoleplayer -> displayplayer (hear snds from viewpt)
    // removed 9-12-98: why not ????
    if (players[displayplayer].viewz != 1)
        S_StartSound (mo, sfx_telept);  // don't start sound on first frame

    return true;
}


//
// G_DeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//
boolean G_DeathMatchSpawnPlayer (int playernum)
{
    int             i,j,n;
    int             selections;

    selections = deathmatch_p - deathmatchstarts;
    if( !selections)
        I_Error("No deathmatch start in this map !");

    if(demoversion<123)
        n=20;
    else
        n=64;

    for (j=0 ; j<n ; j++)
    {
        i = P_Random() % selections;
        if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
        {
            deathmatchstarts[i].type = playernum+1;
            P_SpawnPlayer (&deathmatchstarts[i]);
            return true;
        }
    }

    if(demoversion<113)
    {

    // no good spot, so the player will probably get stuck
        P_SpawnPlayer (&playerstarts[playernum]);
        return true;
    }
    return false;
}

void G_CoopSpawnPlayer (int playernum)
{
    int i;

    // no deathmatch use the spot
    if (G_CheckSpot (playernum, &playerstarts[playernum]) )
    {
        P_SpawnPlayer (&playerstarts[playernum]);
        return;
    }

    // try to spawn at one of the other players spots
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if (G_CheckSpot (playernum, &playerstarts[i]) )
        {
            playerstarts[i].type = playernum+1;     // fake as other player
            P_SpawnPlayer (&playerstarts[i]);
            playerstarts[i].type = i+1;               // restore
            return;
        }
        // he's going to be inside something.  Too bad.
    }

    if(demoversion<113)
        P_SpawnPlayer (&playerstarts[playernum]);
    else
    {
        int  selections;

        selections = deathmatch_p - deathmatchstarts;
        if( !selections)
            I_Error("No deathmatch start in this map !");
        selections = P_Random() % selections;
        deathmatchstarts[selections].type = playernum+1;
        P_SpawnPlayer (&deathmatchstarts[selections]);
    }
}

//
// G_DoReborn
//
void G_DoReborn (int playernum)
{
    player_t*  player = &players[playernum];

    // boris comment : this test is like 'single player game'
    //                 all this kind of hiden variable must be removed
    if (!multiplayer && !cv_deathmatch.value)
    {
        // reload the level from scratch
        gameaction = ga_loadlevel;
    }
    else
    {
        // respawn at the start

        // first dissasociate the corpse
        if(player->mo)
            player->mo->player = NULL;

        // spawn at random spot if in death match
        if (cv_deathmatch.value)
        {
            if(G_DeathMatchSpawnPlayer (playernum))
               return;
        }

        G_CoopSpawnPlayer (playernum);
    }
}


void G_ScreenShot (void)
{
    gameaction = ga_screenshot;
}



// DOOM Par Times
int pars[4][10] =
{
    {0},
    {0,30,75,120,90,165,180,180,30,165},
    {0,90,90,90,120,90,360,240,30,170},
    {0,90,45,90,150,90,90,165,30,135}
};

// DOOM II Par Times
int cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,        //  1-10
    210,150,150,150,210,150,420,150,210,150,    // 11-20
    240,150,180,150,150,300,330,420,300,180,    // 21-30
    120,30                                      // 31-32
};


//
// G_DoCompleted
//
boolean         secretexit;
extern char*    pagename;

void G_ExitLevel (void)
{
    secretexit = false;
    gameaction = ga_completed;
}

// Here's for the german edition.
void G_SecretExitLevel (void)
{
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ( (gamemode == commercial)
      && (W_CheckNumForName("map31")<0))
        secretexit = false;
    else
        secretexit = true;
    gameaction = ga_completed;
}

void G_DoCompleted (void)
{
    int             i;

    gameaction = ga_nothing;

    for (i=0 ; i<MAXPLAYERS ; i++)
        if (playeringame[i])
            G_PlayerFinishLevel (i);        // take away cards and stuff

    if (automapactive)
        AM_Stop ();

    if ( gamemode != commercial)
        switch(gamemap)
        {
          case 8:
            gameaction = ga_victory;
            return;
          case 9:
            for (i=0 ; i<MAXPLAYERS ; i++)
                players[i].didsecret = true;
            break;
        }

    wminfo.didsecret = players[consoleplayer].didsecret;
    wminfo.epsd = gameepisode -1;
    wminfo.last = gamemap -1;

    // wminfo.next is 0 biased, unlike gamemap
    if ( gamemode == commercial)
    {
        if (secretexit)
            switch(gamemap)
            {
              case 15: wminfo.next = 30; break;
              case 31: wminfo.next = 31; break;
            }
        else
            switch(gamemap)
            {
              case 31:
              case 32: wminfo.next = 15; break;
              default: wminfo.next = gamemap;
            }
    }
    else
    {
        if (secretexit)
            wminfo.next = 8;    // go to secret level
        else if (gamemap == 9)
        {
            // returning from secret level
            switch (gameepisode)
            {
              case 1:
                wminfo.next = 3;
                break;
              case 2:
                wminfo.next = 5;
                break;
              case 3:
                wminfo.next = 6;
                break;
              case 4:
                wminfo.next = 2;
                break;
            }
        }
        else
            wminfo.next = gamemap;          // go to next level
    }

    wminfo.maxkills = totalkills;
    wminfo.maxitems = totalitems;
    wminfo.maxsecret = totalsecret;
    wminfo.maxfrags = 0;
    if ( gamemode == commercial )
        wminfo.partime = TICRATE*cpars[gamemap-1];
    else
        wminfo.partime = TICRATE*pars[gameepisode][gamemap];
    wminfo.pnum = consoleplayer;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        wminfo.plyr[i].in = playeringame[i];
        wminfo.plyr[i].skills = players[i].killcount;
        wminfo.plyr[i].sitems = players[i].itemcount;
        wminfo.plyr[i].ssecret = players[i].secretcount;
        wminfo.plyr[i].stime = leveltime;
        memcpy (wminfo.plyr[i].frags, players[i].frags
                , sizeof(wminfo.plyr[i].frags));
    }

    gamestate = GS_INTERMISSION;
    viewactive = false;
    automapactive = false;

    if (statcopy)
        memcpy (statcopy, &wminfo, sizeof(wminfo));

    WI_Start (&wminfo);
}


//
// G_WorldDone
//
// init next level or go to the final scene
void G_WorldDone (void)
{
    gameaction = ga_worlddone;

    if (secretexit)
        players[consoleplayer].didsecret = true;

    if ( gamemode == commercial )
    {
        switch (gamemap)
        {
          case 15:
          case 31:
            if (!secretexit)
                break;
          case 6:
          case 11:
          case 20:
          case 30:
            F_StartFinale ();
            break;
        }
    }
}

void G_DoWorldDone (void)
{
    gamestate = GS_LEVEL;
    gamemap = wminfo.next+1;
    G_DoLoadLevel ();
    gameaction = ga_nothing;
    viewactive = true;
}


//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame (int slot)
{
    if (demoplayback)
        G_DoLoadGame (slot);
    else
    if (server)
        COM_BufAddText(va("load %d\n",slot));
}

#define VERSIONSIZE             16

void G_DoLoadGame (int slot)
{
    int         length;
    int         i;
    int         a,b,c;
    char        vcheck[VERSIONSIZE];
    char        savename[255];

    if (M_CheckParm("-cdrom"))
        sprintf(savename,text[CDROM_SAVEI_NUM],slot);
    else
        sprintf(savename,text[NORM_SAVEI_NUM],slot);

    length = FIL_ReadFile (savename, &savebuffer);
    if (!length)
    {
        CONS_Printf ("Couldn't read file %s", savename);
        return;
    }

    save_p = savebuffer + SAVESTRINGSIZE;

    // skip the description field
    memset (vcheck,0,sizeof(vcheck));
    sprintf (vcheck,"version %i",VERSION);
    if (strcmp (save_p, vcheck))
    {
        CONS_Printf ("Save game from different version\n");
        return;                         // bad version
    }
    save_p += VERSIONSIZE;

    if(demoplayback)  // reset game engine
        G_StopDemo();

    gameskill = *save_p++;
    gameepisode = *save_p++;
    gamemap = *save_p++;
    for (i=0 ; i<MAXPLAYERS ; i++)
        playeringame[i] = *save_p++;

    //added:27-02-98: reset the game version
    demoversion = VERSION;

    usergame      = true;      // will be set false if a demo
    paused        = false;
    demoplayback  = false;
    automapactive = false;
    viewactive    = true;

    // load a base level
    G_InitNew (gameskill, G_BuildMapName(gameepisode, gamemap));

    // get the times
    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    leveltime = (a<<16) + (b<<8) + c;

    // dearchive all the modifications
    P_UnArchivePlayers ();
    P_UnArchiveWorld ();
    P_UnArchiveThinkers ();
    P_UnArchiveSpecials ();

    if(*save_p != 0x1d)
    {
        CONS_Printf ("Bad savegame\n");
        gameaction=GS_DEMOSCREEN;
    }

    // done
    Z_Free (savebuffer);

    multiplayer = playeringame[1];
    if(playeringame[1] && !netgame)
        CV_SetValue(&cv_splitscreen,1);

    if (setsizeneeded)
        R_ExecuteSetViewSize ();

    // draw the pattern into the back screen
    R_FillBackScreen ();
}
/*
int             savegameslot;
char            savedescription[32];
*/
//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame ( int   slot, char* description )
{
    if (server)
        COM_BufAddText(va("save %d \"%s\"\n",slot,description));
}

void G_DoSaveGame (int   savegameslot, char* savedescription)
{
    char        name[100];
    char        name2[VERSIONSIZE];
    char        description[SAVESTRINGSIZE];
    int         length;
    int         i;

    gameaction = ga_nothing;
    if (M_CheckParm("-cdrom"))
        sprintf(name,text[CDROM_SAVEI_NUM],savegameslot);
    else
        sprintf(name,text[NORM_SAVEI_NUM],savegameslot);

    save_p = savebuffer = (byte *)malloc(SAVEGAMESIZE);
    if(!save_p)
    {
        CONS_Printf ("No More free memory for savegame\n");
        return;
    }

    strcpy(description,savedescription);
    description[SAVESTRINGSIZE]=0;
    memcpy (save_p, description, SAVESTRINGSIZE);
    save_p += SAVESTRINGSIZE;
    memset (name2,0,sizeof(name2));
    sprintf (name2,"version %i",VERSION);
    memcpy (save_p, name2, VERSIONSIZE);
    save_p += VERSIONSIZE;

    *save_p++ = gameskill;
    *save_p++ = gameepisode;
    *save_p++ = gamemap;
    for (i=0 ; i<MAXPLAYERS ; i++)
        *save_p++ = playeringame[i];
    *save_p++ = leveltime>>16;
    *save_p++ = leveltime>>8;
    *save_p++ = leveltime;

    P_ArchivePlayers ();
    P_ArchiveWorld ();
    P_ArchiveThinkers ();
    P_ArchiveSpecials ();

    *save_p++ = 0x1d;           // consistancy marker

    length = save_p - savebuffer;
    if (length > SAVEGAMESIZE)
        I_Error ("Savegame buffer overrun");
    FIL_WriteFile (name, savebuffer, length);
    free(savebuffer);

    gameaction = ga_nothing;
//    savedescription[0] = 0;

    players[consoleplayer].message = GGSAVED;

    // draw the pattern into the back screen
    R_FillBackScreen ();
}


//
// G_InitNew
//  Can be called by the startup code or the menu task,
//  consoleplayer, displayplayer, playeringame[] should be set.
//
// Boris comment : something like single player start game
void G_DeferedInitNew (skill_t skill, char* mapname)
{
/*
    int i;

    if(demoplayback)  // reset game engine
        G_StopDemo();
*/
    // boris : i am not sure if all this sets of variable are usefull

    //added:27-02-98: reset version number to the current version
    demoversion   = VERSION;

    netgame       = false;
    multiplayer   = false;
/*
    consoleplayer = 0;
    playeringame[consoleplayer]=1;
    for(i=1;i<MAXPLAYERS;i++)
        playeringame[i] = 0;
*/
    usergame      = true;        // will be set false if a demo
    paused        = false;
    demoplayback  = false;
    automapactive = false;
    viewactive    = true;

    nomonsters    = false;
    CV_SetValue(&cv_deathmatch,0);
    CV_SetValue(&cv_fastmonsters,0);
    CV_SetValue(&cv_splitscreen,0);
    CV_SetValue(&cv_respawnmonsters,0);
    CV_SetValue(&cv_timelimit,0);
    CV_SetValue(&cv_fraglimit,0);
    if (server)
        COM_BufAddText (va("map \"%s\" -skill %d\n",mapname,skill+1));
}


extern boolean playerdeadview;
// BP: I hate global variable but how can i do better ???
boolean StartSplitScreenGame=false;

//
// This is the map command interpretation something like Command_Map_f
//
// called at : map cmd execution, doloadgame, doplaydemo
void G_InitNew (skill_t skill, char* mapname)
{
    int             i;

    //added:27-02-98: disable selected features for compatibility with
    //                older demos, plus reset new features as default
    if(!G_Downgrade (demoversion))
    {
        CONS_Printf("Cannot Downgrade engine\n");
        return;
    }

    if (paused)
    {
        paused = false;
        S_ResumeSound ();
    }

    if (skill > sk_nightmare)
        skill = sk_nightmare;

    M_ClearRandom ();

    if( StartSplitScreenGame ) // set true in the menu
    {
        // spawn player 1 too and show it in the splited screen
        playeringame[1] = 1;

        CV_SetValue (&cv_splitscreen,1);

        D_SendPlayerConfig();
        StartSplitScreenGame = false;
        multiplayer = true;
    }

    if(server)
    {
        if (skill == sk_nightmare )
            CV_SetValue(&cv_respawnmonsters,1);

        if( skill == sk_nightmare )
            CV_SetValue(&cv_fastmonsters,1);
    }

    // force players to be initialized upon first level load
    for (i=0 ; i<MAXPLAYERS ; i++)
        players[i].playerstate = PST_REBORN;

    // for internal maps only
    if (FIL_CheckExtension(mapname))
    {
        // external map file
        strncpy (gamemapname, mapname, MAX_WADPATH);
        gameepisode = 1;
        gamemap = 1;
    }
    else
    {
        // internal game map
        if (W_CheckNumForName(mapname)==-1)
        {
            CONS_Printf("\2Internal game map '%s' not found\n"
                        "(use .wad extension for external maps)\n",mapname);
            D_StartTitle ();
            return;
        }

        gamemapname[0] = 0;             // means not an external wad file
        if (gamemode==commercial)       //doom2
        {
            gamemap = atoi(mapname+3);  // get xx out of MAPxx
            gameepisode = 1;
        }
        else
        {
            gamemap = mapname[3]-'0';           // ExMy
            gameepisode = mapname[1]-'0';
        }
    }

    gameskill     = skill;
    playerdeadview = false;
    viewactive    = true;
    automapactive = false;

    G_DoLoadLevel ();
}


//added:03-02-98:
//
//  'Downgrade' the game engine so that it is compatible with older demo
//   versions. This will probably get harder and harder with each new
//   'feature' that we add to the game. This will stay until it cannot
//   be done a 'clean' way, then we'll have to forget about old demos..
//
boolean G_Downgrade(int version)
{
    int i;

    if (version<109)
    {
        //should return false...
        return false;
    }

    //added:27-02-98: reset new features as default
    G_InitNewFeatures ();

    // smoke trails for skull head attack since v1.25
    if (version<125)
    {
        states[S_SKULL_ATK3].action.acv = NULL;
        states[S_SKULL_ATK4].action.acv = NULL;
    }

    //hmmm.. first time I see an use to the switch without break...
    switch (version)
    {
      case 109:
        // disable rocket trails
        states[S_ROCKET].action.acv = NULL; //NULL like in Doom2 v1.9

        // Boris : for older demos, initalise the new skincolor value
        //         also disable the new preferred weapons order.
        for(i=0;i<4;i++)
        {
            players[i].skincolor = i % MAXSKINCOLORS;
            players[i].originalweaponswitch=true;
        }//eof Boris

      case 111:
        //added:16-02-98: make sure autoaim is used for older
        //                demos not using mouse aiming
        for(i=0;i<MAXPLAYERS;i++)
            players[i].autoaim_toggle = true;

      default:
        break;
    }


    // always true now, might be false in the future, if couldn't
    // go backward and disable all the features...
    return true;
}


//
// DEMO RECORDING
//

#define ZT_FWD          0x01
#define ZT_SIDE         0x02
#define ZT_ANGLE        0x04
#define ZT_BUTTONS      0x08
#define ZT_AIMING       0x10
#define ZT_CHAT         0x20    // no more used
#define ZT_EXTRADATA    0x40
#define DEMOMARKER      0x80    // demoend

ticcmd_t oldcmd[MAXPLAYERS];

void G_ReadDemoTiccmd (ticcmd_t* cmd,int playernum)
{
    if (*demo_p == DEMOMARKER)
    {
        // end of demo data stream
        G_CheckDemoStatus ();
        return;
    }
    if(demoversion<112)
    {
        cmd->forwardmove = READCHAR(demo_p);
        cmd->sidemove = READCHAR(demo_p);
        cmd->angleturn = READBYTE(demo_p)<<8;
        cmd->buttons = READBYTE(demo_p);
        cmd->aiming = 0;
    }
    else
    {
        char ziptic=*demo_p++;

        if(ziptic & ZT_FWD)
            oldcmd[playernum].forwardmove = READCHAR(demo_p);
        if(ziptic & ZT_SIDE)
            oldcmd[playernum].sidemove = READCHAR(demo_p);
        if(ziptic & ZT_ANGLE)
            if(demoversion<125)
                oldcmd[playernum].angleturn = READBYTE(demo_p)<<8;
            else
                oldcmd[playernum].angleturn = READSHORT(demo_p);
        if(ziptic & ZT_BUTTONS)
            oldcmd[playernum].buttons = READBYTE(demo_p);
        if(ziptic & ZT_AIMING)
            if(demoversion<128)
                oldcmd[playernum].aiming = READCHAR(demo_p);
            else
                oldcmd[playernum].aiming = READSHORT(demo_p);
        if(ziptic & ZT_CHAT)
            demo_p++;
        if(ziptic & ZT_EXTRADATA)
            ReadLmpExtraData(&demo_p,playernum);
        else
            ReadLmpExtraData(0,playernum);

        memcpy(cmd,&(oldcmd[playernum]),sizeof(ticcmd_t));
    }
}

void G_WriteDemoTiccmd (ticcmd_t* cmd,int playernum)
{
    char ziptic=0;
    byte *ziptic_p;

    ziptic_p=demo_p++;  // the ziptic
                        // write at the end of this function

    if(cmd->forwardmove != oldcmd[playernum].forwardmove)
    {
        *demo_p++ = cmd->forwardmove;
        oldcmd[playernum].forwardmove = cmd->forwardmove;
        ziptic|=ZT_FWD;
    }

    if(cmd->sidemove != oldcmd[playernum].sidemove)
    {
        *demo_p++ = cmd->sidemove;
        oldcmd[playernum].sidemove=cmd->sidemove;
        ziptic|=ZT_SIDE;
    }

    if(cmd->angleturn != oldcmd[playernum].angleturn)
    {
        *(short *)demo_p = cmd->angleturn;
        demo_p +=2;
        oldcmd[playernum].angleturn=cmd->angleturn;
        ziptic|=ZT_ANGLE;
    }

    if(cmd->buttons != oldcmd[playernum].buttons)
    {
        *demo_p++ = cmd->buttons;
        oldcmd[playernum].buttons=cmd->buttons;
        ziptic|=ZT_BUTTONS;
    }

    if(cmd->aiming != oldcmd[playernum].aiming)
    {
        *(short *)demo_p = cmd->aiming;
        demo_p+=2;
        oldcmd[playernum].aiming=cmd->aiming;
        ziptic|=ZT_AIMING;
    }

    if(AddLmpExtradata(&demo_p,playernum))
        ziptic|=ZT_EXTRADATA;

    *ziptic_p=ziptic;
    //added:16-02-98: attention here for the ticcmd size!
    // latest demos with mouse aiming byte in ticcmd
    if (ziptic_p > demoend - (5*MAXPLAYERS))
    {
        G_CheckDemoStatus ();   // no more space
        return;
    }

//  don't work in network the consistency is not copyed in the cmd
//    demo_p = ziptic_p;
//    G_ReadDemoTiccmd (cmd,playernum);         // make SURE it is exactly the same
}



//
// G_RecordDemo
//
void G_RecordDemo (char* name)
{
    int             i;
    int             maxsize;

    usergame = false; // can't save, can't end
    strcpy (demoname, name);
    strcat (demoname, ".lmp");
    maxsize = 0x20000;
    i = M_CheckParm ("-maxdemo");
    if (i && i<myargc-1)
        maxsize = atoi(myargv[i+1])*1024;
    demobuffer = Z_Malloc (maxsize,PU_STATIC,NULL);
    demoend = demobuffer + maxsize;

    demorecording = true;
}


void G_BeginRecording (void)
{
    int             i;

    demo_p = demobuffer;

    *demo_p++ = VERSION;
    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = cv_deathmatch.value;   // just to be compatible with old demo (no more used)
    *demo_p++ = cv_respawnmonsters.value;// just to be compatible with old demo (no more used)
    *demo_p++ = cv_fastmonsters.value;   // just to be compatible with old demo (no more used)
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;
    *demo_p++ = cv_timelimit.value;      // just to be compatible with old demo (no more used)

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if(playeringame[i])
          *demo_p++ = 1;
        else
          *demo_p++ = 0;
    }

    memset(oldcmd,0,sizeof(oldcmd));
}


//
// G_PlayDemo
//

char   defdemoname[MAX_WADPATH];             //might handle a full path name

void G_DeferedPlayDemo (char* name)
{
    strcpy (defdemoname, name);
    gameaction = ga_playdemo;
}


//
//  Start a demo from a .LMP file or from a wad resource (eg: DEMO1)
//
void G_DoPlayDemo (void)
{
    skill_t skill;
    int     i, episode, map;

//
// load demo file / resource
//

    //it's an internal demo
    if ((i=W_CheckNumForName(defdemoname)) == -1)
    {
        FIL_DefaultExtension(defdemoname,".lmp");
        if (!FIL_ReadFile (defdemoname, &demobuffer) )
        {
            CONS_Printf ("\2ERROR: couldn't open file '%s'.\n", defdemoname);
            goto no_demo;
        }
        demo_p = demobuffer;
    }
    else
        demobuffer = demo_p = W_CacheLumpNum (i, PU_STATIC);

//
// read demo header
//

    gameaction = ga_nothing;

    if ( (demoversion = *demo_p++) < 109 )
    {
        CONS_Printf ("\2ERROR: demo version too old.\n");
        Z_Free (demobuffer);
no_demo:
        gameaction = ga_nothing;
        return;
    }

    if (demoversion < VERSION)
        CONS_Printf ("\2Demo is from an older game version\n");

    skill       = *demo_p++;
    episode     = *demo_p++;
    map         = *demo_p++;
    if (demoversion < 127)
        // push it in the console will be too late set
        cv_deathmatch.value=*demo_p++;
    else
        demo_p++;

    if (demoversion < 128)
        // push it in the console will be too late set
        cv_respawnmonsters.value=*demo_p++;
    else
        demo_p++;
    
    if (demoversion < 128)
    {
        // push it in the console will be too late set
        cv_fastmonsters.value=*demo_p++;
        cv_fastmonsters.func();
    }
    else
        demo_p++;

    nomonsters  = *demo_p++;

    //added:08-02-98: added displayplayer because the status bar links
    // to the display player when playing back a demo.
    displayplayer = consoleplayer = *demo_p++;

     //added:11-01-98:
    //  support old v1.9 demos with ONLY 4 PLAYERS ! Man! what a shame!!!
    if (demoversion==109)
    {
        for (i=0 ; i<4 ; i++)
            playeringame[i] = *demo_p++;
    }
    else
    {
        if(demoversion<128)
        {
           cv_timelimit.value=*demo_p++;
           cv_timelimit.func();
        }
        else
            *demo_p++;

        if (demoversion<113)
        {
            for (i=0 ; i<8 ; i++)
                playeringame[i] = *demo_p++;
        }
        else
            for (i=0 ; i<32 ; i++)
                playeringame[i] = *demo_p++;
#if MAXPLAYERS>32
#error Please add support for old lmps
#endif
    }

    // FIXME: do a proper test here
    multiplayer = playeringame[1];
//        if(consoleplayer==0)
//            secondarydisplayplayer = 1;
//        else
//            secondarydisplayplayer = 0;
//        CV_SetValue(&cv_splitscreen,1);

    memset(oldcmd,0,sizeof(oldcmd));

    // don't spend a lot of time in loadlevel
    if(demoversion<127)
    {
        precache = false;
        G_InitNew (skill, G_BuildMapName(episode, map));
        precache = true;
        CON_ToggleOff (); // will be done at the end of map command
    }
    else
        gamestate = GS_WAITINGPLAYERS;

    demoplayback = true;
    usergame = false;
}

extern int framecount;

//
// G_TimeDemo
//             NOTE: name is a full filename for external demos
//
void G_TimeDemo (char* name)
{
    nodrawers = M_CheckParm ("-nodraw");
    noblit = M_CheckParm ("-noblit");
    timingdemo = true;
    singletics = true;
    framecount = 0;

    G_DeferedPlayDemo (name);
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

// reset engine variable set for the demos
// called from stopdemo command, map command, and g_checkdemoStatus.
void G_StopDemo(void)
{
    int i;

    Z_Free (demobuffer);
    demoplayback  = false;
    netgame       = false;
    multiplayer   = false;
    playeringame[0]=1;
    for(i=1;i<MAXPLAYERS;i++)
        playeringame[i] = 0;
//    respawnparm   = false;
    CV_SetValue(&cv_respawnmonsters,0);
//    fastparm      = false;
    CV_SetValue(&cv_fastmonsters,0);
    nomonsters    = false;
    consoleplayer = 0;
    //added:27-02-98: current VERSION when not playing back demo
    demoversion   = VERSION;

    usergame      = true;
    paused        = false;
    automapactive = false;
    viewactive    = true;

    timingdemo = false;
    singletics = false;
}

boolean G_CheckDemoStatus (void)
{
    if (timingdemo)
    {
        int time;
        float f1,f2;
        time = I_GetTime () - starttime;
        if(!time) return true;
         //added:03-01-98: since I_Error will ShutdownSystem() and
        //   ShutdownSystem() will do a CheckDemoStatus()
        G_StopDemo ();
        timingdemo = false;
        f1=time;
        f2=framecount*TICRATE;
        CONS_Printf ("timed %i gametics in %i realtics\n"
                     "%f secondes, %f avg fps\n"
                     ,gametic,time,f1/TICRATE,f2/f1);
        D_AdvanceDemo ();
        return true;

    }

    if (demoplayback)
    {
        if (singledemo)
            I_Quit ();
        G_StopDemo();
        D_AdvanceDemo ();
        return true;
    }

    if (demorecording)
    {
        *demo_p++ = DEMOMARKER;
        FIL_WriteFile (demoname, demobuffer, demo_p - demobuffer);
        Z_Free (demobuffer);
        demorecording = false;

        CONS_Printf("\2Demo %s recorded\n",demoname);
        return true;
        //I_Quit ();
    }

    return false;
}
