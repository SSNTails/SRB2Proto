
// hu_stuff.h : heads up displays
//

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__


#include "d_event.h"
#include "w_wad.h"
#include "wi_stuff.h"
#include "r_defs.h"

//------------------------------------
//           heads up font
//------------------------------------
#define HU_FONTSTART    '!'     // the first font characters
#define HU_FONTEND      '_'     // the last font characters

#define HU_FONTSIZE     (HU_FONTEND - HU_FONTSTART + 1)


#define HU_CROSSHAIRS   3       // maximum 9 see HU_Init();


//------------------------------------
//           chat stuff
//------------------------------------
#define HU_BROADCAST    5       // first char in chat message

#define HU_MAXMSGLEN    80

extern patch_t*       hu_font[HU_FONTSIZE];

// P_DeathThink set this true to show scores while dead, in dmatch
extern boolean        hu_showscores;

// init heads up data at game startup.
void    HU_Init(void);

// reset heads up when consoleplayer respawns.
void    HU_Start(void);

//
boolean HU_Responder(event_t* ev);

//
void    HU_Ticker(void);
void    HU_Drawer(void);
char    HU_dequeueChatChar(void);
void    HU_Erase(void);

// used by console input
char ForeignTranslation(unsigned char ch);

// set chatmacros cvars point the original or dehacked texts, before
// config.cfg is executed !!
void HU_HackChatmacros (void);

// chatmacro <0-9> "message" console command
void Command_Chatmacro_f (void);

int HU_CreateTeamFragTbl(fragsort_t *fragtab,
                         int dmtotals[],
                         int fragtbl[MAXPLAYERS][MAXPLAYERS]);

#endif
