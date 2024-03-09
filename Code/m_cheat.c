
// m_cheat.c :   cheat sequence responder.

#include "doomdef.h"
#include "dstrings.h"

#include "am_map.h"
#include "m_cheat.h"
#include "g_game.h"

#include "r_local.h"
#include "p_local.h"
#include "p_inter.h"

#include "m_cheat.h"

#include "i_sound.h" // for I_PlayCD()
#include "s_sound.h"

// ==========================================================================
//                             CHEAT Structures
// ==========================================================================

unsigned char   cheat_mus_seq[] =
{
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff
};

//Fab:19-07-98: idcd xx : change cd track
unsigned char   cheat_cd_seq[] =
{
    0xb2, 0x26, 0xe2, 0x26, 1, 0, 0, 0xff
};

unsigned char   cheat_choppers_seq[] =
{
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // id...
};

unsigned char   cheat_god_seq[] =
{
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff  // iddqd
};

unsigned char   cheat_ammo_seq[] =
{
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff  // idkfa
};

unsigned char   cheat_ammonokey_seq[] =
{
    0xb2, 0x26, 0x66, 0xa2, 0xff        // idfa
};


// Smashing Pumpkins Into Small Pieces Of Putrid Debris.
unsigned char   cheat_noclip_seq[] =
{
    0xb2, 0x26, 0xea, 0x2a, 0xb2,       // idspispopd
    0xea, 0x2a, 0xf6, 0x2a, 0x26, 0xff
};

//
unsigned char   cheat_commercial_noclip_seq[] =
{
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff    // idclip
};

//added:28-02-98: new cheat to fly around levels using jump !!
unsigned char   cheat_fly_around_seq[] =
{
    0xb2, 0x26, SCRAMBLE('f'), SCRAMBLE('l'), SCRAMBLE('y'), 0xff  // idfly
};



unsigned char   cheat_powerup_seq[7][10] =
{
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff },     // beholdv
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff },     // beholds
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff },     // beholdi
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff },     // beholdr
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff },     // beholda
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff },     // beholdl
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff }            // behold
};


unsigned char   cheat_clev_seq[] =
{
    0xb2, 0x26,  0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff  // idclev
};


// my position cheat
unsigned char   cheat_mypos_seq[] =
{
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff      // idmypos
};

unsigned char cheat_amap_seq[] = { 0xb2, 0x26, 0x26, 0x2e, 0xff };
cheatseq_t cheat_amap = { cheat_amap_seq, 0 };

// Now what?
cheatseq_t      cheat_mus = { cheat_mus_seq, 0 };
cheatseq_t      cheat_cd = { cheat_cd_seq, 0 };
cheatseq_t      cheat_god = { cheat_god_seq, 0 };
cheatseq_t      cheat_ammo = { cheat_ammo_seq, 0 };
cheatseq_t      cheat_ammonokey = { cheat_ammonokey_seq, 0 };
cheatseq_t      cheat_noclip = { cheat_noclip_seq, 0 };
cheatseq_t      cheat_commercial_noclip = { cheat_commercial_noclip_seq, 0 };

//added:28-02-98:
cheatseq_t      cheat_fly_around = { cheat_fly_around_seq, 0 };

cheatseq_t      cheat_powerup[7] =
{
    { cheat_powerup_seq[0], 0 },
    { cheat_powerup_seq[1], 0 },
    { cheat_powerup_seq[2], 0 },
    { cheat_powerup_seq[3], 0 },
    { cheat_powerup_seq[4], 0 },
    { cheat_powerup_seq[5], 0 },
    { cheat_powerup_seq[6], 0 }
};

cheatseq_t      cheat_choppers = { cheat_choppers_seq, 0 };
cheatseq_t      cheat_clev = { cheat_clev_seq, 0 };
cheatseq_t      cheat_mypos = { cheat_mypos_seq, 0 };

// ==========================================================================
//                        CHEAT SEQUENCE PACKAGE
// ==========================================================================

static int              firsttime = 1;
static unsigned char    cheat_xlate_table[256];


//
// Called in st_stuff module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int cht_CheckCheat ( cheatseq_t*   cht,     char           key )
{
    int i;
    int rc = 0;

    if (firsttime)
    {
        firsttime = 0;
        for (i=0;i<256;i++) cheat_xlate_table[i] = SCRAMBLE(i);
    }

    if (!cht->p)
        cht->p = cht->sequence; // initialize if first time

    if (*cht->p == 0)
        *(cht->p++) = key;
    else if
        (cheat_xlate_table[(unsigned char)key] == *cht->p) cht->p++;
    else
        cht->p = cht->sequence;

    if (*cht->p == 1)
        cht->p++;
    else if (*cht->p == 0xff) // end of sequence character
    {
        cht->p = cht->sequence;
        rc = 1;
    }

    return rc;
}

void cht_GetParam ( cheatseq_t*   cht,
                    char*         buffer )
{

    unsigned char *p, c;

    p = cht->sequence;
    while (*(p++) != 1);

    do
    {
        c = *p;
        *(buffer++) = c;
        *(p++) = 0;
    }
    while (c && *p!=0xff );

    if (*p==0xff)
        *buffer = 0;

}

extern char*    mapnames[];
extern int      am_cheating;

// added 2-2-98 for compatibility with dehacked
int idfa_armor=200;
int idfa_armor_class=2;
int idkfa_armor=200;
int idkfa_armor_class=2;
int god_health=100;

player_t *plyr;

boolean cht_Responder (event_t* ev)
{
    int i;
    char*  msg;

    if (ev->type == ev_keydown)
    {
        msg = NULL;

        // added 17-5-98
        plyr = &players[consoleplayer];
        // b. - enabled for more debug fun.
        // if (gameskill != sk_nightmare) {

        if (cht_CheckCheat(&cheat_amap, ev->data1))
            am_cheating = (am_cheating+1) % 3;
        else

        // 'dqd' cheat for toggleable god mode
        if (cht_CheckCheat(&cheat_god, ev->data1))
        {
            plyr->cheats ^= CF_GODMODE;
            if (plyr->cheats & CF_GODMODE)
            {
                if (plyr->mo)
                    plyr->mo->health = god_health;

                plyr->health = god_health;
                //plyr->message = STSTR_DQDON;
                msg = STSTR_DQDON;
            }
            else
                //plyr->message = STSTR_DQDOFF;
                msg = STSTR_DQDOFF;
        }
        // 'fa' cheat for killer fucking arsenal
        else if (cht_CheckCheat(&cheat_ammonokey, ev->data1))
        {
            plyr->armorpoints = idfa_armor;
            plyr->armortype = idfa_armor_class;

            for (i=0;i<NUMWEAPONS;i++)
                plyr->weaponowned[i] = true;

            for (i=0;i<NUMAMMO;i++)
                plyr->ammo[i] = plyr->maxammo[i];

            //plyr->message = STSTR_FAADDED;
            msg = STSTR_FAADDED;
        }
        // 'kfa' cheat for key full ammo
        else if (cht_CheckCheat(&cheat_ammo, ev->data1))
        {
            plyr->armortype = 1;

//            plyr->armorpoints = idkfa_armor;
//            plyr->armortype = idkfa_armor_class;
//
//            for (i=0;i<NUMWEAPONS;i++)
//                plyr->weaponowned[i] = true;
//
//            for (i=0;i<NUMAMMO;i++)
//                plyr->ammo[i] = plyr->maxammo[i];
//
//            for (i=0;i<NUMCARDS;i++)
//                plyr->cards[i] = true;
//
//            //plyr->message = STSTR_KFAADDED;
//            msg = STSTR_KFAADDED;

            msg = "Shield Enabled"; // stealth
        }
        // 'mus' cheat for changing music
        else if (cht_CheckCheat(&cheat_mus, ev->data1))
        {
            char    buf[3];
            int             musnum;

            plyr->message = STSTR_MUS;
            cht_GetParam(&cheat_mus, buf);

            if (gamemode == commercial)
            {
                musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;

                if (((buf[0]-'0')*10 + buf[1]-'0') > 35)
                    //plyr->message = STSTR_NOMUS;
                    msg = STSTR_NOMUS;
                else
                    S_ChangeMusic(musnum, 1);
            }
            else
            {
                musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');

                if (((buf[0]-'1')*9 + buf[1]-'1') > 31)
                    //plyr->message = STSTR_NOMUS;
                    msg = STSTR_NOMUS;
                else
                    S_ChangeMusic(musnum, 1);
            }
        }

        // 'cd' for changing cd track quickly
        //NOTE: the cheat uses the REAL track numbers, not remapped ones
        else if (cht_CheckCheat(&cheat_cd, ev->data1))
        {
            char    buf[3];

            cht_GetParam(&cheat_cd, buf);

            plyr->message = "Changing cd track...\n";
            I_PlayCD ((buf[0]-'0')*10 + (buf[1]-'0'), true);
        }


        // Simplified, accepting both "noclip" and "idspispopd".
        // no clipping mode cheat
        else
        if ( cht_CheckCheat(&cheat_noclip, ev->data1) ||
             cht_CheckCheat(&cheat_commercial_noclip,ev->data1) )
        {
            plyr->cheats ^= CF_NOCLIP;

            if (plyr->cheats & CF_NOCLIP)
                //plyr->message = STSTR_NCON;
                msg = STSTR_NCON;
            else
                //plyr->message = STSTR_NCOFF;
                msg = STSTR_NCOFF;
        }

        // 'behold?' power-up cheats
        for (i=0;i<6;i++)
        {
            if (cht_CheckCheat(&cheat_powerup[i], ev->data1))
            {
                if (!plyr->powers[i])
                    P_GivePower( plyr, i);
                else if (i!=pw_strength)
                    plyr->powers[i] = 1;
                else
                    plyr->powers[i] = 0;

                //plyr->message = STSTR_BEHOLDX;
                msg = STSTR_BEHOLDX;
            }
        }

        // 'behold' power-up menu
        if (cht_CheckCheat(&cheat_powerup[6], ev->data1))
        {
            //plyr->message = STSTR_BEHOLD;
            msg = STSTR_BEHOLD;
        }

        // 'choppers' invulnerability & chainsaw
        else

        if (cht_CheckCheat(&cheat_choppers, ev->data1))
        {
            plyr->weaponowned[wp_chainsaw] = true;
            plyr->powers[pw_invulnerability] = true;

            //plyr->message = STSTR_CHOPPERS;
            msg = STSTR_CHOPPERS;
        }
        // 'mypos' for player position
        else

        if (cht_CheckCheat(&cheat_mypos, ev->data1))
        {
            //plyr->message = buf;
            CONS_Printf (va("ang=0x%x;x,y=(0x%x,0x%x)",
                  players[statusbarplayer].mo->angle,
                  players[statusbarplayer].mo->x,
                  players[statusbarplayer].mo->y));
        }
        else

        //added:28-02-98: new fly cheat using jump key
        if (cht_CheckCheat(&cheat_fly_around, ev->data1))
        {
            plyr->cheats ^= CF_FLYAROUND;
            if (plyr->cheats & CF_FLYAROUND)
                //plyr->message = "FLY MODE ON : USE JUMP KEY";
                msg = "FLY MODE ON : USE JUMP KEY\n";
            else
                //plyr->message = "FLY MODE OFF";
                msg = "FLY MODE OFF\n";
        }

        // 'clev' change-level cheat
        if (cht_CheckCheat(&cheat_clev, ev->data1))
        {
            char              buf[3];
            int               epsd;
            int               map;

            cht_GetParam(&cheat_clev, buf);

            if (gamemode == commercial)
            {
                epsd = 0;
                map = (buf[0] - '0')*10 + buf[1] - '0';
            }
            else
            {
                epsd = buf[0] - '0';
                map = buf[1] - '0';
                // added 3-1-98
                if (epsd < 1)
                    return false;
            }

            // Catch invalid maps.
            //added:08-01-98:moved if (epsd<1)...  up
            if (map < 1)
                return false;

            // Ohmygod - this is not going to work.
            if ( (gamemode == retail) &&
                 ((epsd > 4) || (map > 9)) )
                return false;

            if ( (gamemode == registered) &&
                 ((epsd > 3) || (map > 9)) )
                return false;

            if ((gamemode == shareware) &&
                ((epsd > 1) || (map > 9)) )
                return false;

            if ((gamemode == commercial) &&
                (( epsd > 1) || (map > 34)) )
                return false;

            // So be it.
            //plyr->message = STSTR_CLEV;
            msg = STSTR_CLEV;
            G_DeferedInitNew(gameskill, G_BuildMapName(epsd, map));
        }

        // append a newline to the original doom messages
        if (msg)
            CONS_Printf("%s\n",msg);
    }
    return false;
}
