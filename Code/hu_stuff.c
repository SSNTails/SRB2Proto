
// hu_stuff.c : heads up displays, cleaned up (hasta la vista hu_lib)
//              because a lot of code was unnecessary now

#include "doomdef.h"
#include "hu_stuff.h"

#include "d_netcmd.h"
#include "d_clisrv.h"

#include "g_game.h"
#include "g_input.h"

#include "i_video.h"

// Data.
#include "dstrings.h"
#include "st_stuff.h"   //added:05-02-98: ST_HEIGHT
#include "r_local.h"
#include "wi_stuff.h"  // for drawranckings

#include "keys.h"
#include "v_video.h"

#include "w_wad.h"
#include "z_zone.h"

// coords are scaled
#define HU_INPUTX       0
#define HU_INPUTY       0

// DOOM shareware/registered/retail (Ultimate) names.
#define HU_TITLE        (text[HUSTR_E1M1_NUM+(gameepisode-1)*9+gamemap-1])
// DOOM 2 map names.
#define HU_TITLE2       (text[HUSTR_1_NUM+gamemap-1])
// Plutonia WAD map names.
#define HU_TITLEP       (text[PHUSTR_1_NUM+gamemap-1])
// TNT WAD map names.
#define HU_TITLET       (text[THUSTR_1_NUM+gamemap-1])

#define HU_TITLEX       0
#define HU_TITLEY       ((BASEVIDHEIGHT-ST_HEIGHT-1) - SHORT(hu_font[0]->height))


//-------------------------------------------
//              heads up font
//-------------------------------------------
patch_t*                hu_font[HU_FONTSIZE];


static player_t*        plr;
boolean                 chat_on;

static char             w_title[80];            //mapname
static char             w_inputbuffer[MAXPLAYERS][80];
static char             w_chat[HU_MAXMSGLEN];

static boolean          headsupactive = false;

boolean                 hu_showscores;        // draw deathmatch rankings

static char             hu_tick;

//-------------------------------------------
//              misc vars
//-------------------------------------------

consvar_t*   chat_macros[10];

extern byte*       whitemap;  // color translation table for red->white font
extern boolean     automapactive;


//added:16-02-98: crosshair 0=off, 1=cross, 2=angle, 3=point, see m_menu.c
patch_t*           crosshair[3];     //3 precached crosshair graphics


// -------
// protos.
// -------
void   HU_drawDeathmatchRankings (void);
void   HU_drawCrosshair (void);


//======================================================================
//                 KEYBOARD LAYOUTS FOR ENTERING TEXT
//======================================================================

char*     shiftxform;

char french_shiftxform[] =
{
    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(', ')', '*', '+',
    '?', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    '0', // shift-0
    '1', // shift-1
    '2', // shift-2
    '3', // shift-3
    '4', // shift-4
    '5', // shift-5
    '6', // shift-6
    '7', // shift-7
    '8', // shift-8
    '9', // shift-9
    '/',
    '.', // shift-;
    '<',
    '+', // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"', '_',
    '\'', // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127

};

char english_shiftxform[] =
{

    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(', ')', '*', '+',
    '<', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    ')', // shift-0
    '!', // shift-1
    '@', // shift-2
    '#', // shift-3
    '$', // shift-4
    '%', // shift-5
    '^', // shift-6
    '&', // shift-7
    '*', // shift-8
    '(', // shift-9
    ':',
    ':', // shift-;
    '<',
    '+', // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"', '_',
    '\'', // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};


char frenchKeyMap[128]=
{
    0,
    1,2,3,4,5,6,7,8,9,10,
    11,12,13,14,15,16,17,18,19,20,
    21,22,23,24,25,26,27,28,29,30,
    31,
    ' ','!','"','#','$','%','&','%','(',')','*','+',';','-',':','!',
    '0','1','2','3','4','5','6','7','8','9',':','M','<','=','>','?',
    '@','Q','B','C','D','E','F','G','H','I','J','K','L',',','N','O',
    'P','A','R','S','T','U','V','Z','X','Y','W','^','\\','$','^','_',
    '@','Q','B','C','D','E','F','G','H','I','J','K','L',',','N','O',
    'P','A','R','S','T','U','V','Z','X','Y','W','^','\\','$','^',127
};


char ForeignTranslation(unsigned char ch)
{
    return ch < 128 ? frenchKeyMap[ch] : ch;
}


//======================================================================
//                          HEADS UP INIT
//======================================================================

// just after
void Command_Say_f (void);
void Command_Sayto_f (void);
void Command_Sayteam_f (void);
void Got_Saycmd(char **p,int playernum);

// Initialise Heads up
// once at game startup.
//
void HU_Init(void)
{

    int         i;
    int         j;
    char        buffer[9];

    COM_AddCommand ("say"    , Command_Say_f);
    COM_AddCommand ("sayto"  , Command_Sayto_f);
    COM_AddCommand ("sayteam", Command_Sayteam_f);
    RegisterNetXCmd(XD_SAY,Got_Saycmd);

    // set shift translation table
    if (language==french)
        shiftxform = french_shiftxform;
    else
        shiftxform = english_shiftxform;

    // cache the heads-up font for entire game execution
    j = HU_FONTSTART;
    for (i=0;i<HU_FONTSIZE;i++)
    {
        sprintf(buffer, "STCFN%.3d", j++);
        hu_font[i] = (patch_t *) W_CachePatchName(buffer, PU_STATIC);
    }

    //hu_showscores = false;    //added:05-02-98:deathmatch rankings overlay off

    // cache the crosshairs, dont bother to know which one is being used,
    // just cache them 3 all, they're so small anyway.
    for(i=0;i<HU_CROSSHAIRS;i++)
    {
       sprintf(buffer, "CROSHAI%c", '1'+i);
       crosshair[i] = (patch_t *) W_CachePatchName(buffer, PU_STATIC);
    }
}


void HU_Stop(void)
{
    headsupactive = false;
}


// Reset Heads up when consoleplayer spawns
//
//
void HU_Start(void)
{

    int         i;
    char*       s;

    if (headsupactive)
        HU_Stop();

    plr = &players[consoleplayer];
    chat_on = false;

    switch ( gamemode )
    {
      case shareware:
      case registered:
      case retail:
        s = HU_TITLE;
        break;
  // FIXME
  //  case pack_plut:
  //    s = HU_TITLEP;
  //    break;
  //  case pack_tnt:
  //    s = HU_TITLET;
  //    break;
      case commercial:
      default:
         s = HU_TITLE2;
         break;
    }
    strcpy(w_title,s);

    // empty the inputbuffers
    for (i=0 ; i<MAXPLAYERS ; i++)
        w_inputbuffer[i][0] = '\0';

    headsupactive = true;

}



//======================================================================
//                            EXECUTION
//======================================================================

void TeamPlay_OnChange(void)
{
    int i;
    char s[50];

    // Change the name of the teams

    if(cv_teamplay.value==1)
    {
        // color
        for(i=0;i<MAXSKINCOLORS;i++)
        {
            sprintf(s,"%s team",Color_Names[i]);
            strcpy(team_names[i],s);
        }
    }
    else
    if(cv_teamplay.value==2)
    {
        // skins

        for(i=0;i<numskins;i++)
        {
            sprintf(s,"%s team",skins[i].name);
            strcpy(team_names[i],s);
        }
    }
}

void Command_Say_f (void)
{
    char buf[255];
    int i,j;

    if((j=COM_Argc())<2)
    {
        CONS_Printf ("say <message> : send a message\n");
        return;
    }

    buf[0]=0;
    strcpy(&buf[1],COM_Argv(1));
    for(i=2;i<j;i++)
    {
        strcat(&buf[1]," ");
        strcat(&buf[1],COM_Argv(i));
    }
    SendNetXCmd(XD_SAY,buf,strlen(buf+1)+2); // +2 because 1 for buf[0] and the other for null terminated string
}

void Command_Sayto_f (void)
{
    char buf[255];
    int i,j;

    if((j=COM_Argc())<3)
    {
        CONS_Printf ("sayto <playername|playernum> <message> : send a message to a player\n");
        return;
    }

    buf[0]=nametonum(COM_Argv(1));
    if(buf[0]==-1)
        return;
    strcpy(&buf[1],COM_Argv(2));
    for(i=3;i<j;i++)
    {
        strcat(&buf[1]," ");
        strcat(&buf[1],COM_Argv(i));
    }
    SendNetXCmd(XD_SAY,buf,strlen(buf+1)+2);
}

void Command_Sayteam_f (void)
{
    char buf[255];
    int i,j;

    if((j=COM_Argc())<2)
    {
        CONS_Printf ("sayteam <message> : send a message to your team\n");
        return;
    }

    buf[0]=-consoleplayer;
    strcpy(&buf[1],COM_Argv(1));
    for(i=2;i<j;i++)
    {
        strcat(&buf[1]," ");
        strcat(&buf[1],COM_Argv(i));
    }
    SendNetXCmd(XD_SAY,buf,strlen(buf+1)+2); // +2 because 1 for buf[0] and the other for null terminated string
}

// netsyntax : to : byte  1->32  player 1 to 32
//                        0      all
//                       -1->-32 say team -numplayer of the sender

void Got_Saycmd(char **p,int playernum)
{
    char to;
    to=*(*p)++;

    if(to==0 || to==consoleplayer || consoleplayer==playernum
       || (to<0 && ST_SameTeam(&players[consoleplayer],&players[-to])) )
         CONS_Printf("\3%s: %s\n", player_names[playernum], *p);

    *p+=strlen(*p)+1;
}

//  Handles key input and string input
//
boolean HU_keyInChatString (char *s, char ch)
{
    int         l;

    if (ch >= ' ' && ch <= '_')
    {
        l = strlen(s);
        if (l<HU_MAXMSGLEN-1)
        {
            s[l++]=ch;
            s[l]=0;
            return true;
        }
        return false;
    }
    else
        if (ch == KEY_BACKSPACE)
        {
            l = strlen(s);
            if (l)
                s[--l]=0;
            else
                return false;
        }
        else
            if (ch != KEY_ENTER)
                return false; // did not eat key

    return true; // ate the key
}


//
//
extern boolean playerdeadview; //hack from P_DeathThink()
void HU_Ticker(void)
{
    player_t    *pl;

    hu_tick++;
    hu_tick &= 7;        //currently only to blink chat input cursor

    // display message if necessary
    // (display the viewplayer's messages)
    pl = &players[displayplayer];

    if (cv_showmessages.value && pl->message)
    {
        CONS_Printf ("%s\n",pl->message);
        pl->message = 0;
    }

    //deathmatch rankings overlay if press key or while in death view
    if (cv_deathmatch.value)
    {
        if (gamekeydown[gamecontrol[gc_scores][0]] ||
            gamekeydown[gamecontrol[gc_scores][1]] )
            hu_showscores = !chat_on;
        else
            hu_showscores = playerdeadview; //hack from P_DeathThink()
    }

}




#define QUEUESIZE               128

static char     chatchars[QUEUESIZE];
static int      head = 0;
static int      tail = 0;

//
//
char HU_dequeueChatChar(void)
{
    char c;

    if (head != tail)
    {
        c = chatchars[tail];
        tail = (tail + 1) & (QUEUESIZE-1);
    }
    else
    {
        c = 0;
    }

    return c;
}

//
//
void HU_queueChatChar(char c)
{
    if (((head + 1) & (QUEUESIZE-1)) == tail)
    {
        plr->message = HUSTR_MSGU;      //message not send
    }
    else
    {
        if (c == KEY_BACKSPACE)
        {
            if(tail!=head)
                head = (head - 1) & (QUEUESIZE-1);
        }
        else
        {
            chatchars[head] = c;
            head = (head + 1) & (QUEUESIZE-1);
        }
    }

    // send automaticly the message (no more chat char)
    if(c==KEY_ENTER)
    {
        char buf[255],c;
        int i=0;

        do {
            c=HU_dequeueChatChar();
            buf[i++]=c;
        } while(c);
        if(i>3)
            COM_BufInsertText (va("say %s",buf));
    }
}

extern int     con_keymap;

//
//  Returns true if key eaten
//
boolean HU_Responder (event_t *ev)
{
static boolean        shiftdown = false;
static boolean        altdown   = false;

    boolean             eatkey = false;
    char*               macromessage;
    unsigned char       c;


    if (ev->data1 == KEY_SHIFT)
    {
        shiftdown = (ev->type == ev_keydown);
        return false;
    }
    else if (ev->data1 == KEY_ALT)
    {
        altdown = (ev->type == ev_keydown);
        return false;
    }

    if (ev->type != ev_keydown)
        return false;

   // only KeyDown events now...

    if (!chat_on)
    {
       // enter chat mode
        if (ev->data1==gamecontrol[gc_talkkey][0]
         || ev->data1==gamecontrol[gc_talkkey][1])
        {
            eatkey = chat_on = true;
            w_chat[0] = 0;
            HU_queueChatChar(HU_BROADCAST);
        }
    }
    else
    {
        c = ev->data1;

        // use console translations
        if (con_keymap==french)
            c = ForeignTranslation(c);   
        if (shiftdown)
            c = shiftxform[c];

        // send a macro
        if (altdown)
        {
            c = c - '0';
            if (c > 9)
                return false;

            macromessage = chat_macros[c]->string;

            // kill last message with a '\n'
            HU_queueChatChar(KEY_ENTER); // DEBUG!!!

            // send the macro message
            while (*macromessage)
                HU_queueChatChar(*macromessage++);
            HU_queueChatChar(KEY_ENTER);

            // leave chat mode and notify that it was sent
            chat_on = false;
            eatkey = true;
        }
        else
        {
            if (language==french)
                c = ForeignTranslation(c);
            if (shiftdown || (c >= 'a' && c <= 'z'))
                c = shiftxform[c];
            eatkey = HU_keyInChatString(w_chat,c);
            if (eatkey)
            {
                // static unsigned char buf[20]; // DEBUG
                HU_queueChatChar(c);

                // sprintf(buf, "KEY: %d => %d", ev->data1, c);
                //      plr->message = buf;
            }
            if (c == KEY_ENTER)
            {
                chat_on = false;
            }
            else if (c == KEY_ESCAPE)
                chat_on = false;
        }
    }

    return eatkey;
}


//======================================================================
//                         HEADS UP DRAWING
//======================================================================

//  Draw chat input
//
static void HU_DrawChat (void)
{
    int  i,c,y;

    c=0;
    i=0;
    y=HU_INPUTY;
    while (w_chat[i])
    {
        V_DrawCharacter( HU_INPUTX + (c<<3), y, w_chat[i++]);

        c++;
        if (c>=(vid.width>>3))
        {
            c = 0;
            y+=8;
        }

    }

    if (hu_tick<4)
        V_DrawMappedPatch (HU_INPUTX + (c<<3),
                           y,0,
                           hu_font['_'-HU_FONTSTART],
                           whitemap);
}


//  Heads up displays drawer, call each frame
//
void HU_Drawer(void)
{
    // draw chat string plus cursor
    if (chat_on)
        HU_DrawChat ();

    // mapname
    if (automapactive)
        V_DrawString(HU_TITLEX,HU_TITLEY,w_title);

    // draw deathmatch rankings
    if (hu_showscores)
        HU_drawDeathmatchRankings ();

    // draw the crosshair, not when viewing demos
    if (viewactive && cv_crosshair.value && !demoplayback)
        HU_drawCrosshair ();
}


//======================================================================
//                 HUD MESSAGES CLEARING FROM SCREEN
//======================================================================

//  Clear old messages from the borders around the view window
//  (only for reduced view, refresh the borders when needed)
//
//  startline  : y coord to start clear,
//  clearlines : how many lines to clear.
//
extern int     con_clearlines;  // lines of top of screen to refresh
extern boolean con_hudupdate;   // hud messages have changed, need refresh
static int     oldclearlines;

void HU_Erase (void)
{
    int topline;
    int bottomline;
    int y,yoffset;

    //faB: clear hud msgs on double buffer (Glide mode)
    boolean secondframe;
    static  int     secondframelines;

//    int x;      //DEBUG
//static int col;

    if (con_clearlines==oldclearlines && !con_hudupdate)
        return;

    // clear the other frame in double-buffer modes
    secondframe = (con_clearlines!=oldclearlines);
    if (secondframe)
        secondframelines = oldclearlines;
    
    // clear the message lines that go away, so use _oldclearlines_
    bottomline = oldclearlines;
    oldclearlines = con_clearlines;

    if (automapactive || viewwindowx==0)   // hud msgs don't need to be cleared
        return;

    // software mode copies view border pattern & beveled edges from the backbuffer
    if (rendermode==render_soft)
    {
        topline = 0;
    for (y=topline,yoffset=y*vid.width; y<bottomline ; y++,yoffset+=vid.width)
    {
        if (y < viewwindowy || y >= viewwindowy + viewheight)
            R_VideoErase(yoffset, vid.width); // erase entire line
        else
        {
            R_VideoErase(yoffset, viewwindowx); // erase left border
                                                // erase right border
            R_VideoErase(yoffset + viewwindowx + viewwidth, viewwindowx);
        }
    }
        con_hudupdate = false;      // if it was set..
    }

//DEBUG
//    for (x=0;x<vid.width;x++)
//    {
//         screens[0][yoffset+x] = col;
//         screens[0][vid.height*vid.width-vid.width+x] = col;
//    }
//    col++;

}



//======================================================================
//                   IN-LEVEL DEATHMATCH RANKINGS
//======================================================================

// count frags for each team
int HU_CreateTeamFragTbl(fragsort_t *fragtab,int dmtotals[],int fragtbl[MAXPLAYERS][MAXPLAYERS])
{
    int i,j,k,scorelines,team;

    scorelines = 0;
    for (i=0; i<MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            if(cv_teamplay.value==1)
                team=players[i].skincolor;
            else
                team=players[i].skin;

            for(j=0;j<scorelines;j++)
                if (fragtab[j].num == team)
                { // found there team
                     if(fragtbl)
                     {
                         for(k=0;k<MAXPLAYERS;k++)
                             if(playeringame[k])
                                 if(cv_teamplay.value==1)
                                     fragtbl[team][players[k].skincolor] +=
                                                        players[i].frags[k];
                                 else
                                     fragtbl[team][players[k].skin] +=
                                                        players[i].frags[k];
                     }

                     fragtab[j].count += ST_PlayerFrags(i);
                     if(dmtotals)
                         dmtotals[team]=fragtab[j].count;
                     break;
                }
            if (j==scorelines)
            {   // team not found add it

                if(fragtbl)
                    for(k=0;k<MAXPLAYERS;k++)
                        fragtbl[team][k] = 0;

                fragtab[scorelines].count = ST_PlayerFrags(i);
                fragtab[scorelines].num   = team;
                fragtab[scorelines].color = players[i].skincolor;
                fragtab[scorelines].name  = team_names[team];

                if(fragtbl)
                {
                    for(k=0;k<MAXPLAYERS;k++)
                        if(playeringame[k])
                            if(cv_teamplay.value==1)
                                fragtbl[team][players[k].skincolor] +=
                                                   players[i].frags[k];
                            else
                                fragtbl[team][players[k].skin] +=
                                                   players[i].frags[k];
                }

                if(dmtotals)
                    dmtotals[team]=fragtab[scorelines].count;

                scorelines++;
            }
        }
    }
    return scorelines;
}


//
//  draw Deathmatch Rankings
//
void HU_drawDeathmatchRankings (void)
{
    patch_t*     p;
    fragsort_t   fragtab[MAXPLAYERS];
    int          i;
    int          scorelines;
    int          whiteplayer;

    // draw the ranking title panel
    p = W_CachePatchName("RANKINGS",PU_CACHE);
    V_DrawScaledPatch ((BASEVIDWIDTH-p->width)/2,5,0,p);

    // count frags for each present player
    scorelines = 0;
    for (i=0; i<MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            fragtab[scorelines].count = ST_PlayerFrags(i);
            fragtab[scorelines].num   = i;
            fragtab[scorelines].color = players[i].skincolor;
            fragtab[scorelines].name  = player_names[i];
            scorelines++;
        }
    }

    //Fab:25-04-98: when you play, you quickly see your frags because your
    //  name is displayed white, when playback demo, you quicly see who's the
    //  view.
    whiteplayer = demoplayback ? displayplayer : consoleplayer;

    if (scorelines>9)
        scorelines = 9; //dont draw past bottom of screen, show the best only

    if(cv_teamplay.value==0)
        WI_drawRancking(NULL,80,70,fragtab,scorelines,true,whiteplayer);
    else
    {
        // draw the frag to the right
//        WI_drawRancking("Individual",170,70,fragtab,scorelines,true,whiteplayer);

        scorelines = HU_CreateTeamFragTbl(fragtab,NULL,NULL);

        // and the team frag to the left
        WI_drawRancking("Teams",80,70,fragtab,scorelines,true,players[whiteplayer].skincolor);
    }
}


// draw the Crosshair, at the exact center of the view.
//
// Crosshairs are pre-cached at HU_Init
#ifdef __WIN32__
    extern float gr_basewindowcentery;
    extern float gr_viewheight;
#endif
void HU_drawCrosshair (void)
{

}


//======================================================================
//                    CHAT MACROS COMMAND & VARS
//======================================================================

// better do HackChatmacros() because the strings are NULL !!

consvar_t cv_chatmacro1 = {"_chatmacro1", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro2 = {"_chatmacro2", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro3 = {"_chatmacro3", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro4 = {"_chatmacro4", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro5 = {"_chatmacro5", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro6 = {"_chatmacro6", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro7 = {"_chatmacro7", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro8 = {"_chatmacro8", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro9 = {"_chatmacro9", NULL, CV_SAVE,NULL};
consvar_t cv_chatmacro0 = {"_chatmacro0", NULL, CV_SAVE,NULL};


// set the chatmacros original text, before config is executed
// if a dehacked patch was loaded, it will set the hacked texts,
// but the config.cfg will override it.
//
void HU_HackChatmacros (void)
{
    int    i;

    // this is either the original text, or dehacked ones
    cv_chatmacro0.string = HUSTR_CHATMACRO0;
    cv_chatmacro1.string = HUSTR_CHATMACRO1;
    cv_chatmacro2.string = HUSTR_CHATMACRO2;
    cv_chatmacro3.string = HUSTR_CHATMACRO3;
    cv_chatmacro4.string = HUSTR_CHATMACRO4;
    cv_chatmacro5.string = HUSTR_CHATMACRO5;
    cv_chatmacro6.string = HUSTR_CHATMACRO6;
    cv_chatmacro7.string = HUSTR_CHATMACRO7;
    cv_chatmacro8.string = HUSTR_CHATMACRO8;
    cv_chatmacro9.string = HUSTR_CHATMACRO9;

    // link chatmacros to cvars
    chat_macros[0] = &cv_chatmacro0;
    chat_macros[1] = &cv_chatmacro1;
    chat_macros[2] = &cv_chatmacro2;
    chat_macros[3] = &cv_chatmacro3;
    chat_macros[4] = &cv_chatmacro4;
    chat_macros[5] = &cv_chatmacro5;
    chat_macros[6] = &cv_chatmacro6;
    chat_macros[7] = &cv_chatmacro7;
    chat_macros[8] = &cv_chatmacro8;
    chat_macros[9] = &cv_chatmacro9;

    // register chatmacro vars ready for config.cfg
    for (i=0;i<10;i++)
       CV_RegisterVar (chat_macros[i]);
}


//  chatmacro <0-9> "chat message"
//
void Command_Chatmacro_f (void)
{
    int    i;

    if (COM_Argc()<2)
    {
        CONS_Printf ("chatmacro <0-9> : view chatmacro\n"
                     "chatmacro <0-9> \"chat message\" : change chatmacro\n");
        return;
    }

    i = atoi(COM_Argv(1)) % 10;

    if (COM_Argc()==2)
    {
        CONS_Printf("chatmacro %d is \"%s\"\n",i,chat_macros[i]->string);
        return;
    }

    // change a chatmacro
    CV_Set (chat_macros[i], COM_Argv(2));
}
