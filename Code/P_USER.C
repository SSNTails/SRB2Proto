// p_user.c :            
//      Player related stuff.
//      Bobbing POV/weapon, movement.
//      Pending weapon.

#include "doomdef.h"
#include "d_event.h"
#include "g_game.h"
#include "p_local.h"
#include "r_main.h"
#include "s_sound.h"
#include "r_things.h" // Tails 03-01-2000

extern int         numskins; // Thanks SOM! Tails 03-01-2000
extern skin_t      skins[MAXSKINS]; // Thanks SOM! Tails 03-01-2000

// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP         32


//
// Movement.
//

// 16 pixels of bob
#define MAXBOB  0x100000

boolean         onground;



//
// P_Thrust
// Moves the given origin along a given angle.
//
void P_Thrust ( mobj_t*       mo,
                angle_t       angle,
                fixed_t       move )
{

    angle >>= ANGLETOFINESHIFT;

    mo->momx += FixedMul(move,finecosine[angle]);
    mo->momy += FixedMul(move,finesine[angle]);
}

void P_ThokUp ( mobj_t*       mo,
                angle_t       angle,
                fixed_t       move )
{

    angle >>= ANGLETOFINESHIFT;

    mo->momz += FixedMul(move,finesine[angle]);
}



//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight (player_t* player)
{
    int         angle;
    fixed_t     bob;
    fixed_t     viewheight;

    // Regular movement bobbing
    // (needs to be calculated for gun swing
    // even if not on ground)
    // OPTIMIZE: tablify angle
    // Note: a LUT allows for effects
    //  like a ramp with low health.
    player->bob =
        FixedMul (player->mo->momx, player->mo->momx)
        + FixedMul (player->mo->momy,player->mo->momy);

    player->bob >>= 2;

    if (player->bob>MAXBOB)
        player->bob = MAXBOB;

    if ((player->cheats & CF_NOMOMENTUM) || !onground)
    {
        //added:15-02-98: it seems to be useless code!
        //player->viewz = player->mo->z + (cv_viewheight.value<<FRACBITS);

        //if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
        //    player->viewz = player->mo->ceilingz-4*FRACUNIT;

        player->viewz = player->mo->z + player->viewheight;
        return;
    }

    angle = (FINEANGLES/20*leveltime)&FINEMASK;
    bob = FixedMul ( player->bob/2, finesine[angle]);


    // move viewheight
    viewheight = cv_viewheight.value << FRACBITS; // default eye view height

    if (player->playerstate == PST_LIVE)
    {
        player->viewheight += player->deltaviewheight;

        if (player->viewheight > viewheight)
        {
            player->viewheight = viewheight;
            player->deltaviewheight = 0;
        }

        if (player->viewheight < viewheight/2)
        {
            player->viewheight = viewheight/2;
            if (player->deltaviewheight <= 0)
                player->deltaviewheight = 1;
        }

        if (player->deltaviewheight)
        {
            player->deltaviewheight += FRACUNIT/4;
            if (!player->deltaviewheight)
                player->deltaviewheight = 1;
        }
    }
    player->viewz = player->mo->z + player->viewheight + bob;

    if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
        player->viewz = player->mo->ceilingz-4*FRACUNIT;
}



//
// P_MovePlayer
//
void P_MovePlayer (player_t* player)
{
    ticcmd_t*           cmd;

    cmd = &player->cmd;

#ifndef ABSOLUTEANGLE
    player->mo->angle += (cmd->angleturn<<16);
#else
    if(demoversion<125)
        player->mo->angle += (cmd->angleturn<<16);
    else
        player->mo->angle = (cmd->angleturn<<16);
#endif

    // Do not let the player control movement
    //  if not onground.
    onground = ((player->mo->z <= player->mo->floorz) ||
               (player->cheats & CF_FLYAROUND));
		

    if(demoversion<128)
    {
        boolean  jumpover = player->cheats & CF_JUMPOVER;
        if (cmd->forwardmove && (onground || jumpover))
        {
            // dirty hack to let the player avatar walk over a small wall
            // while in the air
            if (jumpover && player->mo->momz > 0)
                P_Thrust (player->mo, player->mo->angle, 5*5120); // Changed by Tails: 9-14-99
            else
                if (!jumpover && player->skin == 1)
                    P_Thrust (player->mo, player->mo->angle, cmd->forwardmove*3072); // Changed by Tails: 9-14-99
            else
                if (!jumpover && player->skin == 0)
                    P_Thrust (player->mo, player->mo->angle, cmd->forwardmove*5120);
        }
    
        if (cmd->sidemove && onground)
            P_Thrust (player->mo, player->mo->angle-ANG90, cmd->sidemove*2048); // Changed by Tails: 9-14-99

//        player->aiming = (signed char)cmd->aiming;
    }

    else
    {
        fixed_t   movepushforward=0,movepushside=0;
        player->aiming = cmd->aiming<<16;

        if (cmd->forwardmove)
        {
           if(player->powers[pw_strength]) // do you have super sneakers? Tails 02-28-2000
             {
             if (player->skin == 1)
               {
               movepushforward = cmd->forwardmove * 6144; // then go faster!! Tails 02-28-2000
               }
            else if (player->skin == 0)
               {
               movepushforward = cmd->forwardmove * 10240; // then go faster!! Tails 02-28-2000
               }
             }
          else // if not, then run normally Tails 02-28-2000
           if (player->skin == 1)
              {
               movepushforward = cmd->forwardmove * 3072; // Changed by Tails: 9-14-99
              }
          else
           if (player->skin == 0)
              {
               movepushforward = cmd->forwardmove * 5120; // Changed by Tails: 9-14-99
              }
             if (player->mo->eflags & MF_UNDERWATER)
            {
                // half forward speed when waist under water
                // a little better grip if feets touch the ground
                if (!onground)
                    movepushforward >>= 1;
                else
                    movepushforward = movepushforward *3/4;
            }
            else
            {
                // allow very small movement while in air for gameplay
                if (!onground)
                {  
                 movepushforward >>= 2; // Proper air movement - Changed by Tails: 9-13-99
                }

                //Dont accelerate while spinning: Stealth 2-5-00
                if (player->mo->eflags & MF_SPINNING)
                {
                 movepushforward=0;
                }
            }

            P_Thrust (player->mo, player->mo->angle, movepushforward);

        }

        if (cmd->sidemove)
        {
            movepushside = cmd->sidemove * 2048;
            if (player->mo->eflags & MF_UNDERWATER)
            {
                if (!onground)
                    movepushside >>= 1;
                else
                    movepushside = movepushside *3/4;
            }
            else 
                if (!onground)
                    movepushside >>= 3;

            P_Thrust (player->mo, player->mo->angle-ANG90, movepushside);
        }

        // mouselook swim when waist underwater
        player->mo->eflags &= ~MF_SWIMMING;
        if (player->mo->eflags & MF_UNDERWATER)
        {
             fixed_t a;
            // swim up/down full move when forward full speed
            a = FixedMul( movepushforward*50, finesine[ (player->aiming>>ANGLETOFINESHIFT) ] >>5 );

            /* a little hack to don't have screen moving
            if( a > cv_gravity.value>>2 || a < 0 )*/
            if ( a != 0 ) {
                player->mo->eflags |= MF_SWIMMING;
                player->mo->momz += a;
                
            }
        }
    }

if (!(player->mo->state == &states[S_PLAY_ABL1] || player->mo->state == &states[S_PLAY_ABL2]))
{
player->powers[pw_tailsfly] = 0;
}

// start tails putput noise Tails 03-05-2000
if (player->mo->state == &states[S_PLAY_ABL1] && player->skin == 1 && player->powers[pw_tailsfly] && player->skin == 1)
{
            S_StartSound (player->mo, sfx_sawidl);
}

if (player->powers[pw_tailsfly] == 1 && player->skin == 1)
{
        P_SetMobjState (player->mo, S_PLAY_SPC4);
}

if (player->mo->state->nextstate == S_PLAY_SPC1 && player->skin == 1 && !player->powers[pw_tailsfly])
{
            S_StartSound (player->mo, sfx_sawful);
}
// end tails putput noise Tails 03-05-2000

// start shield spawn code Tails 03-04-2000
if (player->powers[pw_blueshield])
{
    P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_GREENORB);
}   

// start improved player landing friction Tails 03-03-2000
if ((player->mo->eflags & MF_JUSTHITFLOOR) && !(cmd->forwardmove))
     {
	player->mo->momx = player->mo->momx/2;
	player->mo->momy = player->mo->momy/2;
      }
// end improved player landing friction Tails 03-03-2000

// start timer code tails 02-29-2000
        player->armorpoints=leveltime/35;
// end timer code tails 02-29-2000

//start invincibility sparkles spawn code tails
if (player->powers[pw_invulnerability])
   {
    P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_IVSP);
   }
//end invincibility sparkles spawn code tails

// start resume normal music tails
if ((player->powers[pw_invulnerability] == 1) || (player->powers[pw_strength] == 1))
   {
    S_ChangeMusic(mus_runnin + gamemap - 1, 1);
   }
// end resume normal music tails

//start shield spawn code
//Edited to work with new shield handling: Stealth 12-26-99
if (player->powers[pw_strength])
   {
    P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_THOK);
   }
//end shield spawn code

    //Homing Attack: Stealth 12-31-99
//       if(player->wants_to_thok && player->thok_dist)
//       {
//            player->mo->angle=player->thok_angle;
//            P_Thrust (player->mo, player->mo->angle, 5*512000);
//            P_ThokUp (player->mo, player->thok_angle, 5*512000);
//            S_StartSound (player->mo, sfx_pdiehi);
//            P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_THOK);
//       player->wants_to_thok=0;
//       player->thok_dist=999999;
//       }
    player->thok_dist=512*FRACUNIT;

// start moved homing junk here tails 02-27-2000
       if(player->wants_to_thok && player->thok_dist)
       {
//            player->mo->angle=player->thok_angle;
            P_Thrust (player->mo, player->mo->angle, 5*512000); // SSNTails 08-19-2023 note: Approximately 39.0625*FRACUNIT
            P_ThokUp (player->mo, player->thok_angle, 5*512000);
            S_StartSound (player->mo, sfx_pdiehi);
            P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_THOK);
        player->wants_to_thok=0;
        player->thok_dist=0;
        player->thok_angle=0;
       }
// end moved homing junk here tails 02-27-2000

    if(cmd->buttons &BT_ATTACK)
      {
       player->releasedash = 1; // the first spindash rev
           if((player->mo->eflags & MF_STARTDASH) && (player->mo->eflags & MF_SPINNING) && (player->releasedash))
     {
     player->mo->eflags &= ~MF_STARTDASH;
     P_Thrust (player->mo, player->mo->angle, 10*player->dashspeed); // catapult forward ho!! Tails 02-27-2000
     S_StartSound (player->mo, sfx_punch);
     player->dashspeed = 0;
     }
      }

    if(!(cmd->buttons &BT_ATTACK))
     {
       player->releasedash = 0;
      }

    if(!(cmd->buttons &BT_USE))
    {
     player->usedown = false;
    }

    //Spinning and Spindashing
    if(cmd->buttons &BT_USE) // subsequent revs
    {

    if(((player->mo->z <= player->mo->floorz) && (player->mo->momz || !(player->mo->momx || player->mo->momy)) && (player->mo->state == &states[S_PLAY]) && (!(player->usedown)) && (!(player->mo->eflags & MF_SPINNING))))
     {
    P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_THOK);
    player->mo->eflags |= MF_STARTDASH;
    player->mo->eflags |= MF_SPINNING;
    player->dashspeed+=FRACUNIT; // more speed as you rev more Tails 03-01-2000
    P_SetMobjState (player->mo, S_PLAY_ATK3);
    S_StartSound (player->mo, sfx_noway);
            player->usedown = true;
     }

    if((player->mo->eflags & MF_STARTDASH) && (!(player->usedown)))
       {
        P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_THOK);
        P_SetMobjState (player->mo, S_PLAY_ATK3);
        player->dashspeed+=FRACUNIT;
        S_StartSound (player->mo, sfx_noway);
       }

     if(!(player->mo->momz) && (player->mo->momy || player->mo->momx) && (!(player->usedown)) && (!(player->mo->eflags & MF_SPINNING)))
     {
      player->mo->eflags |= MF_SPINNING;
      P_SetMobjState (player->mo, S_PLAY_ATK3);
      S_StartSound (player->mo, sfx_sawup);
      player->usedown = true;
     }

}

    if((player->mo->momz || !(player->mo->momx || player->mo->momy)) && (player->mo->eflags & MF_SPINNING) && (!(player->mo->eflags & MF_STARTDASH))) // has the player stopped & is still spinning?
     {
     player->mo->eflags &= ~MF_SPINNING;
     player->mo->eflags &= ~MF_STARTDASH;
     P_SetMobjState (player->mo, S_PLAY); // returning from a roll
     player->usedown = false;
      }

    //added:22-02-98: jumping
    if (cmd->buttons & BT_JUMP)
    {
        // can't jump while in air, can't jump while jumping

        if (!player->jumpdown &&
             (onground || (player->mo->eflags & MF_UNDERWATER)) )
             
        {
            if (onground)
                player->mo->momz = JUMPGRAVITY*1.5; // Proper Jumping - Changed by Tails: 9-13-99
                if(!(player->mo->eflags & MF_JUMPED)) // Tails 9-15-99 Spin Attack
		player->mo->eflags += MF_JUMPED; // Tails 9-15-99 Spin Attack
		player->mo->eflags &= ~MF_ONGROUND; // Tails 9-15-99 Spin Attack
		player->mo->eflags &= ~MF_JUSTHITFLOOR; // Tails 9-15-99 Spin Attack
                
        //      else //water content // Tails 9-24-99
           //     player->mo->momz = JUMPGRAVITY*1.5; // Tails 9-24-99

            //TODO: goub gloub when push up in water

            if ( !(player->cheats & CF_FLYAROUND) && onground && !(player->mo->eflags & MF_UNDERWATER))
            {
                S_StartSound (player->mo, sfx_jump);
                P_SetMobjState (player->mo, S_PLAY_ATK3); // Tails 9-24-99
                // keep jumping ok if FLY mode.
                player->jumpdown = true;
            }
// Player now spins underwater! Joy! Tails 10-31-99
            if ( !(player->cheats & CF_FLYAROUND) && onground && (player->mo->eflags & MF_UNDERWATER))
            {
                P_SetMobjState (player->mo, S_PLAY_ATK3); // Tails 9-24-99
                S_StartSound (player->mo, sfx_jump);
                if(!(player->mo->eflags & MF_JUMPED)) // Tails 9-15-99 Spin Attack
		player->mo->eflags += MF_JUMPED; // Tails 9-15-99 Spin Attack
		player->mo->eflags &= ~MF_ONGROUND; // Tails 9-15-99 Spin Attack
                // keep jumping ok if FLY mode.
                player->jumpdown = true;
            }
// end underwater spin code

        }

//Moved "Thok" code, placing it on jump button and killing the repeat: Stealth 12-25-99
//TODO: place tails flight and knuckles gliding here if they are added to game
//skin[player->skin].typechar == 1

        else if ((player->mo->eflags & MF_JUMPED) && !(player->powers[pw_tailsfly]) && !(player->jumpdown) && (player->skin == 1))
            {
           if (!(player->mo->state == &states[S_PLAY_ABL1] || player->mo->state == &states[S_PLAY_ABL2]))
              {
                P_SetMobjState (player->mo, S_PLAY_ABL1);
              }
            player->mo->momz = JUMPGRAVITY*1;
            player->jumpdown = true;
            player->powers[pw_tailsfly] = 350;
            player->mo->eflags &= ~MF_JUMPED;
            }

        else if ((player->powers[pw_tailsfly]) && !(player->jumpdown) && (player->skin == 1))
            {
           if (!(player->mo->state == &states[S_PLAY_ABL1] || player->mo->state == &states[S_PLAY_ABL2]))
              {
                P_SetMobjState (player->mo, S_PLAY_ABL1);
              }
            player->mo->momz = JUMPGRAVITY*1;
            player->jumpdown = true;
            }

        else if ((player->mo->eflags & MF_JUMPED) && !(player->jumpdown) && (player->skin == 0))
           {
         //   P_Thrust (player->mo, player->mo->angle, 5*512000);
         //   S_StartSound (player->mo, sfx_pdiehi);
         //   P_SpawnMobj (player->mo->x, player->mo->y, player->mo->z + (player->mo->info->height / 2), MT_THOK);
            player->wants_to_thok=1;
            player->jumpdown = true;
           }

    }
    else
        player->jumpdown = false;

    if  ( (cmd->forwardmove || cmd->sidemove)
         && (player->mo->state == &states[S_PLAY] || player->mo->state == &states[S_PLAY_TAP1] || player->mo->state == &states[S_PLAY_TAP2]))
    {
        P_SetMobjState (player->mo, S_PLAY_RUN1);
    }

}

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
#define ANG5    (ANG90/18)

void P_DeathThink (player_t* player)
{
    angle_t             angle;
    angle_t             delta;
    mobj_t*             attacker;       //added:22-02-98:
    fixed_t             dist;           //added:22-02-98:
    int                 pitch;          //added:22-02-98:

    P_MovePsprites (player);

    // fall to the ground
    if (player->viewheight > 6*FRACUNIT)
        player->viewheight -= FRACUNIT;

    if (player->viewheight < 6*FRACUNIT)
        player->viewheight = 6*FRACUNIT;

    player->deltaviewheight = 0;
    onground = player->mo->z <= player->mo->floorz;

    P_CalcHeight (player);

    attacker = player->attacker;

    if (attacker && attacker != player->mo)
    {
        angle = R_PointToAngle2 (player->mo->x,
                                 player->mo->y,
                                 player->attacker->x,
                                 player->attacker->y);

        delta = angle - player->mo->angle;

        if (delta < ANG5 || delta > (unsigned)-ANG5)
        {
            // Looking at killer,
            //  so fade damage flash down.
            player->mo->angle = angle;

            if (player->damagecount)
                player->damagecount--;
        }
        else if (delta < ANG180)
            player->mo->angle += ANG5;
        else
            player->mo->angle -= ANG5;

        //added:22-02-98:
        // change aiming to look up or down at the attacker (DOESNT WORK)
        // FIXME : the aiming returned seems to be too up or down... later


            dist = P_AproxDistance (attacker->x - player->mo->x, attacker->y - player->mo->y);
            //if (dist)
            //    pitch = FixedMul ((160<<FRACBITS), FixedDiv (attacker->z + (attacker->height>>1), dist)) >>FRACBITS;
            //else
            //    pitch = 0;
            pitch = (attacker->z - player->mo->z)>>17;
            player->aiming = G_ClipAimingPitch (&pitch);

    }
    else if (player->damagecount)
        player->damagecount--;

    if (player->cmd.buttons & BT_JUMP) // Respawn with Jump button Tails 12-04-99
        player->playerstate = PST_REBORN;
}


//
// P_MoveCamera : make sure the camera is not outside the world
//                and looks at the player avatar
//

camera_t camera;

//#define VIEWCAM_DIST    (128<<FRACBITS)
//#define VIEWCAM_HEIGHT  (20<<FRACBITS)

consvar_t cv_cam_dist   = {"cam_dist","128",CV_FLOAT,NULL};
consvar_t cv_cam_height = {"cam_height","20",CV_FLOAT,NULL};
consvar_t cv_cam_speed  = {"cam_speed","0.25",CV_FLOAT,NULL};

void P_ResetCamera (player_t *player)
{
    fixed_t             x;
    fixed_t             y;
    fixed_t             z;

    camera.chase = true;
    x = player->mo->x;
    y = player->mo->y;
    z = player->mo->z + (cv_viewheight.value<<FRACBITS);

    // hey we should make sure that the sounds are heard from the camera
    // instead of the marine's head : TO DO

    // set bits for the camera
    if (!camera.mo)
        camera.mo = P_SpawnMobj (x,y,z, MT_CHASECAM);
    else
    {
        camera.mo->x = x;
        camera.mo->y = y;
        camera.mo->z = z;
    }

    camera.mo->angle = player->mo->angle;
    camera.aiming = 0;
}

boolean PTR_FindCameraPoint (intercept_t* in)
{
/*    int         side;
    fixed_t             slope;
    fixed_t             dist;
    line_t*             li;

    li = in->d.line;

    if ( !(li->flags & ML_TWOSIDED) )
        return false;

    // crosses a two sided line
    //added:16-02-98: Fab comments : sets opentop, openbottom, openrange
    //                lowfloor is the height of the lowest floor
    //                         (be it front or back)
    P_LineOpening (li);

    dist = FixedMul (attackrange, in->frac);

    if (li->frontsector->floorheight != li->backsector->floorheight)
    {
        //added:18-02-98: comments :
        // find the slope aiming on the border between the two floors
        slope = FixedDiv (openbottom - cameraz , dist);
        if (slope > aimslope)
            return false;
    }

    if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
    {
        slope = FixedDiv (opentop - shootz , dist);
        if (slope < aimslope)
            goto hitline;
    }

    return true;

    // hit line
  hitline:*/
    // stop the search
    return false;
}

fixed_t cameraz;

void P_MoveChaseCamera (player_t *player)
{
    angle_t             angle;
    fixed_t             x,y,z ,viewpointx,viewpointy;
    fixed_t             dist;
    mobj_t*             mo;
    subsector_t*        newsubsec;
    float               f1,f2;

    if (!camera.mo)
        P_ResetCamera (player);
    mo = player->mo;

    angle = mo->angle;

    // sets ideal cam pos
    dist  = cv_cam_dist.value;
    x = mo->x - FixedMul( finecosine[(angle>>ANGLETOFINESHIFT) & FINEMASK], dist);
    y = mo->y - FixedMul(   finesine[(angle>>ANGLETOFINESHIFT) & FINEMASK], dist);
    z = mo->z + (cv_viewheight.value<<FRACBITS) + cv_cam_height.value;

/*    P_PathTraverse ( mo->x, mo->y, x, y, PT_ADDLINES, PTR_UseTraverse );*/

    // move camera down to move under lower ceilings
    newsubsec = R_IsPointInSubsector ((mo->x + camera.mo->x)>>1,(mo->y + camera.mo->y)>>1);
              
    if (!newsubsec)
    {
        // use player sector 
        if (mo->subsector->sector->ceilingheight - camera.mo->height < z)
            z = mo->subsector->sector->ceilingheight - camera.mo->height-11*FRACUNIT; // don't be blocked by a opened door
    }
    else
    // camera fit ?
    if (newsubsec->sector->ceilingheight - camera.mo->height < z)
        // no fit
        z = newsubsec->sector->ceilingheight - camera.mo->height-11*FRACUNIT;
        // is the camera fit is there own sector
    newsubsec = R_PointInSubsector (camera.mo->x,camera.mo->y);
        if (newsubsec->sector->ceilingheight - camera.mo->height < z)
        z = newsubsec->sector->ceilingheight - camera.mo->height-11*FRACUNIT;


    // point viewed by the camera
    // this point is just 64 unit forward the player
    dist = 64 << FRACBITS;
    viewpointx = mo->x + FixedMul( finecosine[(angle>>ANGLETOFINESHIFT) & FINEMASK], dist);
    viewpointy = mo->y + FixedMul( finesine[(angle>>ANGLETOFINESHIFT) & FINEMASK], dist);

    camera.mo->angle = R_PointToAngle2(camera.mo->x,camera.mo->y,
                                       viewpointx  ,viewpointy);

    // folow the player
    camera.mo->momx = FixedMul(x - camera.mo->x,cv_cam_speed.value);
    camera.mo->momy = FixedMul(y - camera.mo->y,cv_cam_speed.value);
    camera.mo->momz = FixedMul(z - camera.mo->z,cv_cam_speed.value);
//    camera.mo->angle= mo->angle;

    // compute aming to look the player
    f1=FIXED_TO_FLOAT(mo->x-camera.mo->x);
    f2=FIXED_TO_FLOAT(mo->y-camera.mo->y);
    dist=sqrt(f1*f1+f2*f2)*FRACUNIT;
    angle=R_PointToAngle2(0,camera.mo->z,
                                  dist ,mo->z+(mo->height>>1));

    G_ClipAimingPitch(&angle);
    dist=camera.aiming-angle;
    camera.aiming-=(dist>>4);
    

    // the old code
//    adif = (camera.mo->angle - angle);   // warning signed value
//    camera.mo->angle -= ((int)adif>>4);
}

#ifdef CLIENTPREDICTION
void P_MoveSpirit (player_t* player,ticcmd_t *cmd)
{

#ifdef PARANOIA
    if(!player)
        I_Error("P_MoveSpirit : player null");
    if(!player->spirit)
        I_Error("P_MoveSpirit : player->spirit null");
    if(!cmd)
        I_Error("P_MoveSpirit : cmd null");
#endif

//    player->spirit->angle = localangle;

    if (cmd->forwardmove)
        P_Thrust (player->spirit, player->spirit->angle, cmd->forwardmove*2048);

    if (cmd->sidemove && onground)
        P_Thrust (player->spirit, player->spirit->angle-ANG90, cmd->sidemove*2048);

//    camera.aiming = (signed char)cmd->aiming;
}
#endif


//
// P_PlayerThink
//

boolean playerdeadview; //Fab:25-04-98:show dm rankings while in death view

void P_PlayerThink (player_t* player)
{
    ticcmd_t*           cmd;
    weapontype_t        newweapon;
    int                 waterz;

#ifdef PARANOIA
    if(!player->mo) I_Error("p_playerthink : players[%d].mo == NULL",player-players);
#endif

    // fixme: do this in the cheat code
    if (player->cheats & CF_NOCLIP)
        player->mo->flags |= MF_NOCLIP;
    else
        player->mo->flags &= ~MF_NOCLIP;

    // chain saw run forward
    cmd = &player->cmd;
    if (player->mo->flags & MF_JUSTATTACKED)
    {
// added : now angle turn is a absolute value not relative
#ifndef ABSOLUTEANGLE
        cmd->angleturn = 0;
#endif
        cmd->forwardmove = 0xc800/512;
        cmd->sidemove = 0;
        player->mo->flags &= ~MF_JUSTATTACKED;
    }

    if (player->playerstate == PST_DEAD)
    {
        //Fab:25-04-98: show the dm rankings while dead, only in deathmatch
        if (player==&players[displayplayer])
            playerdeadview = true;

        P_DeathThink (player);

        //added:26-02-98:camera may still move when guy is dead
        if (camera.chase)
            P_MoveChaseCamera (&players[displayplayer]);
        return;
    }
    else
        if (player==&players[displayplayer])
            playerdeadview = false;

    // check water content, set stuff in mobj
    P_MobjCheckWater (player->mo);

    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if (player->mo->reactiontime)
        player->mo->reactiontime--;
    else
        P_MovePlayer (player);

    //added:22-02-98: bob view only if looking by the marine's eyes
    if (!camera.chase)
        P_CalcHeight (player);

    //added:26-02-98: calculate the camera movement
    if (camera.chase && player==&players[displayplayer])
        P_MoveChaseCamera (&players[displayplayer]);

    // check special sectors : damage & secrets
    P_PlayerInSpecialSector (player);

    //
    // water splashes
    //
    if (demoversion>=125 && player->specialsector >= 887 &&
                            player->specialsector <= 888)
    {
        if ((player->mo->momx >  (2*FRACUNIT) ||
             player->mo->momx < (-2*FRACUNIT) ||
             player->mo->momy >  (2*FRACUNIT) ||
             player->mo->momy < (-2*FRACUNIT) ||
             player->mo->momz >  (2*FRACUNIT)) &&  // jump out of water
             !(gametic & 31)          )
        {
            //
            // make sur we disturb the surface of water (we touch it)
            //
            if (player->specialsector==887)
                //FLAT TEXTURE 'FWATER'
                waterz = player->mo->subsector->sector->floorheight + (FRACUNIT/4);
            else
                //faB's current water hack using negative sector tags
                waterz = player->mo->subsector->sector->watersector->floorheight; //SOM:

            // half in the water
            if(player->mo->eflags & MF_TOUCHWATER)
            {
                if (player->mo->z <= player->mo->floorz) // onground
                {
                    fixed_t whater_height=waterz-player->mo->subsector->sector->floorheight;

                    if( whater_height<(player->mo->height>>2 ))
                        S_StartSound (player->mo, sfx_splash);
                    else
                        S_StartSound (player->mo, sfx_floush);
                }
                else
                    S_StartSound (player->mo, sfx_floush);
            }                   
        }
    }

    // Check for weapon change.
    if (cmd->buttons & BT_CHANGE)
    {

        // The actual changing of the weapon is done
        //  when the weapon psprite can do it
        //  (read: not in the middle of an attack).
        newweapon = (cmd->buttons&BT_WEAPONMASK)>>BT_WEAPONSHIFT;
        if(demoversion<128)
        {
            if (newweapon == wp_fist
                && player->weaponowned[wp_chainsaw]
                && !(player->readyweapon == wp_chainsaw
                     && player->powers[pw_strength]))
            {
                newweapon = wp_chainsaw;
            }
        
            if ( (gamemode == commercial)
                && newweapon == wp_shotgun
                && player->weaponowned[wp_supershotgun]
                && player->readyweapon != wp_supershotgun)
            {
                newweapon = wp_supershotgun;
            }
        }
        else
        {
            if(cmd->buttons&BT_EXTRAWEAPON)
               switch(newweapon) {
                  case wp_shotgun : 
                       if( gamemode == commercial && player->weaponowned[wp_supershotgun])
                           newweapon = wp_supershotgun;
                       break;
                  case wp_fist :
                       if( gamemode == commercial && player->weaponowned[wp_chainsaw])
                           newweapon = wp_chainsaw;
                       break;
                  default:
                       break;
               }
        }

        if (player->weaponowned[newweapon]
            && newweapon != player->readyweapon)
        {
            // Do not go to plasma or BFG in shareware,
            //  even if cheated.
            if ((newweapon != wp_plasma
                 && newweapon != wp_bfg)
                || (gamemode != shareware) )
            {
                player->pendingweapon = newweapon;
            }
        }
    }

    // check for use
    if (cmd->buttons & BT_USE)
    {
        if (!player->usedown)
        {
            P_UseLines (player);
            player->usedown = true;
        }
    }
    else
        player->usedown = false;

    // cycle psprites
    P_MovePsprites (player);
    // Counters, time dependend power ups.

    // Strength counts up to diminish fade.
    if (player->powers[pw_strength])
        player->powers[pw_strength]--; // decrease timer Tails 02-28-2000

    if (player->powers[pw_invulnerability])
        player->powers[pw_invulnerability]--;

    // the MF_SHADOW activates the tr_transhi translucency while it is set
    // (it doesnt use a preset value through FF_TRANSMASK)
    if (player->powers[pw_invisibility])
        if (! --player->powers[pw_invisibility] )
            player->mo->flags &= ~MF_SHADOW;

    if (player->powers[pw_infrared])
        player->powers[pw_infrared]--;

    if (player->powers[pw_ironfeet])
        player->powers[pw_ironfeet]--;

    if (player->powers[pw_tailsfly]) // tails fly
        player->powers[pw_tailsfly]--; // counter Tails 03-05-2000

    if (player->damagecount)
        player->damagecount--;

    if (player->bonuscount)
        player->bonuscount--;

    // Handling colormaps.
    if (player->powers[pw_invulnerability])
    {
        if (player->powers[pw_invulnerability] > 4*TICRATE
            || (player->powers[pw_invulnerability]&8) )
            player->fixedcolormap = INVERSECOLORMAP;
        else
            player->fixedcolormap = 0;
    }
    else if (player->powers[pw_infrared])
    {
        if (player->powers[pw_infrared] > 4*TICRATE
            || (player->powers[pw_infrared]&8) )
        {
            // almost full bright
            player->fixedcolormap = 1;
        }
        else
            player->fixedcolormap = 0;
    }
    else
        player->fixedcolormap = 0;

}
