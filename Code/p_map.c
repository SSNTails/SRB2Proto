// p_map.c :  movement, collision handling.
//      Shooting and aiming.

#include "doomdef.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "p_inter.h"
#include "r_state.h"
#include "r_main.h"
#include "r_sky.h"
#include "s_sound.h"

#include "r_splats.h"   //faB: testing


fixed_t         tmbbox[4];
mobj_t*         tmthing;
int             tmflags;
fixed_t         tmx;
fixed_t         tmy;


// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean         floatok;

fixed_t         tmfloorz;
fixed_t         tmceilingz;
fixed_t         tmdropoffz;

mobj_t*         tmfloorthing;   // the thing corresponding to tmfloorz
                                // or NULL if tmfloorz is from a sector

//added:28-02-98: used at P_ThingHeightClip() for moving sectors
fixed_t         tmsectorfloorz;
fixed_t         tmsectorceilingz;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t*         ceilingline;

// set by PIT_CheckLine() for any line that stopped the PIT_CheckLine()
// that is, for any line which is 'solid'
line_t*         blockingline;
extern mobj_t*  blockthing; // Tails 9-15-99 Spin Attack

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
#define MAXSPECIALCROSS         8

line_t*         spechit[MAXSPECIALCROSS];
int             numspechit;



//
// TELEPORT MOVE
//

//
// PIT_StompThing
//
boolean PIT_StompThing (mobj_t* thing)
{
    fixed_t     blockdist;

    if (!(thing->flags & MF_SHOOTABLE) )
        return true;

    blockdist = thing->radius + tmthing->radius;

    if ( abs(thing->x - tmx) >= blockdist
         || abs(thing->y - tmy) >= blockdist )
    {
        // didn't hit it
        return true;
    }

    // don't clip against self
    if (thing == tmthing)
        return true;

    // monsters don't stomp things except on boss level
    if ( !tmthing->player && gamemap != 30)
        return false;

    P_DamageMobj (thing, tmthing, tmthing, 10000);

    return true;
}


//
// P_TeleportMove
//
boolean
P_TeleportMove
( mobj_t*       thing,
  fixed_t       x,
  fixed_t       y )
{
    int                 xl;
    int                 xh;
    int                 yl;
    int                 yh;
    int                 bx;
    int                 by;

    subsector_t*        newsubsec;

    // kill anything occupying the position
    tmthing = thing;
    tmflags = thing->flags;

    tmx = x;
    tmy = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = R_PointInSubsector (x,y);
    ceilingline = NULL;

    // The base floor/ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
    tmceilingz = newsubsec->sector->ceilingheight;

    validcount++;
    numspechit = 0;

    // stomp on any things contacted
    xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
        for (by=yl ; by<=yh ; by++)
            if (!P_BlockThingsIterator(bx,by,PIT_StompThing))
                return false;

    // the move is ok,
    // so link the thing into its new position
    P_UnsetThingPosition (thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition (thing);

    return true;
}


// =========================================================================
//                       MOVEMENT ITERATOR FUNCTIONS
// =========================================================================


//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
boolean PIT_CheckLine (line_t* ld)
{
    if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
        || tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
        || tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
        || tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
        return true;

    if (P_BoxOnLineSide (tmbbox, ld) != -1)
        return true;

    // A line has been hit

    // The moving thing's destination position will cross
    // the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.

    if (!ld->backsector) {
        blockingline = ld;
        return false;           // one sided line
    }

    // missil and Camera can cross uncrossable line
    if (!(tmthing->flags & MF_MISSILE) &&
        !(tmthing->type == MT_CHASECAM) )
    {
        if (ld->flags & ML_BLOCKING)
            return false;       // explicitly blocking everything

        if ( !(tmthing->player) &&
             ld->flags & ML_BLOCKMONSTERS )
            return false;       // block monsters only
    }

    // set openrange, opentop, openbottom
    P_LineOpening (ld);

    // adjust floor / ceiling heights
    if (opentop < tmceilingz)
    {
        tmsectorceilingz = tmceilingz = opentop;
        ceilingline = ld;
    }

    if (openbottom > tmfloorz)
        tmsectorfloorz = tmfloorz = openbottom;

    if (lowfloor < tmdropoffz)
        tmdropoffz = lowfloor;

    // if contacted a special line, add it to the list
    if (ld->special)
    {
        spechit[numspechit] = ld;
        numspechit++;
    }

    return true;
}

//
// PIT_CheckThing
//
boolean PIT_CheckThing (mobj_t* thing)
{
    fixed_t             blockdist;
//    fixed_t             zblockdist;
    boolean             solid;
    int                 damage;

    //added:22-02-98:
    fixed_t             topz;
    fixed_t             tmtopz;

    if (!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE) ))
        return true;

    blockdist = thing->radius + tmthing->radius;

    if ( abs(thing->x - tmx) >= blockdist ||
         abs(thing->y - tmy) >= blockdist )
    {
        // didn't hit it
        return true;
    }

    // don't clip against self
    if (thing == tmthing)
        return true;

    // check for skulls slamming into things
    if (tmthing->flags & MF_SKULLFLY)
    {
        damage = ((P_Random()%8)+1)*tmthing->info->damage;

        P_DamageMobj (thing, tmthing, tmthing, damage);

        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->momx = tmthing->momy = tmthing->momz = 0;

        P_SetMobjState (tmthing, tmthing->info->spawnstate);

        return false;           // stop moving
    }


    // missiles can hit other things
    if (tmthing->flags & MF_MISSILE)
    {
        // see if it went over / under
        if (tmthing->z > thing->z + thing->height)
            return true;                // overhead
        if (tmthing->z+tmthing->height < thing->z)
            return true;                // underneath

        if (tmthing->target && (
            tmthing->target->type == thing->type ||
            (tmthing->target->type == MT_KNIGHT  && thing->type == MT_BRUISER)||
            (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT) ) )
        {
            // Don't hit same species as originator.
            if (thing == tmthing->target)
                return true;

            if (thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
        }

        if (! (thing->flags & MF_SHOOTABLE) )
        {
            // didn't do any damage
            return !(thing->flags & MF_SOLID);
        }

        // damage / explode
        damage = ((P_Random()%8)+1)*tmthing->info->damage;
        P_DamageMobj (thing, tmthing, tmthing->target, damage);

        // don't traverse any more
        return false;
    }

    // check for special pickup
    if (thing->flags & MF_SPECIAL)
    {
        solid = thing->flags&MF_SOLID;
        if (tmflags&MF_PICKUP)
        {
            // can remove thing
            P_TouchSpecialThing (thing, tmthing);
        }
        return !solid;
    }


    //added:24-02-98:compatibility with old demos, it used to return with...
    //added:27-02-98:for version 112+, nonsolid things pass through other things
    if (demoversion<112 || !(tmthing->flags & MF_SOLID))
        return !(thing->flags & MF_SOLID);

    //added:22-02-98: added z checking at last
    if (thing->flags & MF_SOLID)
    {
        // pass under
        tmtopz = tmthing->z + tmthing->height;

        if ( tmtopz < thing->z)
        {
            if (thing->z < tmceilingz)
                tmceilingz = thing->z;
            return true;
        }

        topz = thing->z + thing->height + FRACUNIT;

        // block only when jumping not high enough,
        // (dont climb max. 24units while already in air)
        // if not in air, let P_TryMove() decide if its not too high
        if (tmthing->player &&
            tmthing->z < topz &&
            tmthing->z > tmthing->floorz)  // block while in air
            return false;

        if (topz > tmfloorz)
            tmfloorz = topz;
            tmfloorthing = thing;       //thing we may stand on

// Z Damage works!! YESSSS!!!! I'VE SPENT WAY TOO MUCH TIME ON IT!! Tails 11-03-99
        // Fixed it up -- Stealth
           {
//           if (tmthing->z == tmfloorz)
           if ((tmthing->z <= tmfloorz) && tmthing->type==MT_PLAYER)
           {
             if(((tmthing->eflags & MF_JUMPED && !(tmthing->eflags & MF_ONGROUND)) || (tmthing->eflags & MF_JUSTJUMPED && !(tmthing->eflags & MF_ONGROUND)) || (tmthing->player->powers[pw_invulnerability] && !(tmthing->eflags & MF_ONGROUND)) || (tmthing->eflags & MF_SPINNING && !(tmthing->eflags & MF_ONGROUND))))
//              if(((tmthing->state->nextstate==S_PLAY_ATK3 || tmthing->state->nextstate==S_PLAY_ATK4 || tmthing->state->nextstate==S_PLAY_ATK5 || tmthing->state->nextstate==S_PLAY_ATK6) && tmthing->eflags & ~MF_ONGROUND) || tmthing->player->powers[pw_invulnerability])
             {
               //SOM: Don't kill spring boards!
               switch(thing->type)
                {
                 case MT_POSSESSED:
                 case MT_SHOTGUY:
                 case MT_UNDEAD:
                 case MT_FATSO:
                 case MT_CHAINGUY:
                 case MT_TROOP:
                 case MT_SERGEANT:
                 case MT_SHADOWS:
                 case MT_CYBORG:
                 case MT_PAIN:
                 case MT_MISC50:
                 case MT_MISC31:
                 case MT_MISC48:
                 case MT_MISC10:
                 case MT_MISC11:
                 case MT_MISC74:
                 case MT_INV: // invincibility box tails
                 case MT_BABY:
                  {
                   P_DamageMobj(thing, tmthing, tmthing, 1);
                   tmthing->player->mo->momz = JUMPGRAVITY*1.5;
                   if((tmthing->eflags & MF_JUSTJUMPED))
                   {
                    tmthing->eflags &= ~MF_JUSTJUMPED;
                   }
                   if (!(tmthing->eflags & MF_JUMPED))
                   {
                    tmthing->eflags += MF_JUMPED;
                   }
                   if ((tmthing->eflags & MF_JUSTHITFLOOR))
                   {
                    tmthing->eflags &= ~MF_JUSTHITFLOOR;
                   }
                   P_SetMobjState (tmthing->player->mo, S_PLAY_ATK3);
                   break;
                  }
                 case MT_MISC70:
                  {
                   tmthing->player->mo->momz = JUMPGRAVITY*3;
                   P_SetMobjState (thing, S_HEADSONSTICK2);
                   if ((tmthing->eflags & MF_JUSTHITFLOOR))
                   {
                    tmthing->eflags &= ~MF_JUSTHITFLOOR;
                   }
                   if((tmthing->eflags & MF_JUSTJUMPED))
                   {
                    tmthing->eflags &= ~MF_JUSTJUMPED;
                   }
                   if ((tmthing->eflags & MF_JUMPED))
                   {
                    tmthing->eflags &= ~MF_JUMPED;
                   }
                   P_SetMobjState (tmthing->player->mo, S_PLAY_PLG1);
                   break;
                  }
                 case MT_MISC84:
                  {
                   tmthing->player->mo->momz = JUMPGRAVITY*5;
                   if ((tmthing->eflags & MF_JUSTHITFLOOR))
                   {
                    tmthing->eflags &= ~MF_JUSTHITFLOOR;
                   }
                   if((tmthing->eflags & MF_JUSTJUMPED))
                   {
                    tmthing->eflags &= ~MF_JUSTJUMPED;
                   }
                   if ((tmthing->eflags & MF_JUMPED))
                   {
                    tmthing->eflags &= ~MF_JUMPED;
                   }
                   P_SetMobjState (thing, S_COLONGIBS2);
                   P_SetMobjState (tmthing->player->mo, S_PLAY_PLG1);
                   break;
                  }
                 default:
 //                  tmthing->player->mo->momx = 0;
 //                  tmthing->player->mo->momy = 0;
                   break;
                }
               }

            else if(((tmthing->eflags & MF_JUMPED) || (tmthing->eflags & MF_JUSTJUMPED) || tmthing->player->powers[pw_invulnerability] || (tmthing->eflags & MF_SPINNING))  && tmthing->type==MT_PLAYER)
//            else if((tmthing->state->nextstate==S_PLAY_ATK3 || tmthing->state->nextstate==S_PLAY_ATK4 || tmthing->state->nextstate==S_PLAY_ATK5 || tmthing->state->nextstate==S_PLAY_ATK6) || tmthing->player->powers[pw_invulnerability])
               {
               //SOM: Don't kill spring boards!
               switch(thing->type) {
                 case MT_POSSESSED:
                 case MT_SHOTGUY:
                 case MT_UNDEAD:
                 case MT_FATSO:
                 case MT_CHAINGUY:
                 case MT_TROOP:
                 case MT_SERGEANT:
                 case MT_SHADOWS:
                 case MT_CYBORG:
                 case MT_PAIN:
                   P_DamageMobj(thing, tmthing, tmthing, 1);
//                   thing->player->mo->momz = -JUMPGRAVITY*1.5;
//                   if((thing->eflags & MF_JUSTJUMPED)) {
//                   thing->eflags &= ~MF_JUSTJUMPED; }
//                   if(!(thing->eflags & MF_JUMPED)) {
//                   thing->eflags += MF_JUMPED;
//                     }
//                   if((thing->eflags & MF_JUSTHITFLOOR)) {
//                   thing->eflags &= ~MF_JUSTHITFLOOR; }
//                   P_SetMobjState (thing->player->mo, S_PLAY_ATK3);
                   break;
                 default:
 //                  tmthing->player->mo->momx = 0;
 //                  tmthing->player->mo->momy = 0;
                   break;
                  }
                  }  

             else
               {
               //SOM: Spring boards don't kill!
               switch(thing->type) {
                 case MT_POSSESSED:
                 case MT_SHOTGUY:
                 case MT_UNDEAD:
                 case MT_FATSO:
                 case MT_CHAINGUY:
                 case MT_TROOP:
                 case MT_SERGEANT:
                 case MT_SHADOWS:
                 case MT_CYBORG:
                 case MT_PAIN:
//                   tmthing->player->mo->momz = JUMPGRAVITY*1.5;
                   P_DamageMobj(tmthing, thing, thing, 1);
//                   tmthing->player->mo->momx = -tmthing->player->mo->momx;
//                   tmthing->player->mo->momy = -tmthing->player->mo->momy;
                   break;
                 case MT_MISC70:
                   tmthing->player->mo->momz = JUMPGRAVITY*3;
                   P_SetMobjState (thing, S_HEADSONSTICK2);
                   if ((tmthing->eflags & MF_JUSTHITFLOOR)) {
                   tmthing->eflags &= ~MF_JUSTHITFLOOR; }
                   if((tmthing->eflags & MF_JUSTJUMPED)) {
                   tmthing->eflags &= ~MF_JUSTJUMPED; }
                   if ((tmthing->eflags & MF_JUMPED)) {
                   tmthing->eflags &= ~MF_JUMPED; }
                   P_SetMobjState (tmthing->player->mo, S_PLAY_PLG1);
             //      tmthing->player->mo->momx = 0;
             //      tmthing->player->mo->momy = 0;
                   break;
                 case MT_MISC84:
                   tmthing->player->mo->momz = JUMPGRAVITY*5;
                   if ((tmthing->eflags & MF_JUSTHITFLOOR)) {
                   tmthing->eflags &= ~MF_JUSTHITFLOOR; }
                   if((tmthing->eflags & MF_JUSTJUMPED)) {
                   tmthing->eflags &= ~MF_JUSTJUMPED; }
                   if ((tmthing->eflags & MF_JUMPED)) {
                   tmthing->eflags &= ~MF_JUMPED; }
                   P_SetMobjState (thing, S_COLONGIBS2);
                   P_SetMobjState (tmthing->player->mo, S_PLAY_PLG1);
         //          tmthing->player->mo->momx = 0;
        //           tmthing->player->mo->momy = 0;
                   break;
                 default:
                   break;
                }
             }
          }
        }

// Start fan code Tails 12-03-99
           if (tmthing->z >= tmfloorz)
               {
               switch(thing->type) {
                 case MT_MISC34:
                 tmthing->player->mo->momz = JUMPGRAVITY;
                   if ((tmthing->eflags & MF_JUSTHITFLOOR)) {
                   tmthing->eflags &= ~MF_JUSTHITFLOOR; }
                   if((tmthing->eflags & MF_JUSTJUMPED)) {
                   tmthing->eflags &= ~MF_JUSTJUMPED; }
                   if ((tmthing->eflags & MF_JUMPED)) {
                   tmthing->eflags &= ~MF_JUMPED; }
                 P_SetMobjState (tmthing->player->mo, S_PLAY_PAIN);
                 break;
                 default:
                 break;
             }
           }
// End fan code Tails 12-03-99
      }

    // not solid not blocked
    return true;
}



// =========================================================================
//                         MOVEMENT CLIPPING
// =========================================================================

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  tmfloorz
//  tmceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//

//added:27-02-98:
//
// tmfloorz
//     the nearest floor or thing's top under tmthing
// tmceilingz
//     the nearest ceiling or thing's bottom over tmthing
//
boolean P_CheckPosition ( mobj_t*       thing,
                          fixed_t       x,
                          fixed_t       y )
{
    int                 xl;
    int                 xh;
    int                 yl;
    int                 yh;
    int                 bx;
    int                 by;
    subsector_t*        newsubsec;

    tmthing = thing;
    tmflags = thing->flags;

    tmx = x;
    tmy = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = R_PointInSubsector (x,y);
    ceilingline = blockingline = NULL;

    // The base floor / ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    tmfloorz = tmsectorfloorz = tmdropoffz = newsubsec->sector->floorheight;
    tmceilingz = tmsectorceilingz = newsubsec->sector->ceilingheight;

    // tmfloorthing is set when tmfloorz comes from a thing's top
    tmfloorthing = NULL;

    validcount++;
    numspechit = 0;

    if ( tmflags & MF_NOCLIP )
        return true;

    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.

    // BP: added MF_NOCLIPTHING :used by camera to don't be blocked by things
    if(!(thing->flags & MF_NOCLIPTHING))
    {
    xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
        for (by=yl ; by<=yh ; by++)
             // Tails 9-15-99 Spin Attack
             // SOM: Begin jumping damage code
             if (!P_BlockThingsIterator(bx, by, PIT_CheckThing)) 
                 {
                 //SOM: Improved the jumping damage code
     // made even better -- Stealth
             if((thing->player && (thing->eflags & MF_JUMPED) || (thing->eflags & MF_JUSTJUMPED)) || (tmthing->player && tmthing->player->powers[pw_invulnerability])) // Made even betterer -- SSNTails 08-19-2023
                   {
                   //SOM: Don't kill spring boards!
                   switch(blockthing->type) {
                     case MT_POSSESSED:
                     case MT_SHOTGUY:
                     case MT_UNDEAD:
                     case MT_FATSO:
                     case MT_CHAINGUY:
                     case MT_TROOP:
                     case MT_SERGEANT:
                     case MT_SHADOWS:
                     case MT_CYBORG:
                     case MT_PAIN:
                     case MT_MISC50:
                     case MT_MISC31:
                     case MT_MISC48:
                     case MT_MISC10:
                     case MT_MISC11:
                     case MT_MISC74:
                     case MT_INV: // invincibility box tails
                     case MT_BABY:
                       P_DamageMobj(blockthing, thing, thing, 1);
                       P_SetMobjState (thing->player->mo, S_PLAY_ATK3);
                   if ((thing->eflags & MF_JUSTHITFLOOR)) {
                   thing->eflags &= ~MF_JUSTHITFLOOR; }
                   if((thing->eflags & MF_JUSTJUMPED)) {
                   thing->eflags &= ~MF_JUSTJUMPED; }
                   if (!(thing->eflags & MF_JUMPED)) {
                   thing->eflags += MF_JUMPED; }
                       thing->player->mo->momx = -thing->player->mo->momx;
                       thing->player->mo->momy = -thing->player->mo->momy;
                       break;
                     default:
    //                   thing->player->mo->momx = 0;
     //                  thing->player->mo->momy = 0;
                       break;
                     }
                   }

                 else
                   {
                   //SOM: Spring boards don't kill!
                   switch(blockthing->type) {
                     case MT_POSSESSED:
                     case MT_SHOTGUY:
                     case MT_UNDEAD:
                     case MT_FATSO:
                     case MT_CHAINGUY:
                     case MT_TROOP:
                     case MT_SERGEANT:
                     case MT_SHADOWS:
                     case MT_CYBORG:
                     case MT_PAIN:
                       P_DamageMobj(thing, blockthing, blockthing, 1);
                       break;
                     default:
       //                thing->momx = thing->momy = 0;
                       break;
                     }
                   }

                 return false;
                 }
             // SOM: End jumping damage code 



          //  if (!P_BlockThingsIterator(bx,by,PIT_CheckThing)) ORIGINAL
           //     return false; ORIGINAL
    }
    // check lines
    xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
        for (by=yl ; by<=yh ; by++)
            if (!P_BlockLinesIterator (bx,by,PIT_CheckLine))
                return false;

    return true;
}


//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
boolean P_TryMove ( mobj_t*       thing,
                    fixed_t       x,
                    fixed_t       y )
{
    fixed_t     oldx;
    fixed_t     oldy;
    int         side;
    int         oldside;
    line_t*     ld;

    floatok = false;
    if (!P_CheckPosition (thing, x, y))
        return false;           // solid wall or thing

    if ( !(thing->flags & MF_NOCLIP) )
    {
        fixed_t maxstep = MAXSTEPMOVE;
        if (tmceilingz - tmfloorz < thing->height)
            return false;       // doesn't fit

        floatok = true;

        if ( !(thing->flags & MF_TELEPORT) &&
             tmceilingz - thing->z < thing->height)
            return false;       // mobj must lower itself to fit

        // jump out of water
        if((thing->eflags & (MF_UNDERWATER|MF_TOUCHWATER))==(MF_UNDERWATER|MF_TOUCHWATER))
            maxstep=37*FRACUNIT;

        if ( !(thing->flags & MF_TELEPORT) &&
             (tmfloorz - thing->z > maxstep ) )
            return false;       // too big a step up

        if ( !(thing->flags&(MF_DROPOFF|MF_FLOAT))
             && tmfloorz - tmdropoffz > MAXSTEPMOVE )
            return false;       // don't stand over a dropoff
    }

    // the move is ok,
    // so link the thing into its new position
    P_UnsetThingPosition (thing);

    //added:28-02-98: gameplay hack : walk over a small wall while jumping
    //                stop jumping it succeeded
    // BP: removed in 1.28 because we can mode in air now
    if ( demoplayback<128 && thing->player &&
         (thing->player->cheats & CF_JUMPOVER) )
    {
        if (tmfloorz > thing->floorz + MAXSTEPMOVE)
            thing->momz >>= 2;
    }

    oldx = thing->x;
    oldy = thing->y;
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->x = x;
    thing->y = y;

    //added:28-02-98:
    if (tmfloorthing)
        thing->eflags &= ~MF_ONGROUND;  //not on real floor
    else
        thing->eflags |= MF_ONGROUND;

    P_SetThingPosition (thing);

    // if any special lines were hit, do the effect
    if ( !(thing->flags&(MF_TELEPORT|MF_NOCLIP)) &&
         (thing->type != MT_CHASECAM) )
    {
        while (numspechit--)
        {
            // see if the line was crossed
            ld = spechit[numspechit];
            side = P_PointOnLineSide (thing->x, thing->y, ld);
            oldside = P_PointOnLineSide (oldx, oldy, ld);
            if (side != oldside)
            {
                if (ld->special)
                    P_CrossSpecialLine (ld-lines, oldside, thing);
            }
        }
    }

    return true;
}


//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//
boolean P_ThingHeightClip (mobj_t* thing)
{
    boolean             onfloor;

    onfloor = (thing->z <= thing->floorz);

    P_CheckPosition (thing, thing->x, thing->y);
    // what about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    if(thing->type == MT_MISC3 || thing->type == MT_MISC2)
      return true;

    if (!tmfloorthing && onfloor && !(thing->flags & MF_NOGRAVITY))
    {
        // walking monsters rise and fall with the floor
        thing->z = thing->floorz;
    }
    else
    {
        // don't adjust a floating monster unless forced to
        //added:18-04-98:test onfloor
        if (!onfloor)
            if (thing->z+thing->height > tmsectorceilingz)
                thing->z = thing->ceilingz - thing->height;

        thing->flags &= ~MF_ONGROUND;
    }

    //debug : be sure it falls to the floor
    thing->eflags &= ~MF_ONGROUND;

    //added:28-02-98:
    // test sector bouding top & bottom, not things

    //if (tmsectorceilingz - tmsectorfloorz < thing->height)
    //    return false;

    if (thing->ceilingz - thing->floorz < thing->height
        //imp dans imp map01
        && thing->z >= thing->floorz)
    {
        return false;
    }

    return true;
}



//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
fixed_t         bestslidefrac;
fixed_t         secondslidefrac;

line_t*         bestslideline;
line_t*         secondslideline;

mobj_t*         slidemo;

fixed_t         tmxmove;
fixed_t         tmymove;



//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
void P_HitSlideLine (line_t* ld)
{
    int                 side;

    angle_t             lineangle;
    angle_t             moveangle;
    angle_t             deltaangle;

    fixed_t             movelen;
    fixed_t             newlen;


    if (ld->slopetype == ST_HORIZONTAL)
    {
        tmymove = 0;
        return;
    }

    if (ld->slopetype == ST_VERTICAL)
    {
        tmxmove = 0;
        return;
    }

    side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);

    lineangle = R_PointToAngle2 (0,0, ld->dx, ld->dy);

    if (side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);
    deltaangle = moveangle-lineangle;

    if (deltaangle > ANG180)
        deltaangle += ANG180;
    //  I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_AproxDistance (tmxmove, tmymove);
    newlen = FixedMul (movelen, finecosine[deltaangle]);

    tmxmove = FixedMul (newlen, finecosine[lineangle]);
    tmymove = FixedMul (newlen, finesine[lineangle]);
}


//
// PTR_SlideTraverse
//
boolean PTR_SlideTraverse (intercept_t* in)
{
    line_t*     li;

#ifdef PARANOIA
    if (!in->isaline)
        I_Error ("PTR_SlideTraverse: not a line?");
#endif

    li = in->d.line;

    if ( ! (li->flags & ML_TWOSIDED) )
    {
        if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
        {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening (li);

    if (openrange < slidemo->height)
        goto isblocking;                // doesn't fit

    if (opentop - slidemo->z < slidemo->height)
        goto isblocking;                // mobj is too high

    if (openbottom - slidemo->z > 24*FRACUNIT )
        goto isblocking;                // too big a step up

    // this line doesn't block movement
    return true;

    // the line does block movement,
    // see if it is closer than best so far
  isblocking:
    if (in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    return false;       // stop
}



//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
void P_SlideMove (mobj_t* mo)
{
    fixed_t             leadx;
    fixed_t             leady;
    fixed_t             trailx;
    fixed_t             traily;
    fixed_t             newx;
    fixed_t             newy;
    int                 hitcount;

    slidemo = mo;
    hitcount = 0;

  retry:
    if (++hitcount == 3)
        goto stairstep;         // don't loop forever


    // trace along the three leading corners
    if (mo->momx > 0)
    {
        leadx = mo->x + mo->radius;
        trailx = mo->x - mo->radius;
    }
    else
    {
        leadx = mo->x - mo->radius;
        trailx = mo->x + mo->radius;
    }

    if (mo->momy > 0)
    {
        leady = mo->y + mo->radius;
        traily = mo->y - mo->radius;
    }
    else
    {
        leady = mo->y - mo->radius;
        traily = mo->y + mo->radius;
    }

    bestslidefrac = FRACUNIT+1;

    P_PathTraverse ( leadx, leady, leadx+mo->momx, leady+mo->momy,
                     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( trailx, leady, trailx+mo->momx, leady+mo->momy,
                     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( leadx, traily, leadx+mo->momx, traily+mo->momy,
                     PT_ADDLINES, PTR_SlideTraverse );

    // move up to the wall
    if (bestslidefrac == FRACUNIT+1)
    {
        // the move most have hit the middle, so stairstep
      stairstep:
        if (!P_TryMove (mo, mo->x, mo->y + mo->momy))
            P_TryMove (mo, mo->x + mo->momx, mo->y);
        return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if (bestslidefrac > 0)
    {
        newx = FixedMul (mo->momx, bestslidefrac);
        newy = FixedMul (mo->momy, bestslidefrac);

        if (!P_TryMove (mo, mo->x+newx, mo->y+newy))
            goto stairstep;
    }

    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT-(bestslidefrac+0x800);

    if (bestslidefrac > FRACUNIT)
        bestslidefrac = FRACUNIT;

    if (bestslidefrac <= 0)
        return;

    tmxmove = FixedMul (mo->momx, bestslidefrac);
    tmymove = FixedMul (mo->momy, bestslidefrac);

    P_HitSlideLine (bestslideline);     // clip the moves

    mo->momx = tmxmove;
    mo->momy = tmymove;

    if (!P_TryMove (mo, mo->x+tmxmove, mo->y+tmymove))
    {
        goto retry;
    }
}


//
// P_LineAttack
//
mobj_t*         linetarget;     // who got hit (or NULL)
mobj_t*         shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
fixed_t         shootz;

int             la_damage;
fixed_t         attackrange;

fixed_t         aimslope;

// slopes to top and bottom of target
extern fixed_t  topslope;
extern fixed_t  bottomslope;


//
// PTR_AimTraverse
// Sets linetarget and aimslope when a target is aimed at.
//
//added:15-02-98: comment
// Returns true if the thing is not shootable, else continue through..
//
boolean PTR_AimTraverse (intercept_t* in)
{
    line_t*             li;
    mobj_t*             th;
    fixed_t             slope;
    fixed_t             thingtopslope;
    fixed_t             thingbottomslope;
    fixed_t             dist;

    if (in->isaline)
    {
        li = in->d.line;

        if ( !(li->flags & ML_TWOSIDED) )
            return false;               // stop

        // Crosses a two sided line.
        // A two sided line will restrict
        // the possible target ranges.
        P_LineOpening (li);

        if (openbottom >= opentop)
            return false;               // stop

        dist = FixedMul (attackrange, in->frac);

        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            slope = FixedDiv (openbottom - shootz , dist);
            if (slope > bottomslope)
                bottomslope = slope;
        }

        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            slope = FixedDiv (opentop - shootz , dist);
            if (slope < topslope)
                topslope = slope;
        }

        if (topslope <= bottomslope)
            return false;               // stop

        return true;                    // shot continues
    }

    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
        return true;                    // can't shoot self

    if (!(th->flags&MF_SHOOTABLE))
        return true;                    // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul (attackrange, in->frac);
    thingtopslope = FixedDiv (th->z+th->height - shootz , dist);

    //added:15-02-98: bottomslope is negative!
    if (thingtopslope < bottomslope)
        return true;                    // shot over the thing

    thingbottomslope = FixedDiv (th->z - shootz, dist);

    if (thingbottomslope > topslope)
        return true;                    // shot under the thing

    // this thing can be hit!
    if (thingtopslope > topslope)
        thingtopslope = topslope;

    if (thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    //added:15-02-98: find the slope just in the middle(y) of the thing!
    aimslope = (thingtopslope+thingbottomslope)/2;
    linetarget = th;

    return false;                       // don't go any farther
}


//
// PTR_ShootTraverse
//
//added:18-02-98: added clipping the shots on the floor and ceiling.
//
boolean PTR_ShootTraverse (intercept_t* in)
{
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;
    fixed_t             frac;

    line_t*             li;
    sector_t*           sector;
    mobj_t*             th;

    fixed_t             slope;
    fixed_t             dist;
    fixed_t             thingtopslope;
    fixed_t             thingbottomslope;

    //added:18-02-98:
    fixed_t        distz;    //dist between hit z on wall       and gun z
    fixed_t        clipz;    //dist between hit z on floor/ceil and gun z
    boolean        hitplane;    //true if we clipped z on floor/ceil plane
    boolean        diffheights; //check for sky hacks with different ceil heights

    int      sectorside; 

    if (in->isaline)
    {
        //shut up compiler, otherwise it's only used when TWOSIDED
        diffheights = false;

        li = in->d.line;

        if (li->special)
            P_ShootSpecialLine (shootthing, li);

        if ( !(li->flags & ML_TWOSIDED) )
            goto hitline;

        // crosses a two sided line
        //added:16-02-98: Fab comments : sets opentop, openbottom, openrange
        //                lowfloor is the height of the lowest floor
        //                         (be it front or back)
        P_LineOpening (li);

        dist = FixedMul (attackrange, in->frac);

        // hit lower texture ?
        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            //added:18-02-98: comments :
            // find the slope aiming on the border between the two floors
            slope = FixedDiv (openbottom - shootz , dist);
            if (slope > aimslope)
                goto hitline;
        }

        // hit upper texture ?
        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            //added:18-02-98: remember : diff ceil heights
            diffheights = true;

            slope = FixedDiv (opentop - shootz , dist);
            if (slope < aimslope)
                goto hitline;
        }

        // shot continues
        return true;


        // hit line
      hitline:

        // position a bit closer
        frac = in->frac - FixedDiv (4*FRACUNIT,attackrange);
        dist = FixedMul (frac, attackrange);    //dist to hit on line

        distz = FixedMul (aimslope, dist);      //z add between gun z and hit z
        z = shootz + distz;                     // hit z on wall

        //added:17-02-98: clip shots on floor and ceiling
        //                use a simple triangle stuff a/b = c/d ...
        // BP:13-3-99: fix the side usage
        hitplane = false;
        sectorside=P_PointOnLineSide(shootthing->x,shootthing->y,li);
        if( li->sidenum[sectorside] != -1 ) // can happen in nocliping mode
        {
            sector = sides[li->sidenum[sectorside]].sector;
            if ((z > sector->ceilingheight) && distz)
            {
                clipz = sector->ceilingheight - shootz;
                frac = FixedDiv( FixedMul(frac,clipz), distz );
                hitplane = true;
            }
            else
                if ((z < sector->floorheight) && distz)
                {
                    clipz = shootz - sector->floorheight;
                    frac = -FixedDiv( FixedMul(frac,clipz), distz );
                    hitplane = true;
                }
        }
        //SPLAT TEST ----------------------------------------------------------
        #ifdef WALLSPLATS
        if (!hitplane && demoversion>=128)
        {
            divline_t   divl;
            fixed_t     frac;

            P_MakeDivline (li, &divl);
            frac = P_InterceptVector (&divl, &trace);
            R_AddWallSplat (li, "A_DMG1", z, frac, SPLATDRAWMODE_SHADE);
        }
        #endif
        // --------------------------------------------------------- SPLAT TEST


        x = trace.x + FixedMul (trace.dx, frac);
        y = trace.y + FixedMul (trace.dy, frac);

        if (li->frontsector->ceilingpic == skyflatnum)
        {
            // don't shoot the sky!
            if (z > li->frontsector->ceilingheight)
                return false;

            //added:24-02-98: compatibility with older demos
            if (demoversion<112)
            {
                diffheights = true;
                hitplane = false;
            }

            // it's a sky hack wall
            if  (!hitplane &&      //added:18-02-98:not for shots on planes
                 li->backsector &&
                 diffheights &&    //added:18-02-98:skip only REAL sky hacks
                                   //   eg: they use different ceil heights.
                 li->backsector->ceilingpic == skyflatnum)
                return false;
        }

        // Spawn bullet puffs.
        P_SpawnPuff (x,y,z);

        // don't go any farther
        return false;
    }

    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
        return true;            // can't shoot self

    if (!(th->flags&MF_SHOOTABLE))
        return true;            // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul (attackrange, in->frac);
    thingtopslope = FixedDiv (th->z+th->height - shootz , dist);

    if (thingtopslope < aimslope)
        return true;            // shot over the thing

    thingbottomslope = FixedDiv (th->z - shootz, dist);

    if (thingbottomslope > aimslope)
        return true;            // shot under the thing


    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv (10*FRACUNIT,attackrange);

    x = trace.x + FixedMul (trace.dx, frac);
    y = trace.y + FixedMul (trace.dy, frac);
    z = shootz + FixedMul (aimslope, FixedMul(frac, attackrange));

    if (demoversion<125)
    {
        // Spawn bullet puffs or blood spots,
        // depending on target type.
        if (in->d.thing->flags & MF_NOBLOOD)
            P_SpawnPuff (x,y,z);
        else
            P_SpawnBlood (x,y,z, la_damage);
    }

    if (la_damage)
        hitplane = P_DamageMobj (th, shootthing, shootthing, la_damage);
    else
        hitplane = false;

    if (demoversion>=125)
    {
        // Spawn bullet puffs or blood spots,
        // depending on target type.
        if (in->d.thing->flags & MF_NOBLOOD)
            P_SpawnPuff (x,y,z);
        else
            if (hitplane) {
                P_SpawnBloodSplats (x,y,z, la_damage, &trace);
                return false;
            }
    }

    // don't go any farther
    return false;

}


//
// P_AimLineAttack
//
fixed_t P_AimLineAttack ( mobj_t*       t1,
                          angle_t       angle,
                          fixed_t       distance )
{
    fixed_t     x2;
    fixed_t     y2;

#ifdef PARANOIA
    if(!t1)
       I_Error("P_aimlineattack: mobj == NULL !!!");
#endif

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;

    x2 = t1->x + (distance>>FRACBITS)*finecosine[angle];
    y2 = t1->y + (distance>>FRACBITS)*finesine[angle];
    shootz = t1->z + (t1->height>>1) + 8*FRACUNIT;

    // can't shoot outside view angles

    //added:15-02-98: Fab comments...
    // Doom's base engine says that at a distance of 160,
    // the 2d graphics on the plane x,y correspond 1/1 with plane units
    topslope = 100*FRACUNIT/160;
    bottomslope = -100*FRACUNIT/160;

    attackrange = distance;
    linetarget = NULL;

    //added:15-02-98: comments
    // traverse all linedefs and mobjs from the blockmap containing t1,
    // to the blockmap containing the dest. point.
    // Call the function for each mobj/line on the way,
    // starting with the mobj/linedef at the shortest distance...
    P_PathTraverse ( t1->x, t1->y,
                     x2, y2,
                     PT_ADDLINES|PT_ADDTHINGS,
                     PTR_AimTraverse );

    //added:15-02-98: linetarget is only for mobjs, not for linedefs
    if (linetarget)
        return aimslope;

    return 0;
}


//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//
//added:16-02-98: Fab comments...
//                t1       est l'attaquant (player ou monstre)
//                angle    est l'angle de tir sur le plan x,y (orientation)
//                distance est la port‚e maximale de la balle
//                slope    est la pente vers la destination (up/down)
//                damage   est les degats infliges par la balle
void P_LineAttack ( mobj_t*       t1,
                    angle_t       angle,
                    fixed_t       distance,
                    fixed_t       slope,
                    int           damage )
{
    fixed_t     x2;
    fixed_t     y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;

    // player autoaimed attack, 
    if(demoversion<128 || !t1->player || linetarget)
    {   
        // BP: changed from * to fixedmul
        x2 = t1->x + FixedMul(distance,finecosine[angle]);
        y2 = t1->y + FixedMul(distance,finesine[angle]);
    }
    else
    {
        fixed_t cosangle=finecosine[t1->player->aiming>>ANGLETOFINESHIFT];

        x2 = t1->x + FixedMul(FixedMul(distance,finecosine[angle]),cosangle);
        y2 = t1->y + FixedMul(FixedMul(distance,finesine[angle]),cosangle); 
    }

    shootz = t1->z + (t1->height>>1) + 8*FRACUNIT;
    attackrange = distance;
    aimslope = slope;

    P_PathTraverse ( t1->x, t1->y,
                     x2, y2,
                     PT_ADDLINES|PT_ADDTHINGS,
                     PTR_ShootTraverse );
}

//
// USE LINES
//
mobj_t*         usething;

boolean PTR_UseTraverse (intercept_t* in)
{
    int         side;

    if (!in->d.line->special)
    {
        P_LineOpening (in->d.line);
        if (openrange <= 0)
        {
            S_StartSound (usething, sfx_noway);

            // can't use through a wall
            return false;
        }
        // not a special line, but keep checking
        return true ;
    }

    side = 0;
    if (P_PointOnLineSide (usething->x, usething->y, in->d.line) == 1)
        side = 1;

    //  return false;           // don't use back side
    P_UseSpecialLine (usething, in->d.line, side);

    // can't use for than one special line in a row
    return false;
}


//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
void P_UseLines (player_t*      player)
{
    int         angle;
    fixed_t     x1;
    fixed_t     y1;
    fixed_t     x2;
    fixed_t     y2;

    usething = player->mo;

    angle = player->mo->angle >> ANGLETOFINESHIFT;

    x1 = player->mo->x;
    y1 = player->mo->y;
    x2 = x1 + (USERANGE>>FRACBITS)*finecosine[angle];
    y2 = y1 + (USERANGE>>FRACBITS)*finesine[angle];

    P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
}


//
// RADIUS ATTACK
//
mobj_t*         bombsource;
mobj_t*         bombspot;
int             bombdamage;


//
// PIT_RadiusAttack
// "bombsource" is the creature
// that caused the explosion at "bombspot".
//
boolean PIT_RadiusAttack (mobj_t* thing)
{
    fixed_t     dx;
    fixed_t     dy;
    fixed_t     dz;
    fixed_t     dist;

    if (!(thing->flags & MF_SHOOTABLE) )
        return true;

    // Boss spider and cyborg
    // take no damage from concussion.
    if (thing->type == MT_CYBORG
        || thing->type == MT_SPIDER)
        return true;

    dx = abs(thing->x - bombspot->x);
    dy = abs(thing->y - bombspot->y);

    dist = dx>dy ? dx : dy;
    dist -= thing->radius;

    //added:22-02-98: now checks also z dist for rockets exploding
    //                above yer head...
    if (demoversion>=112)
    {
        dz = abs(thing->z+(thing->height>>1) - bombspot->z);
        dist = dist > dz ? dist : dz;
    }
    dist >>= FRACBITS;

    if (dist < 0)
        dist = 0;

    if (dist >= bombdamage)
        return true;    // out of range

    if ( P_CheckSight (thing, bombspot) )
    {
        // must be in direct path
        P_DamageMobj (thing, bombspot, bombsource, bombdamage - dist);
    }

    return true;
}


//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void P_RadiusAttack ( mobj_t*       spot,
                      mobj_t*       source,
                      int           damage )
{
    int         x;
    int         y;

    int         xl;
    int         xh;
    int         yl;
    int         yh;

    fixed_t     dist;

    dist = (damage+MAXRADIUS)<<FRACBITS;
    yh = (spot->y + dist - bmaporgy)>>MAPBLOCKSHIFT;
    yl = (spot->y - dist - bmaporgy)>>MAPBLOCKSHIFT;
    xh = (spot->x + dist - bmaporgx)>>MAPBLOCKSHIFT;
    xl = (spot->x - dist - bmaporgx)>>MAPBLOCKSHIFT;
    bombspot = spot;
    bombsource = source;
    bombdamage = damage;

    for (y=yl ; y<=yh ; y++)
        for (x=xl ; x<=xh ; x++)
            P_BlockThingsIterator (x, y, PIT_RadiusAttack );
}



//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//
boolean         crushchange;
boolean         nofit;


//
// PIT_ChangeSector
//
boolean PIT_ChangeSector (mobj_t*       thing)
{
    mobj_t*     mo;

    if (P_ThingHeightClip (thing))
    {
        // keep checking
        return true;
    }

    // crunch bodies to giblets
    if (thing->health <= 0)
    {
        P_SetMobjState (thing, S_GIBS);

        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;
        thing->skin = 0;

        //added:22-02-98: lets have a neat 'crunch' sound!
        S_StartSound (thing, sfx_slop);

        // keep checking
        return true;
    }

    // crunch dropped items
    if (thing->flags & MF_DROPPED)
    {
        P_RemoveMobj (thing);

        // keep checking
        return true;
    }

    if (! (thing->flags & MF_SHOOTABLE) )
    {
        // assume it is bloody gibs or something
        return true;
    }

    nofit = true;

    if (crushchange && !(leveltime&3) )
    {
        P_DamageMobj(thing,NULL,NULL,10);

        // spray blood in a random direction
        mo = P_SpawnMobj (thing->x,
                          thing->y,
                          thing->z + thing->height/2, MT_BLOOD);

        mo->momx = (P_Random() - P_Random ())<<12;
        mo->momy = (P_Random() - P_Random ())<<12;
    }

    // keep checking (crush other things)
    return true;
}



//
// P_ChangeSector
//
boolean P_ChangeSector ( sector_t*     sector,
                         boolean       crunch )
{
    int         x;
    int         y;

    nofit = false;
    crushchange = crunch;

    // re-check heights for all things near the moving sector
    for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
        for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
            P_BlockThingsIterator (x, y, PIT_ChangeSector);


    return nofit;
}
