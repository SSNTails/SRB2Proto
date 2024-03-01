
// r_things.h :     Rendering of moving objects, sprites.
//

#ifndef __R_THINGS__
#define __R_THINGS__

#include "sounds.h"

// number of sprite lumps for spritewidth,offset,topoffset lookup tables
// Fab: this is a hack : should allocate the lookup tables per sprite
#define     MAXSPRITELUMPS     4096

#define MAXVISSPRITES   256 // added 2-2-98 was 128

extern vissprite_t      vissprites[MAXVISSPRITES];
extern vissprite_t*     vissprite_p;
extern vissprite_t      vsprsortedhead;

// Constant arrays used for psprite clipping
//  and initializing clipping.
extern short            negonearray[MAXVIDWIDTH];
extern short            screenheightarray[MAXVIDWIDTH];

// vars for R_DrawMaskedColumn
extern short*           mfloorclip;
extern short*           mceilingclip;
extern fixed_t          spryscale;
extern fixed_t          sprtopscreen;

extern fixed_t          pspritescale;
extern fixed_t          pspriteiscale;
extern fixed_t          pspriteyscale;  //added:02-02-98:for aspect ratio


void R_DrawMaskedColumn (column_t* column);

void R_SortVisSprites (void);

//faB: find sprites in wadfile, replace existing, add new ones
//     (only sprites from namelist are added or replaced)
void R_AddSpriteDefs (char** namelist, int wadnum);

void R_AddSprites (sector_t* sec);
void R_AddPSprites (void);
void R_DrawSprite (vissprite_t* spr);
void R_InitSprites (char** namelist);
void R_ClearSprites (void);
void R_DrawSprites (void);  //draw all vissprites
void R_DrawMasked (void);

void
R_ClipVisSprite
( vissprite_t*          vis,
  int                   xl,
  int                   xh );


// -----------
// SKINS STUFF
// -----------
#define SKINNAMESIZE 16
#define DEFAULTSKIN  "sonic"   // Changed by Tails: 9-13-99

typedef struct
{
    char        name[SKINNAMESIZE+1];   // short descriptive name of the skin
    spritedef_t spritedef;
    char        faceprefix[4];          // 3 chars+'\0', default is "STF"
    char        typechar[1]; // character type identification Tails 03-01-2000

    // specific sounds per skin
    short       soundsid[NUMSKINSOUNDS]; // sound # in S_sfx table

} skin_t;

extern int       numskins;
extern skin_t    skins[MAXSKINS];
//extern CV_PossibleValue_t skin_cons_t[MAXSKINS+1];
extern consvar_t cv_skin;

//void    R_InitSkins (void);
void    SetPlayerSkin(int playernum,char *skinname);
int     R_SkinAvailable (char* name);
void    R_AddSkins (int wadnum);

#endif __R_THINGS__
