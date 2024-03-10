// r_things.c : Refresh of things, i.e. objects represented by sprites.

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "r_local.h"
#include "sounds.h"             //skin sounds
#include "st_stuff.h"
#include "w_wad.h"
#include "z_zone.h"

#include "i_video.h"            //rendermode

static void R_InitSkins (void);

#define MINZ                  (FRACUNIT*4)
#define BASEYCENTER           (BASEVIDHEIGHT/2)


typedef struct
{
    int         x1;
    int         x2;

    int         column;
    int         topclip;
    int         bottomclip;

} maskdraw_t;



//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
fixed_t         pspritescale;
fixed_t         pspriteyscale;  //added:02-02-98:aspect ratio for psprites
fixed_t         pspriteiscale;

lighttable_t**  spritelights;

// constant arrays
//  used for psprite clipping and initializing clipping
short           negonearray[MAXVIDWIDTH];
short           screenheightarray[MAXVIDWIDTH];


//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//  and range check thing_t sprites patches
spritedef_t*    sprites;
int             numsprites;

spriteframe_t   sprtemp[29];
int             maxframe;
char*           spritename;



// ==========================================================================
//
//  New sprite loading routines for Legacy : support sprites in pwad,
//  dehacked sprite renaming, replacing not all frames of an existing
//  sprite, add sprites at run-time, add wads at run-time.
//
// ==========================================================================

//
//
//
void R_InstallSpriteLump ( int           lumppat,     // graphics patch
                           int           lumpid,      // identifier
                           unsigned      frame,
                           unsigned      rotation,
                           boolean       flipped )
{
    int         r;

    if (frame >= 29 || rotation > 8)
        I_Error("R_InstallSpriteLump: "
                "Bad frame characters in lump %i", lumpid);

    if ((int)frame > maxframe)
        maxframe = frame;

    if (rotation == 0)
    {
        // the lump should be used for all rotations
        if (sprtemp[frame].rotate == 0 && devparm)
            CONS_Printf ("R_InitSprites: Sprite %s frame %c has "
                        "multiple rot=0 lump", spritename, 'A'+frame);

        if (sprtemp[frame].rotate == 1 && devparm)
            CONS_Printf ("R_InitSprites: Sprite %s frame %c has rotations "
                        "and a rot=0 lump", spritename, 'A'+frame);

        sprtemp[frame].rotate = 0;
        for (r=0 ; r<8 ; r++)
        {
            sprtemp[frame].lumppat[r] = lumppat;
            sprtemp[frame].lumpid[r]  = lumpid;
            sprtemp[frame].flip[r] = (byte)flipped;
        }
        return;
    }

    // the lump is only used for one rotation
    if (sprtemp[frame].rotate == 0 && devparm)
        CONS_Printf ("R_InitSprites: Sprite %s frame %c has rotations "
                     "and a rot=0 lump", spritename, 'A'+frame);

    sprtemp[frame].rotate = 1;

    // make 0 based
    rotation--;

    if (sprtemp[frame].lumpid[rotation] != -1 && devparm)
        CONS_Printf ("R_InitSprites: Sprite %s : %c : %c "
                     "has two lumps mapped to it",
                     spritename, 'A'+frame, '1'+rotation);

    // lumppat & lumpid are the same for original Doom, but different
    // when using sprites in pwad : the lumppat points the new graphics
    sprtemp[frame].lumppat[rotation] = lumppat;
    sprtemp[frame].lumpid[rotation] = lumpid;
    sprtemp[frame].flip[rotation] = (byte)flipped;
}


// Install a single sprite, given its identifying name (4 chars)
//
// (originally part of R_AddSpriteDefs)
//
// Pass: name of sprite : 4 chars
//       spritedef_t
//       wadnum         : wad number, indexes wadfiles[], where patches
//                        for frames are found
//       startlump      : first lump to search for sprite frames
//       endlump        : AFTER the last lump to search
//
// Returns true if the sprite was succesfully added
//
boolean R_AddSingleSpriteDef (char* sprname, spritedef_t* spritedef, int wadnum, int startlump, int endlump)
{
    int         l;
    int         intname;
    int         frame;
    int         rotation;
    lumpinfo_t* lumpinfo;
    patch_t     patch;

    intname = *(int *)sprname;

    memset (sprtemp,-1, sizeof(sprtemp));
    maxframe = -1;

    // are we 'patching' a sprite already loaded ?
    // if so, it might patch only certain frames, not all
    if (spritedef->numframes) // (then spriteframes is not null)
    {
        // copy the already defined sprite frames
        memcpy (sprtemp, spritedef->spriteframes,
                spritedef->numframes * sizeof(spriteframe_t));
        maxframe = spritedef->numframes - 1;
    }

    // scan the lumps,
    //  filling in the frames for whatever is found
    lumpinfo = wadfiles[wadnum]->lumpinfo;

    for (l=startlump ; l<endlump ; l++)
    {
        if (*(int *)lumpinfo[l].name == intname)
        {
            frame = lumpinfo[l].name[4] - 'A';
            rotation = lumpinfo[l].name[5] - '0';

            // skip NULL sprites from very old dmadds pwads
            if (W_LumpLength( (wadnum<<16)+l )<=8)
                continue;

            // store sprite info in lookup tables
            //FIXME:numspritelumps do not duplicate sprite replacements
            W_ReadLumpHeader ((wadnum<<16)+l, &patch, sizeof(patch_t));
            spritewidth[numspritelumps] = SHORT(patch.width)<<FRACBITS;
            spriteoffset[numspritelumps] = SHORT(patch.leftoffset)<<FRACBITS;
            spritetopoffset[numspritelumps] = SHORT(patch.topoffset)<<FRACBITS;
            //----------------------------------------------------

            R_InstallSpriteLump ((wadnum<<16)+l, numspritelumps, frame, rotation, false);

            if (lumpinfo[l].name[6])
            {
                frame = lumpinfo[l].name[6] - 'A';
                rotation = lumpinfo[l].name[7] - '0';
                R_InstallSpriteLump ((wadnum<<16)+l, numspritelumps, frame, rotation, true);
            }

            if (++numspritelumps>=MAXSPRITELUMPS)
                I_Error("R_AddSingleSpriteDef: too much sprite replacements (numspritelumps)\n");
        }
    }

    //
    // if no frames found for this sprite
    //
    if (maxframe == -1)
    {
        // the first time (which is for the original wad),
        // all sprites should have their initial frames
        // and then, patch wads can replace it
        // we will skip non-replaced sprite frames, only if
        // they have already have been initially defined (original wad)

        //check only after all initial pwads added
        //if (spritedef->numframes == 0)
        //    I_Error ("R_AddSpriteDefs: no initial frames found for sprite %s\n",
        //             namelist[i]);

        // sprite already has frames, and is not replaced by this wad
        return false;
    }

    maxframe++;

    //
    //  some checks to help development
    //
    for (frame = 0 ; frame < maxframe ; frame++)
    {
        switch ((int)sprtemp[frame].rotate)
        {
          case -1:
            // no rotations were found for that frame at all
            I_Error ("R_InitSprites: No patches found "
                     "for %s frame %c", sprname, frame+'A');
            break;

          case 0:
            // only the first rotation is needed
            break;

          case 1:
            // must have all 8 frames
            for (rotation=0 ; rotation<8 ; rotation++)
                // we test the patch lump, or the id lump whatever
                // if it was not loaded the two are -1
                if (sprtemp[frame].lumppat[rotation] == -1)
                    I_Error ("R_InitSprites: Sprite %s frame %c "
                             "is missing rotations",
                             sprname, frame+'A');
            break;
        }
    }

    // allocate space for the frames present and copy sprtemp to it
    if (spritedef->numframes &&             // has been allocated
        spritedef->numframes < maxframe)   // more frames are defined ?
    {
        Z_Free (spritedef->spriteframes);
        spritedef->spriteframes = NULL;
    }

    // allocate this sprite's frames
    if (spritedef->spriteframes == NULL)
        spritedef->spriteframes =
            Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);

    spritedef->numframes = maxframe;
    memcpy (spritedef->spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));

    return true;
}



//
// Search for sprite replacements, whose names are in namelist
//
void R_AddSpriteDefs (char** namelist, int wadnum)
{
    int         i;

    int         start;
    int         end;

    int         addsprites;

    // find the sprites section in this pwad
    // we need at least the S_END
    // (not really, but for speedup)

    start = W_CheckNumForNamePwad ("S_START",wadnum,0);
    if (start==-1)
        start = W_CheckNumForNamePwad ("SS_START",wadnum,0); //deutex compatib.
    if (start==-1)
        start=0;      // search frames from start of wad
                              // (lumpnum low word is 0)
    else
        start++;   // just after S_START

    start &= 0xFFFF;    // 0 based in lumpinfo

    end = W_CheckNumForNamePwad ("S_END",wadnum,0);
    if (end==-1)
        end = W_CheckNumForNamePwad ("SS_END",wadnum,0);     //deutex compatib.
    if (end==-1)
    {
        if (devparm)
            CONS_Printf ("no sprites in pwad %d\n", wadnum);
        return;
        //I_Error ("R_AddSpriteDefs: S_END, or SS_END missing for sprites "
        //         "in pwad %d\n",wadnum);
    }
    end &= 0xFFFF;

    //
    // scan through lumps, for each sprite, find all the sprite frames
    //
    addsprites = 0;
    for (i=0 ; i<numsprites ; i++)
    {
        spritename = namelist[i];

        if (R_AddSingleSpriteDef (spritename, &sprites[i], wadnum, start, end) )
        {
            // if a new sprite was added (not just replaced)
            addsprites++;
            if (devparm)
                CONS_Printf ("sprite %s set in pwad %d\n", namelist[i], wadnum);//Fab
        }
    }

    CONS_Printf ("%d sprites added from file %s\n", addsprites, wadfiles[wadnum]->filename);//Fab
    //CONS_Error ("press enter\n");
}



//
// GAME FUNCTIONS
//
vissprite_t     vissprites[MAXVISSPRITES];
vissprite_t*    vissprite_p;
int             newvissprite;



//
// R_InitSprites
// Called at program start.
//
void R_InitSprites (char** namelist)
{
    int         i;
    char**      check;

    for (i=0 ; i<MAXVIDWIDTH ; i++)
    {
        negonearray[i] = -1;
    }

    //
    // count the number of sprite names, and allocate sprites table
    //
    check = namelist;
    while (*check != NULL)
        check++;
    numsprites = NUMSPRITES;// check - namelist;
    // SSNTails
    // TODO: Setting this to NUMSPRITES is a hack... why?

    if (!numsprites)
        I_Error ("R_AddSpriteDefs: no sprites in namelist\n");

    sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_STATIC, NULL);
    memset (sprites, 0, numsprites * sizeof(*sprites));

    // find sprites in each -file added pwad
    for (i=0; i<numwadfiles; i++)
        R_AddSpriteDefs (namelist, i);

    //
    // now check for skins
    //

    // it can be is do before loading config for skin cvar possible value
    R_InitSkins ();
    for (i=0; i<numwadfiles; i++)
        R_AddSkins (i);


    //
    // check if all sprites have frames
    //
    /*
    for (i=0; i<numsprites; i++)
         if (sprites[i].numframes<1)
             CONS_Printf ("R_InitSprites: sprite %s has no frames at all\n", sprnames[i]);
    */
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
    vissprite_p = vissprites;
}


//
// R_NewVisSprite
//
vissprite_t     overflowsprite;

vissprite_t* R_NewVisSprite (void)
{
    if (vissprite_p == &vissprites[MAXVISSPRITES])
        return &overflowsprite;

    vissprite_p++;
    return vissprite_p-1;
}



//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
short*          mfloorclip;
short*          mceilingclip;

fixed_t         spryscale;
fixed_t         sprtopscreen;

void R_DrawMaskedColumn (column_t* column)
{
    int         topscreen;
    int         bottomscreen;
    fixed_t     basetexturemid;

    basetexturemid = dc_texturemid;

    for ( ; column->topdelta != 0xff ; )
    {
        // calculate unclipped screen coordinates
        //  for post
        topscreen = sprtopscreen + spryscale*column->topdelta;
        bottomscreen = topscreen + spryscale*column->length;

        dc_yl = (topscreen+FRACUNIT-1)>>FRACBITS;
        dc_yh = (bottomscreen-1)>>FRACBITS;

        if (dc_yh >= mfloorclip[dc_x])
            dc_yh = mfloorclip[dc_x]-1;
        if (dc_yl <= mceilingclip[dc_x])
            dc_yl = mceilingclip[dc_x]+1;

        if (dc_yl <= dc_yh)
        {
            dc_source = (byte *)column + 3;
            dc_texturemid = basetexturemid - (column->topdelta<<FRACBITS);
            // dc_source = (byte *)column + 3 - column->topdelta;

            // Drawn by either R_DrawColumn
            //  or (SHADOW) R_DrawFuzzColumn.
            colfunc ();
        }
        column = (column_t *)(  (byte *)column + column->length + 4);
    }

    dc_texturemid = basetexturemid;
}



//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite ( vissprite_t*          vis,
                       int                   x1,
                       int                   x2 )
{
    column_t*           column;
    int                 texturecolumn;
    fixed_t             frac;
    patch_t*            patch;


    //Fab:R_InitSprites now sets a wad lump number
    patch = W_CacheLumpNum (vis->patch/*+firstspritelump*/, PU_CACHE);

    dc_colormap = vis->colormap;
    if (!dc_colormap)
    {
        if (vis->transmap)
        {
            // NULL colormap = draw using translucency (ex-fuzzy draw)
            colfunc = fuzzcolfunc;
            dc_transmap = vis->transmap;    //Fab:29-04-98: translucency table
        }
        else
            // shadecolfunc uses 'colormaps'
            colfunc = shadecolfunc;
    }
    else if (vis->mobjflags & MF_TRANSLATION)
    {
        colfunc = transcolfunc;
        dc_translation = translationtables - 256 +
            ( (vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
    }

    //dc_iscale = abs(vis->xiscale)>>detailshift;  ???
    dc_iscale = FixedDiv (FRACUNIT, vis->scale);
    dc_texturemid = vis->texturemid;

    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);

    for (dc_x=vis->x1 ; dc_x<=vis->x2 ; dc_x++, frac += vis->xiscale)
    {
        texturecolumn = frac>>FRACBITS;
#ifdef RANGECHECK
        if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
            I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif
        column = (column_t *) ((byte *)patch +
                               LONG(patch->columnofs[texturecolumn]));
        R_DrawMaskedColumn (column);
    }

    colfunc = basecolfunc;
}


//
// R_ProjectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
void R_ProjectSprite (mobj_t* thing)
{
    fixed_t             tr_x;
    fixed_t             tr_y;

    fixed_t             gxt;
    fixed_t             gyt;

    fixed_t             tx;
    fixed_t             tz;

    fixed_t             xscale;
    fixed_t             yscale; //added:02-02-98:aaargll..if I were a math-guy!!!

    int                 x1;
    int                 x2;

    spritedef_t*        sprdef;
    spriteframe_t*      sprframe;
    int                 lump;

    unsigned            rot;
    boolean             flip;

    int                 index;

    vissprite_t*        vis;

    angle_t             ang;
    fixed_t             iscale;

    // transform the origin point
    tr_x = thing->x - viewx;
    tr_y = thing->y - viewy;

    gxt = FixedMul(tr_x,viewcos);
    gyt = -FixedMul(tr_y,viewsin);

    tz = gxt-gyt;

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    // aspect ratio stuff :
    xscale = FixedDiv(projection, tz);
    yscale = FixedDiv(projectiony, tz);

    gxt = -FixedMul(tr_x,viewsin);
    gyt = FixedMul(tr_y,viewcos);
    tx = -(gyt+gxt);

    // too far off the side?
    if (abs(tx)>(tz<<2))
        return;

    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if ((unsigned)thing->sprite >= numsprites)
        I_Error ("R_ProjectSprite: invalid sprite number %i ",
                 thing->sprite);
#endif

    //Fab:02-08-98: 'skin' override spritedef currently used for skin
    if (thing->skin)
        sprdef = &((skin_t *)thing->skin)->spritedef;
    else
        sprdef = &sprites[thing->sprite];

#ifdef RANGECHECK
    if ( (thing->frame&FF_FRAMEMASK) >= sprdef->numframes )
        I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
                 thing->sprite, thing->frame);
#endif
    sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        ang = R_PointToAngle (thing->x, thing->y);
        rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
        //Fab: lumpid is the index for spritewidth,spriteoffset... tables
        lump = sprframe->lumpid[rot];
        flip = (boolean)sprframe->flip[rot];
    }
    else
    {
        // use single rotation for all views
        rot = 0;                        //Fab: for vis->patch below
        lump = sprframe->lumpid[0];     //Fab: see note above
        flip = (boolean)sprframe->flip[0];
    }

    // calculate edges of the shape
    tx -= spriteoffset[lump];
    x1 = (centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS;

    // off the right side?
    if (x1 > viewwidth)
        return;

    tx +=  spritewidth[lump];
    x2 = ((centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = R_NewVisSprite ();
    vis->mobjflags = thing->flags;
    vis->scale = yscale;           //<<detailshift;
    vis->gx = thing->x;
    vis->gy = thing->y;
    vis->gz = thing->z;
    vis->gzt = thing->z + spritetopoffset[lump];
    vis->texturemid = vis->gzt - viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
    iscale = FixedDiv (FRACUNIT, xscale);

    if (flip)
    {
        vis->startfrac = spritewidth[lump]-1;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale*(vis->x1-x1);

    //Fab: lumppat is the lump number of the patch to use, this is different
    //     than lumpid for sprites-in-pwad : the graphics are patched
    vis->patch = sprframe->lumppat[rot];


//
// determine the colormap (lightlevel & special effects)
//
    // specific translucency
    if (thing->frame & (FF_TRANSMASK|FF_SMOKESHADE))
    {
        vis->colormap = NULL;

        if (thing->frame & FF_SMOKESHADE)
            // faB: BIG HACK!!! I'm lazy.. colormap NULL & transmap NULL
            //       means use SHADE DRAWER. see R_DrawVisSprite
            vis->transmap = NULL;
        else
            vis->transmap = (thing->frame & FF_TRANSMASK) - 0x10000 + transtables;
    }
    else if (thing->flags & MF_SHADOW)
    {
        // actually only the player should use this (temporary invisibility)
        // because now the translucency is set through FF_TRANSMASK
        vis->colormap = NULL;
        vis->transmap = (tr_transhi<<FF_TRANSSHIFT) - 0x10000 + transtables;
    }
    else if (fixedcolormap)
    {
        // fixed map : all the screen has the same colormap
        //  eg: negative effect of invulnerability
        vis->colormap = fixedcolormap;
    }
    else if (thing->frame & FF_FULLBRIGHT)
    {
        // full bright : goggles
        vis->colormap = colormaps;
    }
    else
    {
        // diminished light
        index = xscale>>(LIGHTSCALESHIFT-detailshift);

        if (index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE-1;

        vis->colormap = spritelights[index];
    }
}




//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites (sector_t* sec)
{
    mobj_t*             thing;
    int                 lightnum;

    if (rendermode != render_soft)
        return;

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sec->validcount == validcount)
        return;

    // Well, now it will be done.
    sec->validcount = validcount;

    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+extralight;

    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS-1];
    else
        spritelights = scalelight[lightnum];

    // Handle all things in sector.
    for (thing = sec->thinglist ; thing ; thing = thing->snext)
        if((thing->flags & MF_NOSECTOR)==0)
        R_ProjectSprite (thing);
}


//
// R_DrawPSprite
//
extern int centery;
extern int centerypsp;
void R_DrawPSprite (pspdef_t* psp)
{
    fixed_t             tx;
    int                 x1;
    int                 x2;
    spritedef_t*        sprdef;
    spriteframe_t*      sprframe;
    int                 lump;
    boolean             flip;
    vissprite_t*        vis;
    vissprite_t         avis;

    // decide which patch to use
#ifdef RANGECHECK
    if ( (unsigned)psp->state->sprite >= numsprites)
        I_Error ("R_ProjectSprite: invalid sprite number %i ",
                 psp->state->sprite);
#endif
    sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
    if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
        I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
                 psp->state->sprite, psp->state->frame);
#endif
    sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

    //Fab:debug
    //if (sprframe==NULL)
    //    I_Error("sprframes NULL for state %d\n", psp->state - states);

    //Fab: see the notes in R_ProjectSprite about lumpid,lumppat
    lump = sprframe->lumpid[0];
    flip = (boolean)sprframe->flip[0];

    // calculate edges of the shape

    //added:08-01-98:replaced mul by shift
    tx = psp->sx-((BASEVIDWIDTH/2)<<FRACBITS); //*FRACUNITS);

    //added:02-02-98:spriteoffset should be abs coords for psprites, based on
    //               320x200
    tx -= spriteoffset[lump];
    x1 = (centerxfrac + FixedMul (tx,pspritescale) ) >>FRACBITS;

    // off the right side
    if (x1 > viewwidth)
        return;

    tx +=  spritewidth[lump];
    x2 = ((centerxfrac + FixedMul (tx, pspritescale) ) >>FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    if(cv_splitscreen.value)
        vis->texturemid = (120<<(FRACBITS))+FRACUNIT/2-(psp->sy-spritetopoffset[lump]);
    else
        vis->texturemid = (BASEYCENTER<<FRACBITS)+FRACUNIT/2-(psp->sy-spritetopoffset[lump]);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
    vis->scale = pspriteyscale;  //<<detailshift;

    if (flip)
    {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = spritewidth[lump]-1;
    }
    else
    {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale*(vis->x1-x1);

    //Fab: see above for more about lumpid,lumppat
    vis->patch = sprframe->lumppat[0];

    if (viewplayer->mo->flags & MF_SHADOW)      // invisibility effect
    {
        vis->colormap = NULL;   // use translucency

        // in Doom2, it used to switch between invis/opaque the last seconds
        // now it switch between invis/less invis the last seconds
        if (viewplayer->powers[pw_invisibility] > 4*TICRATE
            || viewplayer->powers[pw_invisibility] & 8)
            vis->transmap = (tr_transhi<<FF_TRANSSHIFT) - 0x10000 + transtables;
        else
            vis->transmap = (tr_transmed<<FF_TRANSSHIFT) - 0x10000 + transtables;
    }
    else if (fixedcolormap)
    {
        // fixed color
        vis->colormap = fixedcolormap;
    }
    else if (psp->state->frame & FF_FULLBRIGHT)
    {
        // full bright
        vis->colormap = colormaps;
    }
    else
    {
        // local light
        vis->colormap = spritelights[MAXLIGHTSCALE-1];
    }

    R_DrawVisSprite (vis, vis->x1, vis->x2);
}



//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites (void)
{
    int         i;
    int         lightnum;
    pspdef_t*   psp;

    int kikhak;

    if (rendermode != render_soft)
        return;

    // get light level
    lightnum =
        (viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT)
        +extralight;

    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS-1];
    else
        spritelights = scalelight[lightnum];

    // clip to screen bounds
    mfloorclip = screenheightarray;
    mceilingclip = negonearray;

    //added:06-02-98: quickie fix for psprite pos because of freelook
    kikhak = centery;
    centery = centerypsp;             //for R_DrawColumn
    centeryfrac = centery<<FRACBITS;  //for R_DrawVisSprite

    // add all active psprites
    for (i=0, psp=viewplayer->psprites;
         i<NUMPSPRITES;
         i++,psp++)
    {
        if (psp->state)
            R_DrawPSprite (psp);
    }

    //added:06-02-98: oooo dirty boy
    centery = kikhak;
    centeryfrac = centery<<FRACBITS;
}




//
// R_SortVisSprites
//
vissprite_t     vsprsortedhead;


void R_SortVisSprites (void)
{
    int                 i;
    int                 count;
    vissprite_t*        ds;
    vissprite_t*        best=NULL;      //shut up compiler
    vissprite_t         unsorted;
    fixed_t             bestscale;

    count = vissprite_p - vissprites;

    unsorted.next = unsorted.prev = &unsorted;

    if (!count)
        return;

    for (ds=vissprites ; ds<vissprite_p ; ds++)
    {
        ds->next = ds+1;
        ds->prev = ds-1;
    }

    vissprites[0].prev = &unsorted;
    unsorted.next = &vissprites[0];
    (vissprite_p-1)->next = &unsorted;
    unsorted.prev = vissprite_p-1;

    // pull the vissprites out by scale
    //best = 0;         // shut up the compiler warning
    vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
    for (i=0 ; i<count ; i++)
    {
        bestscale = MAXINT;
        for (ds=unsorted.next ; ds!= &unsorted ; ds=ds->next)
        {
            if (ds->scale < bestscale)
            {
                bestscale = ds->scale;
                best = ds;
            }
        }
        best->next->prev = best->prev;
        best->prev->next = best->next;
        best->next = &vsprsortedhead;
        best->prev = vsprsortedhead.prev;
        vsprsortedhead.prev->next = best;
        vsprsortedhead.prev = best;
    }
}



//
// R_DrawSprite
//
//Fab:26-04-98:
// NOTE : uses con_clipviewtop, so that when console is on,
//        don't draw the part of sprites hidden under the console
void R_DrawSprite (vissprite_t* spr)
{
    drawseg_t*          ds;
    short               clipbot[MAXVIDWIDTH];
    short               cliptop[MAXVIDWIDTH];
    int                 x;
    int                 r1;
    int                 r2;
    fixed_t             scale;
    fixed_t             lowscale;
    int                 silhouette;

    for (x = spr->x1 ; x<=spr->x2 ; x++)
        clipbot[x] = cliptop[x] = -2;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
    {
        // determine if the drawseg obscures the sprite
        if (ds->x1 > spr->x2
         || ds->x2 < spr->x1
         || (!ds->silhouette
             && !ds->maskedtexturecol) )
        {
            // does not cover sprite
            continue;
        }

        r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

        if (ds->scale1 > ds->scale2)
        {
            lowscale = ds->scale2;
            scale = ds->scale1;
        }
        else
        {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }

        if (scale < spr->scale
            || ( lowscale < spr->scale
                 && !R_PointOnSegSide (spr->gx, spr->gy, ds->curline) ) )
        {
            // masked mid texture?
            if (ds->maskedtexturecol)
                R_RenderMaskedSegRange (ds, r1, r2);
            // seg is behind sprite
            continue;
        }


        // clip this piece of the sprite
        silhouette = ds->silhouette;

        if (spr->gz >= ds->bsilheight)
            silhouette &= ~SIL_BOTTOM;

        if (spr->gzt <= ds->tsilheight)
            silhouette &= ~SIL_TOP;

        if (silhouette == 1)
        {
            // bottom sil
            for (x=r1 ; x<=r2 ; x++)
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
        }
        else if (silhouette == 2)
        {
            // top sil
            for (x=r1 ; x<=r2 ; x++)
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
        }
        else if (silhouette == 3)
        {
            // both
            for (x=r1 ; x<=r2 ; x++)
            {
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }

    }

    // all clipping has been performed, so draw the sprite

    // check for unclipped columns
    for (x = spr->x1 ; x<=spr->x2 ; x++)
    {
        if (clipbot[x] == -2)
            clipbot[x] = viewheight;

        if (cliptop[x] == -2)
            //Fab:26-04-98: was -1, now clips against console bottom
            cliptop[x] = con_clipviewtop;
    }

    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr, spr->x1, spr->x2);
}


//
//  Draw all vissprites
//
void R_DrawSprites (void)
{
    vissprite_t*        spr;

    //faB: dont mess with 3Dfx cache of sprites, if 3Dfx mode, don't
    //     cache the software sprites
    if (rendermode!= render_soft)
        return;

    R_SortVisSprites ();
    if (vissprite_p > vissprites)
    {
        // draw all vissprites back to front
        for (spr = vsprsortedhead.next ;
             spr != &vsprsortedhead ;
             spr=spr->next)
        {

            R_DrawSprite (spr);
        }
    }
}


//
// R_DrawMasked
//
void R_DrawMasked (void)
{
    drawseg_t*          ds;

    // hack: called at r_drawplanes for test water
    //R_DrawSprites ();

    // render any remaining masked mid textures
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange (ds, ds->x1, ds->x2);

}



// ==========================================================================
//
//                              SKINS CODE
//
// ==========================================================================

int         numskins=0;
skin_t      skins[MAXSKINS];
// don't work because it must be inistilised before the config load
//#define SKINVALUES
#ifdef SKINVALUES
CV_PossibleValue_t skin_cons_t[MAXSKINS+1];
#endif

void Sk_SetDefaultValue(skin_t *skin)
{
    int   i;
    //
    // setup the 'marine' as default skin
    //
    memset (skin, 0, sizeof(skin_t));
    strcpy (skin->name, DEFAULTSKIN);
    strcpy (skin->faceprefix, "STF");
    strcpy (skin->typechar, "0"); // Tails 03-01-2000
    for (i=0;i<sfx_freeslot0;i++)
        if (S_sfx[i].skinsound!=-1)
        {
            skin->soundsid[S_sfx[i].skinsound] = i;
        }
    memcpy(&skins[0].spritedef, &sprites[SPR_PLAY], sizeof(spritedef_t));

}

//
// Initialize the basic skins
//
void R_InitSkins (void)
{
#ifdef SKINVALUES
    int i;

    for(i=0;i<=MAXSKINS;i++)
    {
        skin_cons_t[i].value=0;
        skin_cons_t[i].strvalue=NULL;
    }
#endif

    // initialize free sfx slots for skin sounds
    S_InitRuntimeSounds ();

    Sk_SetDefaultValue(&skins[0]);
#ifdef SKINVALUES
    skin_cons_t[0].strvalue=skins[0].name;
#endif

    // make the standard Doom2 marine as the default skin
    numskins = 1;
}

// returns true if the skin name is found (loaded from pwad)
int R_SkinAvailable (char* name)
{
    int  i;

    for (i=0;i<numskins;i++)
    {
        if (stricmp(skins[i].name,name)==0)
            return i;
    }
    return 0;
}

extern boolean st_firsttime;

// network code calls this when a 'skin change' is received
void SetPlayerSkin (int playernum, char *skinname)
{
    int   i;

    for(i=0;i<numskins;i++)
    {
        if (stricmp(skins[i].name,skinname)==0)
        {
            // change the face graphics
            if (playernum==statusbarplayer &&
            // for save time test it there is a real change
                strcmp (skins[players[playernum].skin].faceprefix, skins[i].faceprefix) )
            {
                ST_unloadFaceGraphics ();
                ST_loadFaceGraphics (skins[i].faceprefix);
                st_firsttime = true;        // force refresh of status bar
            }

            players[playernum].skin = i;
            if (players[playernum].mo)
                players[playernum].mo->skin = &skins[i];

            return;
        }
    }

    CONS_Printf("Skin %s not found\n",skinname);
    players[playernum].skin = 0;  // not found put the old marine skin

    // a copy of the skin value
    // so that dead body detached from respawning player keeps the skin
    if (players[playernum].mo)
        players[playernum].mo->skin = &skins[0];
}

//
// Add skins from a pwad, each skin preceded by 'S_SKIN' marker
//

// Does the same is in w_wad, but check only for
// the first 6 characters (this is so we can have S_SKIN1, S_SKIN2..
// for wad editors that don't like multiple resources of the same name)
//
int W_CheckForSkinMarkerInPwad (int wadid, int startlump)
{
    int         i;
    int         v1;
    lumpinfo_t* lump_p;

    union {
                char    s[4];
                int             x;
    } name4;

    strncpy (name4.s, "S_SK", 4);
    v1 = name4.x;

    // scan forward, start at <startlump>
    if (startlump < wadfiles[wadid]->numlumps)
    {
        lump_p = wadfiles[wadid]->lumpinfo + startlump;
        for (i = startlump; i<wadfiles[wadid]->numlumps; i++,lump_p++)
        {
            if ( *(int *)lump_p->name == v1 &&
                 lump_p->name[4] == 'I'     &&
                 lump_p->name[5] == 'N')
            {
                return ((wadid<<16)+i);
            }
        }
    }
    return -1; // not found
}

//
// Find skin sprites, sounds & optional status bar face, & add them
//
void R_AddSkins (int wadnum)
{
    int         lumpnum;
    int         lastlump;

    lumpinfo_t* lumpinfo;
    char*       sprname;
    int         intname;

    char*       buf;
    char*       buf2;

    char*       token;
    char*       value;

    int         i,size;

    //
    // search for all skin markers in pwad
    //

    lastlump = 0;
    while ( (lumpnum=W_CheckForSkinMarkerInPwad (wadnum, lastlump))!=-1 )
    {
        if (numskins>MAXSKINS)
        {
            CONS_Printf ("ignored skin (%d skins maximum)\n",MAXSKINS);
            lastlump++;
            continue; //faB:so we know how many skins couldn't be added
        }

        buf  = W_CacheLumpNum (lumpnum, PU_CACHE);
        size = W_LumpLength (lumpnum);

        // for strtok
        buf2 = (char *) malloc (size+1);
        if(!buf2)
             I_Error("R_AddSkins: No more free memory\n");
        memcpy (buf2,buf,size);
        buf2[size] = '\0';

        // set defaults
        Sk_SetDefaultValue(&skins[numskins]);
        sprintf (skins[numskins].name,"skin %d",numskins);

        // parse
        token = strtok (buf2, "\r\n= ");
        while (token)
        {
            if(token[0]=='/' && token[1]=='/') // skip comments
            {
                token = strtok (NULL, "\r\n"); // skip end of line
                goto next_token;               // find the real next token
            }

            value = strtok (NULL, "\r\n= ");
//            CONS_Printf("token = %s, value = %s",token,value);
//            CONS_Error("ga");

            if (!value)
                I_Error ("R_AddSkins: syntax error in S_SKIN lump# %d in WAD %s\n", lumpnum&0xFFFF, wadfiles[wadnum]->filename);

            if (!stricmp(token,"name"))
            {
                // the skin name must uniquely identify a single skin
                // I'm lazy so if name is already used I leave the 'skin x'
                // default skin name set above
                if (!R_SkinAvailable (value))
                {
                    strncpy (skins[numskins].name, value, SKINNAMESIZE);
                    strlwr (skins[numskins].name);
                }
            }
            else
            if (!stricmp(token,"face"))
            {
                strncpy (skins[numskins].faceprefix, value, 3);
                strupr (skins[numskins].faceprefix);
            }
// start character type identification Tails 03-01-2000
            else
            if (!stricmp(token,"typechar"))
            {
                strncpy (skins[numskins].typechar, value, 1);
                strupr (skins[numskins].typechar);
            }
// end character type identification Tails 03-01-2000
            else
            {
                int found=false;
                // copy name of sounds that are remapped for this skin
                for (i=0;i<sfx_freeslot0;i++)
                {
                    if (S_sfx[i].skinsound!=-1 &&
                        !stricmp(S_sfx[i].name, token+2) )
                    {
                        skins[numskins].soundsid[S_sfx[i].skinsound]=
                            S_AddSoundFx(value+2,S_sfx[i].singularity);
                        found=true;
                    }
                }
                if(!found)
                    I_Error("R_AddSkins: Unknow keyword '%s' in S_SKIN lump# %d (WAD %s)\n",token,lumpnum&0xFFFF,wadfiles[wadnum]->filename);
            }
next_token:
            token = strtok (NULL,"\r\n= ");
        }

        free(buf2);
        lumpnum &= 0xFFFF;      // get rid of wad number

        // get the base name of this skin's sprite (4 chars)
        lumpnum++;
        lumpinfo = wadfiles[wadnum]->lumpinfo;
        sprname = lumpinfo[lumpnum].name;
        intname = *(int *)sprname;

        // skip to end of this skin's frames
        lastlump = lumpnum;
        while (*(int *)lumpinfo[lastlump].name == intname)
            lastlump++;

        // allocate (or replace) sprite frames, and set spritedef
        R_AddSingleSpriteDef (sprname, &skins[numskins].spritedef, wadnum, lumpnum, lastlump);

        CONS_Printf ("added skin '%s'\n", skins[numskins].name);
#ifdef SKINVALUES
        skin_cons_t[numskins].value=numskins;
        skin_cons_t[numskins].strvalue=skins[numskins].name;
#endif

        numskins++;
    }

    return;
}
