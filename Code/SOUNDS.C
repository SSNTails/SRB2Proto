// sounds.c : music/sound tables, and related sound routines
//
// Note: the tables were originally created by a sound utility at Id,
//       kept as a sample, DOOM2 sounds.

#include "doomtype.h"
#include "sounds.h"
#include "r_defs.h"
#include "r_things.h"
#include "z_zone.h"

// NOTE: add \0 for stringlen=6, to allow dehacked patching

//
// Information about all the music
//

musicinfo_t S_music[NUMMUSIC] =
{
    { 0 },
    { "e1m1\0\0", 0 },
    { "e1m2\0\0", 0 },
    { "e1m3\0\0", 0 },
    { "e1m4\0\0", 0 },
    { "e1m5\0\0", 0 },
    { "e1m6\0\0", 0 },
    { "e1m7\0\0", 0 },
    { "e1m8\0\0", 0 },
    { "e1m9\0\0", 0 },
    { "e2m1\0\0", 0 },
    { "e2m2\0\0", 0 },
    { "e2m3\0\0", 0 },
    { "e2m4\0\0", 0 },
    { "e2m5\0\0", 0 },
    { "e2m6\0\0", 0 },
    { "e2m7\0\0", 0 },
    { "e2m8\0\0", 0 },
    { "e2m9\0\0", 0 },
    { "e3m1\0\0", 0 },
    { "e3m2\0\0", 0 },
    { "e3m3\0\0", 0 },
    { "e3m4\0\0", 0 },
    { "e3m5\0\0", 0 },
    { "e3m6\0\0", 0 },
    { "e3m7\0\0", 0 },
    { "e3m8\0\0", 0 },
    { "e3m9\0\0", 0 },
    { "inter\0" , 0 },
    { "intro\0" , 0 },
    { "bunny\0" , 0 },
    { "victor"  , 0 },
    { "introa"  , 0 },
    { "runnin"  , 0 },
    { "stalks"  , 0 },
    { "countd"  , 0 },
    { "betwee"  , 0 },
    { "doom\0\0", 0 },
    { "the_da"  , 0 },
    { "shawn\0" , 0 },
    { "ddtblu"  , 0 },
    { "in_cit"  , 0 },
    { "dead\0\0", 0 },
    { "stlks2"  , 0 },
    { "theda2"  , 0 },
    { "doom2\0" , 0 },
    { "ddtbl2"  , 0 },
    { "runni2"  , 0 },
    { "dead2\0" , 0 },
    { "stlks3"  , 0 },
    { "romero"  , 0 },
    { "shawn2"  , 0 },
    { "messag"  , 0 },
    { "count2"  , 0 },
    { "ddtbl3"  , 0 },
    { "ampie\0" , 0 },
    { "theda3"  , 0 },
    { "adrian"  , 0 },
    { "messg2"  , 0 },
    { "romer2"  , 0 },
    { "tense\0" , 0 },
    { "shawn3"  , 0 },
    { "openin"  , 0 },
    { "evil\0\0", 0 },
    { "ultima"  , 0 },
    { "read_m"  , 0 },
    { "dm2ttl"  , 0 },
    { "dm2int"  , 0 },
    { "invinc"  , 0 },
};


//
// Information about all the sfx
//

sfxinfo_t S_sfx[NUMSFX] =
{
  // S_sfx[0] needs to be a dummy for odd reasons.
//         singularity(U)        pitch      skinsound   (U) for UNUSED
//               |      priority(U) volume  |
//               |        |  link   |   data|
  { "none"     , false,   0, 0, -1, -1, 0, -1},

  { "pistol"   , false,  64, 0, -1, -1, 0, -1},
  { "shotgn"   , false,  64, 0, -1, -1, 0, -1},
  { "sgcock"   , false,  64, 0, -1, -1, 0, -1},
  { "dshtgn"   , false,  64, 0, -1, -1, 0, -1},
  { "dbopn\0"  , false,  64, 0, -1, -1, 0, -1},
  { "dbcls\0"  , false,  64, 0, -1, -1, 0, -1},
  { "dbload"   , false,  64, 0, -1, -1, 0, -1},
  { "plasma"   , false,  64, 0, -1, -1, 0, -1},
  { "bfg\0\0\0", false,  64, 0, -1, -1, 0, -1},
  { "sawup\0"  , false,  64, 0, -1, -1, 0, -1},
  { "sawidl"   , false, 118, 0, -1, -1, 0, -1},
  { "sawful"   , false,  64, 0, -1, -1, 0, -1},
  { "sawhit"   , false,  64, 0, -1, -1, 0, -1},
  { "rlaunc"   , false,  64, 0, -1, -1, 0, -1},
  { "rxplod"   , false,  70, 0, -1, -1, 0, -1},
  { "firsht"   , false,  70, 0, -1, -1, 0, -1},
  { "firxpl"   , false,  70, 0, -1, -1, 0, -1},
  { "pstart"   , false, 100, 0, -1, -1, 0, -1},
  { "pstop\0"  , false, 100, 0, -1, -1, 0, -1},
  { "doropn"   , false, 100, 0, -1, -1, 0, -1},
  { "dorcls"   , false, 100, 0, -1, -1, 0, -1},
  { "stnmov"   , false, 119, 0, -1, -1, 0, -1},
  { "swtchn"   , false,  78, 0, -1, -1, 0, -1},
  { "swtchx"   , false,  78, 0, -1, -1, 0, -1},
  { "plpain"   , false,  96, 0, -1, -1, 0, SKSPLPAIN},
  { "dmpain"   , false,  96, 0, -1, -1, 0, -1},
  { "popain"   , false,  96, 0, -1, -1, 0, -1},
  { "vipain"   , false,  96, 0, -1, -1, 0, -1},
  { "mnpain"   , false,  96, 0, -1, -1, 0, -1},
  { "pepain"   , false,  96, 0, -1, -1, 0, -1},
  { "slop\0\0" , false,  78, 0, -1, -1, 0, SKSSLOP},
  { "itemup"   ,  true,  78, 0, -1, -1, 0, -1},
  { "wpnup"    ,  true,  78, 0, -1, -1, 0, -1},
  { "oof\0\0\0", false,  96, 0, -1, -1, 0, SKSOOF},
  { "telept"   , false,  32, 0, -1, -1, 0, -1},
  { "posit1"   ,  true,  98, 0, -1, -1, 0, -1},
  { "posit2"   ,  true,  98, 0, -1, -1, 0, -1},
  { "posit3"   ,  true,  98, 0, -1, -1, 0, -1},
  { "bgsit1"   ,  true,  98, 0, -1, -1, 0, -1},
  { "bgsit2"   ,  true,  98, 0, -1, -1, 0, -1},
  { "sgtsit"   ,  true,  98, 0, -1, -1, 0, -1},
  { "cacsit"   ,  true,  98, 0, -1, -1, 0, -1},
  { "brssit"   ,  true,  94, 0, -1, -1, 0, -1},
  { "cybsit"   ,  true,  92, 0, -1, -1, 0, -1},
  { "spisit"   ,  true,  90, 0, -1, -1, 0, -1},
  { "bspsit"   ,  true,  90, 0, -1, -1, 0, -1},
  { "kntsit"   ,  true,  90, 0, -1, -1, 0, -1},
  { "vilsit"   ,  true,  90, 0, -1, -1, 0, -1},
  { "mansit"   ,  true,  90, 0, -1, -1, 0, -1},
  { "pesit\0"  ,  true,  90, 0, -1, -1, 0, -1},
  { "sklatk"   , false,  70, 0, -1, -1, 0, -1},
  { "sgtatk"   , false,  70, 0, -1, -1, 0, -1},
  { "skepch"   , false,  70, 0, -1, -1, 0, -1},
  { "vilatk"   , false,  70, 0, -1, -1, 0, -1},
  { "claw\0\0" , false,  70, 0, -1, -1, 0, -1},
  { "skeswg"   , false,  70, 0, -1, -1, 0, -1},
  { "pldeth"   , false,  32, 0, -1, -1, 0, SKSPLDETH},
  { "pdiehi"   , false,  32, 0, -1, -1, 0, SKSPDIEHI},
  { "podth1"   , false,  70, 0, -1, -1, 0, -1},
  { "podth2"   , false,  70, 0, -1, -1, 0, -1},
  { "podth3"   , false,  70, 0, -1, -1, 0, -1},
  { "bgdth1"   , false,  70, 0, -1, -1, 0, -1},
  { "bgdth2"   , false,  70, 0, -1, -1, 0, -1},
  { "sgtdth"   , false,  70, 0, -1, -1, 0, -1},
  { "cacdth"   , false,  70, 0, -1, -1, 0, -1},
  { "skldth"   , false,  70, 0, -1, -1, 0, -1},
  { "brsdth"   , false,  32, 0, -1, -1, 0, -1},
  { "cybdth"   , false,  32, 0, -1, -1, 0, -1},
  { "spidth"   , false,  32, 0, -1, -1, 0, -1},
  { "bspdth"   , false,  32, 0, -1, -1, 0, -1},
  { "vildth"   , false,  32, 0, -1, -1, 0, -1},
  { "kntdth"   , false,  32, 0, -1, -1, 0, -1},
  { "pedth\0"  , false,  32, 0, -1, -1, 0, -1},
  { "skedth"   , false,  32, 0, -1, -1, 0, -1},
  { "posact"   ,  true, 120, 0, -1, -1, 0, -1},
  { "bgact\0"  ,  true, 120, 0, -1, -1, 0, -1},
  { "dmact\0"  ,  true, 120, 0, -1, -1, 0, -1},
  { "bspact"   ,  true, 100, 0, -1, -1, 0, -1},
  { "bspwlk"   ,  true, 100, 0, -1, -1, 0, -1},
  { "vilact"   ,  true, 100, 0, -1, -1, 0, -1},
  { "noway\0"  , false,  78, 0, -1, -1, 0, SKSNOWAY},
  { "barexp"   , false,  60, 0, -1, -1, 0, -1},
  { "punch\0"  , false,  64, 0, -1, -1, 0, SKSPUNCH},
  { "hoof\0\0" , false,  70, 0, -1, -1, 0, -1},
  { "metal\0"  , false,  70, 0, -1, -1, 0, -1},
  { "chgun\0"  , false,  64, &S_sfx[sfx_pistol], 150, 0, 0, -1},
  { "tink\0\0" , false,  60, 0, -1, -1, 0, -1},
  { "bdopn\0"  , false, 100, 0, -1, -1, 0, -1},
  { "bdcls\0"  , false, 100, 0, -1, -1, 0, -1},
  { "itmbk\0"  , false, 100, 0, -1, -1, 0, -1},
  { "flame\0"  , false,  32, 0, -1, -1, 0, -1},
  { "flamst"   , false,  32, 0, -1, -1, 0, -1},
  { "getpow"   , false,  60, 0, -1, -1, 0, -1},
  { "bospit"   , false,  70, 0, -1, -1, 0, -1},
  { "boscub"   , false,  70, 0, -1, -1, 0, -1},
  { "bossit"   , false,  70, 0, -1, -1, 0, -1},
  { "bospn\0"  , false,  70, 0, -1, -1, 0, -1},
  { "bosdth"   , false,  70, 0, -1, -1, 0, -1},
  { "manatk"   , false,  70, 0, -1, -1, 0, -1},
  { "mandth"   , false,  70, 0, -1, -1, 0, -1},
  { "sssit\0"  , false,  70, 0, -1, -1, 0, -1},
  { "ssdth\0"  , false,  70, 0, -1, -1, 0, -1},
  { "keenpn"   , false,  70, 0, -1, -1, 0, -1},
  { "keendt"   , false,  70, 0, -1, -1, 0, -1},
  { "skeact"   , false,  70, 0, -1, -1, 0, -1},
  { "skesit"   , false,  70, 0, -1, -1, 0, -1},
  { "skeatk"   , false,  70, 0, -1, -1, 0, -1},
  { "radio\0"  , false,  60, 0, -1, -1, 0, SKSRADIO},

  //added:22-02-98: sound when the player avatar jumps in air 'hmpf!'
  { "jump\0\0" , false,  60, 0, -1, -1, 0, SKSJUMP},
  { "ouch\0\0" , false,  64, 0, -1, -1, 0, SKSOUCH},

  //added:09-08-98:test water sounds
  { "gloop\0"  , false,  60, 0, -1, -1, 0, -1},
  { "splash"   , false,  64, 0, -1, -1, 0, -1},
  { "floush"   , false,  64, 0, -1, -1, 0, -1}
  // skin sounds free slots to add sounds at run time (Boris HACK!!!)
  // initialized to NULL
};


// Prepare free sfx slots to add sfx at run time
void S_InitRuntimeSounds (void)
{
    int  i;

    for (i=sfx_freeslot0; i<=sfx_lastfreeslot; i++)
        S_sfx[i].name = NULL;
}

// Add a new sound fx into a free sfx slot.
//
int S_AddSoundFx (char *name,int singularity)
{
    int i;

    for(i=sfx_freeslot0;i<NUMSFX;i++)
    {
        if(!S_sfx[i].name)
        {
            S_sfx[i].name=(char *)Z_Malloc(7,PU_STATIC,NULL);
            strncpy(S_sfx[i].name,name,6);
            S_sfx[i].name[6]='\0';
            S_sfx[i].singularity=singularity;
            S_sfx[i].priority=60;
            S_sfx[i].link=0;
            S_sfx[i].pitch=-1;
            S_sfx[i].volume=-1;
            S_sfx[i].lumpnum=-1;
            S_sfx[i].skinsound=-1;
            S_sfx[i].usefulness=-1;

            // if precache load it here ! todo !
            S_sfx[i].data=0;
            return i;
        }
    }
    CONS_Printf("\2No more free sound slots\n");
    return 0;
}

void S_RemoveSoundFx (int id)
{
    if (id>=sfx_freeslot0 &&
        id<=sfx_lastfreeslot &&
        S_sfx[id].name)
    {
        Z_ChangeTag (S_sfx[id].name,PU_CACHE);
        S_sfx[id].name=NULL;
        S_sfx[id].lumpnum=-1;
        if(S_sfx[id].data)
        {
             Z_ChangeTag(S_sfx[id].data, PU_CACHE);
             S_sfx[id].data = 0;
        }
    }
}
