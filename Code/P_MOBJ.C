// p_mobj.c :  Moving object handling. Spawn functions.

#include "doomdef.h"
#include "g_game.h"
#include "g_input.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "p_local.h"
#include "p_inter.h"
#include "p_setup.h"    //levelflats to test if mobj in water sector
#include "r_main.h"
#include "r_things.h"
#include "r_sky.h"
#include "s_sound.h"
#include "z_zone.h"
#include "m_random.h"

#ifdef WALLSPLAT
#include "r_splats.h"   //faB: in dev.
#endif

    player_t *plyr;

// protos.
void CV_ViewHeight_OnChange (void);
void CV_Gravity_OnChange (void);

CV_PossibleValue_t viewheight_cons_t[]={{16,"MIN"},{56,"MAX"},{0,NULL}};

consvar_t cv_viewheight = {"viewheight","41",0,viewheight_cons_t,NULL};

//Fab:26-07-98:
consvar_t cv_gravity = {"gravity","0.5",CV_NETVAR|CV_FLOAT|CV_CALL|CV_NOINIT,NULL,CV_Gravity_OnChange}; // No longer required in autoexec.cfg! Tails 12-01-99

// just echo to everybody for multiplayer
void CV_Gravity_OnChange (void)
{
    CONS_Printf ("gravity set to %s\n", cv_gravity.string);
}

//
// P_SetMobjState
// Returns true if the mobj is still present.
//

boolean P_SetMobjState ( mobj_t*       mobj,
                         statenum_t    state )
{
    state_t*    st;

    do
    {
        if (state == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_RemoveMobj (mobj);
            return false;
        }

        st = &states[state];
        mobj->state = st;
        mobj->tics = st->tics;
        mobj->sprite = st->sprite;
        mobj->frame = st->frame;   // FF_FRAMEMASK for frame, and other bits

        // Modified handling.
        // Call action functions when the state is set
        if (st->action.acp1)
            st->action.acp1(mobj);

        state = st->nextstate;
    } while (!mobj->tics);

    return true;
}


//
// P_ExplodeMissile
//
void P_ExplodeMissile (mobj_t* mo)
{
    mo->momx = mo->momy = mo->momz = 0;

    P_SetMobjState (mo, mobjinfo[mo->type].deathstate);

    mo->tics -= P_Random()&3;

    if (mo->tics < 1)
        mo->tics = 1;

    mo->flags &= ~MF_MISSILE;

    if (mo->info->deathsound)
        S_StartSound (mo, mo->info->deathsound);
}


//
// P_XYMovement
//
#define STOPSPEED               0xffff
#define FRICTION                0xe800   //0.90625

//added:22-02-98: adds friction on the xy plane
void P_XYFriction (mobj_t* mo)
{
    //valid only if player avatar
    player_t*   player = mo->player;

    if (mo->momx > -STOPSPEED
        && mo->momx < STOPSPEED
        && mo->momy > -STOPSPEED
        && mo->momy < STOPSPEED
        && (!player
            || (player->cmd.forwardmove== 0
                && player->cmd.sidemove == 0 ) ) )
    {
        // if in a walking frame, stop moving
        if ( player&&(unsigned)((player->mo->state - states)- S_PLAY_RUN1) < 64)
            P_SetMobjState (player->mo, S_PLAY);

        mo->momx = 0;
        mo->momy = 0;
    }
    else
    {
        mo->momx = FixedMul (mo->momx, FRICTION);
        mo->momy = FixedMul (mo->momy, FRICTION);
    }
}

void P_XYMovement (mobj_t* mo)
{
    fixed_t     ptryx;
    fixed_t     ptryy;
    player_t*   player;
    fixed_t     xmove;
    fixed_t     ymove;

    //added:18-02-98: if it's stopped
    if (!mo->momx && !mo->momy)
    {
        if (mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->momx = mo->momy = mo->momz = 0;

            //added:18-02-98: comment: set in 'search new direction' state?
            P_SetMobjState (mo, mo->info->spawnstate);
        }
        return;
    }

    player = mo->player;        //valid only if player avatar

    if (mo->momx > MAXMOVE)
        mo->momx = MAXMOVE;
    else if (mo->momx < -MAXMOVE)
        mo->momx = -MAXMOVE;

    if (mo->momy > MAXMOVE)
        mo->momy = MAXMOVE;
    else if (mo->momy < -MAXMOVE)
        mo->momy = -MAXMOVE;

    xmove = mo->momx;
    ymove = mo->momy;

    do
    {
        if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
        {
            ptryx = mo->x + xmove/2;
            ptryy = mo->y + ymove/2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = 0;
        }

        if (!P_TryMove (mo, ptryx, ptryy))
        {
            // blocked move

            // gameplay issue : let the marine move forward while trying
            //                  to jump over a small wall
            //    (normally it can not 'walk' while in air)
            // BP:1.28 no more use Cf_JUMPOVER, but i have put it for backward lmp compatibility
 //    Removed this code....not needed for SRB2-- Tails 10-24-99
 //           if (mo->player)
 //           {
 //               if (tmfloorz - mo->z > MAXSTEPMOVE)
 //                   if (mo->momz > 0)
 //                       mo->player->cheats |= CF_JUMPOVER;
 //                   else
 //                       mo->player->cheats &= ~CF_JUMPOVER;
 //          }

            //added:26-02-98: slidemove also for ChaseCam
            // note : the SPIRIT have a valide player field
            if (mo->player || mo->type==MT_CHASECAM)
            {   // try to slide along it
                P_SlideMove (mo);
            }
            else if (mo->flags & MF_MISSILE)
            {
                // explode a missile
                if (ceilingline &&
                    ceilingline->backsector &&
                    ceilingline->backsector->ceilingpic == skyflatnum)
                {
                    // Hack to prevent missiles exploding
                    // against the sky.
                    // Does not handle sky floors.
                    P_RemoveMobj (mo);
                    return;
                }

                // draw damage on wall
                //SPLAT TEST ----------------------------------------------------------
                #ifdef WALLSPLATS
                if (blockingline)   //set by last P_TryMove() that failed
                {
                    divline_t   divl;
                    divline_t   misl;
                    fixed_t     frac;

                    P_MakeDivline (blockingline, &divl);
                    misl.x = mo->x;
                    misl.y = mo->y;
                    misl.dx = mo->momx;
                    misl.dy = mo->momy;
                    frac = P_InterceptVector (&divl, &misl);
                    R_AddWallSplat (blockingline, "A_DMG3", mo->z, frac, SPLATDRAWMODE_SHADE);
                }
                #endif
                // --------------------------------------------------------- SPLAT TEST

                P_ExplodeMissile (mo);
            }
            else
                mo->momx = mo->momy = 0;
        }
        else
            // hack for playability : walk in-air to jump over a small wall
            if (mo->player)
                mo->player->cheats &= ~CF_JUMPOVER;


    } while (xmove || ymove);

    // slow down
    if (player)
    {
        if (player->cheats & CF_NOMOMENTUM)
        {
            // debug option for no sliding at all
            mo->momx = mo->momy = 0;
            return;
        }
        else
        if (player->cheats & CF_FLYAROUND)
        {
            P_XYFriction (mo);
            return;
        }
    }

    if (mo->flags & (MF_MISSILE | MF_SKULLFLY) )
        return;         // no friction for missiles ever

    // slow down in water, not too much for playability issues
    if (demoversion>=128 && (mo->eflags & MF_UNDERWATER))
    {
        mo->momx = FixedMul (mo->momx, FRICTION*3/4);
        mo->momy = FixedMul (mo->momy, FRICTION*3/4);
        return;
    }

// start ice! Tails 11-29-99
    if ((player && player->specialsector == 689) && mo->z <= mo->floorz) // Crash fix SSNTails 08-19-2023
       {
        mo->momx = FixedMul (mo->momx, FRICTION*1.1);
        mo->momy = FixedMul (mo->momy, FRICTION*1.1);
        return;
       }
// end ice! Tails 11-29-99
// start spinning friction Tails 02-28-2000
   if (player && (player->mo->eflags & MF_SPINNING)) // Crash fix SSNTails 08-19-2023
      {
        mo->momx = FixedMul (mo->momx, FRICTION*1.1);
        mo->momy = FixedMul (mo->momy, FRICTION*1.1);
        return;
       }
// end spinning friction Tails 02-28-2000

    if (mo->z > mo->floorz)
        return;         // no friction when airborne

    if (mo->flags & MF_CORPSE)
    {
        // do not stop sliding
        //  if halfway off a step with some momentum
        if (mo->momx > FRACUNIT/4
            || mo->momx < -FRACUNIT/4
            || mo->momy > FRACUNIT/4
            || mo->momy < -FRACUNIT/4)
        {
            if (mo->floorz != mo->subsector->sector->floorheight)
                return;
        }
    }
    P_XYFriction (mo);

}

//
// P_ZMovement
//
void P_ZMovement (mobj_t* mo)
{
    fixed_t     dist;
    fixed_t     delta;

    // check for smooth step up
    if (mo->player && mo->z < mo->floorz && mo->type!=MT_SPIRIT)
    {
        mo->player->viewheight -= mo->floorz - mo->z;

        mo->player->deltaviewheight
            = ((cv_viewheight.value<<FRACBITS) - mo->player->viewheight)>>3;
    }

    // adjust height
    mo->z += mo->momz;

    if ( mo->flags & MF_FLOAT
         && mo->target)
    {
        // float down towards target if too close
        if ( !(mo->flags & MF_SKULLFLY)
             && !(mo->flags & MF_INFLOAT) )
        {
            dist = P_AproxDistance (mo->x - mo->target->x,
                                    mo->y - mo->target->y);

            delta =(mo->target->z + (mo->height>>1)) - mo->z;

            if (delta<0 && dist < -(delta*3) )
                mo->z -= FLOATSPEED;
            else if (delta>0 && dist < (delta*3) )
                mo->z += FLOATSPEED;
        }

    }

    // clip movement

    if (mo->z <= mo->floorz)
    {
        // hit the floor

        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        if (mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->momz = -mo->momz;
        }
// start spin -- Stealth
        if (mo->player)
        {
         if((mo->momz || !(mo->momx || mo->momy)) && (mo->eflags & MF_SPINNING) && mo->health)
         {
          mo->eflags &= ~MF_SPINNING;
          P_SetMobjState (mo, S_PLAY);
         }
        }
// end spin -- Stealth
        if (mo->momz < 0) // falling
        {
            if (mo->player && (mo->momz < -8*FRACUNIT))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->player->deltaviewheight = mo->momz>>3;
 //               S_StartSound (mo, sfx_oof); Don't say OOF!! Tails 11-05-99
            }

            // set it once and not continuously
            // if (mo->z < mo->floorz)        ORIGINAL
            // mo->eflags |= MF_JUSTHITFLOOR; ORIGINAL
            //SOM: It must be <= not just <
            if(mo->eflags & MF_JUSTJUMPED)
              mo->eflags &= ~MF_JUSTJUMPED;
// Stealth
            if (mo->z <= mo->floorz) // Tails 9-15-99 Spin Attack
              {
              mo->eflags |= MF_JUSTHITFLOOR; // Tails 9-15-99 Spin Attack
              if(mo->eflags & MF_JUMPED) // Tails 9-15-99 Spin Attack
              mo->eflags |= MF_JUSTJUMPED;
              mo->eflags &= ~MF_JUMPED; // Tails 9-15-99 Spin Attack
                }

// end Stealth

             if(!(mo->eflags & MF_SPINNING))
                {
                 mo->eflags &= ~MF_STARTDASH; // dashing stuff tails 02-27-2000
                 }

            //SOM: Flingrings bounce
            if(mo->type == MT_FLINGRING)
              mo->momz = -mo->momz * 0.7;
            else
              mo->momz = 0;
        }

        mo->z = mo->floorz;

        if ( (mo->flags & MF_MISSILE)
             && !(mo->flags & MF_NOCLIP) )
        {
            P_ExplodeMissile (mo);
            return;
        }
    }
    else if (! (mo->flags & MF_NOGRAVITY) )             // Gravity here!
    {
        fixed_t     gravityadd;
        
        //Fab: NOT SURE WHETHER IT IS USEFUL, just put it here too
        //     TO BE SURE there is no problem for the release..
        //     (this is done in P_Mobjthinker below normally)
//        mo->eflags &= ~MF_JUSTHITFLOOR;
        
/*
        if (mo->momz == 0)
            mo->momz = -cv_gravity.value*2;      // push down
        else
            mo->momz -= cv_gravity.value;        // accelerate fall
*/
        gravityadd = -cv_gravity.value;

        // if waist under water, slow down the fall
        if ( mo->eflags & MF_UNDERWATER) {
            if ( mo->eflags & MF_SWIMMING )
                gravityadd = 0;     // gameplay: no gravity while swimming
            else
                gravityadd >>= 2;
        }
        else if (mo->momz==0)
            // mobj at stop, no floor, so feel the push of gravity!
            gravityadd <<= 1;

        mo->momz += gravityadd;
    }

    if (mo->z + mo->height > mo->ceilingz)
    {
        mo->z = mo->ceilingz - mo->height;

        //added:22-02-98: player avatar hits his head on the ceiling, ouch!
        if (mo->player && (demoversion>=112)  
            && !(mo->player->cheats & CF_FLYAROUND) && mo->momz>8*FRACUNIT )
            S_StartSound (mo, sfx_ouch);

        // hit the ceiling
        if (mo->momz > 0)
            mo->momz = 0;

        if (mo->flags & MF_SKULLFLY)
        {       // the skull slammed into something
            mo->momz = -mo->momz;
        }

        if ( (mo->flags & MF_MISSILE)
             && !(mo->flags & MF_NOCLIP) )
        {
            P_ExplodeMissile (mo);
            return;
        }
    }

    // z friction in water
    if (demoversion>=128 && 
        ((mo->eflags & MF_TOUCHWATER) || (mo->eflags & MF_UNDERWATER)) && 
        !(mo->flags & (MF_MISSILE | MF_SKULLFLY)))
    {
         mo->momz = FixedMul (mo->momz, FRICTION*1.02); // Tails 9-24-99
    }

}



//
// P_NightmareRespawn
//
void
P_NightmareRespawn (mobj_t* mobj)
{
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;
    subsector_t*        ss;
    mobj_t*             mo;
    mapthing_t*         mthing;

    x = mobj->spawnpoint.x << FRACBITS;
    y = mobj->spawnpoint.y << FRACBITS;

    // somthing is occupying it's position?
    if (!P_CheckPosition (mobj, x, y) )
        return; // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = P_SpawnMobj (mobj->x,
                      mobj->y,
                      mobj->subsector->sector->floorheight , MT_TFOG);
    // initiate teleport sound
    S_StartSound (mo, sfx_telept);

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector (x,y);

    mo = P_SpawnMobj (x, y, ss->sector->floorheight , MT_TFOG);

    S_StartSound (mo, sfx_telept);

    // spawn the new monster
    mthing = &mobj->spawnpoint;

    // spawn it
    if (mobj->info->flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    // inherit attributes from deceased one
    mo = P_SpawnMobj (x,y,z, mobj->type);
    mo->spawnpoint = mobj->spawnpoint;
    mo->angle = ANG45 * (mthing->angle/45);

    if (mthing->options & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->reactiontime = 18;

    // remove the old monster,
    P_RemoveMobj (mobj);
}


consvar_t cv_respawnmonsters = {"respawnmonsters","0",CV_NETVAR,CV_OnOff};
consvar_t cv_respawnmonsterstime = {"respawnmonsterstime","12",CV_NETVAR,NULL};


//
// P_MobjCheckWater : check for water, set stuff in mobj_t struct for
//                    movement code later, this is called either by P_MobjThinker() or P_PlayerThink()
void P_MobjCheckWater (mobj_t* mobj)
{
    sector_t* sector;
    fixed_t   z;
    int       oldeflags;

    if( demoversion<128 || mobj->type==MT_SPLASH) // splash don't do splash
        return;
    //
    // see if we are in water, and set some flags for later
    //
    sector = mobj->subsector->sector;
    oldeflags = mobj->eflags;
    // current water hack uses negative sector, and FWATER floorpic
    //SOM: Hack to use watersector insted of tag
    if (sector->watersector || levelflats[sector->floorpic].iswater)
    {
        if (sector->watersector)  //water hack
            z = sector->watersector->floorheight;
        else
            z = sector->floorheight + (FRACUNIT/4); // water texture

        // save for P_XYMovement()
        mobj->waterz = z;

        if (mobj->z<=z && mobj->z+mobj->height > z)
            mobj->eflags |= MF_TOUCHWATER;
        else
            mobj->eflags &= ~MF_TOUCHWATER;

        if (mobj->z+(mobj->height>>1) <= z)
            mobj->eflags |= MF_UNDERWATER;
        else
            mobj->eflags &= ~MF_UNDERWATER;
                }
    else
        mobj->eflags &= ~(MF_UNDERWATER|MF_TOUCHWATER);
/*
    if( (mobj->eflags ^ oldeflags) & MF_TOUCHWATER)
        CONS_Printf("touchewater %d\n",mobj->eflags & MF_TOUCHWATER ? 1 : 0);
    if( (mobj->eflags ^ oldeflags) & MF_UNDERWATER)
        CONS_Printf("underwater %d\n",mobj->eflags & MF_UNDERWATER ? 1 : 0);
*/
    // blood doesnt make noise when it falls in water
    if(  !(oldeflags & (MF_TOUCHWATER|MF_UNDERWATER)) 
       && ((mobj->eflags & MF_TOUCHWATER) || 
           (mobj->eflags & MF_UNDERWATER)    )         
      && mobj->type != MT_BLOOD)
        P_SpawnSplash (mobj, (sector->tag>=0));
}


//
// P_MobjThinker
//
void P_MobjThinker (mobj_t* mobj)
{
    boolean   checkedpos = false;  //added:22-02-98:

    // check mobj against possible water content, before movement code
    P_MobjCheckWater (mobj);

    if(mobj->player && mobj->eflags & MF_JUSTHITFLOOR && mobj->z==mobj->floorz && mobj->health)
    {
     P_SetMobjState (mobj, S_PLAY);
     if(mobj->eflags & MF_ONGROUND)
     {
      if(mobj->eflags & MF_JUMPED)
      {
       mobj->eflags &= ~MF_JUMPED;
      }
      if(mobj->eflags & MF_JUSTJUMPED)
      {
       mobj->eflags &= ~MF_JUSTJUMPED;
      }
//     if(mobj->eflags & ~MF_ONGROUND)
//     {
//      mobj->eflags &= MF_ONGROUND;
//     }
     }
    }

    //SOM: Check fuse
    if(mobj->fuse) {
      mobj->fuse --;
      if(!mobj->fuse) {
        if(mobj->info->deathstate)
          P_ExplodeMissile(mobj);
        else
          P_SetMobjState(mobj, S_DISS); // make sure they dissapear tails
        }
      }
    

    //
    // momentum movement
    //
    if ( mobj->momx ||
         mobj->momy ||
        (mobj->flags&MF_SKULLFLY) )
    {
        P_XYMovement (mobj);
        checkedpos = true;

        // FIXME: decent NOP/NULL/Nil function pointer please.
        if (mobj->thinker.function.acv == (actionf_v) (-1))
            return;             // mobj was removed
    }

    //added:28-02-98: always do the gravity bit now, that's simpler
    //                BUT CheckPosition only if wasn't do before.
    if ( !( (mobj->eflags & MF_ONGROUND) &&
            (mobj->z == mobj->floorz) &&
            !mobj->momz
          ) )
    {
        // if didnt check things Z while XYMovement, do the necessary now
        if (!checkedpos && (demoversion>=112))
        {
            // FIXME : should check only with things, not lines
            P_CheckPosition (mobj, mobj->x, mobj->y);

            /* ============ BIG DIRTY MESS : FIXME FAB!!! =============== */
            mobj->floorz = tmfloorz;
            mobj->ceilingz = tmceilingz;
            if (tmfloorthing)
                mobj->eflags &= ~MF_ONGROUND;  //not on real floor
            else
                mobj->eflags |= MF_ONGROUND;
          //      mobj->eflags -= MF_JUMPED;
            /* ============ BIG DIRTY MESS : FIXME FAB!!! =============== */

            // now mobj->floorz should be the current sector's z floor
            // or a valid thing's top z
        }

        P_ZMovement (mobj);

        // FIXME: decent NOP/NULL/Nil function pointer please.
        if (mobj->thinker.function.acv == (actionf_v) (-1))
            return;             // mobj was removed
    }
    else
        mobj->eflags &= ~MF_JUSTHITFLOOR;

    // cycle through states,
    // calling action functions at transitions
    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic
        if (!mobj->tics)
            if (!P_SetMobjState (mobj, mobj->state->nextstate) )
                return;         // freed itself
    }
    else
    {
        // check for nightmare respawn
        if (! (mobj->flags & MF_COUNTKILL) )
            return;

        if (!cv_respawnmonsters.value)
            return;

        mobj->movecount++;

        if (mobj->movecount < cv_respawnmonsterstime.value*TICRATE)
            return;

        if ( leveltime&31 )
            return;

        if (P_Random () > 4)
            return;

        P_NightmareRespawn (mobj);
    }
// shield positions -- Stealth
/*    if(mobj->type==MT_GREENORB || mobj->type==MT_BLUEORB  || mobj->type==MT_YELLOWORB|| mobj->type==MT_BLACKORB || mobj->type==MT_IVSR || mobj->type==MT_IVSS)
    {
     P_UnsetThingPosition (mobj);
     mobj->x=source->player->mo->x;
     mobj->y=source->player->mo->y;
     mobj->z=source->player->mo->z + ((source->player->mo->info->height /2) -4);
     mobj->angle=source->player->mo->angle;
     P_SetThingPosition (mobj);
    }

    if(mobj->type==MT_MISC2)
    {
     if(mobj->x<plyr->mo->x && mobj->momx < 12)
     {
      mobj->momx++;
     }
     if(mobj->x>plyr->mo->x && mobj->momx > -12)
     {
      mobj->momx--;
     }
     if(mobj->y<plyr->mo->y && mobj->momy < 12)
     {
      mobj->momy++;
     }
     if(mobj->y>plyr->mo->y && mobj->momy > -12)
     {
      mobj->momy--;
     }
     if(mobj->z<plyr->mo->z && mobj->momz < 12)
     {
      mobj->momz++;
     }
     if(mobj->z>plyr->mo->z && mobj->momz > -12)
     {
      mobj->momz--;
     }
//     mobj->angle = R_PointToAngle2 (mobj->x,
//                                    mobj->y,
//                                    plyr->mo->x,
//                                    plyr->mo->y);
//     P_Thrust(mobj, mobj->angle, 10);
//     P_MoveRing(mobj);
     P_UnsetThingPosition (mobj);
     mobj->momx=5;
     mobj->x+=mobj->momx;
     mobj->y+=mobj->momy;
     P_SetThingPosition (mobj);
    }

//      if(mobj->type==MT_TROOP)
// start more thoking -- Stealth
if(!(mobj->type==MT_PLAYER) && (mobj->flags & MF_SHOOTABLE))
      {
       if(plyr->wants_to_thok)
       {
            S_StartSound (plyr->mo, sfx_pdiehi);
        if(  (abs(plyr->mo->x-mobj->x) + abs(plyr->mo->y-mobj->y)) < plyr->thok_dist || plyr->thok_dist==0)
        {
            S_StartSound (plyr->mo, sfx_pdiehi);
         plyr->thok_dist=abs(plyr->mo->x-mobj->x) + abs(plyr->mo->y-mobj->y);
         plyr->mo->angle=R_PointToAngle2(plyr->mo->x, plyr->mo->y, mobj->x, mobj->y);
         plyr->thok_angle=R_PointToAngle2(plyr->mo->x, plyr->mo->z, mobj->x, mobj->z);
        }
       }
      } */
}
// end more thoking -- Stealth

//
// P_SpawnMobj
//
mobj_t* P_SpawnMobj ( fixed_t       x,
                      fixed_t       y,
                      fixed_t       z,
                      mobjtype_t    type )
{
    mobj_t*     mobj;
    state_t*    st;
    mobjinfo_t* info;

    mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
    memset (mobj, 0, sizeof (*mobj));
    info = &mobjinfo[type];

    mobj->type = type;
    mobj->info = info;
    mobj->x = x;
    mobj->y = y;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;

    mobj->health = info->spawnhealth;

    if (gameskill != sk_nightmare)
        mobj->reactiontime = info->reactiontime;

    // added 4-9-98: dont get out of synch
    if (mobj->type == MT_SPIRIT || mobj->type == MT_CHASECAM)
        mobj->lastlook = 0;
    else
        mobj->lastlook = P_Random () % MAXPLAYERS;

    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet
    st = &states[info->spawnstate];

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame; // FF_FRAMEMASK for frame, and other bits..

    // set subsector and/or block links
    P_SetThingPosition (mobj);

    mobj->floorz = mobj->subsector->sector->floorheight;
    mobj->ceilingz = mobj->subsector->sector->ceilingheight;

    //added:27-02-98: if ONFLOORZ, stack the things one on another
    //                so they do not occupy the same 3d space
    //                allow for some funny thing arrangements!
    if (z == ONFLOORZ)
    {
        //if (!P_CheckPosition(mobj,x,y))
            // we could send a message to the console here, saying
            // "no place for spawned thing"...

        //added:28-02-98: defaults onground
        mobj->eflags |= MF_ONGROUND;

        //added:28-02-98: dirty hack : dont stack monsters coz it blocks
        //                moving floors and anyway whats the use of it?
     /* if (mobj->flags & MF_SPECIAL)
        {
            mobj->z = mobj->floorz;

            // first check the tmfloorz
            P_CheckPosition(mobj,x,y);
            mobj->z = tmfloorz+FRACUNIT;

            // second check at the good z pos
            P_CheckPosition(mobj,x,y);

            mobj->floorz = tmfloorz;
            mobj->ceilingz = tmsectorceilingz;
            mobj->z = tmfloorz;
            // thing not on solid ground
            if (tmfloorthing)
                mobj->eflags &= ~MF_ONGROUND;

            //if (mobj->type == MT_BARREL)
            //   fprintf(stderr,"barrel at z %d floor %d ceiling %d\n",mobj->z,mobj->floorz,mobj->ceilingz);

        }
        else*/
            mobj->z = mobj->floorz;

    }
    else if (z == ONCEILINGZ)
        mobj->z = mobj->ceilingz - mobj->info->height;
    else
    {
        //CONS_Printf("mobj spawned at z %d\n",z>>16);
        mobj->z = z;
    }

    // added 16-6-98: special hack for spirit
    if(mobj->type == MT_SPIRIT)
        mobj->thinker.function.acv = (actionf_v) (-1);
    else
    {
        mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
        P_AddThinker (&mobj->thinker);
    }

    //SOM: Fuse for bunnies, squirls, and flingrings
      if(mobj->type == MT_BIRD || mobj->type == MT_SQRL)
        mobj->fuse = 300 + (P_Random() % 50);

    return mobj;
}


//
// P_RemoveMobj
//
mapthing_t      itemrespawnque[ITEMQUESIZE];
int             itemrespawntime[ITEMQUESIZE];
int             iquehead;
int             iquetail;


void P_RemoveMobj (mobj_t* mobj)
{
    if ((mobj->flags & MF_SPECIAL)
        && !(mobj->flags & MF_DROPPED)
        && (mobj->type != MT_INV)
        && (mobj->type != MT_INS))
    {
        itemrespawnque[iquehead] = mobj->spawnpoint;
        itemrespawntime[iquehead] = leveltime;
        iquehead = (iquehead+1)&(ITEMQUESIZE-1);

        // lose one off the end?
        if (iquehead == iquetail)
            iquetail = (iquetail+1)&(ITEMQUESIZE-1);
    }

    // unlink from sector and block lists
    P_UnsetThingPosition (mobj);

    // stop any playing sound
    S_StopSound (mobj);

    // free block
    P_RemoveThinker ((thinker_t*)mobj);
}


consvar_t cv_itemrespawntime={"respawnitemtime","30",CV_NETVAR,NULL};
consvar_t cv_itemrespawn    ={"respawnitem"    , "0",CV_NETVAR,CV_OnOff};

//
// P_RespawnSpecials
//
void P_RespawnSpecials (void)
{
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;

    subsector_t*        ss;
    mobj_t*             mo;
    mapthing_t*         mthing;

    int                 i;

    // only respawn items in deathmatch
    if (!cv_itemrespawn.value)
        return; //

    // nothing left to respawn?
    if (iquehead == iquetail)
        return;

    // the first item in the queue is the first to respawn
    // wait at least 30 seconds
    if (leveltime - itemrespawntime[iquetail] < cv_itemrespawntime.value*TICRATE)
        return;

    mthing = &itemrespawnque[iquetail];

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector (x,y);
    mo = P_SpawnMobj (x, y, ss->sector->floorheight , MT_IFOG);
    S_StartSound (mo, sfx_itmbk);

    // find which type to spawn
    for (i=0 ; i< NUMMOBJTYPES ; i++)
    {
        if (mthing->type == mobjinfo[i].doomednum)
            break;
    }

    // spawn it
    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mo = P_SpawnMobj (x,y,z, i);
    mo->spawnpoint = *mthing;
    mo->angle = ANG45 * (mthing->angle/45);

    // pull it from the que
    iquetail = (iquetail+1)&(ITEMQUESIZE-1);
}

// used when we are going from deathmatch 2 to deathmatch 1
void P_RespawnWeapons(void)
{
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;

    subsector_t*        ss;
    mobj_t*             mo;
    mapthing_t*         mthing;

    int                 i,j,freeslot;

    freeslot=iquetail;
    for(j=iquetail;j!=iquehead;j=(j+1)&(ITEMQUESIZE-1))
    {
        mthing = &itemrespawnque[j];

        i=0;
        switch(mthing->type) {
            case 2001 : //mobjinfo[MT_SHOTGUN].doomednum  :
                 i=MT_SHOTGUN;
                 break;
            case 82   : //mobjinfo[MT_SUPERSHOTGUN].doomednum :
                 i=MT_SUPERSHOTGUN;
                 break;
            case 2002 : //mobjinfo[MT_CHAINGUN].doomednum :
                 i=MT_CHAINGUN;
                 break;
            case 2006 : //mobjinfo[MT_MISC25].doomednum   : // bfg9000
                 i=MT_MISC25;
                 break;
            case 2004 : //mobjinfo[MT_MISC28].doomednum   : // plasma launcher
                 i=MT_MISC28;
                 break;
            case 2003 : //mobjinfo[MT_MISC27].doomednum   : // rocket launcher
                 i=MT_MISC27;
                 break;
            case 2005 : //mobjinfo[MT_MISC26].doomednum   : // shainsaw
                 i=MT_MISC26;
                 break;
            default:
                 if(freeslot!=j)
                 {
                     itemrespawnque[freeslot]=itemrespawnque[j];
                     itemrespawntime[freeslot]=itemrespawntime[j];
                 }

                 freeslot=(freeslot+1)&(ITEMQUESIZE-1);
                 continue;
        }
        // respwan it
        x = mthing->x << FRACBITS;
        y = mthing->y << FRACBITS;

        // spawn a teleport fog at the new spot
        ss = R_PointInSubsector (x,y);
        mo = P_SpawnMobj (x, y, ss->sector->floorheight , MT_IFOG);
        S_StartSound (mo, sfx_itmbk);

        // spawn it
        if (mobjinfo[i].flags & MF_SPAWNCEILING)
            z = ONCEILINGZ;
        else
            z = ONFLOORZ;

        mo = P_SpawnMobj (x,y,z, i);
        mo->spawnpoint = *mthing;
        mo->angle = ANG45 * (mthing->angle/45);
        // here don't increment freeslot
    }
    iquehead=freeslot;
}


//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//
void P_SpawnPlayer (mapthing_t* mthing)
{
    player_t*           p;
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;

    mobj_t*             mobj;

    int                 i=mthing->type-1;

    // not playing?
    if (!playeringame[i])
        return;

#ifdef PARANOIA
    if(i<0 && i>=MAXPLAYERS)
        I_Error("P_SpawnPlayer : playernum not in bound (%d)",i);
#endif

    p = &players[i];

    if (p->playerstate == PST_REBORN)
        G_PlayerReborn (mthing->type-1);

    x           = mthing->x << FRACBITS;
    y           = mthing->y << FRACBITS;
    z           = ONFLOORZ;
    mobj        = P_SpawnMobj (x,y,z, MT_PLAYER);

#ifdef CLIENTPREDICTION
    //added 1-6-98 : for movement prediction
    p->spirit = P_SpawnMobj (x,y,z, MT_SPIRIT);
#endif

    // set color translations for player sprites
    // added 6-2-98 : change color : now use skincolor (befor is mthing->type-1
    mobj->flags |= (p->skincolor)<<MF_TRANSSHIFT;

    //
    // set 'spritedef' override in mobj for player skins.. (see ProjectSprite)
    // (usefulness : when body mobj is detached from player (who respawns),
    //  the dead body mobj retain the skin through the 'spritedef' override).
    mobj->skin = &skins[p->skin];

    mobj->angle = ANG45 * (mthing->angle/45);
    if (p==&players[consoleplayer])
        localangle = mobj->angle;
    else
    if (p==&players[secondarydisplayplayer])
        localangle2 = mobj->angle;
    mobj->player = p;
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = cv_viewheight.value<<FRACBITS;
    // added 2-12-98
    p->viewz = p->mo->z + p->viewheight;

    // setup gun psprite
    P_SetupPsprites (p);

    // give all cards in death match mode
    if (cv_deathmatch.value)
        for (i=0 ; i<NUMCARDS ; i++)
            p->cards[i] = true;

    if (mthing->type-1 == consoleplayer)
    {
        // wake up the status bar
        ST_Start ();
        // wake up the heads up text
        HU_Start ();
    }

    if(camera.chase && displayplayer==mthing->type-1)
       P_ResetCamera(p);
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
void P_SpawnMapThing (mapthing_t* mthing)
{
    int                 i;
    int                 bit;
    mobj_t*             mobj;
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;

    // count deathmatch start positions
    if (mthing->type == 11)
    {
        if (deathmatch_p < &deathmatchstarts[MAX_DM_STARTS])
        {
            memcpy (deathmatch_p, mthing, sizeof(*mthing));
            deathmatch_p->type=0; // put it valide
            deathmatch_p++;
        }
        return;
    }

    // check for players specially
    // added 9-2-98 type 5 -> 8 player[x] starts for cooperative
    //              support ctfdoom cooperative playerstart
    if (mthing->type <=4 || (mthing->type<=4028 && mthing->type>=4001) )
    {
        if(mthing->type>4000)
             mthing->type=mthing->type-4001+5;

        // save spots for respawning in network games
        playerstarts[mthing->type-1] = *mthing;
        if (cv_deathmatch.value==0 && demoversion<128)
            P_SpawnPlayer (mthing);

        return;
    }

    // check for apropriate skill level
    if (!multiplayer && (mthing->options & 16))
        return;

    if (gameskill == sk_baby)
        bit = 1;
    else if (gameskill == sk_nightmare)
        bit = 4;
    else
        bit = 1<<(gameskill-1);

    if (!(mthing->options & bit) )
        return;

    // find which type to spawn
    for (i=0 ; i< NUMMOBJTYPES ; i++)
        if (mthing->type == mobjinfo[i].doomednum)
            break;

    if (i==NUMMOBJTYPES)
    {
        CONS_Printf ("\2P_SpawnMapThing: Unknown type %i at (%i, %i)\n",
                      mthing->type,
                      mthing->x, mthing->y);
        return;
    }

    // don't spawn keycards and players in deathmatch
    if (cv_deathmatch.value && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    // don't spawn any monsters if -nomonsters
    if (nomonsters
        && ( i == MT_SKULL
             || (mobjinfo[i].flags & MF_COUNTKILL)) )
    {
        return;
    }

    // spawn it
    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mobj = P_SpawnMobj (x,y,z, i);
    mobj->spawnpoint = *mthing;

    if (mobj->tics > 0)
        mobj->tics = 1 + (P_Random () % mobj->tics);
    if (mobj->flags & MF_COUNTKILL)
        totalkills++;
    if (mobj->flags & MF_COUNTITEM)
        totalitems++;

    mobj->angle = ANG45 * (mthing->angle/45);
    if (mthing->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnSplash
//
// when player moves in water
void P_SpawnSplash (mobj_t* mo, boolean flatwater)
                                // flatwater : old water FWATER flat texture
{
    mobj_t*     th;
    fixed_t     z;

    if (demoversion<125)
        return;
    
    // we are supposed to be in water sector and my current
    // hack uses negative tag as water height
    if (flatwater)
        z = mo->subsector->sector->floorheight + (FRACUNIT/4);
    else
        z = mo->subsector->sector->watersector->floorheight; //SOM: waterhack

    // need to touch the surface because the splashes only appear at surface
    if (mo->z > z || mo->z + mo->height < z)
        return;

    // note pos +1 +1 so it doesn't eat the sound of the player..
    th = P_SpawnMobj (mo->x+1,mo->y+1,z, MT_SPLASH);
    //if( z - mo->subsector->sector->floorheight > 4*FRACUNIT)
        S_StartSound (th, sfx_gloop);
    //else
    //    S_StartSound (th,sfx_splash);
    th->tics -= P_Random()&3;

    if (th->tics < 1)
        th->tics = 1;

    // get rough idea of speed
    /*
    thrust = (mo->momx + mo->momy) >> FRACBITS+1;

    if (thrust >= 2 && thrust<=3)
        P_SetMobjState (th,S_SPLASH2);
    else
    if (thrust < 2)
        P_SetMobjState (th,S_SPLASH3);
    */
}


// --------------------------------------------------------------------------
// P_SpawnSmoke
// --------------------------------------------------------------------------
// when player gets hurt by lava/slime, spawn at feet
void P_SpawnSmoke ( fixed_t       x,
                    fixed_t       y,
                    fixed_t       z )
{
    mobj_t*     th;

    if (demoversion<125)
        return;

    x = x - ((P_Random()&8) * FRACUNIT) - 4*FRACUNIT;
    y = y - ((P_Random()&8) * FRACUNIT) - 4*FRACUNIT;
    z += (P_Random()&3) * FRACUNIT;


    th = P_SpawnMobj (x,y,z, MT_SMOK);
    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
        th->tics = 1;
}



// --------------------------------------------------------------------------
// P_SpawnPuff
// --------------------------------------------------------------------------
extern fixed_t attackrange;

void P_SpawnPuff ( fixed_t       x,
                   fixed_t       y,
                   fixed_t       z )
{
    mobj_t*     th;

    z += ((P_Random()-P_Random())<<10);

    th = P_SpawnMobj (x,y,z, MT_PUFF);
    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
        th->tics = 1;

    // don't make punches spark on the wall
    if (attackrange == MELEERANGE)
        P_SetMobjState (th, S_PUFF3);
}



// --------------------------------------------------------------------------
// P_SpawnBlood
// --------------------------------------------------------------------------

static mobj_t*  bloodthing;

#ifdef WALLSPLATS
boolean PTR_BloodTraverse (intercept_t* in)
{
    line_t*             li;
    divline_t   divl;
    fixed_t     frac;

    if (in->isaline)
    {
        li = in->d.line;

        if ( !(li->flags & ML_TWOSIDED) )
            goto hitline;

        P_LineOpening (li);

        // quick hack, should check if passes through here
        return true;

hitline:
        P_MakeDivline (li, &divl);
        frac = P_InterceptVector (&divl, &trace);
        R_AddWallSplat (li, "BLUDC0", bloodthing->z + ((P_Random()-P_Random())<<3), frac, SPLATDRAWMODE_TRANS);
        return false;
    }

    //continue
    return true;
}
#endif

// P_SpawnBloodSplats
// the new SpawnBlood : this one first calls P_SpawnBlood for the usual blood sprites
// then spawns blood splats around on walls
//
void P_SpawnBloodSplats ( fixed_t       x,
                          fixed_t       y,
                          fixed_t       z,
                          int           damage,
                          divline_t*    damagedir )
{
#ifdef WALLSPLATS
static int  counter =0;
    fixed_t x2,y2;
    angle_t angle, anglesplat;
    int     distance;
    int     numsplats;
    int     i;
#endif
    // spawn the usual falling blood sprites at location
    P_SpawnBlood (x,y,z,damage);
    //CONS_Printf ("spawned blood counter %d\n", counter++);
    if( demoversion<128 )
        return;


#ifdef WALLSPLATS
    // traverse all linedefs and mobjs from the blockmap containing t1,
    // to the blockmap containing the dest. point.
    // Call the function for each mobj/line on the way,
    // starting with the mobj/linedef at the shortest distance...

    // get direction of damage
    x2 = x + damagedir->dx;
    y2 = y + damagedir->dy;
    angle = R_PointToAngle2 (x,y,x2,y2);
    distance = damage * 8;
    numsplats = damage / 3;

    //CONS_Printf ("spawning %d bloodsplats at distance of %d\n", numsplats, distance);
    //CONS_Printf ("damage %d\n", damage);

    //uses 'bloodthing' set by P_SpawnBlood()
    for (i=0; i<3; i++) {
        // find random angle between 0-180deg centered on damage angle
        anglesplat = angle + (((P_Random() - 128) * FINEANGLES/512)<<ANGLETOFINESHIFT);
        x2 = x + distance*finecosine[anglesplat>>ANGLETOFINESHIFT];
        y2 = y + distance*finesine[anglesplat>>ANGLETOFINESHIFT];
        //CONS_Printf ("traversepath cangle %d angle %d fuck %d\n", (angle>>ANGLETOFINESHIFT)*360/FINEANGLES,
        //    (anglesplat>>ANGLETOFINESHIFT)*360/FINEANGLES, (P_Random() - 128));

        P_PathTraverse ( x, y,
                         x2, y2,
                         PT_ADDLINES, //|PT_ADDTHINGS,
                         PTR_BloodTraverse );
    }
#endif

#ifdef FLOORSPLATS
    // add a test floor splat
    R_AddFloorSplat (bloodthing->subsector, "STEP2", x, y, bloodthing->floorz, SPLATDRAWMODE_SHADE);
#endif
}


// P_SpawnBlood
// spawn a blood sprite with falling z movement, at location
// the duration and first sprite frame depends on the damage level
// the more damage, the longer is the sprite animation
void P_SpawnBlood ( fixed_t       x,
                    fixed_t       y,
                    fixed_t       z,
                    int           damage )
{
    mobj_t*     th;

    z += ((P_Random()-P_Random())<<10);
    th = P_SpawnMobj (x,y,z, MT_BLOOD);
    if(demoversion>=128)
    {
        th->momx = (P_Random()-P_Random())<<12; //faB:19jan99
        th->momy = (P_Random()-P_Random())<<12; //faB:19jan99
    }
    th->momz = FRACUNIT*2;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
        th->tics = 1;

    if (damage <= 12 && damage >= 9)
        P_SetMobjState (th,S_BLOOD2);
    else if (damage < 9)
        P_SetMobjState (th,S_BLOOD3);

    bloodthing = th;
}


//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void P_CheckMissileSpawn (mobj_t* th)
{
    th->tics -= P_Random()&3;
    if (th->tics < 1)
        th->tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->x += (th->momx>>1);
    th->y += (th->momy>>1);
    th->z += (th->momz>>1);

    if (!P_TryMove (th, th->x, th->y))
        P_ExplodeMissile (th);
}


//
// P_SpawnMissile
//
mobj_t* P_SpawnMissile ( mobj_t*       source,
                         mobj_t*       dest,
                         mobjtype_t    type )
{
    mobj_t*     th;
    angle_t     an;
    int         dist;

#ifdef PARANOIA
    if(!source)
        I_Error("P_SpawnMissile : no source");
    if(!dest)
        I_Error("P_SpawnMissile : no dest");
#endif
    th = P_SpawnMobj (source->x,
                      source->y,
                      source->z + 4*8*FRACUNIT, type);

    if (th->info->seesound)
        S_StartSound (th, th->info->seesound);

    th->target = source;        // where it came from
    an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);

    // fuzzy player
    if (dest->flags & MF_SHADOW)
    {
        an += (P_Random()<<20); // WARNING: don't put this in one line 
        an -= (P_Random()<<20); // else this expretion is ambiguous (evaluation order not diffined)
    }

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul (th->info->speed, finecosine[an]);
    th->momy = FixedMul (th->info->speed, finesine[an]);

    dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
    dist = dist / th->info->speed;

    if (dist < 1)
        dist = 1;

    th->momz = (dest->z - source->z) / dist;
    P_CheckMissileSpawn (th);

    return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void P_SpawnPlayerMissile ( mobj_t*       source,
                            mobjtype_t    type,
              //added:16-02-98: needed the player here for the aiming
              player_t*     player )
{
    mobj_t*     th;
    angle_t     an;

    fixed_t     x;
    fixed_t     y;
    fixed_t     z;
    fixed_t     slope;

    // angle at which you fire, is player angle
    an = source->angle;

    //added:16-02-98: autoaim is now a toggle
    if (player->autoaim_toggle && cv_allowautoaim.value)
    {
        // see which target is to be aimed at
        slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

        if (!linetarget)
        {
            an += 1<<26;
            slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

            if (!linetarget)
            {
                an -= 2<<26;
                slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
            }

            if (!linetarget)
            {
                an = source->angle;
                slope = 0;
            }
        }
    }

    //added:18-02-98: if not autoaim, or if the autoaim didnt aim something,
    //                use the mouseaiming
    if (!(player->autoaim_toggle && cv_allowautoaim.value)
                                || (!linetarget && demoversion>111))
        if(demoversion>=128)
            slope = AIMINGTOSLOPE(player->aiming);
        else
            slope = (player->aiming<<FRACBITS)/160;

    x = source->x;
    y = source->y;
    z = source->z + 4*8*FRACUNIT;

    th = P_SpawnMobj (x,y,z, type);

    if (th->info->seesound)
        S_StartSound (th, th->info->seesound);

    th->target = source;

    th->angle = an;
    th->momx = FixedMul( th->info->speed, finecosine[an>>ANGLETOFINESHIFT]);
    th->momy = FixedMul( th->info->speed, finesine[an>>ANGLETOFINESHIFT]);
    
    if( demoversion<=128 )
    {   // 1.28 fix, allow full aiming must be much precise
        th->momx = FixedMul(th->momx,finecosine[player->aiming>>ANGLETOFINESHIFT]);
        th->momy = FixedMul(th->momy,finecosine[player->aiming>>ANGLETOFINESHIFT]);
    }
    th->momz = FixedMul( th->info->speed, slope);

    P_CheckMissileSpawn (th);
}
