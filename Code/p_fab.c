// p_fab.c : some new action routines, separated from the original doom
//           sources, so that you can include it or remove it easy.
//

#include "doomdef.h"
#include "g_game.h"
#include "p_local.h"
#include "r_state.h"
#include "p_fab.h"
#include "m_random.h"

//
// Action routine, for the ROCKET thing.
// This one adds trails of smoke to the rocket.
// The action pointer of the S_ROCKET state must point here to take effect.
// This routine is based on the Revenant Fireball Tracer code A_Tracer()
//
void A_SmokeTrailer (mobj_t* actor)
{
    mobj_t*     th;

    if (gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket
    if (demoversion<125 && // rocket trails spawnpuff from v1.11 to v1.24
        demoversion>=111) // skull trails since v1.25
        P_SpawnPuff (actor->x, actor->y, actor->z);

    // add the smoke behind the rocket
    th = P_SpawnMobj (actor->x-actor->momx,
                      actor->y-actor->momy,
                      actor->z, MT_SMOK);

    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;
    if (th->tics < 1)
        th->tics = 1;
}


//  Set the translucency map for each frame state of mobj
//
void R_SetTrans (statenum_t state1, statenum_t state2, transnum_t transmap)
{
    state_t*   state = &states[state1];
    do
    {
        state->frame |= (transmap<<FF_TRANSSHIFT);
        state++;
    } while (state1++<state2);
}


//  hack the translucency in the states for a set of standard doom sprites
//
void P_SetTranslucencies (void)
{

    //mobjinfo[MT_TRACER].flags |= MF_SHADOW;       //revenant fireball
    R_SetTrans (S_TRACER, S_TRACER2, tr_transfir);
     R_SetTrans (S_TRACEEXP1, S_TRACEEXP3, tr_transmed);

    //mobjinfo[MT_SMOKE].flags |= MF_SHADOW;          //rev. fireball. smoke trail
    R_SetTrans (S_SMOKE1, S_SMOKE5, tr_transmed);

    //mobjinfo[MT_TROOPSHOT].flags |= MF_SHADOW;      //imp fireball
    R_SetTrans (S_TBALL1, S_TBALL2, tr_transfir);
     R_SetTrans (S_TBALLX1, S_TBALLX3, tr_transmed);

    //mobjinfo[MT_FIRE].flags |= MF_SHADOW;           //archvile attack
    R_SetTrans (S_FIRE1, S_FIRE30, tr_transmed);
    //R_SetTrans (S_FIRE11, S_FIRE20, tr_transmed);
    //R_SetTrans (S_FIRE21, S_FIRE30, tr_transmor);


    //mobjinfo[MT_BFG].flags |= MF_SHADOW;            //bfg ball
    R_SetTrans (S_BFGSHOT, S_BFGSHOT2, tr_transfir);
     R_SetTrans (S_BFGLAND, S_BFGLAND3, tr_transmed);
     R_SetTrans (S_BFGLAND4, S_BFGLAND6, tr_transmor);
    R_SetTrans (S_BFGEXP, 0, tr_transmed);
    R_SetTrans (S_BFGEXP2, S_BFGEXP4, tr_transmor);

    //mobjinfo[MT_PLASMA].flags |= MF_SHADOW;         //plasma bullet
    R_SetTrans (S_PLASBALL, S_PLASBALL2, tr_transfir);
     R_SetTrans (S_PLASEXP, S_PLASEXP2, tr_transmed);
     R_SetTrans (S_PLASEXP3, S_PLASEXP5, tr_transmor);

    //mobjinfo[MT_PUFF].flags |= MF_SHADOW;           //bullet puff
    R_SetTrans (S_PUFF1, S_PUFF4, tr_transmor);

    //mobjinfo[MT_TFOG].flags |= MF_SHADOW;           //teleport fog
    R_SetTrans (S_TFOG, S_TFOG5, tr_transmed);
    R_SetTrans (S_TFOG6, S_TFOG10, tr_transmor);

    //mobjinfo[MT_IFOG].flags |= MF_SHADOW;           //respawn item fog
    R_SetTrans (S_IFOG, S_IFOG5, tr_transmed);

    //mobjinfo[MT_EMMY].flags |= MF_SHADOW;         //emerald transies tails
    R_SetTrans (S_EMMY1, S_EMMY2, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY3, S_EMMY4, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY5, S_EMMY6, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY7, S_EMMY8, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY9, S_EMMY10, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY11, S_EMMY12, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY13, S_EMMY14, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY15, S_EMMY16, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY17, S_EMMY18, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY19, S_EMMY20, tr_transmed); // emerald transies tails
    R_SetTrans (S_EMMY21, 0, tr_transmed); // emerald transies tails
    //mobjinfo[MT_INV].flags |= MF_SHADOW;            //invulnerability
//    R_SetTrans (S_PINV, S_PINV4, tr_transmed);
    //mobjinfo[MT_INS].flags |= MF_SHADOW;            //blur artifact
    R_SetTrans (S_PINS, S_PINS4, tr_transmed);
    //mobjinfo[MT_MEGA].flags |= MF_SHADOW;           //megasphere
    R_SetTrans (S_MEGA, S_MEGA4, tr_transmed);

    //added:18-01-98: torches... with a special tinttab
    //mobjinfo[MT_MISC41].flags |= MF_SHADOW; blue torch
    //mobjinfo[MT_MISC42].flags |= MF_SHADOW;
    //mobjinfo[MT_MISC43].flags |= MF_SHADOW;
    R_SetTrans (S_GREENTORCH, S_REDTORCH4, tr_transfx1);
    //mobjinfo[MT_MISC44].flags |= MF_SHADOW; short blue torch
    //mobjinfo[MT_MISC45].flags |= MF_SHADOW;
    //mobjinfo[MT_MISC46].flags |= MF_SHADOW;
    R_SetTrans (S_GTORCHSHRT, S_RTORCHSHRT4, tr_transfx1);

    // flaming barrel !!
//    R_SetTrans (S_BBAR1, S_BBAR3, tr_transfx1); Don't make the seaweed trans!! Tails 10-31-99

    //added:27-02-98: transparency ahoy!! people want it, they have it!!
    //mobjinfo[MT_SKULL].flags |= MF_SHADOW;           //lost soul
    R_SetTrans (S_SKULL_STND, S_SKULL_DIE6, tr_transfx1);
    //mobjinfo[MT_BRUISERSHOT].flags |= MF_SHADOW;     //baron shot
    R_SetTrans (S_BRBALL1, S_BRBALL2, tr_transfir);
     R_SetTrans (S_BRBALLX1, S_BRBALLX3, tr_transmed);
    //mobjinfo[MT_SPAWNFIRE].flags |= MF_SHADOW;       //demon spawnfire
    R_SetTrans (S_SPAWNFIRE1, S_SPAWNFIRE3, tr_transfir);
    R_SetTrans (S_SPAWNFIRE4, S_SPAWNFIRE8, tr_transmed);
    //mobjinfo[MT_HEADSHOT].flags |= MF_SHADOW;        //caco fireball
    R_SetTrans (S_RBALL1, S_RBALL2, tr_transfir);
     R_SetTrans (S_RBALLX1, S_RBALLX3, tr_transmed);

    //mobjinfo[MT_ARACHPLAZ].flags |= MF_SHADOW;       //arachno shot
    R_SetTrans (S_ARACH_PLAZ, S_ARACH_PLAZ2, tr_transfir);
     R_SetTrans (S_ARACH_PLEX, S_ARACH_PLEX2, tr_transmed);
     R_SetTrans (S_ARACH_PLEX3, S_ARACH_PLEX4, tr_transmor);
     R_SetTrans (S_ARACH_PLEX5, 0, tr_transhi);

    //mobjinfo[MT_BLOOD].flags |= MF_SHADOW;           //blood puffs!
    //R_SetTrans (S_BLOOD1, 0, tr_transmed);
    //R_SetTrans (S_BLOOD2, S_BLOOD3, tr_transmor);

    //mobjinfo[MT_MISC38].flags |= MF_SHADOW;          //eye in symbol
    R_SetTrans (S_EVILEYE, S_EVILEYE4, tr_transmed);

    //mobjinfo[MT_FATSHOT].flags |= MF_SHADOW;         //mancubus fireball
    R_SetTrans (S_FATSHOT1, S_FATSHOT2, tr_transfir);
     R_SetTrans (S_FATSHOTX1, S_FATSHOTX3, tr_transmed);

     // hi translucency for spectres..
     //R_SetTrans (S_SARG_STND, S_SARG_RAISE6, tr_transhi);

     // rockets explosion
     R_SetTrans (S_EXPLODE1, S_EXPLODE2, tr_transfir);
     R_SetTrans (S_EXPLODE3, 0, tr_transmed);

    //Fab: lava/slime damage smoke test
    R_SetTrans (S_SMOK1, S_SMOK5, tr_transmed/*(FF_SMOKESHADE>>FF_TRANSSHIFT)*/);
    R_SetTrans (S_SPLASH1, S_SPLASH3, tr_transmor);

    R_SetTrans (S_THOK1, 0, tr_transmed); // Thok! mobj Tails 12-05-99

// if higher translucency needed, toy around with the other tr_trans variables

    R_SetTrans (S_BORB1, S_YORB1, tr_transmed); // Tails shield translucencies 12-30-99

    R_SetTrans (S_GORB1, S_KORB1, tr_transmed); // Tails shield translucencies 12-30-99

    R_SetTrans (S_SPRK1, S_SPRK2, tr_transfir); // start trans spark tails
    R_SetTrans (S_SPRK3, S_SPRK4, tr_transfir);
    R_SetTrans (S_SPRK5, S_SPRK6, tr_transmed);
    R_SetTrans (S_SPRK7, S_SPRK8, tr_transmed);
    R_SetTrans (S_SPRK9, S_SPRK10, tr_transmor);
    R_SetTrans (S_SPRK11, S_SPRK12, tr_transmor);
    R_SetTrans (S_SPRK13, S_SPRK14, tr_transhi);
    R_SetTrans (S_SPRK15, S_SPRK16, tr_transhi); // end trans spark tails

    R_SetTrans (S_IVSP1, S_IVSP2, tr_transmed); // start invincibility spark tails
    R_SetTrans (S_IVSP3, S_IVSP4, tr_transmed);
    R_SetTrans (S_IVSP5, S_IVSP6, tr_transmed);
    R_SetTrans (S_IVSP7, S_IVSP8, tr_transmed);
    R_SetTrans (S_IVSP9, S_IVSP10, tr_transmed);
    R_SetTrans (S_IVSP11, S_IVSP12, tr_transmed);
    R_SetTrans (S_IVSP13, S_IVSP14, tr_transmed);
    R_SetTrans (S_IVSP15, S_IVSP16, tr_transmed);
    R_SetTrans (S_IVSP17, S_IVSP18, tr_transmed);
    R_SetTrans (S_IVSP19, S_IVSP20, tr_transmed);
    R_SetTrans (S_IVSP21, S_IVSP22, tr_transmed);
    R_SetTrans (S_IVSP23, S_IVSP24, tr_transmed);
    R_SetTrans (S_IVSP25, S_IVSP26, tr_transmed);
    R_SetTrans (S_IVSP27, S_IVSP28, tr_transmed);
    R_SetTrans (S_IVSP29, 0, tr_transmed);
    R_SetTrans (S_IVSQ1, S_IVSQ2, tr_transmed);
    R_SetTrans (S_IVSQ3, 0, tr_transmed); // end invincibility spark tails

}


// =======================================================================
//                    FUNKY DEATHMATCH COMMANDS
// =======================================================================

void BloodTime_OnChange (void);

// how much tics to last for the last (third) frame of blood (S_BLOODx)
consvar_t cv_bloodtime = {"bloodtime","1",CV_NETVAR|CV_CALL|CV_NOINIT,CV_Unsigned,BloodTime_OnChange};

// Called when var. 'bloodtime' is changed : set the blood states duration
//
void BloodTime_OnChange (void)
{
    int t;

    t = cv_bloodtime.value;     // seconds

    if (t<1)
    {
        t = 1;
        CV_SetValue (&cv_bloodtime,t);
        return;
    }

    states[S_BLOOD1].tics = 8;
    states[S_BLOOD2].tics = 8;
    states[S_BLOOD3].tics = (t*TICRATE) - 16;

    CONS_Printf ("blood lasts for %d seconds\n", t);
}


void D_AddDeathmatchCommands (void)
{
    CV_RegisterVar (&cv_solidcorpse);                 //p_enemy

    CV_RegisterVar (&cv_bloodtime);

}
