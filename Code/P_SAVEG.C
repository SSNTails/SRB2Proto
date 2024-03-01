// p_saveg.c :  Archiving: SaveGame I/O.

#include "doomdef.h"
#include "g_game.h"
#include "p_local.h"
#include "r_state.h"
#include "z_zone.h"
#include "w_wad.h"
#include "p_setup.h"
#include "byteptr.h"

byte*           save_p;


// Pads save_p to a 4-byte boundary
//  so that the load/save works on SGI&Gecko.
#ifdef SGISUX
#define PADSAVEP()      save_p += (4 - ((int) save_p & 3)) & 3
#else
#define PADSAVEP()
#endif


//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
    int         i;
    int         j;
    player_t*   dest;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if (!playeringame[i])
            continue;

        PADSAVEP();

        dest = (player_t *)save_p;
        memcpy (dest,&players[i],sizeof(player_t));
        save_p += sizeof(player_t);
        for (j=0 ; j<NUMPSPRITES ; j++)
        {
            if (dest->psprites[j].state)
            {
                dest->psprites[j].state
                    = (state_t *)(dest->psprites[j].state-states);
            }
        }
    }
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
    int         i;
    int         j;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if (!playeringame[i])
            continue;

        PADSAVEP();

        memcpy (&players[i],save_p, sizeof(player_t));
        save_p += sizeof(player_t);

        // will be set when unarc thinker
        players[i].mo = NULL;
        players[i].message = NULL;
        players[i].attacker = NULL;

        for (j=0 ; j<NUMPSPRITES ; j++)
        {
            if (players[i]. psprites[j].state)
            {
                players[i]. psprites[j].state
                    = &states[ (int)players[i].psprites[j].state ];
            }
        }
    }
}

#define SD_FLOORHT  0x01
#define SD_CEILHT   0x02
#define SD_FLOORPIC 0x04
#define SD_CEILPIC  0x08
#define SD_LIGHT    0x10
#define SD_SPECIAL  0x20
//#define SD_TAG      0x40

#define LD_FLAG     0x01
#define LD_SPECIAL  0x02
//#define LD_TAG      0x04
#define LD_S1TEXOFF 0x08
#define LD_S1TOPTEX 0x10
#define LD_S1BOTTEX 0x20
#define LD_S1MIDTEX 0x40
#define LD_DIFF2    0x08

#define LD_S2TEXOFF 0x01
#define LD_S2TOPTEX 0x02
#define LD_S2BOTTEX 0x04
#define LD_S2MIDTEX 0x08


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int                 i;
    int           statsec=0,statline=0;
    //sector_t*     sec;
    line_t*       li;
    side_t*       si;
    byte*         put;

    // reload the map just to see difference
    mapsector_t   *ms;
    mapsidedef_t  *msd;
    maplinedef_t  *mld;
    sector_t      *ss;
    byte          diff,diff2;

    ms = W_CacheLumpNum (lastloadedmaplumpnum+ML_SECTORS,PU_CACHE);
    ss = sectors;
    put = save_p;

    for (i=0 ; i<numsectors ; i++, ss++, ms++)
    {
        diff=0;
        if (ss->floorheight != SHORT(ms->floorheight)<<FRACBITS)
            diff |= SD_FLOORHT;
        if (ss->ceilingheight != SHORT(ms->ceilingheight)<<FRACBITS)
            diff |= SD_CEILHT;
        //
        //  flats
        //
        // P_AddLevelFlat should not add but just return the number
        if (ss->floorpic != P_AddLevelFlat (ms->floorpic,levelflats))
            diff |= SD_FLOORPIC;
        if (ss->ceilingpic != P_AddLevelFlat (ms->ceilingpic,levelflats))
            diff |= SD_CEILPIC;

        if (ss->lightlevel != SHORT(ms->lightlevel))
            diff |= SD_LIGHT;
        if (ss->special != SHORT(ms->special))
            diff |= SD_SPECIAL;
//        if (ss->tag != SHORT(ms->tag))
//            diff |= SD_TAG;

        if(diff)
        {
            statsec++;

            WRITESHORT(put,i);
            WRITEBYTE(put,diff);
            if( diff & SD_FLOORHT )     WRITEFIXED(put,ss->floorheight);
            if( diff & SD_CEILHT  )     WRITEFIXED(put,ss->ceilingheight);
            if( diff & SD_FLOORPIC)
            {
                memcpy(put,levelflats[ss->floorpic].name,8);
                put+=8;
            }
            if( diff & SD_CEILPIC )
            {
                memcpy(put,levelflats[ss->ceilingpic].name,8);
                put+=8;
            }
            if( diff & SD_LIGHT   )     WRITESHORT(put,(short)ss->lightlevel);
            if( diff & SD_SPECIAL )     WRITESHORT(put,(short)ss->special);
//            if( diff & SD_TAG     )
//                *((short *)put)++=(short)ss->tag;
        }
    }
    *((unsigned short *)put)++=0xffff;
    /*
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
        *put++ = sec->floorheight >> FRACBITS;
        *put++ = sec->ceilingheight >> FRACBITS;
        *put++ = sec->floorpic;
        *put++ = sec->ceilingpic;
        *put++ = sec->lightlevel;
        *put++ = sec->special;          // needed?
        *put++ = sec->tag;              // needed?
    }
    */

    mld = W_CacheLumpNum (lastloadedmaplumpnum+ML_LINEDEFS,PU_CACHE);
    msd = W_CacheLumpNum (lastloadedmaplumpnum+ML_SIDEDEFS,PU_CACHE);
    li = lines;
    // do lines
    for (i=0 ; i<numlines ; i++,mld++,li++)
    {
        diff=0;diff2=0;

        if(li->flags != SHORT(mld->flags))
            diff |= LD_FLAG;
        if(li->special != SHORT(mld->special))
            diff |= LD_SPECIAL;
//        if(li->tag != SHORT(mld->tag))
//            diff |= LD_TAG;

        if (li->sidenum[0] != -1)
        {
            si = &sides[li->sidenum[0]];
            if (si->textureoffset != SHORT(msd[li->sidenum[0]].textureoffset)<<FRACBITS)
                diff |= LD_S1TEXOFF;
            if (si->toptexture != R_TextureNumForName(msd[li->sidenum[0]].toptexture) )
                diff |= LD_S1TOPTEX;
            if (si->bottomtexture != R_TextureNumForName(msd[li->sidenum[0]].bottomtexture) )
                diff |= LD_S1BOTTEX;
            if (si->midtexture != R_TextureNumForName(msd[li->sidenum[0]].midtexture) )
                diff |= LD_S1MIDTEX;
        }
        if (li->sidenum[1] != -1)
        {
            si = &sides[li->sidenum[1]];
            if (si->textureoffset != SHORT(msd[li->sidenum[1]].textureoffset)<<FRACBITS)
                diff2 |= LD_S2TEXOFF;
            if (si->toptexture != R_TextureNumForName(msd[li->sidenum[1]].toptexture) )
                diff2 |= LD_S2TOPTEX;
            if (si->bottomtexture != R_TextureNumForName(msd[li->sidenum[1]].bottomtexture) )
                diff2 |= LD_S2BOTTEX;
            if (si->midtexture != R_TextureNumForName(msd[li->sidenum[1]].midtexture) )
                diff2 |= LD_S2MIDTEX;
            if(diff2)
                diff |= LD_DIFF2;

        }

        if(diff)
        {
            statline++;
            WRITESHORT(put,(short)i);
            WRITEBYTE(put,diff);
            if( diff & LD_DIFF2    )     WRITEBYTE(put,diff2);
            if( diff & LD_FLAG     )     WRITESHORT(put,li->flags);
            if( diff & LD_SPECIAL  )     WRITESHORT(put,li->special);

            si = &sides[li->sidenum[0]];
            if( diff & LD_S1TEXOFF )     WRITEFIXED(put,si->textureoffset);
            if( diff & LD_S1TOPTEX )     WRITESHORT(put,si->toptexture);
            if( diff & LD_S1BOTTEX )     WRITESHORT(put,si->bottomtexture);
            if( diff & LD_S1MIDTEX )     WRITESHORT(put,si->midtexture);

            si = &sides[li->sidenum[1]];
            if( diff2 & LD_S2TEXOFF )    WRITEFIXED(put,si->textureoffset);
            if( diff2 & LD_S2TOPTEX )    WRITESHORT(put,si->toptexture);
            if( diff2 & LD_S2BOTTEX )    WRITESHORT(put,si->bottomtexture);
            if( diff2 & LD_S2MIDTEX )    WRITESHORT(put,si->midtexture);
        }
    }
    WRITEUSHORT(put,0xffff);


//    CONS_Printf("sector saved %d/%d, line saved %d/%d\n",statsec,numsectors,statline,numlines);
/*
//        *((short *)put)++ = li->tag;
        for (j=0 ; j<2 ; j++)
        {
            if (li->sidenum[j] == -1)
                continue;


            *put++ = si->textureoffset >> FRACBITS;
// UNMODIFIED
//            *put++ = si->rowoffset >> FRACBITS;
            *put++ = si->toptexture;
            *put++ = si->bottomtexture;
            *put++ = si->midtexture;
        }
    }
*/
    save_p = put;
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int                 i;
//    int         j;
//    sector_t*   sec;
    line_t*     li;
    side_t*     si;
    byte*       get;
    byte        diff,diff2;

    get = save_p;

    while (1)
    {
        i=*((unsigned short *)get)++;

        if (i==0xffff)
            break;

        diff=READBYTE(get);
        if( diff & SD_FLOORHT )  sectors[i].floorheight   = READFIXED(get);
        if( diff & SD_CEILHT )   sectors[i].ceilingheight = READFIXED(get);
        if( diff & SD_FLOORPIC )
        {
            sectors[i].floorpic = P_AddLevelFlat (get,levelflats);
            get+=8;
        }
        if( diff & SD_CEILPIC )
        {
            sectors[i].ceilingpic = P_AddLevelFlat (get,levelflats);
            get+=8;
        }
        if( diff & SD_LIGHT )    sectors[i].lightlevel = READSHORT(get);
        if( diff & SD_SPECIAL )  sectors[i].special    = READSHORT(get);
//        if( diff & SD_TAG )
//            sectors[i].tag=*((short *)get)++;
    }

    /*
    get = (short *)save_p;

    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
        sec->floorheight = *get++ << FRACBITS;
        sec->ceilingheight = *get++ << FRACBITS;
        sec->floorpic = *get++;
        sec->ceilingpic = *get++;
        sec->lightlevel = *get++;
        sec->special = *get++;          // needed?
        sec->tag = *get++;              // needed?
        sec->specialdata = 0;
        sec->soundtarget = 0;
    }
    */

    while(1)
    {
        i=READUSHORT(get);

        if (i==0xffff)
            break;
        diff = *get++;
        li = &lines[i];

        if( diff & LD_DIFF2    )
            diff2 = READBYTE(get);
        else
            diff2 = 0;
        if( diff & LD_FLAG     )    li->flags = READSHORT(get);
        if( diff & LD_SPECIAL  )    li->special = READSHORT(get);

        si = &sides[li->sidenum[0]];
        if( diff & LD_S1TEXOFF )    si->textureoffset = READFIXED(get);
        if( diff & LD_S1TOPTEX )    si->toptexture    = READSHORT(get);
        if( diff & LD_S1BOTTEX )    si->bottomtexture = READSHORT(get);
        if( diff & LD_S1MIDTEX )    si->midtexture    = READSHORT(get);

        si = &sides[li->sidenum[1]];
        if( diff2 & LD_S2TEXOFF )   si->textureoffset = READFIXED(get);
        if( diff2 & LD_S2TOPTEX )   si->toptexture    = READSHORT(get);
        if( diff2 & LD_S2BOTTEX )   si->bottomtexture = READSHORT(get);
        if( diff2 & LD_S2MIDTEX )   si->midtexture    = READSHORT(get);
    }

/*
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
        li->flags = *get++;
        li->special = *get++;
        li->tag = *get++;
        for (j=0 ; j<2 ; j++)
        {
            if (li->sidenum[j] == -1)
                continue;
            si = &sides[li->sidenum[j]];
            si->textureoffset = *get++ << FRACBITS;
            si->rowoffset = *get++ << FRACBITS;
            si->toptexture = *get++;
            si->bottomtexture = *get++;
            si->midtexture = *get++;
        }
    }
*/
    save_p = get;
}


//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj

} thinkerclass_t;


// subset of field to be saved about mobj_t
typedef struct savedmobj_s
{
    void*         pointer;
    fixed_t       x;
    fixed_t       y;
    fixed_t       z;
    angle_t       angle;  // orientation
    spritenum_t   sprite; // used to find patch_t and flip value
    int           frame;  // frame number, plus bits see p_pspr.h
    fixed_t       radius;
    fixed_t       height;
    fixed_t       momx;
    fixed_t       momy;
    fixed_t       momz;
    int           validcount;
    mobjtype_t    type;
    int           tics;   // state tic counter
    int           state;
    int           flags;
    int           eflags;
    int           health;
    int           movecount;
    int           reactiontime;
    int           threshold;
    int           lastlook;
    mapthing_t    spawnpoint;
    fixed_t       waterz;
    byte          movedir;        // 0-7
    byte          player;
    void*         tracer;
    void*         target;
    //SOM: Fuses should be in save games
    int           fuse;
} savedmobj_t;



//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
    thinker_t*          th;
    savedmobj_t*        mobj;

    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
        if (th->function.acp1 == (actionf_p1)P_MobjThinker)
        {
            *save_p++ = tc_mobj;
            PADSAVEP();
            mobj = (savedmobj_t *)save_p;
            mobj->pointer    =  th;
            mobj->x          =  ((mobj_t *)th)->x;
            mobj->y          =  ((mobj_t *)th)->y;
            mobj->z          =  ((mobj_t *)th)->z;
            mobj->angle      =  ((mobj_t *)th)->angle;
            mobj->sprite     =  ((mobj_t *)th)->sprite;
            mobj->frame      =  ((mobj_t *)th)->frame;
            mobj->radius     =  ((mobj_t *)th)->radius;
            mobj->height     =  ((mobj_t *)th)->height;
            mobj->momx       =  ((mobj_t *)th)->momx;
            mobj->momy       =  ((mobj_t *)th)->momy;
            mobj->momz       =  ((mobj_t *)th)->momz;
            mobj->validcount =  ((mobj_t *)th)->validcount;
            mobj->type       =  ((mobj_t *)th)->type;
            mobj->tics       =  ((mobj_t *)th)->tics;
            mobj->state      =  ((mobj_t *)th)->state - states;
            mobj->flags      =  ((mobj_t *)th)->flags;
            mobj->eflags     =  ((mobj_t *)th)->eflags;
            mobj->health     =  ((mobj_t *)th)->health;
            mobj->movedir    =  ((mobj_t *)th)->movedir;
            mobj->movecount  =  ((mobj_t *)th)->movecount;
            mobj->reactiontime =  ((mobj_t *)th)->reactiontime;
            mobj->threshold  =  ((mobj_t *)th)->threshold;
            if(((mobj_t *)th)->player)
                mobj->player =  (((mobj_t *)th)->player-players) + 1;
            else
                mobj->player =  0;
            mobj->lastlook   =  ((mobj_t *)th)->lastlook;
            mobj->spawnpoint =  ((mobj_t *)th)->spawnpoint;
            mobj->waterz     =  ((mobj_t *)th)->waterz;

            mobj->target     =  ((mobj_t *)th)->target;
            mobj->tracer     =  ((mobj_t *)th)->tracer;
            //SOM: Put fuse in SG
            mobj->fuse       =  ((mobj_t *)th)->fuse;

            save_p += sizeof(savedmobj_t);
        }

        // I_Error ("P_ArchiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    *save_p++ = tc_end;
}

// Now save the pointers, tracer and target, but at load time we must
// relink to this, the savegame contain the old position in the pointer
// field copyed in the info field temporarely, but finaly we just search
// for to old postion and relink to
mobj_t *FindNewPosition(void *oldposition)
{
    thinker_t*          th;
    mobj_t*             mobj;

    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
        mobj = (mobj_t *)th;
        if( (void *)mobj->info == oldposition)
            return mobj;
    }
    if(devparm) CONS_Printf("\2not found\n");
    return NULL;
}

//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
    byte                tclass;
    thinker_t*          currentthinker;
    thinker_t*          next;
    mobj_t*             mobj;
    savedmobj_t         *th;

    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
        next = currentthinker->next;

        if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
            P_RemoveMobj ((mobj_t *)currentthinker);
        else
            Z_Free (currentthinker);

        currentthinker = next;
    }
    P_InitThinkers ();

    // read in saved thinkers
    while (1)
    {
        tclass = *save_p++;
        switch (tclass)
        {
          case tc_end:
            goto c_sux;     // end of list

          case tc_mobj:
            PADSAVEP();
            mobj = Z_Malloc (sizeof(mobj_t), PU_LEVEL, NULL);
            memset (mobj, 0, sizeof (mobj_t));
            th = (savedmobj_t *)save_p;
            mobj->x     =  th->x;
            mobj->y     =  th->y;
            mobj->z     =  th->z;
            mobj->angle =  th->angle;
            mobj->sprite=  th->sprite;
            mobj->frame =  th->frame;
            mobj->radius=  th->radius;
            mobj->height=  th->height;
            mobj->momx  =  th->momx;
            mobj->momy  =  th->momy;
            mobj->momz  =  th->momz;
            mobj->validcount =  th->validcount;
            mobj->type =  th->type;
            mobj->tics =  th->tics;
            mobj->state = &states[th->state];
            mobj->flags =  th->flags;
            mobj->eflags =  th->eflags;
            mobj->health =  th->health;
            mobj->movedir =  th->movedir;
            mobj->movecount  =  th->movecount;
            mobj->reactiontime =  th->reactiontime;
            mobj->threshold  =  th->threshold;
            mobj->lastlook   =  th->lastlook;
            mobj->spawnpoint =  th->spawnpoint;
            mobj->waterz     =  th->waterz;

            mobj->target     = th->target;
            mobj->tracer     = th->tracer;
            //SOM: Get fuse from SG
            mobj->fuse       = th->fuse;
            mobj->info       = (mobjinfo_t *)th->pointer;  // temporarely, set when leave this function

            if (th->player)
            {
                // added for angle prediction
                if(consoleplayer==th->player-1)
                    localangle=mobj->angle;
                if(secondarydisplayplayer==th->player-1)
                    localangle2=mobj->angle;

                mobj->player = &players[th->player-1];
                mobj->player->mo = mobj;
            }

            // now set deductable field
            mobj->skin = NULL;
            //mobj->info = &mobjinfo[mobj->type];

            // set sprev, snext, bprev, bnext, subsector
            P_SetThingPosition (mobj);

            mobj->floorz = mobj->subsector->sector->floorheight;
            mobj->ceilingz = mobj->subsector->sector->ceilingheight;
            mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
            P_AddThinker (&mobj->thinker);
            save_p += sizeof(savedmobj_t);
            break;

          default:
            I_Error ("Unknown tclass %i in savegame",tclass);
        }

    }

c_sux:
    for (currentthinker = thinkercap.next ; currentthinker != &thinkercap ; currentthinker=currentthinker->next)
    {
         mobj = (mobj_t *)currentthinker;
         if (mobj->tracer)
             mobj->tracer = FindNewPosition(mobj->tracer);
         if (mobj->target)
             mobj->target = FindNewPosition(mobj->target);
    }
    for (currentthinker = thinkercap.next ; currentthinker != &thinkercap ; currentthinker=currentthinker->next)
    {
         mobj = (mobj_t *)currentthinker;
         mobj->info = &mobjinfo[mobj->type];
    }
}


//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials

} specials_e;



//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void P_ArchiveSpecials (void)
{
    thinker_t*          th;
    ceiling_t*          ceiling;
    vldoor_t*           door;
    floormove_t*        floor;
    plat_t*             plat;
    lightflash_t*       flash;
    strobe_t*           strobe;
    glow_t*             glow;
    int                 i;

    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
        if (th->function.acv == (actionf_v)NULL)
        {
            for (i = 0; i < MAXCEILINGS;i++)
                if (activeceilings[i] == (ceiling_t *)th)
                    break;

            if (i<MAXCEILINGS)
            {
                *save_p++ = tc_ceiling;
                PADSAVEP();
                ceiling = (ceiling_t *)save_p;
                memcpy (ceiling, th, sizeof(*ceiling));
                save_p += sizeof(*ceiling);
                ceiling->sector = (sector_t *)(ceiling->sector - sectors);
            }
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
        {
            *save_p++ = tc_ceiling;
            PADSAVEP();
            ceiling = (ceiling_t *)save_p;
            memcpy (ceiling, th, sizeof(*ceiling));
            save_p += sizeof(*ceiling);
            ceiling->sector = (sector_t *)(ceiling->sector - sectors);
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_VerticalDoor)
        {
            *save_p++ = tc_door;
            PADSAVEP();
            door = (vldoor_t *)save_p;
            memcpy (door, th, sizeof(*door));
            save_p += sizeof(*door);
            door->sector = (sector_t *)(door->sector - sectors);
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_MoveFloor)
        {
            *save_p++ = tc_floor;
            PADSAVEP();
            floor = (floormove_t *)save_p;
            memcpy (floor, th, sizeof(*floor));
            save_p += sizeof(*floor);
            floor->sector = (sector_t *)(floor->sector - sectors);
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_PlatRaise)
        {
            *save_p++ = tc_plat;
            PADSAVEP();
            plat = (plat_t *)save_p;
            memcpy (plat, th, sizeof(*plat));
            save_p += sizeof(*plat);
            plat->sector = (sector_t *)(plat->sector - sectors);
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_LightFlash)
        {
            *save_p++ = tc_flash;
            PADSAVEP();
            flash = (lightflash_t *)save_p;
            memcpy (flash, th, sizeof(*flash));
            save_p += sizeof(*flash);
            flash->sector = (sector_t *)(flash->sector - sectors);
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
        {
            *save_p++ = tc_strobe;
            PADSAVEP();
            strobe = (strobe_t *)save_p;
            memcpy (strobe, th, sizeof(*strobe));
            save_p += sizeof(*strobe);
            strobe->sector = (sector_t *)(strobe->sector - sectors);
            continue;
        }

        if (th->function.acp1 == (actionf_p1)T_Glow)
        {
            *save_p++ = tc_glow;
            PADSAVEP();
            glow = (glow_t *)save_p;
            memcpy (glow, th, sizeof(*glow));
            save_p += sizeof(*glow);
            glow->sector = (sector_t *)(glow->sector - sectors);
            continue;
        }
    }

    // add a terminating marker
    *save_p++ = tc_endspecials;

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
    byte                tclass;
    ceiling_t*          ceiling;
    vldoor_t*           door;
    floormove_t*        floor;
    plat_t*             plat;
    lightflash_t*       flash;
    strobe_t*           strobe;
    glow_t*             glow;


    // read in saved thinkers
    while (1)
    {
        tclass = *save_p++;
        switch (tclass)
        {
          case tc_endspecials:
            return;     // end of list

          case tc_ceiling:
            PADSAVEP();
            ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
            memcpy (ceiling, save_p, sizeof(*ceiling));
            save_p += sizeof(*ceiling);
            ceiling->sector = &sectors[(int)ceiling->sector];
            ceiling->sector->specialdata = ceiling;

            if (ceiling->thinker.function.acp1)
                ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;

            P_AddThinker (&ceiling->thinker);
            P_AddActiveCeiling(ceiling);
            break;

          case tc_door:
            PADSAVEP();
            door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
            memcpy (door, save_p, sizeof(*door));
            save_p += sizeof(*door);
            door->sector = &sectors[(int)door->sector];
            door->sector->specialdata = door;
            door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
            P_AddThinker (&door->thinker);
            break;

          case tc_floor:
            PADSAVEP();
            floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
            memcpy (floor, save_p, sizeof(*floor));
            save_p += sizeof(*floor);
            floor->sector = &sectors[(int)floor->sector];
            floor->sector->specialdata = floor;
            floor->thinker.function.acp1 = (actionf_p1)T_MoveFloor;
            P_AddThinker (&floor->thinker);
            break;

          case tc_plat:
            PADSAVEP();
            plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
            memcpy (plat, save_p, sizeof(*plat));
            save_p += sizeof(*plat);
            plat->sector = &sectors[(int)plat->sector];
            plat->sector->specialdata = plat;

            if (plat->thinker.function.acp1)
                plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;

            P_AddThinker (&plat->thinker);
            P_AddActivePlat(plat);
            break;

          case tc_flash:
            PADSAVEP();
            flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
            memcpy (flash, save_p, sizeof(*flash));
            save_p += sizeof(*flash);
            flash->sector = &sectors[(int)flash->sector];
            flash->thinker.function.acp1 = (actionf_p1)T_LightFlash;
            P_AddThinker (&flash->thinker);
            break;

          case tc_strobe:
            PADSAVEP();
            strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
            memcpy (strobe, save_p, sizeof(*strobe));
            save_p += sizeof(*strobe);
            strobe->sector = &sectors[(int)strobe->sector];
            strobe->thinker.function.acp1 = (actionf_p1)T_StrobeFlash;
            P_AddThinker (&strobe->thinker);
            break;

          case tc_glow:
            PADSAVEP();
            glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
            memcpy (glow, save_p, sizeof(*glow));
            save_p += sizeof(*glow);
            glow->sector = &sectors[(int)glow->sector];
            glow->thinker.function.acp1 = (actionf_p1)T_Glow;
            P_AddThinker (&glow->thinker);
            break;

          default:
            I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
                     "in savegame",tclass);
        }

    }

}
