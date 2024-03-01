// p_inter.c :  handling interactions (i.e., collisions).

#include "doomdef.h"
#include "i_system.h"   //I_Tactile currently has no effect
#include "am_map.h"
#include "dstrings.h"
#include "g_game.h"
#include "m_random.h"
#include "p_local.h"
#include "p_inter.h"
#include "s_sound.h"
#include "r_main.h"
#include "st_stuff.h"

#define BONUSADD        6


//SOM: Hack
void P_PlayerRingBurst(player_t* player);
//END HACK

//Prototype needed to use function: Stealth 12-26-99
void P_Thrust ( mobj_t*       mo,
                angle_t       angle,
                fixed_t       move );

// a weapon is found with two clip loads,
// a big item has five clip loads
int     maxammo[NUMAMMO] = {200, 50, 300, 50};
int     clipammo[NUMAMMO] = {10, 4, 20, 1};

// added 4-2-98 (Boris) for dehacked patch
// (i don't like that but do you see another solution ?)
int     MAXHEALTH= 900; // Up to 900 rings! Tails 11-03-99

//
// GET STUFF
//

// added by Boris : for dehackin'
int    ammopershoot[NUMWEAPONS]={0,1,1,1,1,1,40,0,2};
extern int BFGCELLS;

// added by Boris : preferred weapons order
void VerifFavoritWeapon (player_t *player)
{
    int    actualprior,i;

    if (player->pendingweapon != wp_nochange)
        return;

    actualprior=0;

    for (i=0; i<NUMWEAPONS; i++)
    {
        // skip super shotgun for non-Doom2
        if (gamemode!=commercial && i==wp_supershotgun)
            continue;

        if ( player->weaponowned[i] &&
             actualprior < player->favoritweapon[i] &&
             player->ammo[weaponinfo[i].ammo] >= ammopershoot[i] )
        {
            player->pendingweapon = i;
            actualprior = player->favoritweapon[i];
        }
    }

    if (player->pendingweapon==player->readyweapon)
        player->pendingweapon = wp_nochange;
}

//
// P_GiveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

boolean P_GiveAmmo ( player_t*     player,
                     ammotype_t    ammo,
                     int           num )
{
    int         oldammo;

    if (ammo == am_noammo)
        return false;

    if (ammo < 0 || ammo > NUMAMMO)
    {
        CONS_Printf ("\2P_GiveAmmo: bad type %i", ammo);
        return false;
    }

    if ( player->ammo[ammo] == player->maxammo[ammo]  )
        return false;

    if (num)
        num *= clipammo[ammo];
    else
        num = clipammo[ammo]/2;

    if (gameskill == sk_baby
        || gameskill == sk_nightmare)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }


    oldammo = player->ammo[ammo];
    player->ammo[ammo] += num;

    if (player->ammo[ammo] > player->maxammo[ammo])
        player->ammo[ammo] = player->maxammo[ammo];

    // If non zero ammo,
    // don't change up weapons,
    // player was lower on purpose.
    if (oldammo)
        return true;

    // We were down to zero,
    // so select a new weapon.
    // Preferences are not user selectable.

    // Boris hack for preferred weapons order...
    if(!player->originalweaponswitch)
    {
       if(player->ammo[weaponinfo[player->readyweapon].ammo]
                             < ammopershoot[player->readyweapon])
         VerifFavoritWeapon(player);
       return true;
    }
    else //eof Boris
    switch (ammo)
    {
      case am_clip:
        if (player->readyweapon == wp_fist)
        {
            if (player->weaponowned[wp_chaingun])
                player->pendingweapon = wp_chaingun;
            else
                player->pendingweapon = wp_pistol;
        }
        break;

      case am_shell:
        if (player->readyweapon == wp_fist
            || player->readyweapon == wp_pistol)
        {
            if (player->weaponowned[wp_shotgun])
                player->pendingweapon = wp_shotgun;
        }
        break;

      case am_cell:
        if (player->readyweapon == wp_fist
            || player->readyweapon == wp_pistol)
        {
            if (player->weaponowned[wp_plasma])
                player->pendingweapon = wp_plasma;
        }
        break;

      case am_misl:
        if (player->readyweapon == wp_fist)
        {
            if (player->weaponowned[wp_missile])
                player->pendingweapon = wp_missile;
        }
      default:
        break;
    }

    return true;
}


//
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
boolean P_GiveWeapon ( player_t*     player,
                       weapontype_t  weapon,
                       boolean       dropped )
{
    boolean     gaveammo;
    boolean     gaveweapon;
    if (multiplayer && (cv_deathmatch.value!=2) && !dropped )

    {
        // leave placed weapons forever on net games
        if (player->weaponowned[weapon])
            return false;

        player->bonuscount += BONUSADD;
        player->weaponowned[weapon] = true;

        if (cv_deathmatch.value)
            P_GiveAmmo (player, weaponinfo[weapon].ammo, 5);
        else
            P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);

        // Boris hack preferred weapons order...
        if(player->originalweaponswitch
        || player->favoritweapon[weapon] > player->favoritweapon[player->readyweapon])
            player->pendingweapon = weapon;     // do like Doom2 original

        //added:16-01-98:changed consoleplayer to displayplayer
        //               (hear the sounds from the viewpoint)
        if (player == &players[displayplayer] || (cv_splitscreen.value && player==&players[secondarydisplayplayer]))
            S_StartSound (NULL, sfx_wpnup);
        return false;
    }

    if (weaponinfo[weapon].ammo != am_noammo)
    {
        // give one clip with a dropped weapon,
        // two clips with a found weapon
        if (dropped)
            gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 1);
        else
            gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);
    }
    else
        gaveammo = false;

    if (player->weaponowned[weapon])
        gaveweapon = false;
    else
    {
        gaveweapon = true;
        player->weaponowned[weapon] = true;
        if (player->originalweaponswitch
        || player->favoritweapon[weapon] > player->favoritweapon[player->readyweapon])
            player->pendingweapon = weapon;    // Doom2 original stuff
    }

    return (gaveweapon || gaveammo);
}



//
// P_GiveBody
// Returns false if the body isn't needed at all
//
boolean P_GiveBody ( player_t*     player,
                     int           num )
{
    if (player->health >= MAXHEALTH)
        return false;

    player->health += num;
    if (player->health > MAXHEALTH)
        player->health = MAXHEALTH;
    player->mo->health = player->health;

    return true;
}



//
// P_GiveArmor
// Returns false if the armor is worse
// than the current armor.
//
boolean P_GiveArmor ( player_t*     player,
                      int           armortype )
{
    int         hits;

    hits = armortype*100;
    if (player->armorpoints >= hits)
        return false;   // don't pick up

    player->armortype = armortype;
    player->armorpoints = hits;

    return true;
}



//
// P_GiveCard
//
void P_GiveCard ( player_t*     player,
                  card_t        card )
{
    if (player->cards[card])
        return;

    player->bonuscount = BONUSADD;
    player->cards[card] = 1;
}


//
// P_GivePower
//
boolean P_GivePower ( player_t*     player,
                      int /*powertype_t*/   power )
{
    if (power == pw_invulnerability)
    {
        player->powers[power] = INVULNTICS;
        return true;
    }

    if (power == pw_invisibility)
    {
        player->powers[power] = INVISTICS;
        player->mo->flags |= MF_SHADOW;
        return true;
    }

    if (power == pw_infrared)
    {
        player->powers[power] = INFRATICS;
        return true;
    }

    if (power == pw_ironfeet)
    {
        player->powers[power] = IRONTICS;
        return true;
    }

    if (power == pw_strength)
    {
//        P_GiveBody (player, 100); // Don't give rings! Tails 02-28-2000
        player->powers[power] = SPEEDTICS; // using SPEEDTICS to hold timer for super sneakers Tails 02-28-2000
        return true;
    }
//start tails fly & shield Tails 03-05-2000
    if (power == pw_blueshield)
    {
     return true;
    }

    if (power == pw_tailsfly)
    {
        player->powers[power] = TAILSTICS;
        return true;
    }
//end tails fly & shield Tails 03-05-2000
    if (player->powers[power])
        return false;   // already got it

    player->powers[power] = 1;
    return true;
}

// Boris stuff : dehacked patches hack
int max_armor=200;
int green_armor_class=1;
int blue_armor_class=2;
int maxsoul=200;
int soul_health=100;
int mega_health=200;
// eof Boris


//
// P_TouchSpecialThing
//
void P_TouchSpecialThing ( mobj_t*       special,
                           mobj_t*       toucher )

{
    player_t*   player;
    int         i;
//    fixed_t     delta;  unused as of current 12-25-99 Stealth
    int         sound;

//    delta = special->z - toucher->z;

//    if (delta > toucher->height
//        || delta < -8*FRACUNIT)
//    {
//        // out of reach
//        return;
//    }
    if(toucher->z > (special->z + special->info->height))
      return;
    if(special->z > (toucher->z + toucher->info->height))
      return;


    sound = sfx_itemup;
    player = toucher->player;

    // Dead thing touching.
    // Can happen with a sliding player corpse.
    if (toucher->health <= 0)
        return;

    // Identify by sprite.
    switch (special->sprite)
    {
        // armor
      case SPR_ARM1:
        if (!P_GiveArmor (player, green_armor_class))
            return;
        player->message = GOTARMOR;
        break;

      case SPR_ARM2:
        if (!P_GiveArmor (player, blue_armor_class))
            return;
        player->message = GOTMEGA;
        break;

        // bonus items
      case SPR_BON1:
        player->health++;               // can go over 100%
        if (player->health > 9*MAXHEALTH) // go up to 900 rings Tails 11-01-99
            player->health = 9*MAXHEALTH; // go up to 900 rings Tails 11-01-99
        player->mo->health = player->health;
        P_SpawnMobj (special->x,special->y,special->z + (special->info->height / 2), MT_SPARK);
        if(cv_showmessages.value==1)
            player->message = GOTHTHBONUS;
        break;

      case SPR_BON2:
        player->armorpoints++;          // can go over 100%
        if (player->armorpoints > max_armor)
            player->armorpoints = max_armor;
        if (!player->armortype)
            player->armortype = 1;
        if(cv_showmessages.value==1)
           player->message = GOTARMBONUS;
        break;

      case SPR_EMMY:
        player->health += soul_health;
        if (player->health > maxsoul)
            player->health = maxsoul;
        player->mo->health = player->health;
        player->message = GOTSUPER;
        sound = sfx_getpow;
        break;

      case SPR_MEGA:
        if (gamemode != commercial)
            return;
        player->health = mega_health;
        player->mo->health = player->health;
        P_GiveArmor (player,2);
        player->message = GOTMSPHERE;
        sound = sfx_getpow;
        break;

        // cards
        // leave cards for everyone
      case SPR_BKEY:
        if (!player->cards[it_bluecard])
            player->message = GOTBLUECARD;
        P_GiveCard (player, it_bluecard);
        if (!multiplayer)
            break;
        return;

      case SPR_YKEY:
        if (!player->cards[it_yellowcard])
            player->message = GOTYELWCARD;
        P_GiveCard (player, it_yellowcard);
        if (!multiplayer)
            break;
        return;

      case SPR_RKEY:
        if (!player->cards[it_redcard])
            player->message = GOTREDCARD;
        P_GiveCard (player, it_redcard);
        if (!multiplayer)
            break;
        return;

      case SPR_BSKU:
        if (!player->cards[it_blueskull])
            player->message = GOTBLUESKUL;
        P_GiveCard (player, it_blueskull);
        if (!multiplayer)
            break;
        return;

      case SPR_YSKU:
        if (!player->cards[it_yellowskull])
            player->message = GOTYELWSKUL;
        P_GiveCard (player, it_yellowskull);
        if (!multiplayer)
            break;
        return;

      case SPR_RSKU:
        if (!player->cards[it_redskull])
            player->message = GOTREDSKULL;
        P_GiveCard (player, it_redskull);
        if (!multiplayer)
            break;
        return;
// start dummy ringboxes tails
        // medikits, heals
      case SPR_RGBX: // dummy ringbox pickup tails
        if (!P_GiveBody (player, 10))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTSTIM;
        break;

      case SPR_SRGB: // dummy super ringbox pickup tails
        if (!P_GiveBody (player, 25))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTMEDIKIT;
        break;
// end dummy ringboxes tails
        // power ups
      case SPR_PINV: // this is the invincibility powerup Tails 02-28-2000
        if (!P_GivePower (player, pw_invulnerability))
            return;
        player->message = GOTINVUL;
        sound = sfx_getpow;
        break;

      case SPR_PSTR: // this is the super sneaker powerup Tails 02-28-2000
        if (!P_GivePower (player, pw_strength))
            return;
        player->message = NULL; // Tails 02-28-2000
//        if (player->readyweapon != wp_fist) // don't need
//            player->pendingweapon = wp_fist; // this slop Tails 02-28-2000
//        sound = sfx_None; // Don't play sound! Tails 02-28-2000
        break;

      case SPR_PINS:
        if (!P_GivePower (player, pw_invisibility)) // this is the temp invincibility after getting hit Tails 02-28-2000
            return;
        player->message = NULL; // Tails 02-28-2000
//        sound = sfx_None; // Tails 02-28-2000
        break;

      case SPR_SUIT:
        if (!P_GivePower (player, pw_ironfeet))
            return;
        player->message = GOTSUIT;
        sound = sfx_getpow;
        break;

      case SPR_PMAP:
        if (!P_GivePower (player, pw_allmap))
            return;
        player->message = GOTMAP;
        sound = sfx_getpow;
        break;

      case SPR_PVIS:
        if (!P_GivePower (player, pw_infrared))
            return;
        player->message = NULL; // no message Tails 02-28-2000
        sound = sfx_None; // no sound Tails 02-28-2000
        break;

        // ammo
      case SPR_CLIP:
        if (special->flags & MF_DROPPED)
        {
            if (!P_GiveAmmo (player,am_clip,0))
                return;
        }
        else
        {
            if (!P_GiveAmmo (player,am_clip,1))
                return;
        }
        if(cv_showmessages.value==1)
            player->message = GOTCLIP;
        break;

      case SPR_AMMO:
        if (!P_GiveAmmo (player, am_clip,5))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTCLIPBOX;
        break;

      case SPR_ROCK:
        if (!P_GiveAmmo (player, am_misl,1))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTROCKET;
        break;

      case SPR_BROK:
        if (!P_GiveAmmo (player, am_misl,5))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTROCKBOX;
        break;

      case SPR_CELL:
        if (!P_GiveAmmo (player, am_cell,1))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTCELL;
        break;

      case SPR_CELP:
        if (!P_GiveAmmo (player, am_cell,5))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTCELLBOX;
        break;

      case SPR_SHEL:
        if (!P_GiveAmmo (player, am_shell,1))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTSHELLS;
        break;

      case SPR_SBOX:
        if (!P_GiveAmmo (player, am_shell,5))
            return;
        if(cv_showmessages.value==1)
            player->message = GOTSHELLBOX;
        break;

      case SPR_BPAK:
        if (!player->backpack)
        {
            for (i=0 ; i<NUMAMMO ; i++)
                player->maxammo[i] *= 2;
            player->backpack = true;
        }
        for (i=0 ; i<NUMAMMO ; i++)
            P_GiveAmmo (player, i, 1);
        player->message = GOTBACKPACK;
        break;

        // weapons
      case SPR_BFUG:
        if (!P_GiveWeapon (player, wp_bfg, false) )
            return;
        player->message = GOTBFG9000;
        sound = sfx_wpnup;
        break;

      case SPR_MGUN:
        if (!P_GiveWeapon (player, wp_chaingun, special->flags&MF_DROPPED) )
            return;
        player->message = GOTCHAINGUN;
        sound = sfx_wpnup;
        break;

      case SPR_CSAW:
        if (!P_GiveWeapon (player, wp_chainsaw, false) )
            return;
        player->message = GOTCHAINSAW;
        sound = sfx_wpnup;
        break;

      case SPR_LAUN:
        if (!P_GiveWeapon (player, wp_missile, false) )
            return;
        player->message = GOTLAUNCHER;
        sound = sfx_wpnup;
        break;

      case SPR_PLAS:
        if (!P_GiveWeapon (player, wp_plasma, false) )
            return;
        player->message = GOTPLASMA;
        sound = sfx_wpnup;
        break;

      case SPR_SHOT:
        if (!P_GiveWeapon (player, wp_shotgun, special->flags&MF_DROPPED ) )
            return;
        player->message = GOTSHOTGUN;
        sound = sfx_wpnup;
        break;

      case SPR_SGN2:
        if (!P_GiveWeapon (player, wp_supershotgun, special->flags&MF_DROPPED ) )
            return;
        player->message = GOTSHOTGUN2;
        sound = sfx_wpnup;
        break;

      default:
        CONS_Printf ("\2P_TouchSpecialThing: Unknown gettable thing\n");
        return;
    }

    if (special->flags & MF_COUNTITEM)
        player->itemcount++;
    P_RemoveMobj (special);
    player->bonuscount += BONUSADD;

    //added:16-01-98:consoleplayer -> displayplayer (hear sounds from viewpoint)
    if (player == &players[displayplayer] || (cv_splitscreen.value && player==&players[secondarydisplayplayer]))
        S_StartSound (NULL, sound);
}



#ifdef thatsbuggycode
//
//  Tell each supported thing to check again its position,
//  because the 'base' thing has vanished or diminished,
//  the supported things might fall.
//
//added:28-02-98:
void P_CheckSupportThings (mobj_t* mobj)
{
    fixed_t   supportz = mobj->z + mobj->height;

    while ((mobj = mobj->supportthings))
    {
        // only for things above support thing
        if (mobj->z > supportz)
            mobj->eflags |= MF_CHECKPOS;

    }
}


//
//  If a thing moves and supportthings,
//  move the supported things along.
//
//added:28-02-98:
void P_MoveSupportThings (mobj_t* mobj, fixed_t xmove, fixed_t ymove, fixed_t zmove)
{
    fixed_t   supportz = mobj->z + mobj->height;
    mobj_t    *mo = mobj->supportthings;

    while (mo)
    {
        //added:28-02-98:debug
        if (mo==mobj)
        {
            mobj->supportthings = NULL;
            break;
        }

        // only for things above support thing
        if (mobj->z > supportz)
        {
            mobj->eflags |= MF_CHECKPOS;
            mobj->momx += xmove;
            mobj->momy += ymove;
            mobj->momz += zmove;
        }

        mo = mo->supportthings;
    }
}


//
//  Link a thing to it's 'base' (supporting) thing.
//  When the supporting thing will move or change size,
//  the supported will then be aware.
//
//added:28-02-98:
void P_LinkFloorThing(mobj_t*   mobj)
{
    mobj_t*     mo;
    mobj_t*     nmo;

    // no supporting thing
    if (!(mo = mobj->floorthing))
        return;

    // link mobj 'above' the lower mobjs, so that lower supporting
    // mobjs act upon this mobj
    while ( (nmo = mo->supportthings) &&
            (nmo->z<=mobj->z) )
    {
        // dont link multiple times
        if (nmo==mobj)
            return;

        mo = nmo;
    }
    mo->supportthings = mobj;
    mobj->supportthings = nmo;
}


//
//  Unlink a thing from it's support,
//  when it's 'floorthing' has changed,
//  before linking with the new 'floorthing'.
//
//added:28-02-98:
void P_UnlinkFloorThing(mobj_t*   mobj)
{
    mobj_t*     mo;

    if (!(mo = mobj->floorthing))      // just to be sure (may happen)
       return;

    while (mo->supportthings)
    {
        if (mo->supportthings == mobj)
        {
            mo->supportthings = NULL;
            break;
        }
        mo = mo->supportthings;
    }
}
#endif


// Death messages relating to the target (dying) player
//
void P_DeathMessages ( mobj_t*       source,
                       mobj_t*       target )
{
    int     w;
    char    *str;

    if (target && target->player)
    {
        if (source && source->player)
        {
            if (source->player==target->player)
                CONS_Printf("%s suicides\n", player_names[target->player-players]);
            else
            {
                w = source->player->readyweapon;

                switch(w)
                {
                  case wp_fist:
                     str = "%s was beaten to a pulp by %s\n";
                     break;
                  case wp_pistol:
                     str = "%s was gunned by %s\n";
                     break;
                  case wp_shotgun:
                     str = "%s was shot down by %s\n";
                     break;
                  case wp_chaingun:
                     str = "%s was machine-gunned by %s\n";
                     break;
                  case wp_missile:
                     str = "%s was catched up by %s's rocket\n";
                     if (target->health < -target->info->spawnhealth &&
                         target->info->xdeathstate)
                         str = "%s was gibbed by %s's rocket\n";
                     break;
                  case wp_plasma:
                     str = "%s eats %s's toaster\n";
                     break;
                  case wp_bfg:
                     str = "%s enjoy %s's big fraggin' gun\n";
                     break;
                  case wp_chainsaw:
                     str = "%s was divided up into little pieces by %s's chainsaw\n";
                     break;
                  case wp_supershotgun:
                     str = "%s ate 2 loads of %s's buckshot\n";
                     break;
                  default:
                     str = "%s was killed by %s\n";
                     break;
                }

                CONS_Printf(str,player_names[target->player-players],
                                player_names[source->player-players]);
            }
        }
        else
        {
            CONS_Printf(player_names[target->player-players]);

            if (!source)
            {
                // environment kills
                w = target->player->specialsector;      //see p_spec.c

                if (w==5)
                    CONS_Printf(" dies in hellslime\n");
                else if (w==7)
                    CONS_Printf(" gulped a load of nukage\n");
                else if (w==16 || w==4)
                    CONS_Printf(" dies in super hellslime/strobe hurt\n");
                else
                    CONS_Printf(" dies in special sector\n");
            }
            else
            {
                // check for lava,slime,water,crush,fall,monsters..
                if (source->type == MT_BARREL)
                {
                    if (source->target->player)
                        CONS_Printf(" was barrel-fragged by %s\n",
                                    player_names[source->target->player-players]);
                    else
                        CONS_Printf(" dies from a barrel explosion\n");
                }
                else
                switch (source->type)
                {
                  case MT_POSSESSED:
                    CONS_Printf(" was shot by a possessed\n"); break;
                  case MT_SHOTGUY:
                    CONS_Printf(" was shot down by a shotguy\n"); break;
                  case MT_VILE:
                    CONS_Printf(" was blasted by an Arch-vile\n"); break;
                  case MT_FATSO:
                    CONS_Printf(" was exploded by a Mancubus\n"); break;
                  case MT_CHAINGUY:
                    CONS_Printf(" was punctured by a Chainguy\n"); break;
                  case MT_TROOP:
                    CONS_Printf(" was fried by an Imp\n"); break;
                  case MT_SERGEANT:
                    CONS_Printf(" was eviscerated by a Demon\n"); break;
                  case MT_SHADOWS:
                    CONS_Printf(" was mauled by a Shadow Demon\n"); break;
                  case MT_HEAD:
                    CONS_Printf(" was fried by a Caco-demon\n"); break;
                  case MT_BRUISER:
                    CONS_Printf(" was smashed by a Revenant\n"); break;
                  case MT_KNIGHT:
                    CONS_Printf(" was slain by a Hell-Knight\n"); break;
                  case MT_SKULL:
                    CONS_Printf(" was killed by a Skull\n"); break;
                  case MT_SPIDER:
                    CONS_Printf(" was killed by a Spider\n"); break;
                  case MT_BABY:
                    CONS_Printf(" was killed by a Baby Spider\n"); break;
                  case MT_CYBORG:
                    CONS_Printf(" was crushed by the Cyber-demon\n"); break;
                  case MT_PAIN:
                    CONS_Printf(" was killed by a Pain Elemental\n"); break;
                  case MT_WOLFSS:
                    CONS_Printf(" was killed by a WolfSS\n"); break;
                  default:
                    CONS_Printf(" died\n");
                }
            }
        }
    }

}

// WARNING : check cv_fraglimit>0 before call this function !
void P_CheckFragLimit(player_t *p)
{
    if(cv_teamplay.value)
    {
        int fragteam=0,i;

        for(i=0;i<MAXPLAYERS;i++)
            if(ST_SameTeam(p,&players[i]))
                fragteam += ST_PlayerFrags(i);

        if(cv_fraglimit.value<=fragteam)
            G_ExitLevel();
    }
    else
    {
        if(cv_fraglimit.value<=ST_PlayerFrags(p-players))
            G_ExitLevel();
    }
}


// P_KillMobj
//
//      source is the attacker,
//      target is the 'target' of the attack, target dies...
//                                          113
extern camera_t camera;

void P_KillMobj ( mobj_t*       source,
                  mobj_t*       target )
{
    mobjtype_t  item;
    mobj_t*     mo;

    // dead target is no more shootable
    target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);

    if (target->type != MT_SKULL)
        target->flags &= ~MF_NOGRAVITY;

    //added:22-02-98: remember who exploded the barrel, so that the guy who
    //                shot the barrel which killed another guy, gets the frag!
    //                (source is passed from barrel to barrel also!)
    //                (only for multiplayer fun, does not remember monsters)
    if (target->type == MT_BARREL &&
        source &&
        source->player)
        target->target = source;

    target->flags   |= MF_CORPSE|MF_DROPOFF;
    target->height >>= 2;
    target->radius -= (target->radius>>4);      //for solid corpses

    // show death messages, only if it concern the console player
    // (be it an attacker or a target)
    if (target->player && (target->player == &players[consoleplayer]) )
        P_DeathMessages (source,target);
    else
    if (source && source->player && (source->player == &players[consoleplayer]) )
        P_DeathMessages (source,target);


    // if killed by a player
    if (source && source->player)
    {
      if (multiplayer)
       {
         switch (target->type)
          {
           case MT_MISC10:
    P_SpawnMobj (source->player->mo->x, source->player->mo->y, source->player->mo->z + (source->player->mo->info->height /2), MT_DUMMYRINGBOX);
    S_StartSound (source->player->mo, sfx_itemup);
           break;
           case MT_MISC11:
    P_SpawnMobj (source->player->mo->x, source->player->mo->y, source->player->mo->z + (source->player->mo->info->height /2), MT_DUMMYSUPERRINGBOX);
    S_StartSound (source->player->mo, sfx_itemup);
           break;
           case MT_INV:
       source->player->powers[pw_invulnerability] = 896;
       S_StopMusic();
       S_ChangeMusic(mus_invinc, false);
           break;
           case MT_MISC50:
       source->player->powers[pw_blueshield] = true;
       S_StartSound (source->player->mo, sfx_getpow);
           break;
           case MT_MISC74:
       source->player->powers[pw_strength] = 896;
           break;
           default:
           break;
          }
       }

        // count for intermission
        if (target->flags & MF_COUNTKILL)
            source->player->killcount++;
            source->player->score+=100; // Score! Tails 03-01-2000
 // 

        // count frags if player killed player
        if (target->player)
        {
            source->player->frags[target->player-players]++;

            // check fraglimit cvar
            if (cv_fraglimit.value)
                P_CheckFragLimit(source->player);
        }
    }
    else if (!multiplayer && (target->flags & MF_COUNTKILL))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players[0].killcount++;
    }

    // if a player avatar dies...
    if (target->player)
    {
        // count environment kills against you (you fragged yourself!)
        if (!source)
            target->player->frags[target->player-players]++;

        target->flags &= ~MF_SOLID;                     // does not block
        target->player->playerstate = PST_DEAD;
        P_DropWeapon (target->player);                  // put weapon away

        if (target->player == &players[consoleplayer])
        {
            // don't die in auto map,
            // switch view prior to dying
            if (automapactive)
                AM_Stop ();

            //added:22-02-98: recenter view for next live...
            localaiming = 0;
        }
        if (target->player == &players[secondarydisplayplayer])
        {
            //added:22-02-98: recenter view for next live...
            localaiming2 = 0;
        }
    }

    if (target->health < -target->info->spawnhealth
        && target->info->xdeathstate)
    {
        P_SetMobjState (target, target->info->xdeathstate);
    }
    else
        P_SetMobjState (target, target->info->deathstate);

    target->tics -= P_Random()&3;

    if (target->tics < 1)
        target->tics = 1;

    //  I_StartSound (&actor->r, actor->info->deathsound);


    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    switch (target->type)
    {
      case MT_WOLFSS:
      case MT_POSSESSED:
        item = MT_BIRD;
        break;

      case MT_SHOTGUY:
        item = MT_SQRL;
        break;

      case MT_SERGEANT:
        item = MT_SQRL;
        break;

      case MT_TROOP:
        item = MT_BIRD;
        break;

      default:
        return;
    }

    mo = P_SpawnMobj (target->x,target->y,target->z + (target->info->height / 2), item);
    mo->flags |= MF_DROPPED;    // special versions of items
}




//
// P_DamageMobj
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be NULL (slime, etc)
// "source" is the thing to target after taking damage
//  creature or NULL
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions
// and other environmental stuff.
//
boolean P_DamageMobj ( mobj_t*       target,
                       mobj_t*       inflictor,
                       mobj_t*       source,
                       int           damage )
{
    unsigned    ang;
//    int         saved; unused as of current 12-25-99 Stealth
    player_t*   player;
    fixed_t     thrust;
    int         temp;
    boolean     takedamage;  // false on some case in teamplay

    if ( !(target->flags & MF_SHOOTABLE) )
        return false; // shouldn't happen...

    if (target->health <= 0)
        return false;

    if ( target->flags & MF_SKULLFLY )
    {
        target->momx = target->momy = target->momz = 0;
    }

    player = target->player;
// Tails bounce code moved here 12-05-99
    //SOM: Hack to reduce the player to 1 health whenever hit.
         //9999 Damage value added as flag for instant death 12-25-99 Stealth
         //Movement fixed to bounce at fixed speed in opposite direction: Stealth 12-26-99
    if(player) {

              if ( damage < 1000 && player->powers[pw_blueshield])  //If One-Hit Shield
              {
P_SpawnMobj (target->x,target->y,target->z + (target->info->height / 2), MT_INS); // make flash tails 02-26-2000
               player->powers[pw_blueshield] = false;      //Get rid of shield
               damage=0;                 //Dont take rings away
                 player->mo->momz = JUMPGRAVITY*1.5;
                 player->mo->momx = 0;
                 player->mo->momy = 0;
                 P_Thrust (player->mo, player->mo->angle, -256000);
           //      P_Thrust(player->mo, player->mo->angle, -50);
                   if ((player->mo->eflags & MF_JUSTHITFLOOR)) {
                   player->mo->eflags &= ~MF_JUSTHITFLOOR; }
                   if((player->mo->eflags & MF_JUSTJUMPED)) {
                   player->mo->eflags &= ~MF_JUSTJUMPED; }
                   if ((player->mo->eflags & MF_JUMPED)) {
                   player->mo->eflags &= ~MF_JUMPED; }
               S_StartSound (target, sfx_pldeth);
              }

    else if ( damage < 1000 // start ignore bouncing & such in invulnerability tails 02-26-2000
             && ( (player->cheats&CF_GODMODE)
                  || player->powers[pw_invulnerability] || player->powers[pw_invisibility]) )
{} // end ignore bouncing & such in invulnerability tails 02-26-2000
     else if((player->mo->health > 1 || player->armortype) && !(damage==9999))
                {
                 damage = player->mo->health - 1;
                 player->mo->momz = JUMPGRAVITY*1.5;
P_SpawnMobj (target->x,target->y,target->z + (target->info->height / 2), MT_INS); // make flash tails 02-26-2000
          //       player->mo->momx = -player->mo->momx;
          //       player->mo->momy = -player->mo->momy;
                 player->mo->momx = 0;
                 player->mo->momy = 0;
                 P_Thrust (player->mo, player->mo->angle, -256000);
           //      P_Thrust(player->mo, player->mo->angle, -50);
                   if ((player->mo->eflags & MF_JUSTHITFLOOR)) {
                   player->mo->eflags &= ~MF_JUSTHITFLOOR; }
                   if((player->mo->eflags & MF_JUSTJUMPED)) {
                   player->mo->eflags &= ~MF_JUSTJUMPED; }
                   if ((player->mo->eflags & MF_JUMPED)) {
                   player->mo->eflags &= ~MF_JUMPED; }
                 if(player->armortype==0)
                 {
                  S_StartSound (target, target->info->painsound);
                 } 
                }
//Should only be upward motion when dying: Stealth 12-26-99

                else
                {
                 damage = 1;
                 player->mo->health=1;
                 player->mo->momz = JUMPGRAVITY*1.5;
             //    player->mo->momx = -player->mo->momx;
             //    player->mo->momy = -player->mo->momy;
                 player->mo->momx = 0;
                 player->mo->momy = 0;
                   if ((player->mo->eflags & MF_JUSTHITFLOOR)) {
                   player->mo->eflags &= ~MF_JUSTHITFLOOR; }
                   if((player->mo->eflags & MF_JUSTJUMPED)) {
                   player->mo->eflags &= ~MF_JUSTJUMPED; }
                   if ((player->mo->eflags & MF_JUMPED)) {
                   player->mo->eflags &= ~MF_JUMPED; }
                  }
                 }
// end Tails bounce code moved here 12-05-99

//    if (player && gameskill == sk_baby)
//        damage >>= 1;   // take half damage in trainer mode


    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor
        && !(target->flags & MF_NOCLIP)
        && (!source
            || !source->player
            || source->player->readyweapon != wp_chainsaw))
    {
        ang = R_PointToAngle2 ( inflictor->x,
                                inflictor->y,
                                target->x,
                                target->y);

        thrust = damage*(FRACUNIT>>3)*100/target->info->mass;

        // sometimes a target shot down might fall off a ledge forwards
        if ( damage < 40
             && damage > target->health
             && target->z - inflictor->z > 64*FRACUNIT
             && (P_Random ()&1) )
        {
            ang += ANG180;
            thrust *= 4;
        }

        ang >>= ANGLETOFINESHIFT;
        target->momx += FixedMul (thrust, finecosine[ang]);
        target->momy += FixedMul (thrust, finesine[ang]);

        // added momz (do it better for missiles explotion)
        if (source && demoversion>=124)
        {
            int dist,z;

            if(source==target) // rocket in yourself (suicide)
            {
                viewx=inflictor->x;
                viewy=inflictor->y;
                z=inflictor->z;
            }
            else
            {
                viewx=source->x;
                viewy=source->y;
                z=source->z;
            }
            dist=R_PointToDist(target->x,target->y);

            viewx=0;
            viewy=z;
            ang = R_PointToAngle(dist,target->z);

            ang >>= ANGLETOFINESHIFT;
            target->momz += FixedMul (thrust, finesine[ang]);
        }
    }

    takedamage = false;
    // player specific
    if (player)
    {
        // end of game hell hack
        if (target->subsector->sector->special == 11
            && damage >= target->health)
        {
            damage = target->health - 1;
        }


        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if ( damage < 1000
             && ( (player->cheats&CF_GODMODE)
                  || player->powers[pw_invulnerability] || player->powers[pw_invisibility]) ) // flash tails 02-26-2000
        {
            return false;
        }

        if (player->armortype)
        {
//            if (player->armortype == 1)         Old Legacy Stuff
//                saved = damage/3;
//            else
//                saved = damage/2;
//
//            if (player->armorpoints <= saved)
//            {
//                // armor is used up
//                saved = player->armorpoints;
//                player->armortype = 0;
//            }
//            player->armorpoints -= saved;
//            damage -= saved;

//Shield handling Added 12-25-99 Stealth
/*
              if (player->powers[pw_blueshield])  //If One-Hit Shield
              {
               player->powers[pw_blueshield] = false;      //Get rid of shield
               damage=0;                 //Dont take rings away
               S_StartSound (target, sfx_pldeth);
              }
*/
        }

        // added team play and teamdamage (view logboris at 13-8-98 to understand)
        if( demoversion < 125   || // support old demoversion
            cv_teamdamage.value ||
            damage>1000         || // telefrag
            source==target      ||
            !source             ||
            !source->player     ||
            (
             cv_deathmatch.value
             &&
             (!cv_teamplay.value ||
              !ST_SameTeam(source->player,player)
             )
            )
          )
        {
            player->health -= damage;   // mirror mobj health here for Dave
            if (player->health < 0)
                player->health = 0;
            takedamage = true;

            player->damagecount += damage;  // add damage after armor / invuln

            P_PlayerRingBurst(player);

            if (player->damagecount > 100)
                player->damagecount = 100;  // teleport stomp does 10k points...

            temp = damage < 100 ? damage : 100;

            //added:22-02-98: force feedback ??? electro-shock???
            if (player == &players[consoleplayer])
                I_Tactile (40,10,40+temp*2);
        }
        player->attacker = source;
    }
    else
        takedamage = true;

    // added teamplay and teamdamage (view logboris at 13-8-98 to understand)
    if( takedamage )
    {
        // do the damage
        target->health -= damage;
        if (target->health <= 0)
        {
            P_KillMobj (source, target);
            return true;
        }


        if ( (P_Random () < target->info->painchance)
             && !(target->flags&MF_SKULLFLY) )
        {
            target->flags |= MF_JUSTHIT;    // fight back!

            P_SetMobjState (target, target->info->painstate);
        }

        target->reactiontime = 0;           // we're awake now...
    }

    if ( (!target->threshold || target->type == MT_VILE)
         && source && source != target
         && source->type != MT_VILE)
    {
        // if not intent on another player,
        // chase after this one
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if (target->state == &states[target->info->spawnstate]
            && target->info->seestate != S_NULL)
            P_SetMobjState (target, target->info->seestate);
    }

    return takedamage;
}

// ringtables are tables that have position and momentum data
int  ring_xpos_table[16] = {
0, 0.5, 1, 1.5, 2, 1.5, 1, 0.5,
0, -0.5, -1, -1.5, -2, -1.5, -1, -0.5};

int  ring_ypos_table[16] = {
-2, -1.5, -1, -0.5, -0, 0.5, 1, 1.5,
2, 1.5, 1, 0.5, 0, -0.5, -1, -1.5};

//SOM: Makes rings shoot out everywhere
void P_PlayerRingBurst(player_t* player)
  {
  int       num_rings;
  int       i;
  fixed_t   ringx;
  fixed_t   ringy;
  fixed_t   ringz;
  fixed_t   reqdist = player->mo->info->radius + (25*FRACUNIT);
  mobj_t*   mo;

  //If no health, don't spawn ring!
  if(player->mo->health <= 1)
    return;

  if(player->damagecount > 5)
    num_rings = (player->damagecount / 5) + 5;
  else
    num_rings = player->damagecount;

  if(num_rings > 30)
    return;

//  CONS_Printf("HACK: Rings spawned %i\n", num_rings);
  for(i = num_rings; i; i--)
    {
    ringz = player->mo->z + ((P_Random() % player->mo->info->height) * FRACUNIT);
    ringx = ring_xpos_table[i % 16] * (reqdist);
    ringy = ring_ypos_table[i % 16] * (reqdist);
    mo = P_SpawnMobj(player->mo->x + ringx,
                     player->mo->y + ringy,
                     ringz,
                     MT_FLINGRING);
    mo->momz = JUMPGRAVITY + ((P_Random() % 5) * FRACUNIT);
    mo->momx = ring_xpos_table[i % 16] * (10 * FRACUNIT);
    mo->momy = ring_ypos_table[i % 16] * (10 * FRACUNIT);
    mo->fuse = 200 + (P_Random() % 50);
    }

  return;
  }
                                            
