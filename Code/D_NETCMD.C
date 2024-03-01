
// d_netcmd.h : host/client network commands
//              commands are executed through the command buffer
//              like console commands
//
//              other miscellaneous commands (at the end)

#include "doomdef.h"

#include "console.h"
#include "command.h"

#include "d_netcmd.h"
#include "d_clisrv.h"
#include "i_system.h"
#include "dstrings.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "g_input.h"
#include "m_menu.h"
#include "r_local.h"
#include "r_things.h"
#include "p_inter.h"
#include "p_setup.h"
#include "s_sound.h"
#include "m_misc.h"
#include "am_map.h"
#include "byteptr.h"

// ------
// protos
// ------
void Command_Color_f (void);
void Command_Name_f (void);

void Command_WeaponPref(void);

void Got_NameAndcolor(char **cp,int playernum);
void Got_WeaponPref  (char **cp,int playernum);
void Got_Mapcmd      (char **cp,int playernum);
void Got_ExitLevelcmd(char **cp,int playernum);
void Got_LoadGamecmd (char **cp,int playernum);
void Got_SaveGamecmd (char **cp,int playernum);
void Got_Pause       (char **cp,int playernum);

void TeamPlay_OnChange(void);
void FragLimit_OnChange(void);
void Deahtmatch_OnChange(void);
void TimeLimit_OnChange(void);

void Command_Playdemo_f (void);
void Command_Timedemo_f (void);
void Command_Stopdemo_f (void);
void Command_Map_f (void);

void Command_Addfile (void);
void Command_Pause(void);

void Command_Turbo_f (void);
void Command_Frags_f (void);
void Command_TeamFrags_f(void);
void Command_Version_f (void);
void Command_Quit_f (void);

void Command_CheatNoClip_f (void);
void Command_CheatGod_f (void);
void Command_CheatGimme_f (void);

void Command_Water_f (void);
void Command_ExitLevel_f(void);
void Command_Load_f(void);
void Command_Save_f(void);

// =========================================================================
//                           CLIENT VARIABLES
// =========================================================================

void SendWeaponPref(void);
void SendNameAndColor(void);
void SendNameAndColor2(void);

// these two are just meant to be saved to the config
consvar_t cv_playername           = {"name"                ,"gi joe"   ,CV_SAVE | CV_CALL | CV_NOINIT,NULL,SendNameAndColor};
consvar_t cv_playercolor          = {"color"               ,"0"        ,CV_SAVE | CV_CALL | CV_NOINIT,Color_cons_t,SendNameAndColor};
// player's skin, saved for commodity, when using a favorite skins wad..
consvar_t cv_skin                 = {"skin"                ,DEFAULTSKIN,CV_SAVE | CV_CALL | CV_NOINIT,NULL /*skin_cons_t*/,SendNameAndColor};
consvar_t cv_weaponpref           = {"weaponpref"          ,"014576328",CV_SAVE | CV_CALL | CV_NOINIT,NULL,SendWeaponPref};
consvar_t cv_autoaim              = {"autoaim"             ,"1"        ,CV_SAVE | CV_CALL | CV_NOINIT,CV_OnOff,SendWeaponPref};
consvar_t cv_originalweaponswitch = {"originalweaponswitch","0"        ,CV_SAVE | CV_CALL | CV_NOINIT,CV_OnOff,SendWeaponPref};
// secondary player for splitscreen mode
consvar_t cv_playername2          = {"name2"               ,"big b"    ,CV_SAVE | CV_CALL | CV_NOINIT,NULL,SendNameAndColor2};
consvar_t cv_playercolor2         = {"color2"              ,"1"        ,CV_SAVE | CV_CALL | CV_NOINIT,Color_cons_t,SendNameAndColor2};
consvar_t cv_skin2                = {"skin2"               ,DEFAULTSKIN,CV_SAVE | CV_CALL | CV_NOINIT,NULL /*skin_cons_t*/,SendNameAndColor2};


CV_PossibleValue_t usemouse_cons_t[]={{0,"Off"},{1,"On"},{2,"Force"},{0,NULL}};

#ifdef __WIN32__
#define usejoystick_cons_t  NULL        // accept whatever value
                                        // it is in fact the joystick device number
#else
#ifdef __GO32__
CV_PossibleValue_t usejoystick_cons_t[]={{0,"Off"}
                                        ,{1,"4 BUttons"}
                                        ,{2,"Standart"}
                                        ,{3,"6 Buttons"}
                                        ,{4,"Wingman Extreme"}
                                        ,{5,"Flightstick Pro"}
                                        ,{6,"8 Buttons"}
                                        ,{7,"Sidewinder"}
                                        ,{8,"GamePad Pro"}
                                        ,{9,"Snes lpt1"}
                                        ,{10,"Snes lpt2"}
                                        ,{11,"Snes lpt3"}
                                        ,{12,"Wingman Warrior"}
                                        ,{0,NULL}};
#else
#ifdef LINUX
#define usejoystick_cons_t  NULL
#else
#error "cv_usejoystick don't have possible value for this OS !"
#endif
#endif
#endif

consvar_t cv_usemouse    = {"use_mouse","1", CV_SAVE | CV_CALL,usemouse_cons_t,I_StartupMouse};
consvar_t cv_usejoystick = {"use_joystick","0",CV_SAVE | CV_CALL,usejoystick_cons_t,I_InitJoystick};

CV_PossibleValue_t teamplay_cons_t[]={{0,"Off"},{1,"Color"},{2,"Skin"},{3,NULL}};
CV_PossibleValue_t deathmatch_cons_t[]={{0,"Coop"},{1,"1"},{2,"2"},{3,"3"},{0,NULL}};
CV_PossibleValue_t fraglimit_cons_t[]={{0,"MIN"},{1000,"MAX"},{0,NULL}};

consvar_t cv_teamplay   = {"teamplay"  ,"0",CV_NETVAR | CV_CALL,teamplay_cons_t, TeamPlay_OnChange};
consvar_t cv_teamdamage = {"teamdamage","0",CV_NETVAR,CV_OnOff};

consvar_t cv_fraglimit  = {"fraglimit" ,"0",CV_NETVAR | CV_CALL | CV_NOINIT,fraglimit_cons_t, FragLimit_OnChange};
consvar_t cv_timelimit  = {"timelimit" ,"0",CV_NETVAR | CV_CALL | CV_NOINIT,CV_Unsigned, TimeLimit_OnChange};
consvar_t cv_deathmatch = {"deathmatch","0",CV_NETVAR | CV_CALL,deathmatch_cons_t, Deahtmatch_OnChange};

extern consvar_t cv_playdemospeed;

// =========================================================================
//                           CLIENT STARTUP
// =========================================================================

// register client and server commands
//
void D_RegisterClientCommands (void)
{
    int i;

    for(i=0;i<MAXSKINCOLORS;i++)
        Color_cons_t[i].strvalue=Color_Names[i];

  //
  // register commands
  //
    RegisterNetXCmd(XD_NAMEANDCOLOR,Got_NameAndcolor);
    RegisterNetXCmd(XD_WEAPONPREF,Got_WeaponPref);
    RegisterNetXCmd(XD_MAP,Got_Mapcmd);
    RegisterNetXCmd(XD_EXITLEVEL,Got_ExitLevelcmd);
    RegisterNetXCmd(XD_PAUSE,Got_Pause);

    COM_AddCommand ("playdemo", Command_Playdemo_f);
    COM_AddCommand ("timedemo", Command_Timedemo_f);
    COM_AddCommand ("stopdemo", Command_Stopdemo_f);
    COM_AddCommand ("map", Command_Map_f);

    COM_AddCommand ("addfile", Command_Addfile);
    COM_AddCommand ("pause", Command_Pause);

    COM_AddCommand ("turbo", Command_Turbo_f);     // turbo speed
    COM_AddCommand ("version", Command_Version_f);
    COM_AddCommand ("quit", Command_Quit_f);

    COM_AddCommand ("chatmacro", Command_Chatmacro_f); // hu_stuff.c
    COM_AddCommand ("setcontrol", Command_Setcontrol_f);
    COM_AddCommand ("setcontrol2", Command_Setcontrol2_f);

    COM_AddCommand ("frags",Command_Frags_f);
    COM_AddCommand ("teamfrags",Command_TeamFrags_f);

    COM_AddCommand ("saveconfig",Command_SaveConfig_f);
    COM_AddCommand ("loadconfig",Command_LoadConfig_f);
    COM_AddCommand ("changeconfig",Command_ChangeConfig_f);
    COM_AddCommand ("exitlevel",Command_ExitLevel_f);
    COM_AddCommand ("screenshot",M_ScreenShot);

  //
  // register main variables
  //
    //register these so it is saved to config
    CV_RegisterVar (&cv_playername);
    CV_RegisterVar (&cv_playercolor);
    CV_RegisterVar (&cv_weaponpref);
    CV_RegisterVar (&cv_autoaim);
    CV_RegisterVar (&cv_originalweaponswitch);

    // r_things.c (skin NAME)
    CV_RegisterVar (&cv_skin);
    // secondary player (splitscreen)
    CV_RegisterVar (&cv_skin2);
    CV_RegisterVar (&cv_playername2);
    CV_RegisterVar (&cv_playercolor2);

    //FIXME: not to be here.. but needs be done for config loading
    CV_RegisterVar (&cv_usegamma);
    CV_RegisterVar (&cv_viewsize);

    //m_menu.c
    CV_RegisterVar (&cv_crosshair);
    CV_RegisterVar (&cv_autorun);
    CV_RegisterVar (&cv_invertmouse);
    CV_RegisterVar (&cv_alwaysfreelook);
    CV_RegisterVar (&cv_showmessages);

    //g_input.c
    CV_RegisterVar (&cv_mousesens);
    CV_RegisterVar (&cv_mlooksens);

    CV_RegisterVar (&cv_usemouse);
    CV_RegisterVar (&cv_usejoystick);
    CV_RegisterVar (&cv_allowjump);
    CV_RegisterVar (&cv_allowautoaim);

    //s_sound.c
    CV_RegisterVar (&cv_soundvolume);
    CV_RegisterVar (&cv_musicvolume);
    CV_RegisterVar (&cv_numChannels);

    //i_cdmus.c
    CV_RegisterVar (&cd_volume);
    CV_RegisterVar (&cdUpdate);

    // screen.c ?
    CV_RegisterVar (&cv_scr_width);
    CV_RegisterVar (&cv_scr_height);

    // add cheat commands, I'm bored of deh patches renaming the idclev ! :-)
    COM_AddCommand ("noclip", Command_CheatNoClip_f);
    COM_AddCommand ("god", Command_CheatGod_f);
    COM_AddCommand ("gimme", Command_CheatGimme_f);

    // p_mobj.c
    CV_RegisterVar (&cv_itemrespawntime);
    CV_RegisterVar (&cv_itemrespawn);
    CV_RegisterVar (&cv_respawnmonsters);
    CV_RegisterVar (&cv_respawnmonsterstime);
    CV_RegisterVar (&cv_fastmonsters);

    // WATER HACK TEST UNTIL FULLY FINISHED
    COM_AddCommand ("dev_water", Command_Water_f);

    //misc
    CV_RegisterVar (&cv_teamplay);
    CV_RegisterVar (&cv_teamdamage);
    CV_RegisterVar (&cv_fraglimit);
    CV_RegisterVar (&cv_deathmatch);
    CV_RegisterVar (&cv_timelimit);
    CV_RegisterVar (&cv_playdemospeed);

    COM_AddCommand ("load",Command_Load_f);
    RegisterNetXCmd(XD_LOADGAME,Got_LoadGamecmd);
    COM_AddCommand ("save",Command_Save_f);
    RegisterNetXCmd(XD_SAVEGAME,Got_SaveGamecmd);

/* ideas of commands names from Quake
    "status"
    "notarget"
    "fly"
    "restart"
    "changelevel"
    "connect"
    "reconnect"
    "noclip"
    "tell"
    "kill"
    "spawn"
    "begin"
    "prespawn"
    "ping"
    "give"

    "startdemos"
    "demos"
    "stopdemo"
*/

}

// =========================================================================
//                            CLIENT STUFF
// =========================================================================

//  name <playername> : name has changed
//
void SendNameAndColor(void)
{
    char     buf[MAXPLAYERNAME+1+SKINNAMESIZE+1],*p;

    p=buf;
    WRITEBYTE(p,cv_playercolor.value);
    WRITESTRINGN(p,cv_playername.string,MAXPLAYERNAME);
    *(p-1) = 0; // finish teh string;

    // check if player has the skin loaded (cv_skin may have
    //  the name of a skin that was available in the previous game)
    cv_skin.value=R_SkinAvailable(cv_skin.string);
    if (!cv_skin.value)
        WRITESTRINGN(p,DEFAULTSKIN,SKINNAMESIZE)
    else
        WRITESTRINGN(p,cv_skin.string,SKINNAMESIZE);
    *(p-1) = 0; // finish the string;

    SendNetXCmd(XD_NAMEANDCOLOR,buf,p-buf);
}
// splitscreen
void SendNameAndColor2(void)
{
    char     buf[MAXPLAYERNAME+1+SKINNAMESIZE+1],*p;

    p=buf;
    WRITEBYTE(p,cv_playercolor2.value);
    WRITESTRINGN(p,cv_playername2.string,MAXPLAYERNAME);
    *(p-1) = 0; // finish teh string;

    // check if player has the skin loaded (cv_skin may have
    //  the name of a skin that was available in the previous game)
    cv_skin2.value=R_SkinAvailable(cv_skin2.string);
    if (!cv_skin2.value)
        WRITESTRINGN(p,DEFAULTSKIN,SKINNAMESIZE)
    else
        WRITESTRINGN(p,cv_skin2.string,SKINNAMESIZE);
    *(p-1) = 0; // finish the string;

    SendNetXCmd2(XD_NAMEANDCOLOR,buf,p-buf);
}


void Got_NameAndcolor(char **cp,int playernum)
{
    player_t *p=&players[playernum];

    // color
    p->skincolor=READBYTE(*cp) % MAXSKINCOLORS;

    // a copy of color
    if(p->mo)
        p->mo->flags =  (p->mo->flags & ~MF_TRANSLATION)
                     | ((p->skincolor)<<MF_TRANSSHIFT);

    // name
    if(demoversion>=128)
        READSTRING(*cp,player_names[playernum])
    else
    {
        memcpy(player_names[playernum],*cp,MAXPLAYERNAME);
        *cp+=MAXPLAYERNAME;
    }

    // skin
    if (demoversion<120 || demoversion>=125)
        if(demoversion>=128)
        {
            SetPlayerSkin(playernum,*cp);
            SKIPSTRING(*cp);
        }
        else
    {
        SetPlayerSkin(playernum,*cp);
        *cp+=(SKINNAMESIZE+1);
    }
}


void SendWeaponPref(void)
{
    char buf[NUMWEAPONS+2];

    if(strlen(cv_weaponpref.string)!=NUMWEAPONS)
    {
        CONS_Printf("weaponpref must have %d characters",NUMWEAPONS);
        return;
    }
    buf[0]=cv_originalweaponswitch.value;
    memcpy(buf+1,cv_weaponpref.string,NUMWEAPONS);
    buf[1+NUMWEAPONS]=cv_autoaim.value;
    SendNetXCmd(XD_WEAPONPREF,buf,NUMWEAPONS+2);
    // FIXME : the split screen player have the same weapon pref of the first player
    if(cv_splitscreen.value)
        SendNetXCmd2(XD_WEAPONPREF,buf,NUMWEAPONS+2);
}

void Got_WeaponPref(char **cp,int playernum)
{
    players[playernum].originalweaponswitch = *(*cp)++;
    memcpy(players[playernum].favoritweapon,*cp,NUMWEAPONS);
    *cp+=NUMWEAPONS;
    players[playernum].autoaim_toggle=*(*cp)++;
}

void D_SendPlayerConfig(void)
{
    SendNameAndColor();
    if(cv_splitscreen.value)
        SendNameAndColor2();
    SendWeaponPref();
}

// ========================================================================

//  play a demo, add .lmp for external demos
//  eg: playdemo demo1 plays the internal game demo
//
// byte*   demofile;       //demo file buffer

void Command_Playdemo_f (void)
{
    char    name[256];

    if (COM_Argc() != 2)
    {
        CONS_Printf ("playdemo <demoname> : playback a demo\n");
        return;
    }

    // disconnect from server here ?
    if(demoplayback)
        G_StopDemo();
    if(netgame)
    {
        CONS_Printf("\nYou can't play a demo while in net game\n");
        return;
    }

    // open the demo file
    strcpy (name, COM_Argv(1));
    // dont add .lmp so internal game demos can be played
    //FIL_DefaultExtension (name, ".lmp");

    CONS_Printf ("Playing back demo '%s'.\n", name);

    G_DeferedPlayDemo (name);
}

void Command_Timedemo_f (void)
{
    char    name[256];

    if (COM_Argc() != 2)
    {
        CONS_Printf ("timedemo <demoname> : time a demo\n");
        return;
    }

    // disconnect from server here ?
    if(demoplayback)
        G_StopDemo();
    if(netgame)
    {
        CONS_Printf("\nYou can't play a demo while in net game\n");
        return;
    }

    // open the demo file
    strcpy (name, COM_Argv(1));
    // dont add .lmp so internal game demos can be played
    //FIL_DefaultExtension (name, ".lmp");

    CONS_Printf ("Timing demo '%s'.\n", name);

    G_TimeDemo (name);
}

//  stop current demo
//
void Command_Stopdemo_f (void)
{
    G_CheckDemoStatus ();
    CONS_Printf ("Stopped demo.\n");
}


//  Warp to map code.
//  Called either from map <mapname> console command, or idclev cheat.
//
void Command_Map_f (void)
{
    char buf[MAX_WADPATH+3];
    int i;

    if (COM_Argc()<2 || COM_Argc()>6)
    {
        CONS_Printf ("map <mapname[.wad]> [-skill <1..5>] [-monsters <0/1>]: warp to map\n");
        return;
    }

    if(!server)
    {
        CONS_Printf ("Only the server can change the map\n");
        return;
    }

    if((i=COM_CheckParm("-skill"))!=0)
        buf[0]=atoi(COM_Argv(i+1))-1;
    else
        buf[0]=gameskill;

    if((i=COM_CheckParm("-monsters"))!=0)
        buf[1]=(atoi(COM_Argv(i+1))==0);
    else
        buf[1]=nomonsters; 

// it appere strange to do that here but now i explian my (Boris):
// map is used to load a map with the current settings include in network and
// lmps
// BUT is not realy map that have been pushed on the console is the execution
// of map command (it is the upcode  of got_mapcmd) therefore this parameter
// must be set here. In case if the user run map at the console
    if(demoplayback)
        G_StopDemo(); // reset engine parameter

    strcpy(&buf[2],COM_Argv(1));
    SendNetXCmd(XD_MAP,buf,2+strlen(&buf[2])+1);
}

void Got_Mapcmd(char **cp,int playernum)
{
    char mapname[MAX_WADPATH];
    int skill;

    skill=READBYTE(*cp); 
    if(demoversion>=128)
        nomonsters=READBYTE(*cp); 
    strcpy(mapname,*cp);
    *cp+=strlen(mapname)+1;

    CONS_Printf ("Warping to map...\n");
    if(demoplayback && !timingdemo)
        precache=false;
    G_InitNew (skill, mapname);
    if(demoplayback && !timingdemo)
        precache=true;
    CON_ToggleOff ();
}

void Command_Pause(void)
{
    SendNetXCmd(XD_PAUSE,NULL,0);
}

void Got_Pause(char **cp,int playernum)
{
    paused ^= 1;
    if(!demoplayback)
    {
        if(netgame)
        {
            if( paused )
                CONS_Printf("Game paused by %s\n",player_names[playernum]);
            else
                CONS_Printf("Game unpaused by %s\n",player_names[playernum]);
        }
        
        if (paused) {
            if(!menuactive || netgame)
                S_PauseSound ();
        }
        else
            S_ResumeSound ();
    }
}

//  Add a pwad at run-time
//  Search for sounds, maps, musics, etc..
//
void Command_Addfile (void)
{
    if (COM_Argc()!=2)
    {
        CONS_Printf ("addfile <wadfile.wad> : load wad file\n");
        return;
    }

    P_AddWadFile (COM_Argv(1));
}



// =========================================================================
//                            MISC. COMMANDS
// =========================================================================

//  turbo <10-255>
//
void Command_Turbo_f (void)
{
static fixed_t  originalforwardmove[2] = {0x19, 0x32};
static fixed_t  originalsidemove[2] = {0x18, 0x28};

    int     scale = 200;

    extern int forwardmove[2];
    extern int sidemove[2];

    if (COM_Argc()!=2)
    {
        CONS_Printf("turbo <10-255> : set turbo");
        return;
    }

    scale = atoi (COM_Argv(1));

    if (scale < 10)
        scale = 10;
    if (scale > 255)
        scale = 255;

    CONS_Printf ("turbo scale: %i%%\n",scale);

    forwardmove[0] = originalforwardmove[0]*scale/100;
    forwardmove[1] = originalforwardmove[1]*scale/100;
    sidemove[0] = originalsidemove[0]*scale/100;
    sidemove[1] = originalsidemove[1]*scale/100;
}

void Command_Frags_f (void)
{
    int i,j;

    if(!cv_deathmatch.value)
    {
        CONS_Printf("Frags : show the frag table\n");
        CONS_Printf("Only for deathmatch games\n");
        return;
    }

    for(i=0;i<MAXPLAYERS;i++)
        if(playeringame[i])
        {
            CONS_Printf("%-16s",player_names[i]);
            for(j=0;j<MAXPLAYERS;j++)
                if(playeringame[j])
                    CONS_Printf(" %3d",players[i].frags[j]);
            CONS_Printf("\n");
        }
}

void Command_TeamFrags_f(void)
{
    int i,j;
    fragsort_t unused[MAXPLAYERS];
    int frags[MAXPLAYERS];
    int fragtbl[MAXPLAYERS][MAXPLAYERS];

    if(!cv_deathmatch.value && !cv_teamplay.value)
    {
        CONS_Printf("teamfrags : show the frag table for teams\n");
        CONS_Printf("Only for deathmatch teamplay games\n");
        return;
    }

    HU_CreateTeamFragTbl(unused,frags,fragtbl);

    for(i=0;i<11;i++)
        if(teamingame(i))
        {
            CONS_Printf("%-8s",team_names[i]);
            for(j=0;j<11;j++)
                if(teamingame(j))
                    CONS_Printf(" %3d",fragtbl[i][j]);
            CONS_Printf("\n");
        }
}


//  Returns program version.
//
void Command_Version_f (void)
{
    CONS_Printf ("Doom LEGACY version %i.%i ("
                __TIME__" "__DATE__")\n"VERSIONSTRING,
                VERSION/100,VERSION%100);

}


//  Quit the game immediately
//
void Command_Quit_f (void)
{
    I_Quit();
}

void FragLimit_OnChange(void)
{
    int i;

    if(cv_fraglimit.value>0)
    {
        for(i=0;i<MAXPLAYERS;i++)
            P_CheckFragLimit(&players[i]);
    }
}

extern int levelTimer;
extern int levelTimeCount;

void TimeLimit_OnChange(void)
{
    if(cv_timelimit.value)
    {
        levelTimer = true;
        levelTimeCount = cv_timelimit.value * 60 * TICRATE;
        CONS_Printf("Levels will end after %d minute(s).\n",cv_timelimit.value);
    }
    else
        levelTimer = false;
}

void P_RespawnWeapons(void);
void Deahtmatch_OnChange(void)
{
    if(server)
    {
        if( cv_deathmatch.value>=2 )
            CV_SetValue(&cv_itemrespawn,1);
        else
            CV_SetValue(&cv_itemrespawn,0);
    }
    if( cv_deathmatch.value!=2 )
        P_RespawnWeapons ();

    // give all key to the players
    if (cv_deathmatch.value)
    {
        int i,j;
        for(j=0;j<MAXPLAYERS;j++)
            if(playeringame[j])
                for (i=0 ; i<NUMCARDS ; i++)
                    players[j].cards[i] = true;
    }
}

void Command_ExitLevel_f(void)
{
    if(!server)
    {
        CONS_Printf("Only the server can exit the level\n");
        return;
    }
    SendNetXCmd(XD_EXITLEVEL,NULL,0);
}

void Got_ExitLevelcmd(char **cp,int playernum)
{
    G_ExitLevel();
}


void Command_Load_f(void)
{
    byte slot;

    if(COM_Argc()!=2)
    {
        CONS_Printf("load <slot>: load a saved game\n");
        return;
    }

    if(!server)
    {
        CONS_Printf("Only server can do a load game\n");
        return;
    }

    slot=atoi(COM_Argv(1));
    SendNetXCmd(XD_LOADGAME,&slot,1);
}

void Got_LoadGamecmd(char **cp,int playernum)
{
    byte slot=*(*cp)++;
    G_DoLoadGame(slot);
}

void Command_Save_f(void)
{
    char p[SAVESTRINGSIZE+1];

    if(COM_Argc()!=3)
    {
        CONS_Printf("save <slot> <desciption>: save game\n");
        return;
    }

    if(!server)
    {
        CONS_Printf("Only server can do a save game\n");
        return;
    }

    p[0]=atoi(COM_Argv(1));
    strcpy(&p[1],COM_Argv(2));
    SendNetXCmd(XD_SAVEGAME,&p,strlen(&p[1])+2);
}

void Got_SaveGamecmd(char **cp,int playernum)
{
    byte slot;
    char description[SAVESTRINGSIZE];

    slot=*(*cp)++;
    strcpy(description,*cp);
    *cp+=strlen(description)+1;

    G_DoSaveGame(slot,description);
}


// =========================================================================
//                           CHEAT COMMANDS
// =========================================================================

// st_stuff.c : for dehacked patches
extern int idfa_armor;
extern int idfa_armor_class;
extern int idkfa_armor;
extern int idkfa_armor_class;
extern int god_health;

void Command_CheatNoClip_f (void)
{
    player_t*   plyr;
    if (multiplayer)
        return;

    plyr = &players[consoleplayer];

    plyr->cheats ^= CF_NOCLIP;

    if (plyr->cheats & CF_NOCLIP)
        CONS_Printf (STSTR_NCON);
    else
        CONS_Printf (STSTR_NCOFF);

}

void Command_CheatGod_f (void)
{
    player_t*   plyr;

    if (multiplayer)
        return;

    plyr = &players[consoleplayer];

    plyr->cheats ^= CF_GODMODE;
    if (plyr->cheats & CF_GODMODE)
    {
        if (plyr->mo)
            plyr->mo->health = god_health;

        plyr->health = god_health;
        CONS_Printf (STSTR_DQDON);
    }
    else
        CONS_Printf (STSTR_DQDOFF);
}

void Command_CheatGimme_f (void)
{
    char*     s;
    int       i,j;
    player_t* plyr;

    if (multiplayer) 
        return;

    if (COM_Argc()<2)
    {
        CONS_Printf ("gimme [health] [ammo] [armor] ...\n");
        return;
    }

    plyr = &players[consoleplayer];

    for (i=1; i<COM_Argc(); i++)
    {
        s = COM_Argv(i);

        if (!strncmp(s,"health",6))
        {
            if (plyr->mo)
                plyr->mo->health = god_health;

            plyr->health = god_health;

            CONS_Printf("got health\n");
        }
        else
        if (!strncmp(s,"ammo",4))
        {
            for (j=0;j<NUMAMMO;j++)
                plyr->ammo[j] = plyr->maxammo[j];

            CONS_Printf("got ammo\n");
        }
        else
        if (!strncmp(s,"armor",5))
        {
            plyr->armorpoints = idfa_armor;
            plyr->armortype = idfa_armor_class;

            CONS_Printf("got armor\n");
        }
        else
        if (!strncmp(s,"keys",4))
        {
            for (j=0;j<NUMCARDS;j++)
                plyr->cards[j] = true;

            CONS_Printf("got keys\n");
        }
        else
        if (!strncmp(s,"weapons",7))
        {
            for (j=0;j<NUMWEAPONS;j++)
                plyr->weaponowned[j] = true;

            for (j=0;j<NUMAMMO;j++)
                plyr->ammo[j] = plyr->maxammo[j];

            CONS_Printf("got weapons\n");
        }
        else
        //
        // WEAPONS
        //
        if (!strncmp(s,"chainsaw",8))
        {
            plyr->weaponowned[wp_chainsaw] = true;

            CONS_Printf("got chainsaw\n");
        }
        else
        if (!strncmp(s,"shotgun",7))
        {
            plyr->weaponowned[wp_shotgun] = true;
            plyr->ammo[am_shell] = plyr->maxammo[am_shell];

            CONS_Printf("got shotgun\n");
        }
        else
        if (!strncmp(s,"supershotgun",12))
        {
            if (gamemode == commercial) // only in Doom2
            {
                plyr->weaponowned[wp_supershotgun] = true;
                plyr->ammo[am_shell] = plyr->maxammo[am_shell];

                CONS_Printf("got super shotgun\n");
            }
        }
        else
        if (!strncmp(s,"rocket",6))
        {
            plyr->weaponowned[wp_missile] = true;
            plyr->ammo[am_misl] = plyr->maxammo[am_misl];

            CONS_Printf("got rocket launcher\n");
        }
        else
        if (!strncmp(s,"plasma",6))
        {
            plyr->weaponowned[wp_plasma] = true;
            plyr->ammo[am_cell] = plyr->maxammo[am_cell];

            CONS_Printf("got plasma\n");
        }
        else
        if (!strncmp(s,"bfg",3))
        {
            plyr->weaponowned[wp_bfg] = true;
            plyr->ammo[am_cell] = plyr->maxammo[am_cell];

            CONS_Printf("got bfg\n");
        }
        else
        if (!strncmp(s,"chaingun",8))
        {
            plyr->weaponowned[wp_chaingun] = true;
            plyr->ammo[am_clip] = plyr->maxammo[am_clip];

            CONS_Printf("got chaingun\n");
        }
        else
        //
        // SPECIAL ITEMS
        //
        if (!strncmp(s,"berserk",7))
        {
            if (!plyr->powers[pw_strength])
                P_GivePower( plyr, pw_strength);
            CONS_Printf("got berserk strength\n");
        }
        //
        else
            CONS_Printf ("can't give '%s' : unknown\n", s);


    }
}