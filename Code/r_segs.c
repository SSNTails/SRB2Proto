// r_segs.c :  All the clipping: columns, horizontal spans, sky columns.

#include "doomdef.h"
#include "r_local.h"
#include "r_sky.h"

#include "r_splats.h"           //faB: testing

#include "w_wad.h"
#include "z_zone.h"

extern fixed_t waterheight;

// OPTIMIZE: closed two sided lines as single sided

// True if any of the segs textures might be visible.
boolean         segtextured;

// False if the back side is the same plane.
boolean         markfloor;
boolean         markceiling;
boolean         markwater;   //added:18-02-98:WATER!

boolean         maskedtexture;
int             toptexture;
int             bottomtexture;
int             midtexture;


angle_t         rw_normalangle;
// angle to line origin
int             rw_angle1;

//
// regular wall
//
int             rw_x;
int             rw_stopx;
angle_t         rw_centerangle;
fixed_t         rw_offset;
fixed_t         rw_distance;
fixed_t         rw_scale;
fixed_t         rw_scalestep;
fixed_t         rw_midtexturemid;
fixed_t         rw_toptexturemid;
fixed_t         rw_bottomtexturemid;

int             worldtop;
int             worldbottom;
int             worldhigh;
int             worldlow;
int             waterz;   //added:18-02-98:WATER!

fixed_t         pixhigh;
fixed_t         pixlow;
fixed_t         pixhighstep;
fixed_t         pixlowstep;

fixed_t         topfrac;
fixed_t         topstep;

fixed_t         bottomfrac;
fixed_t         bottomstep;

fixed_t         waterfrac;      //added:18-02-98:WATER!
fixed_t         waterstep;


lighttable_t**  walllights;

short*          maskedtexturecol;



// ==========================================================================
// R_Splats Wall Splats Drawer
// ==========================================================================

#ifdef WALLSPLATS

static void R_DrawSplatColumn (column_t* column)
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

        if (dc_yh >= viewheight)
            dc_yh = viewheight-1;
        if (dc_yl < 0)
            dc_yl = 0;

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


static void R_DrawWallSplats (drawseg_t* ds)
{
    wallsplat_t*    splat;
    seg_t*      seg;
    angle_t     angle, angle1, angle2;
    int         x1, x2;
    unsigned    index;
    column_t*   col;
    patch_t*    patch;
    fixed_t     texturecolumn;

    splat = (wallsplat_t*) linedef->splats;

#ifdef PARANOIA
    if (!splat)
        I_Error ("R_DrawWallSplats: splat is NULL");
#endif

    seg = ds->curline;

    // draw all splats from the line that touches the range of the seg
    for ( ; splat ; splat=splat->next)
    {
        angle1 = R_PointToAngle (splat->v1.x, splat->v1.y);
        angle2 = R_PointToAngle (splat->v2.x, splat->v2.y);
        angle1 = (angle1-viewangle+ANG90)>>ANGLETOFINESHIFT;
        angle2 = (angle2-viewangle+ANG90)>>ANGLETOFINESHIFT;
        x1 = viewangletox[angle1];
        x2 = viewangletox[angle2];
        if (x1 == x2)
            continue;                         // does not cross a pixel

        // splat is not in this seg range
        if (x2 < ds->x1 || x1 > ds->x2)
            continue;

        if (x1 < ds->x1)
            x1 = ds->x1;
        if (x2 > ds->x2)
            x2 = ds->x2;

        // calculate incremental stepping values for texture edges
        rw_scalestep = ds->scalestep;
        spryscale = ds->scale1 + (x1 - ds->x1)*rw_scalestep;
        mfloorclip = floorclip;
        mceilingclip = ceilingclip;

        patch = W_CachePatchNum (splat->patch, PU_CACHE);

        // clip splat range to seg range left
        /*if (x1 < ds->x1)
        {
            spryscale += (rw_scalestep * (ds->x1 - x1));
            x1 = ds->x1;
        }*/
        // clip splat range to seg range right

        frontsector = ds->curline->frontsector;
        dc_texturemid = splat->top + (patch->height<<(FRACBITS-1)) - viewz;
        sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);

        // set drawing mode
        switch (splat->flags & SPLATDRAWMODE_MASK)
        {
        case SPLATDRAWMODE_OPAQUE:
            colfunc = basecolfunc;
            break;
        case SPLATDRAWMODE_TRANS:
            colfunc = fuzzcolfunc;
            dc_transmap = (tr_transmed<<FF_TRANSSHIFT) - 0x10000 + transtables;
            break;
        case SPLATDRAWMODE_SHADE:
            colfunc = shadecolfunc;
            break;
        }
        if (fixedcolormap)
            dc_colormap = fixedcolormap;

        // draw the columns
        for (dc_x = x1 ; dc_x <= x2 ; dc_x++)
        {
            if (!fixedcolormap)
            {
                index = spryscale>>LIGHTSCALESHIFT;
                if (index >=  MAXLIGHTSCALE )
                    index = MAXLIGHTSCALE-1;
                dc_colormap = walllights[index];
            }

            sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
            dc_iscale = 0xffffffffu / (unsigned)spryscale;

            // find column of patch, from perspective
            angle = (rw_centerangle + xtoviewangle[dc_x])>>ANGLETOFINESHIFT;
            texturecolumn = rw_offset - splat->offset - FixedMul(finetangent[angle],rw_distance);
            texturecolumn >>= FRACBITS;

            //texturecolumn &= 7;
            //DEBUG
            /*if (texturecolumn < 0 || texturecolumn>=patch->width) {
                CONS_Printf ("invalid texturecolumn %d width %d\n", texturecolumn, patch->width);
                texturecolumn= 0;
            }*/
            if ((unsigned)texturecolumn >= patch->width)
                texturecolumn = 0;

            // draw the texture
            col = (column_t *) ((byte *)patch + LONG(patch->columnofs[texturecolumn]));
            R_DrawSplatColumn (col);

            spryscale += rw_scalestep;
        }

    }// next splat

    colfunc = basecolfunc;
}

#endif //WALLSPLATS



// ==========================================================================
// R_RenderMaskedSegRange
// ==========================================================================

// If we have a multi-patch texture on a 2sided wall (rare) then we draw
//  it using R_DrawColumn, else we draw it using R_DrawMaskedColumn, this
//  way we don't have to store extra post_t info with each column for
//  multi-patch textures. They are not normally needed as multi-patch
//  textures don't have holes in it. At least not for now.
static int  column2s_length;     // column->length : for multi-patch on 2sided wall = texture->height

void R_Render2sidedMultiPatchColumn (column_t* column)
{
    int         topscreen;
    int         bottomscreen;

        topscreen = sprtopscreen; // + spryscale*column->topdelta;  topdelta is 0 for the wall
        bottomscreen = topscreen + spryscale * column2s_length;

        dc_yl = (sprtopscreen+FRACUNIT-1)>>FRACBITS;
        dc_yh = (bottomscreen-1)>>FRACBITS;

        if (dc_yh >= mfloorclip[dc_x])
            dc_yh = mfloorclip[dc_x]-1;
        if (dc_yl <= mceilingclip[dc_x])
            dc_yl = mceilingclip[dc_x]+1;

        if (dc_yl <= dc_yh)
        {
            dc_source = (byte *)column + 3;
            colfunc ();
        }
}

void R_RenderMaskedSegRange (drawseg_t* ds,
                                                         int        x1,
                                                         int        x2 )
{
    unsigned    index;
    column_t*   col;
    int             lightnum;
    int             texnum;

    void (*colfunc_2s) (column_t*);

        line_t* ldef;   //faB

    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?
    // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
    curline = ds->curline;
    frontsector = curline->frontsector;
    backsector = curline->backsector;
    texnum = texturetranslation[curline->sidedef->midtexture];

    //faB: hack translucent linedef types (201-205 for transtables 1-5)
#if 1
    ldef = curline->linedef;
    if (ldef->special>=201 && ldef->special<=205)
    {
                dc_transmap = ((ldef->special-200)<<FF_TRANSSHIFT) - 0x10000 + transtables;
                colfunc = fuzzcolfunc;
    }
    else
                colfunc = basecolfunc;
#endif

    lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)+extralight;

    if (curline->v1->y == curline->v2->y)
                lightnum--;
    else if (curline->v1->x == curline->v2->x)
                lightnum++;

    if (lightnum < 0)
                walllights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
                walllights = scalelight[LIGHTLEVELS-1];
    else
                walllights = scalelight[lightnum];

    maskedtexturecol = ds->maskedtexturecol;

    rw_scalestep = ds->scalestep;
    spryscale = ds->scale1 + (x1 - ds->x1)*rw_scalestep;
    mfloorclip = ds->sprbottomclip;
    mceilingclip = ds->sprtopclip;

    // find positioning
    if (curline->linedef->flags & ML_DONTPEGBOTTOM)
    {
                dc_texturemid = frontsector->floorheight > backsector->floorheight
                        ? frontsector->floorheight : backsector->floorheight;
                dc_texturemid = dc_texturemid + textureheight[texnum] - viewz;
    }
    else
    {
                dc_texturemid =frontsector->ceilingheight<backsector->ceilingheight
                        ? frontsector->ceilingheight : backsector->ceilingheight;
                dc_texturemid = dc_texturemid - viewz;
    }
    dc_texturemid += curline->sidedef->rowoffset;

    if (fixedcolormap)
                dc_colormap = fixedcolormap;

    //faB: handle case where multipatch texture is drawn on a 2sided wall, multi-patch textures
    //     are not stored per-column with post info anymore in Doom Legacy
    if (textures[texnum]->patchcount==1)
        colfunc_2s = R_DrawMaskedColumn;                    //render the usual 2sided single-patch packed texture
    else {
        colfunc_2s = R_Render2sidedMultiPatchColumn;        //render multipatch with no holes (no post_t info)
        column2s_length = textures[texnum]->height;
    }

    // draw the columns
    for (dc_x = x1 ; dc_x <= x2 ; dc_x++)
    {
                // calculate lighting
                if (maskedtexturecol[dc_x] != MAXSHORT)
                {
                        if (!fixedcolormap)
                        {
                                index = spryscale>>LIGHTSCALESHIFT;

                                if (index >=  MAXLIGHTSCALE )
                                        index = MAXLIGHTSCALE-1;

                                dc_colormap = walllights[index];
                        }

                        sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
                        dc_iscale = 0xffffffffu / (unsigned)spryscale;

                        // draw the texture
                        col = (column_t *)(
                                (byte *)R_GetColumn(texnum,maskedtexturecol[dc_x]) - 3);

                        colfunc_2s (col);
                        maskedtexturecol[dc_x] = MAXSHORT;
                }
                spryscale += rw_scalestep;
    }
    colfunc = basecolfunc;
}




//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS              12
#define HEIGHTUNIT              (1<<HEIGHTBITS)


//profile stuff ---------------------------------------------------------
//#define TIMING
#ifdef TIMING
#include "p5prof.h"
long long mycount;
long long mytotal = 0;
unsigned long   nombre = 100000;
//static   char runtest[10][80];
#endif
//profile stuff ---------------------------------------------------------


void R_RenderSegLoop (void)
{
    angle_t             angle;
    unsigned            index;
    int                 yl;
    int                 yh;
    int                 yw;     //added:18-02-98:WATER!
    int                 mid;
    fixed_t             texturecolumn;
    int                 top;
    int                 bottom;

    //texturecolumn = 0;                                // shut up compiler warning

    for ( ; rw_x < rw_stopx ; rw_x++)
    {
                // mark floor / ceiling areas
                yl = (topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

                // no space above wall?
                if (yl < ceilingclip[rw_x]+1)
                        yl = ceilingclip[rw_x]+1;

                if (markceiling)
                {
                        top = ceilingclip[rw_x]+1;
                        bottom = yl-1;

                        if (bottom >= floorclip[rw_x])
                                bottom = floorclip[rw_x]-1;

                        if (top <= bottom)
                        {
                                ceilingplane->top[rw_x] = top;
                                ceilingplane->bottom[rw_x] = bottom;
                        }
                }

                yh = bottomfrac>>HEIGHTBITS;

                if (yh >= floorclip[rw_x])
                        yh = floorclip[rw_x]-1;

                if (markfloor)
                {
                        top = yh+1;
                        bottom = floorclip[rw_x]-1;
                        if (top <= ceilingclip[rw_x])
                                top = ceilingclip[rw_x]+1;
                        if (top <= bottom)
                        {
                                floorplane->top[rw_x] = top;
                                floorplane->bottom[rw_x] = bottom;
                        }
                }

                if (markwater)
                {
                        //added:18-02-98:WATER!
                        yw = waterfrac>>HEIGHTBITS;

                        // the markwater stuff...
                        if (waterheight<viewz)
                        {
                                top = yw;
                                bottom = waterclip[rw_x]-1;

                                if (top <= ceilingclip[rw_x])
                                        top = ceilingclip[rw_x]+1;
                        }
                        else  //view from under
                        {
                                top = waterclip[rw_x]+1;
                                bottom = yw;

                                if (bottom >= floorclip[rw_x])
                                        bottom = floorclip[rw_x]-1;
                        }
                        if (top <= bottom)
                        {
                                waterplane->top[rw_x] = top;
                                waterplane->bottom[rw_x] = bottom;
                        }

                        // do it only if markwater else not needed!
                        waterfrac += waterstep;   //added:18-02-98:WATER!
                        //dc_wcolormap = colormaps+(32<<8);
                }

                // texturecolumn and lighting are independent of wall tiers
                if (segtextured)
                {
                        // calculate texture offset
                        angle = (rw_centerangle + xtoviewangle[rw_x])>>ANGLETOFINESHIFT;
                        texturecolumn = rw_offset-FixedMul(finetangent[angle],rw_distance);
                        texturecolumn >>= FRACBITS;
                        // calculate lighting
                        index = rw_scale>>LIGHTSCALESHIFT;

                        if (index >=  MAXLIGHTSCALE )
                                index = MAXLIGHTSCALE-1;

                        dc_colormap = walllights[index];
                        dc_x = rw_x;
                        dc_iscale = 0xffffffffu / (unsigned)rw_scale;
                }

                // draw the wall tiers
                if (midtexture)
                {
                        // single sided line
                        dc_yl = yl;
                        dc_yh = yh;
                        dc_texturemid = rw_midtexturemid;
                        dc_source = R_GetColumn(midtexture,texturecolumn);

                        //profile stuff ---------------------------------------------------------
#ifdef TIMING
                        ProfZeroTimer();
#endif
#ifdef HORIZONTALDRAW
                        hcolfunc ();
#else
#ifdef PARANOIA
                        if((int)colfunc==0)
                             I_Error("Colfunc NULL (%d)",(int)colfunc);
#endif
                        colfunc ();
#endif
#ifdef TIMING
                        RDMSR(0x10,&mycount);
                        mytotal += mycount;      //64bit add

                        if(nombre--==0)
                                I_Error("R_DrawColumn CPU Spy reports: 0x%d %d\n", *((int*)&mytotal+1),
                                (int)mytotal );
#endif
                        //profile stuff ---------------------------------------------------------

                        // dont draw anything more for this column, since
                        // a midtexture blocks the view
                        ceilingclip[rw_x] = viewheight;
                        floorclip[rw_x] = -1;
                }
                else
                {
                        // two sided line
                        if (toptexture)
                        {
                                // top wall
                                mid = pixhigh>>HEIGHTBITS;
                                pixhigh += pixhighstep;

                                if (mid >= floorclip[rw_x])
                                        mid = floorclip[rw_x]-1;

                                if (mid >= yl)
                                {
                                        dc_yl = yl;
                                        dc_yh = mid;
                                        dc_texturemid = rw_toptexturemid;
                                        dc_source = R_GetColumn(toptexture,texturecolumn);
#ifdef HORIZONTALDRAW
                                        hcolfunc ();
#else
                                        colfunc ();
#endif
                                        ceilingclip[rw_x] = mid;
                                }
                                else
                                        ceilingclip[rw_x] = yl-1;
                        }
                        else
                        {
                                // no top wall
                                if (markceiling)
                                        ceilingclip[rw_x] = yl-1;
                                if (!waterplane || markwater)
                                        waterclip[rw_x] = yl-1;
                        }

                        if (bottomtexture)
                        {
                                // bottom wall
                                mid = (pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
                                pixlow += pixlowstep;

                                // no space above wall?
                                if (mid <= ceilingclip[rw_x])
                                        mid = ceilingclip[rw_x]+1;

                                if (mid <= yh)
                                {
                                        dc_yl = mid;
                                        dc_yh = yh;
                                        dc_texturemid = rw_bottomtexturemid;
                                        dc_source = R_GetColumn(bottomtexture,
                                                texturecolumn);
#ifdef HORIZONTALDRAW
                                        hcolfunc ();
#else
                                        colfunc ();
#endif
                                        floorclip[rw_x] = mid;
                                        if (waterplane && waterz<worldlow)
                                                waterclip[rw_x] = mid;
                                }
                                else
                                        floorclip[rw_x] = yh+1;
                        }
                        else
                        {
                                // no bottom wall
                                if (markfloor)
                                {
                                        floorclip[rw_x] = yh+1;
                                        if (!waterplane || markwater)
                                                waterclip[rw_x] = yh+1;
                                }
                        }

                        if (maskedtexture)
                        {
                                // save texturecol
                                //  for backdrawing of masked mid texture
                                maskedtexturecol[rw_x] = texturecolumn;
                        }
                }

                rw_scale += rw_scalestep;
                topfrac += topstep;
                bottomfrac += bottomstep;
    }
}



//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange( int   start, int   stop )
{
    fixed_t             hyp;
    fixed_t             sineval;
    angle_t             distangle, offsetangle;
    fixed_t             vtop;
    int                 lightnum;

        static int    countdrawsegs=0;  // current size of drawsegs table
        static int    maxusedseg=0;     // for development/debug, how much are used

    // don't overflow and crash
    //faB: here's an ugly so-called 'removal'.. don't like it coz it's
    //     slow and sub-optimal but hey.. gotta release tomorrow
    if (ds_p == drawsegs + countdrawsegs)
    {
                drawsegs = realloc (drawsegs, sizeof(drawseg_t)*(countdrawsegs+256));
                ds_p = drawsegs + countdrawsegs;
                countdrawsegs += 256;
                //CONS_Printf ("\2reallocated drawsegs to %d\n", countdrawsegs);
                if (devparm)
                {
                        if ((ds_p-drawsegs) > maxusedseg)
                        {
                                maxusedseg = ds_p-drawsegs;
                                CONS_Printf ("maxdrawseg: %d\n", maxusedseg);
                        }
                }
    }

    //faB: limits removal like above SUCKS ... badly
    //if (ds_p == &drawsegs[MAXDRAWSEGS])
    //    return;

#ifdef RANGECHECK
    if (start >=viewwidth || start > stop)
                I_Error ("Bad R_RenderWallRange: %i to %i", start , stop);
#endif

    sidedef = curline->sidedef;
    linedef = curline->linedef;

    // mark the segment as visible for auto map
    linedef->flags |= ML_MAPPED;

    // calculate rw_distance for scale calculation
    rw_normalangle = curline->angle + ANG90;
    offsetangle = abs(rw_normalangle-rw_angle1);

    if (offsetangle > ANG90)
                offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    hyp = R_PointToDist (curline->v1->x, curline->v1->y);
    sineval = finesine[distangle>>ANGLETOFINESHIFT];
    rw_distance = FixedMul (hyp, sineval);


    ds_p->x1 = rw_x = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    rw_stopx = stop+1;

    // calculate scale at both ends and step
    ds_p->scale1 = rw_scale =
                R_ScaleFromGlobalAngle (viewangle + xtoviewangle[start]);

    if (stop > start)
    {
                ds_p->scale2 = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[stop]);
        ds_p->scalestep = rw_scalestep = (ds_p->scale2 - rw_scale) / (stop-start);
    }
    else
    {
                // UNUSED: try to fix the stretched line bug
#if 0
                if (rw_distance < FRACUNIT/2)
                {
                        fixed_t         trx,try;
                        fixed_t         gxt,gyt;

                        trx = curline->v1->x - viewx;
                        try = curline->v1->y - viewy;

                        gxt = FixedMul(trx,viewcos);
                        gyt = -FixedMul(try,viewsin);
                        ds_p->scale1 = FixedDiv(projection, gxt-gyt)<<detailshift;
                }
#endif
                ds_p->scale2 = ds_p->scale1;
    }

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed
    worldtop = frontsector->ceilingheight - viewz;
    worldbottom = frontsector->floorheight - viewz;

        //added:18-02-98:WATER!
        if (waterplane)
        {
                waterz = waterplane->height - viewz;
                if (waterplane->height >= frontsector->ceilingheight)
                        I_Error("eau plus haut que plafond");
        }

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
    ds_p->maskedtexturecol = NULL;

    if (!backsector)
    {
                // single sided line
                midtexture = texturetranslation[sidedef->midtexture];
                // a single sided line is terminal, so it must mark ends
                markfloor = markceiling = true;

                //added:18-02-98:WATER! onesided marque toujours l'eau si ya dlo
                if (waterplane)
                        markwater = true;
                else
                        markwater = false;

                if (linedef->flags & ML_DONTPEGBOTTOM)
                {
                        vtop = frontsector->floorheight +
                                textureheight[sidedef->midtexture];
                        // bottom of texture at bottom
                        rw_midtexturemid = vtop - viewz;
                }
                else
                {
                        // top of texture at top
                        rw_midtexturemid = worldtop;
                }
                rw_midtexturemid += sidedef->rowoffset;

                ds_p->silhouette = SIL_BOTH;
                ds_p->sprtopclip = screenheightarray;
                ds_p->sprbottomclip = negonearray;
                ds_p->bsilheight = MAXINT;
                ds_p->tsilheight = MININT;
    }
    else
    {
                // two sided line
                ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
                ds_p->silhouette = 0;

                if (frontsector->floorheight > backsector->floorheight)
                {
                        ds_p->silhouette = SIL_BOTTOM;
                        ds_p->bsilheight = frontsector->floorheight;
                }
                else if (backsector->floorheight > viewz)
                {
                        ds_p->silhouette = SIL_BOTTOM;
                        ds_p->bsilheight = MAXINT;
                        // ds_p->sprbottomclip = negonearray;
                }

                if (frontsector->ceilingheight < backsector->ceilingheight)
                {
                        ds_p->silhouette |= SIL_TOP;
                        ds_p->tsilheight = frontsector->ceilingheight;
                }
                else if (backsector->ceilingheight < viewz)
                {
                        ds_p->silhouette |= SIL_TOP;
                        ds_p->tsilheight = MININT;
                        // ds_p->sprtopclip = screenheightarray;
                }

                if (backsector->ceilingheight <= frontsector->floorheight)
                {
                        ds_p->sprbottomclip = negonearray;
                        ds_p->bsilheight = MAXINT;
                        ds_p->silhouette |= SIL_BOTTOM;
                }

                if (backsector->floorheight >= frontsector->ceilingheight)
                {
                        ds_p->sprtopclip = screenheightarray;
                        ds_p->tsilheight = MININT;
                        ds_p->silhouette |= SIL_TOP;
                }

                worldhigh = backsector->ceilingheight - viewz;
                worldlow = backsector->floorheight - viewz;

                // hack to allow height changes in outdoor areas
                if (frontsector->ceilingpic == skyflatnum
                        && backsector->ceilingpic == skyflatnum)
                {
                        worldtop = worldhigh;
                }


                if (worldlow != worldbottom
                        || backsector->floorpic != frontsector->floorpic
                        || backsector->lightlevel != frontsector->lightlevel)
                {
                        markfloor = true;
                }
                else
                {
                        // same plane on both sides
                        markfloor = false;
                }


                if (worldhigh != worldtop
                        || backsector->ceilingpic != frontsector->ceilingpic
                        || backsector->lightlevel != frontsector->lightlevel)
                {
                        markceiling = true;
                }
                else
                {
                        // same plane on both sides
                        markceiling = false;
                }

                if (backsector->ceilingheight <= frontsector->floorheight
                        || backsector->floorheight >= frontsector->ceilingheight)
                {
                        // closed door
                        markceiling = markfloor = true;
                }


                //added:18-02-98: WATER! jamais mark si l'eau ne touche pas
                //                d'upper et de bottom
                // (on s'en fout des differences de hauteur de plafond et
                //  de sol, tant que ca n'interrompt pas la surface de l'eau)
                markwater = false;

                // check TOP TEXTURE
                if (worldhigh < worldtop)
                {
                        //added:18-02-98:WATER! toptexture, check si ca touche watersurf
                        if (waterplane &&
                                waterz > worldhigh &&
                                waterz < worldtop)
                                markwater = true;

                        // top texture
                        toptexture = texturetranslation[sidedef->toptexture];
                        if (linedef->flags & ML_DONTPEGTOP)
                        {
                                // top of texture at top
                                rw_toptexturemid = worldtop;
                        }
                        else
                        {
                                vtop =
                                        backsector->ceilingheight
                                        + textureheight[sidedef->toptexture];

                                // bottom of texture
                                rw_toptexturemid = vtop - viewz;
                        }
                }
                // check BOTTOM TEXTURE
                if (worldlow > worldbottom)     //seulement si VISIBLE!!!
                {
                        //added:18-02-98:WATER! bottomtexture, check si ca touche watersurf
                        if (waterplane &&
                                waterz < worldlow &&
                                waterz > worldbottom)
                                markwater = true;


                        // bottom texture
                        bottomtexture = texturetranslation[sidedef->bottomtexture];

                        if (linedef->flags & ML_DONTPEGBOTTOM )
                        {
                                // bottom of texture at bottom
                                // top of texture at top
                                rw_bottomtexturemid = worldtop;
                        }
                        else    // top of texture at top
                                rw_bottomtexturemid = worldlow;
                }

                rw_toptexturemid += sidedef->rowoffset;
                rw_bottomtexturemid += sidedef->rowoffset;

                // allocate space for masked texture tables
                if (sidedef->midtexture)
                {
                        // masked midtexture
                        maskedtexture = true;
                        ds_p->maskedtexturecol = maskedtexturecol = lastopening - rw_x;
                        lastopening += rw_stopx - rw_x;
                }
    }

    // calculate rw_offset (only needed for textured lines)
    segtextured = midtexture | toptexture | bottomtexture | maskedtexture;

    if (segtextured)
    {
                offsetangle = rw_normalangle-rw_angle1;

                if (offsetangle > ANG180)
                        offsetangle = -offsetangle;

                if (offsetangle > ANG90)
                        offsetangle = ANG90;

                sineval = finesine[offsetangle >>ANGLETOFINESHIFT];
                rw_offset = FixedMul (hyp, sineval);

                if (rw_normalangle-rw_angle1 < ANG180)
                        rw_offset = -rw_offset;

                rw_offset += sidedef->textureoffset + curline->offset;
                rw_centerangle = ANG90 + viewangle - rw_normalangle;

                // calculate light table
                //  use different light tables
                //  for horizontal / vertical / diagonal
                // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
                if (!fixedcolormap)
                {
                        lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)+extralight;

                        if (curline->v1->y == curline->v2->y)
                                lightnum--;
                        else if (curline->v1->x == curline->v2->x)
                                lightnum++;

                        if (lightnum < 0)
                                walllights = scalelight[0];
                        else if (lightnum >= LIGHTLEVELS)
                                walllights = scalelight[LIGHTLEVELS-1];
                        else
                                walllights = scalelight[lightnum];
                }
    }

    // if a floor / ceiling plane is on the wrong side
    //  of the view plane, it is definitely invisible
    //  and doesn't need to be marked.

    //added:18-02-98: WATER! cacher ici dans certaines conditions?
    //                la surface eau est visible de dessous et dessus...

    if (frontsector->floorheight >= viewz)
    {
                // above view plane
                markfloor = false;
    }

    if (frontsector->ceilingheight <= viewz
                && frontsector->ceilingpic != skyflatnum)
    {
                // below view plane
                markceiling = false;
    }


    // calculate incremental stepping values for texture edges
    worldtop >>= 4;
    worldbottom >>= 4;

    topstep = -FixedMul (rw_scalestep, worldtop);
    topfrac = (centeryfrac>>4) - FixedMul (worldtop, rw_scale);

    //added:18-02-98:WATER!
    waterz >>= 4;
    if (markwater)
    {
                if (waterplane==NULL)
                        I_Error("fuck no waterplane!");
                waterstep = -FixedMul (rw_scalestep, waterz);
                waterfrac = (centeryfrac>>4) - FixedMul (waterz, rw_scale);
    }

    bottomstep = -FixedMul (rw_scalestep,worldbottom);
    bottomfrac = (centeryfrac>>4) - FixedMul (worldbottom, rw_scale);

    if (backsector)
    {
                worldhigh >>= 4;
                worldlow >>= 4;

                if (worldhigh < worldtop)
                {
                        pixhigh = (centeryfrac>>4) - FixedMul (worldhigh, rw_scale);
                        pixhighstep = -FixedMul (rw_scalestep,worldhigh);
                }

                if (worldlow > worldbottom)
                {
                        pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
                        pixlowstep = -FixedMul (rw_scalestep,worldlow);
                }
    }

    // get a new or use the same visplane
    if (markceiling)
                ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);

    // get a new or use the same visplane
    if (markfloor)
                floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);

    //added:18-02-98: il me faut un visplane pour l'eau...WATER!
    if (markwater)
    {
                if (waterplane==NULL)
                        I_Error("pas de waterplane avec markwater!?");
                else
                        waterplane = R_CheckPlane (waterplane, rw_x, rw_stopx-1);
    }

    // render it
        //added:24-02-98:WATER! unused now, trying something neater
        //if (markwater)
        //    colfunc = R_DrawWaterColumn;
        R_RenderSegLoop ();
        //colfunc = basecolfunc;

#ifdef WALLSPLATS
    if (linedef->splats)
        R_DrawWallSplats (ds_p);
#endif

    // save sprite clipping info
    if ( ((ds_p->silhouette & SIL_TOP) || maskedtexture)
                && !ds_p->sprtopclip)
    {
                memcpy (lastopening, ceilingclip+start, 2*(rw_stopx-start));
                ds_p->sprtopclip = lastopening - start;
                lastopening += rw_stopx - start;
    }

    if ( ((ds_p->silhouette & SIL_BOTTOM) || maskedtexture)
                && !ds_p->sprbottomclip)
    {
                memcpy (lastopening, floorclip+start, 2*(rw_stopx-start));
                ds_p->sprbottomclip = lastopening - start;
                lastopening += rw_stopx - start;
    }

    if (maskedtexture && !(ds_p->silhouette&SIL_TOP))
    {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = MININT;
    }
    if (maskedtexture && !(ds_p->silhouette&SIL_BOTTOM))
    {
                ds_p->silhouette |= SIL_BOTTOM;
                ds_p->bsilheight = MAXINT;
    }
    ds_p++;
}
