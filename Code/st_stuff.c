// st_stuff.c :
//      Status bar code.
//      Does the face/direction indicator animatin.
//      Does palette indicators as well (red pain/berserk, bright pickup)

#include "doomdef.h"

#include "am_map.h"

#include "g_game.h"
#include "m_cheat.h"

#include "screen.h"
#include "r_local.h"
#include "p_local.h"
#include "p_inter.h"
#include "m_random.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "i_video.h"
#include "v_video.h"

#include "keys.h"

#include "z_zone.h"

//protos
void ST_createWidgets(void);

extern fixed_t waterheight;

//
// STATUS BAR DATA
//

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS            1
#define STARTBONUSPALS          9
#define NUMREDPALS              8
#define NUMBONUSPALS            4
// Radiation suit, green shift.
#define RADIATIONPAL            13

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY              96

// For Responder
#define ST_TOGGLECHAT           KEY_ENTER

// Location of status bar
  //added:08-01-98:status bar position changes according to resolution.
#define ST_FX                     143

// Should be set to patch width
//  for tall numbers later on
#define ST_TALLNUMWIDTH         (tallnum[0]->width)

// Number of status faces.
#define ST_NUMPAINFACES         5
#define ST_NUMSTRAIGHTFACES     3
#define ST_NUMTURNFACES         2
#define ST_NUMSPECIALFACES      3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES        2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET           (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET           (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET       (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET        (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE              (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE             (ST_GODFACE+1)

#define ST_FACESX               143
#define ST_FACESY               (ST_Y+0)

#define ST_EVILGRINCOUNT        (2*TICRATE)
#define ST_STRAIGHTFACECOUNT    (TICRATE/2)
#define ST_TURNCOUNT            (1*TICRATE)
#define ST_OUCHCOUNT            (1*TICRATE)
#define ST_RAMPAGEDELAY         (2*TICRATE)

#define ST_MUCHPAIN             20


// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH            3
#define ST_AMMOX                44
#define ST_AMMOY                (ST_Y+3)

// HEALTH number pos.
#define ST_HEALTHWIDTH          3
#define ST_HEALTHX              90
#define ST_HEALTHY              (ST_Y+3)

// Weapon pos.
#define ST_ARMSX                111
#define ST_ARMSY                (ST_Y+4)
#define ST_ARMSBGX              104
#define ST_ARMSBGY              (ST_Y)
#define ST_ARMSXSPACE           12
#define ST_ARMSYSPACE           10

// Frags pos.
#define ST_FRAGSX               138
#define ST_FRAGSY               (ST_Y+3)
#define ST_FRAGSWIDTH           2

// ARMOR number pos.
#define ST_ARMORWIDTH           3
#define ST_ARMORX               221
#define ST_ARMORY               (ST_Y+3)

// Key icon positions.
#define ST_KEY0WIDTH            8
#define ST_KEY0HEIGHT           5
#define ST_KEY0X                239
#define ST_KEY0Y                (ST_Y+3)
#define ST_KEY1WIDTH            ST_KEY0WIDTH
#define ST_KEY1X                239
#define ST_KEY1Y                (ST_Y+13)
#define ST_KEY2WIDTH            ST_KEY0WIDTH
#define ST_KEY2X                239
#define ST_KEY2Y                (ST_Y+23)

// Ammunition counter.
#define ST_AMMO0WIDTH           3
#define ST_AMMO0HEIGHT          6
#define ST_AMMO0X               288
#define ST_AMMO0Y               (ST_Y+5)
#define ST_AMMO1WIDTH           ST_AMMO0WIDTH
#define ST_AMMO1X               288
#define ST_AMMO1Y               (ST_Y+11)
#define ST_AMMO2WIDTH           ST_AMMO0WIDTH
#define ST_AMMO2X               288
#define ST_AMMO2Y               (ST_Y+23)
#define ST_AMMO3WIDTH           ST_AMMO0WIDTH
#define ST_AMMO3X               288
#define ST_AMMO3Y               (ST_Y+17)

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH        3
#define ST_MAXAMMO0HEIGHT       5
#define ST_MAXAMMO0X            314
#define ST_MAXAMMO0Y            (ST_Y+5)
#define ST_MAXAMMO1WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X            314
#define ST_MAXAMMO1Y            (ST_Y+11)
#define ST_MAXAMMO2WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X            314
#define ST_MAXAMMO2Y            (ST_Y+23)
#define ST_MAXAMMO3WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X            314
#define ST_MAXAMMO3Y            (ST_Y+17)

//faB: unused stuff from the Doom alpha version ?
// pistol
//#define ST_WEAPON0X           110
//#define ST_WEAPON0Y           (ST_Y+4)
// shotgun
//#define ST_WEAPON1X           122
//#define ST_WEAPON1Y           (ST_Y+4)
// chain gun
//#define ST_WEAPON2X           134
//#define ST_WEAPON2Y           (ST_Y+4)
// missile launcher
//#define ST_WEAPON3X           110
//#define ST_WEAPON3Y           (ST_Y+13)
// plasma gun
//#define ST_WEAPON4X           122
//#define ST_WEAPON4Y           (ST_Y+13)
// bfg
//#define ST_WEAPON5X           134
//#define ST_WEAPON5Y           (ST_Y+13)

// WPNS title
//#define ST_WPNSX              109
//#define ST_WPNSY              (ST_Y+23)

 // DETH title
//#define ST_DETHX              109
//#define ST_DETHY              (ST_Y+23)

//Incoming messages window location
// #define ST_MSGTEXTX     (viewwindowx)
// #define ST_MSGTEXTY     (viewwindowy+viewheight-18)
//#define ST_MSGTEXTX             0
//#define ST_MSGTEXTY             0     //added:08-01-98:unused
// Dimensions given in characters.
#define ST_MSGWIDTH             52
// Or shall I say, in lines?
#define ST_MSGHEIGHT            1

#define ST_OUTTEXTX             0
#define ST_OUTTEXTY             6

// Width, in characters again.
#define ST_OUTWIDTH             52
 // Height, in lines.
#define ST_OUTHEIGHT            1

#define ST_MAPWIDTH     \
    (strlen(mapnames[(gameepisode-1)*9+(gamemap-1)]))

//added:24-01-98:unused ?
//#define ST_MAPTITLEX  (vid.width - ST_MAPWIDTH * ST_CHATFONTWIDTH)

#define ST_MAPTITLEY            0
#define ST_MAPHEIGHT            1


//added:02-02-98: set true if widgets coords need to be recalculated
boolean     st_recalc;

// main player in game
player_t*        plyr;

// ST_Start() has just been called
boolean          st_firsttime;

// used to execute ST_Init() only once
static int              veryfirsttime = 1;

// lump number for PLAYPAL
static int              lu_palette;

// used for timing
static unsigned int     st_clock;

// used for making messages go away
static int              st_msgcounter=0;

// used when in chat
static st_chatstateenum_t       st_chatstate;

// whether left-side main status bar is active
static boolean          st_statusbaron;

// whether status bar chat is active
static boolean          st_chat;

// value of st_chat before message popped up
static boolean          st_oldchat;

// whether chat window has the cursor on
static boolean          st_cursoron;

// !deathmatch
static boolean          st_notdeathmatch;

// !deathmatch && st_statusbaron
static boolean          st_armson;

// !deathmatch
static boolean          st_fragson;

// main bar left
static patch_t*         sbar;

// 0-9, tall numbers
static patch_t*         tallnum[10];

// tall % sign
static patch_t*         tallpercent;

// 0-9, short, yellow (,different!) numbers
static patch_t*         shortnum[10];

// 3 key-cards, 3 skulls
static patch_t*         keys[NUMCARDS];

// face status patches
static patch_t*         faces[ST_NUMFACES];

// face background
static patch_t*         faceback;

 // main bar right
static patch_t*         armsbg;

// weapon ownership patches
static patch_t*         arms[6][2];

// ready-weapon widget
static st_number_t      w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t      w_frags;

// health widget
static st_percent_t     w_health;

// arms background
static st_binicon_t     w_armsbg;


// weapon ownership widgets
static st_multicon_t    w_arms[6];

// face status widget
static st_multicon_t    w_faces;

// keycard widgets
static st_multicon_t    w_keyboxes[3];

// armor widget
static st_percent_t     w_armor;

// ammo widgets
static st_number_t      w_ammo[4];

// max ammo widgets
static st_number_t      w_maxammo[4];



 // number of frags so far in deathmatch
static int      st_fragscount;

// used to use appopriately pained face
static int      st_oldhealth = -1;

// used for evil grin
static boolean  oldweaponsowned[NUMWEAPONS];

 // count until face changes
static int      st_facecount = 0;

// current face index, used by w_faces
static int      st_faceindex = 0;

// holds key-type for each key box on bar
static int      keyboxes[3];

// a random number per tick
static int      st_randomnumber;



// ------------------------------------------
//             status bar overlay
// ------------------------------------------

// icons for overlay
static   patch_t*   sbohealth;
static   patch_t*   sbofrags;
static   patch_t*   sboarmor;
static   pic_t*   sboammo[NUMWEAPONS];


//
// STATUS BAR CODE
//
void ST_Stop(void);


//added:05-02-98:quickie routine to fill the space around the statusbar when
//               the screen is larger than the stbar.
void ST_drawStatusBarBorders(void)
{
    byte*       src;
    byte*       dest;
    int         x;
    int         y;
    int         i;

    i = vid.height-ST_HEIGHT;   //match with the backtile around the viewwindow
    src  = scr_borderpatch;
    dest = screens[BG];
    for (y=0 ; y<ST_HEIGHT ; y++)
    {
        for (x=0 ; x<vid.width/64 ; x++)
        {
            memcpy (dest, src+((i&63)<<6), 64);
            dest += 64;
        }
        if (vid.width&63)
        {
            memcpy (dest, src+((i&63)<<6), vid.width&63);
            dest += (vid.width&63);
        }
        i++;
    }
}


void ST_refreshBackground(void)
{
    byte*       colormap;
    int         st_x;
    int         st_y;

    if (st_statusbaron)
    {
        if (rendermode==render_soft)
        {
            // stbar is centered in BG buffer
            st_x = ((vid.width-ST_WIDTH)>>1);
            st_y = 0;
            // fill the gaps around the statusbar when sbar is not scaled
            ST_drawStatusBarBorders ();
        }
        else {
            //stbar is scaled to the original aspect ratio, direct to screen
            st_x = 0;
            st_y = BASEVIDHEIGHT-ST_HEIGHT;
        }

        // software mode copies patch to BG buffer,
        // hardware modes directly draw the statusbar to the screen
        V_DrawPatch(st_x, st_y, BG, sbar);

        // draw the faceback for the statusbarplayer
            if (plyr->skincolor==0)
                colormap = colormaps;
            else
                colormap = translationtables - 256 + (plyr->skincolor<<8);

            V_DrawTranslationPatch (st_x+ST_FX, st_y, BG, faceback, colormap);

        // copy the statusbar buffer to the screen
        if ( rendermode==render_soft )
            V_CopyRect(0, 0, BG, vid.width, ST_HEIGHT, 0, ST_Y, FG);
    }
}


// Respond to keyboard input events,
//  intercept cheats.
boolean ST_Responder (event_t* ev)
{

  if (ev->type == ev_keyup)
  {
    // Filter automap on/off : activates the statusbar while automap is active
    if( (ev->data1 & 0xffff0000) == AM_MSGHEADER )
    {
        switch(ev->data1)
        {
          case AM_MSGENTERED:
            st_firsttime = true;        // force refresh of status bar
            break;

          case AM_MSGEXITED:
            break;
        }
    }

  }
  return false;
}



int ST_calcPainOffset(void)
{
    int         health;
    static int  lastcalc;
    static int  oldhealth = -1;

    health = plyr->health > 100 ? 100 : plyr->health;

    //SOM: Show one less ring than acutally have
    health --;

    if (health != oldhealth)
    {
        lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }
    return lastcalc;
}


//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//
void ST_updateFaceWidget(void)
{
    int         i;
    angle_t     badguyangle;
    angle_t     diffang;
    static int  lastattackdown = -1;
    static int  priority = 0;
    boolean     doevilgrin;

    if (priority < 10)
    {
        // dead
        if (!plyr->health)
        {
            priority = 9;
            st_faceindex = ST_DEADFACE;
            st_facecount = 1;
        }
    }

    if (priority < 9)
    {
        if (plyr->bonuscount)
        {
            // picking up bonus
            doevilgrin = false;

            for (i=0;i<NUMWEAPONS;i++)
            {
                if (oldweaponsowned[i] != plyr->weaponowned[i])
                {
                    doevilgrin = true;
                    oldweaponsowned[i] = plyr->weaponowned[i];
                }
            }
            if (doevilgrin)
            {
                // evil grin if just picked up weapon
                priority = 8;
                st_facecount = ST_EVILGRINCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
            }
        }

    }

    if (priority < 8)
    {
        if (plyr->damagecount
            && plyr->attacker
            && plyr->attacker != plyr->mo)
        {
            // being attacked
            priority = 7;

            if (plyr->health - st_oldhealth > ST_MUCHPAIN)
            {
                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                badguyangle = R_PointToAngle2(plyr->mo->x,
                                              plyr->mo->y,
                                              plyr->attacker->x,
                                              plyr->attacker->y);

                if (badguyangle > plyr->mo->angle)
                {
                    // whether right or left
                    diffang = badguyangle - plyr->mo->angle;
                    i = diffang > ANG180;
                }
                else
                {
                    // whether left or right
                    diffang = plyr->mo->angle - badguyangle;
                    i = diffang <= ANG180;
                } // confusing, aint it?


                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset();

                if (diffang < ANG45)
                {
                    // head-on
                    st_faceindex += ST_RAMPAGEOFFSET;
                }
                else if (i)
                {
                    // turn face right
                    st_faceindex += ST_TURNOFFSET;
                }
                else
                {
                    // turn face left
                    st_faceindex += ST_TURNOFFSET+1;
                }
            }
        }
    }

    if (priority < 7)
    {
        // getting hurt because of your own damn stupidity
        if (plyr->damagecount)
        {
            if (plyr->health - st_oldhealth > ST_MUCHPAIN)
            {
                priority = 7;
                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                priority = 6;
                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
            }

        }

    }

    if (priority < 6)
    {
        // rapid firing
        if (plyr->attackdown)
        {
            if (lastattackdown==-1)
                lastattackdown = ST_RAMPAGEDELAY;
            else if (!--lastattackdown)
            {
                priority = 5;
                st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
                st_facecount = 1;
                lastattackdown = 1;
            }
        }
        else
            lastattackdown = -1;

    }

    if (priority < 5)
    {
        // invulnerability
        if ((plyr->cheats & CF_GODMODE)
            || plyr->powers[pw_invulnerability])
        {
            priority = 4;

            st_faceindex = ST_GODFACE;
            st_facecount = 1;

        }

    }

    // look left or look right if the facecount has timed out
    if (!st_facecount)
    {
        st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
        st_facecount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    st_facecount--;

}

boolean ST_SameTeam(player_t *a,player_t *b)
{
    switch (cv_teamplay.value) {
       case 0 : return false;
       case 1 : return (a->skincolor == b->skincolor);
       case 2 : return (a->skin == b->skin);
    }
    return false;
}

// count the frags of the 'statusbar' player
//Fab: made as a tiny routine so ST_overlayDrawer() can use it
//Boris: rename ST_countFrags in to ST_PlayerFrags for use anytime
//       when we need the frags
int ST_PlayerFrags (int playernum)
{
    int    i,frags;

    frags = 0;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if ((cv_teamplay.value==0 && i != playernum)
         || (cv_teamplay.value && !ST_SameTeam(&players[i],&players[playernum])) )
            frags += players[playernum].frags[i];
        else
            frags -= players[playernum].frags[i];
    }

    return frags;
}


void ST_updateWidgets(void)
{
    static int  largeammo = 1994; // means "n/a"
    int         i;

    // must redirect the pointer if the ready weapon has changed.
    //  if (w_ready.data != plyr->readyweapon)
    //  {
    if (weaponinfo[plyr->readyweapon].ammo == am_noammo)
        w_ready.num = &largeammo;
    else
        w_ready.num = &plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
    //{
    // static int tic=0;
    // static int dir=-1;
    // if (!(tic&15))
    //   plyr->ammo[weaponinfo[plyr->readyweapon].ammo]+=dir;
    // if (plyr->ammo[weaponinfo[plyr->readyweapon].ammo] == -100)
    //   dir = 1;
    // tic++;
    // }
    w_ready.data = plyr->readyweapon;

    // if (*w_ready.on)
    //  STlib_updateNum(&w_ready, true);
    // refresh weapon change
    //  }

    // update keycard multiple widgets
    for (i=0;i<3;i++)
    {
        keyboxes[i] = plyr->cards[i] ? i : -1;

        if (plyr->cards[i+3])
            keyboxes[i] = i+3;
    }

    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();

    // used by the w_armsbg widget
    st_notdeathmatch = !cv_deathmatch.value;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !cv_deathmatch.value;

    // used by w_frags widget
    st_fragson = cv_deathmatch.value && st_statusbaron;

    st_fragscount = ST_PlayerFrags(statusbarplayer);

    // get rid of chat window if up because of message
    if (!--st_msgcounter)
        st_chat = st_oldchat;

}

void ST_Ticker (void)
{

    st_clock++;
    st_randomnumber = M_Random();
    ST_updateWidgets();
    st_oldhealth = plyr->health;

}

static int st_palette = 0;


//extern fixed_t waterheight;
void ST_doPaletteStuff(void)
{

    int         palette;
    byte*       pal;
    int         cnt;
//    int         bzc;

    //SOM: NO PALETTE CHANGES!
//    return;

    cnt = plyr->damagecount;

//    if (plyr->powers[pw_strength])
//    {
       // slowly fade the berzerk out
//        bzc = 12 - (plyr->powers[pw_strength]>>6);

//      if (bzc > cnt)
//           cnt = bzc;
//    }

//    if (cnt)
//    {
//        palette = (cnt+7)>>3;

//        if (palette >= NUMREDPALS)
//            palette = NUMREDPALS-1;

//        palette += STARTREDPALS;
//    }
//    else
//    if (plyr->bonuscount)
//    {
//        palette = (plyr->bonuscount+7)>>3;
//
//       if (palette >= NUMBONUSPALS)
//            palette = NUMBONUSPALS-1;
//
//        palette += STARTBONUSPALS;
//    }
//    else
//   if ( plyr->powers[pw_ironfeet] > 4*32
//      || plyr->powers[pw_ironfeet]&8)
//        palette = RADIATIONPAL;
//   else
//        palette = 0;


    //added:28-02-98:quick hack underwater palette
//    if (plyr->mo &&
//       (plyr->mo->z + (cv_viewheight.value<<FRACBITS) < plyr->mo->waterz) )
//        palette = RADIATIONPAL;

      if ((plyr->mo->eflags & MF_UNDERWATER) && (camera.mo->z < plyr->mo->waterz))
       palette = RADIATIONPAL;
// Hack to fix dumb bug from previous underwater palette - Tails 10-31-99
    else
         palette = 0;

    if (palette != st_palette)
    {
        st_palette = palette;
        pal = (byte *) W_CacheLumpNum (lu_palette, PU_CACHE)+palette*768;
        I_SetPalette (pal);
    }

}

void ST_drawWidgets(boolean refresh)
{
    int         i;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !cv_deathmatch.value;

    // used by w_frags widget
    st_fragson = cv_deathmatch.value && st_statusbaron;

    STlib_updateNum(&w_ready, refresh);

    for (i=0;i<4;i++)
    {
        STlib_updateNum(&w_ammo[i], refresh);
        STlib_updateNum(&w_maxammo[i], refresh);
    }

    STlib_updatePercent(&w_health, refresh);
    STlib_updatePercent(&w_armor, refresh);

    STlib_updateBinIcon(&w_armsbg, refresh);

    for (i=0;i<6;i++)
        STlib_updateMultIcon(&w_arms[i], refresh);

    STlib_updateMultIcon(&w_faces, refresh);

    for (i=0;i<3;i++)
        STlib_updateMultIcon(&w_keyboxes[i], refresh);

    STlib_updateNum(&w_frags, refresh);

}

void ST_doRefresh(void)
{

    // draw status bar background to off-screen buff
    ST_refreshBackground();

    // and refresh all widgets
    ST_drawWidgets(true);

}

void ST_diffDraw(void)
{
    // update all widgets
    ST_drawWidgets(false);
}


void ST_Drawer (boolean fullscreen, boolean refresh)
{
    st_statusbaron = (!fullscreen) || automapactive;

    //added:30-01-98:force a set of the palette by doPaletteStuff()
    if (vid.recalc)
        st_palette = -1;

    // Do red-/gold-shifts from damage/items
    ST_doPaletteStuff();

    if (!cv_splitscreen.value)
    {
        // after ST_Start(), screen refresh needed, or vid mode change
        if (st_firsttime || refresh || st_recalc ||
            rendermode!=render_soft )    //faB: always refresh for now, other priorities for now
        {
            if (st_recalc)  //recalc widget coords after vid mode change
            {
                ST_createWidgets ();
                st_recalc = false;
            }
            st_firsttime = false;
            ST_doRefresh();
        }
        else
            // Otherwise, update as little as possible
            ST_diffDraw();
    }
}


static void ST_loadGraphics(void)
{

    int         i;
    char        namebuf[9];

    // Load the numbers, tall and short
    for (i=0;i<10;i++)
    {
        sprintf(namebuf, "STTNUM%d", i);
        tallnum[i] = (patch_t *) W_CachePatchName(namebuf, PU_STATIC);

        sprintf(namebuf, "STYSNUM%d", i);
        shortnum[i] = (patch_t *) W_CachePatchName(namebuf, PU_STATIC);
    }

    // Load percent key.
    //Note: why not load STMINUS here, too?
    tallpercent = (patch_t *) W_CachePatchName("STTPRCNT", PU_STATIC);

    // key cards
    for (i=0;i<NUMCARDS;i++)
    {
        sprintf(namebuf, "STKEYS%d", i);
        keys[i] = (patch_t *) W_CachePatchName(namebuf, PU_STATIC);
    }

    // arms background
    armsbg = (patch_t *) W_CachePatchName("STARMS", PU_STATIC);

    // arms ownership widgets
    for (i=0;i<6;i++)
    {
        sprintf(namebuf, "STGNUM%d", i+2);

        // gray #
        arms[i][0] = (patch_t *) W_CachePatchName(namebuf, PU_STATIC);

        // yellow #
        arms[i][1] = shortnum[i+2];
    }

    // status bar background bits
    sbar = (patch_t *) W_CachePatchName("STBAR", PU_STATIC);

    // the original Doom uses 'STF' as base name for all face graphics
    ST_loadFaceGraphics ("STF");
}


// made separate so that skins code can reload custom face graphics
void ST_loadFaceGraphics (char *facestr)
{
    int   i,j;
    int   facenum;
    char  namelump[9];
    char* namebuf;

    //hack: make sure base face name is no more than 3 chars
    // bug: core dump fixed 19990220 by Kin
    if(strlen(facestr)>3)
    facestr[3]='\0';
    strcpy (namelump, facestr);  // copy base name
    namebuf = namelump;
    while (*namebuf>' ') namebuf++;

    // face states
    facenum = 0;
    for (i=0;i<ST_NUMPAINFACES;i++)
    {
        for (j=0;j<ST_NUMSTRAIGHTFACES;j++)
        {
            sprintf(namebuf, "ST%d%d", i, j);
            faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
        }
        sprintf(namebuf, "TR%d0", i);        // turn right
        faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
        sprintf(namebuf, "TL%d0", i);        // turn left
        faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
        sprintf(namebuf, "OUCH%d", i);       // ouch!
        faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
        sprintf(namebuf, "EVL%d", i);        // evil grin ;)
        faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
        sprintf(namebuf, "KILL%d", i);       // pissed off
        faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
    }
    strcpy (namebuf, "GOD0");
    faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);
    strcpy (namebuf, "DEAD0");
    faces[facenum++] = W_CachePatchName(namelump, PU_STATIC);

    // face backgrounds for different player colors
    //added:08-02-98: uses only STFB0, which is remapped to the right
    //                colors using the player translation tables, so if
    //                you add new player colors, it is automatically
    //                used for the statusbar.
    strcpy (namebuf, "B0");
    faceback = (patch_t *) W_CachePatchName(namelump, PU_STATIC);
}


static void ST_loadData(void)
{
    lu_palette = W_GetNumForName ("PLAYPAL");
    ST_loadGraphics();
}

void ST_unloadGraphics(void)
{

    int i;

    //faB: GlidePatch_t are always purgeable
    if (rendermode==render_soft)
    {
    // unload the numbers, tall and short
    for (i=0;i<10;i++)
    {
        Z_ChangeTag(tallnum[i], PU_CACHE);
        Z_ChangeTag(shortnum[i], PU_CACHE);
    }
    // unload tall percent
    Z_ChangeTag(tallpercent, PU_CACHE);

    // unload arms background
    Z_ChangeTag(armsbg, PU_CACHE);

    // unload gray #'s
    for (i=0;i<6;i++)
        Z_ChangeTag(arms[i][0], PU_CACHE);

    // unload the key cards
    for (i=0;i<NUMCARDS;i++)
        Z_ChangeTag(keys[i], PU_CACHE);

    Z_ChangeTag(sbar, PU_CACHE);
    }

    ST_unloadFaceGraphics ();

    // Note: nobody ain't seen no unloading
    //   of stminus yet. Dude.

}

// made separate so that skins code can reload custom face graphics
void ST_unloadFaceGraphics (void)
{
    int    i;

    //faB: GlidePatch_t are always purgeable
    if (rendermode==render_soft)
    {
    for (i=0;i<ST_NUMFACES;i++)
        Z_ChangeTag(faces[i], PU_CACHE);

    // face background
    Z_ChangeTag(faceback, PU_CACHE);
    }
}


void ST_unloadData(void)
{
    ST_unloadGraphics();
}

void ST_initData(void)
{

    int         i;

    st_firsttime = true;

    //added:16-01-98:'link' the statusbar display to a player, which could be
    //               another player than consoleplayer, for example, when you
    //               change the view in a multiplayer demo with F12.
    if (singledemo)
        statusbarplayer = displayplayer;
    else
        statusbarplayer = consoleplayer;

    plyr = &players[statusbarplayer];

    st_clock = 0;
    st_chatstate = StartChatState;

    st_statusbaron = true;
    st_oldchat = st_chat = false;
    st_cursoron = false;

    st_faceindex = 0;
    st_palette = -1;

    st_oldhealth = -1;

    for (i=0;i<NUMWEAPONS;i++)
        oldweaponsowned[i] = plyr->weaponowned[i];

    for (i=0;i<3;i++)
        keyboxes[i] = -1;

    STlib_init();

}


//added:30-01-98: NOTE: this is called at any level start, view change,
//                      and after vid mode change.
void ST_createWidgets(void)
{
    int i;
    int st_x;
    
    if (rendermode == render_soft)
        st_x = ((vid.width-ST_WIDTH)>>1);
    else
        st_x = 0;   //currently we scale the statusbar

    // ready weapon ammo
    STlib_initNum(&w_ready,
                  st_x + ST_AMMOX,
                  ST_AMMOY,
                  tallnum,
                  &plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
                  &st_statusbaron,
                  ST_AMMOWIDTH );

    // the last weapon type
    w_ready.data = plyr->readyweapon;

    // health percentage
    STlib_initPercent(&w_health,
                      st_x + ST_HEALTHX,
                      ST_HEALTHY,
                      tallnum,
                      &plyr->health,
                      &st_statusbaron,
                      tallpercent);

    // arms background
    STlib_initBinIcon(&w_armsbg,
                      st_x + ST_ARMSBGX,
                      ST_ARMSBGY,
                      armsbg,
                      &st_notdeathmatch,
                      &st_statusbaron);

    // weapons owned
    for(i=0;i<6;i++)
    {
        STlib_initMultIcon(&w_arms[i],
                           st_x + ST_ARMSX+(i%3)*ST_ARMSXSPACE,
                           ST_ARMSY+(i/3)*ST_ARMSYSPACE,
                           arms[i], (int *) &plyr->weaponowned[i+1],
                           &st_armson);
    }

    // frags sum
    STlib_initNum(&w_frags,
                  st_x + ST_FRAGSX,
                  ST_FRAGSY,
                  tallnum,
                  &st_fragscount,
                  &st_fragson,
                  ST_FRAGSWIDTH);

    // faces
    STlib_initMultIcon(&w_faces,
                       st_x + ST_FACESX,
                       ST_FACESY,
                       faces,
                       &st_faceindex,
                       &st_statusbaron);

    // armor percentage - should be colored later
    STlib_initPercent(&w_armor,
                      st_x + ST_ARMORX,
                      ST_ARMORY,
                      tallnum,
                      &plyr->armorpoints,
                      &st_statusbaron, tallpercent);

    // keyboxes 0-2
    STlib_initMultIcon(&w_keyboxes[0],
                       st_x + ST_KEY0X,
                       ST_KEY0Y,
                       keys,
                       &keyboxes[0],
                       &st_statusbaron);

    STlib_initMultIcon(&w_keyboxes[1],
                       st_x + ST_KEY1X,
                       ST_KEY1Y,
                       keys,
                       &keyboxes[1],
                       &st_statusbaron);

    STlib_initMultIcon(&w_keyboxes[2],
                       st_x + ST_KEY2X,
                       ST_KEY2Y,
                       keys,
                       &keyboxes[2],
                       &st_statusbaron);

    // ammo count (all four kinds)
    STlib_initNum(&w_ammo[0],
                  st_x + ST_AMMO0X,
                  ST_AMMO0Y,
                  shortnum,
                  &plyr->ammo[0],
                  &st_statusbaron,
                  ST_AMMO0WIDTH);

    STlib_initNum(&w_ammo[1],
                  st_x + ST_AMMO1X,
                  ST_AMMO1Y,
                  shortnum,
                  &plyr->ammo[1],
                  &st_statusbaron,
                  ST_AMMO1WIDTH);

    STlib_initNum(&w_ammo[2],
                  st_x + ST_AMMO2X,
                  ST_AMMO2Y,
                  shortnum,
                  &plyr->ammo[2],
                  &st_statusbaron,
                  ST_AMMO2WIDTH);

    STlib_initNum(&w_ammo[3],
                  st_x + ST_AMMO3X,
                  ST_AMMO3Y,
                  shortnum,
                  &plyr->ammo[3],
                  &st_statusbaron,
                  ST_AMMO3WIDTH);

    // max ammo count (all four kinds)
    STlib_initNum(&w_maxammo[0],
                  st_x + ST_MAXAMMO0X,
                  ST_MAXAMMO0Y,
                  shortnum,
                  &plyr->maxammo[0],
                  &st_statusbaron,
                  ST_MAXAMMO0WIDTH);

    STlib_initNum(&w_maxammo[1],
                  st_x + ST_MAXAMMO1X,
                  ST_MAXAMMO1Y,
                  shortnum,
                  &plyr->maxammo[1],
                  &st_statusbaron,
                  ST_MAXAMMO1WIDTH);

    STlib_initNum(&w_maxammo[2],
                  st_x + ST_MAXAMMO2X,
                  ST_MAXAMMO2Y,
                  shortnum,
                  &plyr->maxammo[2],
                  &st_statusbaron,
                  ST_MAXAMMO2WIDTH);

    STlib_initNum(&w_maxammo[3],
                  st_x + ST_MAXAMMO3X,
                  ST_MAXAMMO3Y,
                  shortnum,
                  &plyr->maxammo[3],
                  &st_statusbaron,
                  ST_MAXAMMO3WIDTH);

}


static boolean  st_stopped = true;


void ST_Start (void)
{
    if (!st_stopped)
        ST_Stop();

    ST_initData();
    ST_createWidgets();
    st_stopped = false;
    st_recalc = false;  //added:02-02-98: widgets coords have been setup
                        // see ST_drawer()
}

void ST_Stop (void)
{
    if (st_stopped)
        return;

    I_SetPalette (W_CacheLumpNum (lu_palette, PU_CACHE));

    st_stopped = true;
}

//
//  Initializes the status bar,
//  sets the defaults border patch for the window borders.
//

//faB: used by Glide mode, holds lumpnum of flat used to fill space around the viewwindow
int  st_borderpatchnum;

void ST_Init (void)
{
    int     i;

    veryfirsttime = 0;
    ST_loadData();

    //added:26-01-98:screens[4] is allocated at videomode setup, and
    //               set at V_Init(), the first time being at SCR_Recalc()

    // choose and cache the default border patch
    if (gamemode == commercial)
        // DOOM II border patch, original was GRNROCK
        st_borderpatchnum = W_GetNumForName ("FLOOR0_3"); // Tails 10-26-99
    else
        // DOOM border patch.
        st_borderpatchnum = W_GetNumForName ("FLOOR0_3"); // Tails 10-26-99

    scr_borderpatch = W_CacheLumpNum (st_borderpatchnum, PU_STATIC);

    //
    // cache the status bar overlay icons  (fullscreen mode)
    //
    sbohealth = W_CacheLumpName ("SBOHEALT", PU_STATIC);
    sbofrags  = W_CacheLumpName ("SBOFRAGS", PU_STATIC);
    sboarmor  = W_CacheLumpName ("SBOARMOR", PU_STATIC);

    for (i=0;i<NUMWEAPONS;i++)
    {
        if (i>0 && i!=7)
            sboammo[i] = W_CacheLumpName (va("SBOAMMO%c",'0'+i), PU_STATIC);
        else
            sboammo[i] = NULL;
    }
}

 //added:16-01-98: change the status bar too, when pressing F12 while viewing
//                 a demo.
void ST_changeDemoView (void)
{
    //the same routine is called at multiplayer deathmatch spawn
    // so it can be called multiple times
    ST_Start();
}


// =========================================================================
//                         STATUS BAR OVERLAY
// =========================================================================

consvar_t cv_stbaroverlay = {"overlay","kahmf",CV_SAVE,NULL};

boolean   st_overlay;


void ST_AddCommands (void)
{
    CV_RegisterVar (&cv_stbaroverlay);
}


//  Draw a number, scaled, over the view
//  Always draw the number completely since it's overlay
//
extern patch_t*         sttminus;       // from ST_lib

void ST_drawOverlayNum (int       x,            // right border!
                        int       y,
                        int       num,
                        patch_t** numpat,
                        patch_t*  percent )
{
    int       w = SHORT(numpat[0]->width);
    boolean   neg;

    // in the special case of 0, you draw 0
    if (!num)
    {
        V_DrawScaledPatch(x - (w*vid.dupx), y, FG|V_NOSCALESTART, numpat[ 0 ]);
        return;
    }

    neg = num < 0;

    if (neg)
        num = -num;

    // draw the number
    while (num)
    {
        x -= (w * vid.dupx);
        V_DrawScaledPatch(x, y, FG|V_NOSCALESTART, numpat[ num % 10 ]);
        num /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        V_DrawScaledPatch(x - (8*vid.dupx), y, FG|V_NOSCALESTART, sttminus);
}


static inline int SCY( int y )
{ 
    // do not scale to resolution for hardware accelerated
    // because these modes always scale by default
    if ( rendermode == render_soft )
        y = ( y * vid.height ) / BASEVIDHEIGHT;     // scale to resolution

    if ( cv_splitscreen.value )
        y >>= 1;

    // splitscreen view bottom of screen
    if ( cv_splitscreen.value &&
         plyr != &players[statusbarplayer] )
        y += vid.height / 2;     

    return y;
    /*
    if ( !cv_splitscreen.value )
        return ( y * vid.height / BASEVIDHEIGHT );
    else {
        if ( plyr == &players[statusbarplayer] ) 
            return ( y * vid.height / (BASEVIDHEIGHT*2) );
        else
            return ( vid.height / 2 + y * vid.height / (BASEVIDHEIGHT*2) );
    }*/
}


static inline int SCX( int x )
{
    // hardware accelerated modes scale automatically
    if ( rendermode == render_soft )
        return ( ( x * vid.width ) / BASEVIDWIDTH );
    else
        return x;
}


//  Draw the status bar overlay, customisable : the user choose which
//  kind of information to overlay
//
void ST_overlayDrawer (void)
{
    //CONS_Printf("overlay draw %d\n",gametic);
    char*  cmds;
    char   c;
    int    i;
    pic_t* pic;

    cmds = cv_stbaroverlay.string;

    while ((c=*cmds++))
    {
       if (c>='A' && c<='Z')
           c = c + 'a' - 'A';
       switch (c)
       {
         case 'h': // draw health

          if(plyr->health>0) //Added to prevent -1 rings 12-25-99 Stealth
          {
           ST_drawOverlayNum(SCX(112), // was 50 Tails 10-31-99
//                             SCY(198)-(16*vid.dupy),
                             SCY(56)-(16*vid.dupy), // Ring score location Tails 10-31-99
                             plyr->health-1,        // Always have 1 ring when not dead fixed: Stealth 12-25-99
                             tallnum,NULL);
          }

          else  //Added to prevent -1 rings 12-25-99 Stealth
          {
           ST_drawOverlayNum(SCX(112), // was 50 Tails 10-31-99
//                             SCY(198)-(16*vid.dupy),
                             SCY(56)-(16*vid.dupy), // Ring score location Tails 10-31-99
                             plyr->health, 
                             tallnum,NULL);
          }
           if (rendermode==render_soft)
               V_DrawScaledPatch (SCX(16),SCY(42), FG | V_NOSCALESTART,sbohealth); // Was a number I forget and 198 =) Tails 10-31-99
           break;

         case 'f': // draw frags
//           st_fragscount = ST_PlayerFrags(plyr-players); // not using frags Tails 03-01-2000

//           if (cv_deathmatch.value) // don't check for deathmatch Tails 03-01-2000
           {
               ST_drawOverlayNum(SCX(128), // Score Tails 03-01-2000
                                 SCY(9), // Location Tails 03-01-2000
                                 plyr->score,
                                 tallnum,NULL);

               if (rendermode==render_soft)
                   V_DrawScaledPatch (SCX(16),SCY(10), FG | V_NOSCALESTART,sbofrags); // Draw SCORE Tails 03-01-2000
           }
           break;

         case 'a': // draw ammo
           pic = sboammo[plyr->readyweapon];
           if (pic)
           {
               ST_drawOverlayNum(SCX(234),
                                 SCY(198)-(16*vid.dupy),
                                 plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
                                 tallnum,NULL);

               if (rendermode==render_soft)
                   V_DrawScalePic (SCX(236),SCY(198)-(pic->height*vid.dupy),0,pic);
           }
           break;

         case 'k': // draw keys
           c=1;
           for (i=0;i<3;i++)
                if( plyr->cards[i+3] ) // first skull then card
                    V_DrawScaledPatch(SCX(318)-(c++)*(ST_KEY0WIDTH*vid.dupx), SCY(198)-((16+8)*vid.dupy), FG | V_NOSCALESTART, keys[i+3]);
                else
                if( plyr->cards[i] )
                    V_DrawScaledPatch(SCX(318)-(c++)*(ST_KEY0WIDTH*vid.dupx), SCY(198)-((16+8)*vid.dupy), FG | V_NOSCALESTART, keys[i]);
           break;

         case 'm': // draw armor

           ST_drawOverlayNum(SCX(112), // Tails 02-29-2000
                             SCY(40)-(16*vid.dupy), // Draw the current time Tails 02-29-2000
                             plyr->armorpoints,
                             tallnum,NULL);

           if (rendermode==render_soft)
               V_DrawScaledPatch (SCX(17),SCY(26), FG | V_NOSCALESTART,sboarmor); // TIME location Tails 02-29-2000
           break;
       }
    }
}
