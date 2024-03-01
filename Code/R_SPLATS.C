// r_splats.c : floor and wall splats
//              by faB/Legacy

#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_splats.h"
#include "w_wad.h"
#include "z_zone.h"

#ifdef WALLSPLATS
static wallsplat_t  wallsplats[MAXLEVELSPLATS];     // WALL splats
static int          freewallsplat;
#endif

#ifdef FLOORSPLATS
static floorsplat_t floorsplats[MAXLEVELSPLATS];    // FLOOR splats
static int          freefloorsplat;

struct rastery_s {
    fixed_t minx, maxx;     // for each raster line starting at line 0
    fixed_t tx1,ty1;
    fixed_t tx2,ty2;        // start/end points in texture at this line
};
static struct rastery_s rastertab[MAXVIDHEIGHT];

static void prepare_rastertab (void);

//r_plane.c
extern fixed_t                 cachedheight[MAXVIDHEIGHT];
extern fixed_t                 cacheddistance[MAXVIDHEIGHT];
extern fixed_t                 cachedxstep[MAXVIDHEIGHT];
extern fixed_t                 cachedystep[MAXVIDHEIGHT];
extern fixed_t                 basexscale;
extern fixed_t                 baseyscale;
#endif
// for floorsplats, accessed by asm code
struct rastery_s * prastertab;



// --------------------------------------------------------------------------
// setup splat cache
// --------------------------------------------------------------------------
void R_ClearLevelSplats (void)
{
#ifdef WALLSPLATS
    freewallsplat = 0;
    memset (wallsplats, 0, sizeof(wallsplats));
#endif
#ifdef FLOORSPLATS
    freefloorsplat = 0;
    memset (floorsplats, 0, sizeof(floorsplats));

    //setup to draw floorsplats
    prastertab = rastertab;
    prepare_rastertab ();
#endif
}


// ==========================================================================
//                                                                WALL SPLATS
// ==========================================================================
#ifdef WALLSPLATS
// --------------------------------------------------------------------------
// Return a pointer to a splat free for use, or NULL if no more splats are
// available
// --------------------------------------------------------------------------
static wallsplat_t* R_AllocWallSplat (void)
{
    wallsplat_t* splat;
    wallsplat_t* p_splat;
    line_t*  li;

    // find splat to use
    freewallsplat++;
    if (freewallsplat >= MAXLEVELSPLATS)
        freewallsplat = 0;

    // clear the splat from the line if it was in use
    splat = &wallsplats[freewallsplat];
    if ( (li=splat->line) )
    {
        // remove splat from line splats list
        if (li->splats == splat)
            li->splats = splat->next;   //remove from head
        else
        {
            p_splat = li->splats;
            while (p_splat->next)
            {
                if (p_splat->next == splat)
                    p_splat->next = splat->next;
            }
        }
    }

    memset (splat, 0, sizeof(wallsplat_t));
    return splat;
}


// Add a new splat to the linedef:
// top : top z coord
// wallfrac : frac along the linedef vector (0 to FRACUNIT)
// splatpatchname : name of patch to draw
void R_AddWallSplat (line_t*    wallline,
                     char*      patchname,
                     fixed_t    top,
                     fixed_t    wallfrac,
                     int        flags)
{
    fixed_t         fracsplat;
    wallsplat_t*    splat;
    wallsplat_t*    p_splat;
    patch_t*        patch;
    fixed_t         linelength;

    if( demoversion<128 )
        return;

    splat = R_AllocWallSplat ();
    if (!splat)
        return;

    // set the splat
    splat->patch = W_GetNumForName (patchname);
    splat->top = top;
    splat->flags = flags;

    // bad.. but will be needed for drawing anyway..
    patch = W_CachePatchNum (splat->patch, PU_CACHE);

    linelength = P_SegLength((seg_t*)wallline);
    splat->offset = FixedMul(wallfrac, linelength) - (patch->width<<FRACBITS)/2; // needed by draw code
    fracsplat = FixedDiv( ((patch->width<<FRACBITS)>>1) , linelength );
    
    wallfrac -= fracsplat;
    splat->v1.x = wallline->v1->x + FixedMul (wallline->dx, wallfrac);
    splat->v1.y = wallline->v1->y + FixedMul (wallline->dy, wallfrac);
    wallfrac += fracsplat + fracsplat;
    splat->v2.x = wallline->v1->x + FixedMul (wallline->dx, wallfrac);
    splat->v2.y = wallline->v1->y + FixedMul (wallline->dy, wallfrac);

    // insert splat in the linedef splat list
    splat->line = wallline;
    splat->next = NULL;
    if (wallline->splats)
    {
        p_splat = wallline->splats;
        while (p_splat->next)
            p_splat = p_splat->next;
        p_splat->next = splat;
    }
    else
        wallline->splats = splat;
}
#endif // WALLSPLATS


// ==========================================================================
//                                                               FLOOR SPLATS
// ==========================================================================
#ifdef FLOORSPLATS

// --------------------------------------------------------------------------
// Return a pointer to a splat free for use, or NULL if no more splats are
// available
// --------------------------------------------------------------------------
static floorsplat_t* R_AllocFloorSplat (void)
{
    floorsplat_t* splat;
    floorsplat_t* p_splat;
    subsector_t*  sub;

    // find splat to use
    freefloorsplat++;
    if (freefloorsplat >= MAXLEVELSPLATS)
        freefloorsplat = 0;

    // clear the splat from the line if it was in use
    splat = &floorsplats[freefloorsplat];
    if ( (sub=splat->subsector) )
    {
        // remove splat from subsector splats list
        if (sub->splats == splat)
            sub->splats = splat->next;   //remove from head
        else
        {
            p_splat = sub->splats;
            while (p_splat->next)
            {
                if (p_splat->next == splat)
                    p_splat->next = splat->next;
            }
        }
    }

    memset (splat, 0, sizeof(floorsplat_t));
    return splat;
}


// --------------------------------------------------------------------------
// Add a floor splat to the subsector
// --------------------------------------------------------------------------
void R_AddFloorSplat (subsector_t* subsec, char* picname, fixed_t x, fixed_t y, fixed_t z, int flags)
{
    floorsplat_t*    splat;
    floorsplat_t*    p_splat;

    splat = R_AllocFloorSplat ();
    if (!splat)
        return;

    CONS_Printf ("added a floor splat\n");

    // set the splat
    splat->pic = W_GetNumForName (picname);

    splat->flags = flags;
    
    //

    //for test fix 64x64
    // 3--2
    // |  |
    // 0--1
    //
    splat->z = z;
    splat->verts[0].x = splat->verts[3].x = x - (32<<FRACBITS);
    splat->verts[2].x = splat->verts[1].x = x + (31<<FRACBITS);
    splat->verts[3].y = splat->verts[2].y = y + (31<<FRACBITS);
    splat->verts[0].y = splat->verts[1].y = y - (32<<FRACBITS);

    // insert splat in the subsector splat list
    splat->subsector = subsec;
    splat->next = NULL;
    if (subsec->splats)
    {
        p_splat = subsec->splats;
        while (p_splat->next)
            p_splat = p_splat->next;
        p_splat->next = splat;
    }
    else
        subsec->splats = splat;
}


// --------------------------------------------------------------------------
// Before each frame being rendered, clear the visible floorsplats list
// --------------------------------------------------------------------------
static floorsplat_t*   visfloorsplats;

void R_ClearVisibleFloorSplats (void)
{
    visfloorsplats = NULL;
}


// --------------------------------------------------------------------------
// Add a floorsplat to the visible floorsplats list, for the current frame
// --------------------------------------------------------------------------
void R_AddVisibleFloorSplats (subsector_t* subsec)
{
    floorsplat_t* pSplat;
#ifdef PARANOIA
    if (subsec->splats==NULL)
        I_Error ("R_AddVisibleFloorSplats: call with no splats");
#endif

    pSplat = subsec->splats;
    // the splat is not visible from below
    // FIXME: depending on some flag in pSplat->flags, some splats may be visible from 2 sides (above/below)
    if (pSplat->z < viewz) {
        pSplat->nextvis = visfloorsplats;
        visfloorsplats = pSplat;
    }

    while (pSplat->next)
    {
        pSplat = pSplat->next;
        if (pSplat->z < viewz) {
            pSplat->nextvis = visfloorsplats;
            visfloorsplats = pSplat;
        }
    }
}


// tv1,tv2 = x/y qui varie dans la texture, tc = x/y qui est constant.
void    ASMCALL rasterize_segment_tex (int x1, int y1, int x2, int y2, int tv1, int tv2, int tc, int dir);

// current test with floor tile
#define TEXWIDTH 64
#define TEXHEIGHT 64
//#define FLOORSPLATSOLIDCOLOR

// --------------------------------------------------------------------------
// Rasterize the four edges of a floor splat polygon,
// fill the polygon with linear interpolation, call span drawer for each
// scan line
// --------------------------------------------------------------------------
static void R_RenderFloorSplat (floorsplat_t* pSplat, vertex_t* verts, byte* pTex)
{
    // resterizing
    int     miny = vid.height + 1;
        int     maxy = 0;
        int     x, y;
    int     x1, y1, x2, y2;
    byte*   pDest;
        int     tx, ty, tdx, tdy;

    // rendering
    lighttable_t**  planezlight;
    fixed_t         planeheight;
    angle_t     angle;
    fixed_t     distance;
    fixed_t     length;
    unsigned    index;
    int         light;

    fixed_t     offsetx,offsety;

    offsetx = pSplat->verts[0].x & 0x3fffff;
    offsety = pSplat->verts[0].y & 0x3fffff;

        // do for each segment, starting with the first one
    /*CONS_Printf ("floor splat (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n",
                  verts[3].x,verts[3].y,verts[2].x,verts[2].y,
                  verts[1].x,verts[1].y,verts[0].x,verts[0].y);*/

    // do segment a -> top of texture
            x1 = verts[3].x;
            y1 = verts[3].y;
            x2 = verts[2].x;
            y2 = verts[2].y;
    if (y1<0) y1=0;
    if (y1>=vid.height) y1 = vid.height-1;
    if (y2<0) y2=0;
    if (y2>=vid.height) y2 = vid.height-1;
        rasterize_segment_tex (x1, y1, x2, y2, 0, TEXWIDTH-1, 0, 0);
            if( y1 < miny )
                    miny = y1;
            if( y1 > maxy )
                    maxy = y1;

    // do segment b -> right side of texture
                x1 = x2;
                y1 = y2;
                x2 = verts[1].x;
                y2 = verts[1].y;
    if (y1<0) y1=0;
    if (y1>=vid.height) y1 = vid.height-1;
    if (y2<0) y2=0;
    if (y2>=vid.height) y2 = vid.height-1;
                rasterize_segment_tex (x1, y1, x2, y2, 0, TEXHEIGHT-1, TEXWIDTH-1, 1);
                if( y1 < miny )
                    miny = y1;
                if( y1 > maxy )
                    maxy = y1;
        
    // do segment c -> bottom of texture
                x1 = x2;
                y1 = y2;
                x2 = verts[0].x;
                y2 = verts[0].y;
    if (y1<0) y1=0;
    if (y1>=vid.height) y1 = vid.height-1;
    if (y2<0) y2=0;
    if (y2>=vid.height) y2 = vid.height-1;
                rasterize_segment_tex (x1, y1, x2, y2, TEXWIDTH-1, 0, TEXHEIGHT-1, 0);
                if( y1 < miny )
                    miny = y1;
                if( y1 > maxy )
                    maxy = y1;
        
    // do segment d -> left side of texture
                x1 = x2;
                y1 = y2;
                x2 = verts[3].x;
                y2 = verts[3].y;
    if (y1<0) y1=0;
    if (y1>=vid.height) y1 = vid.height-1;
    if (y2<0) y2=0;
    if (y2>=vid.height) y2 = vid.height-1;
                rasterize_segment_tex (x1, y1, x2, y2, TEXHEIGHT-1, 0, 0, 1);
                if( y1 < miny )
                    miny = y1;
                if( y1 > maxy )
                    maxy = y1;

        // remplissage du polygone a 4 cotes AVEC UNE TEXTURE
        //fill_texture_linear( trashbmp, tex->imgdata, miny, maxy );
        //return;

#ifndef FLOORSPLATSOLIDCOLOR

    // prepare values for all the splat
    ds_source = (byte *)W_CacheLumpNum(pSplat->pic,PU_CACHE);
    planeheight = abs(pSplat->z - viewz);
    light = (pSplat->subsector->sector->lightlevel >> LIGHTSEGSHIFT)+extralight;
    if (light >= LIGHTLEVELS)
        light = LIGHTLEVELS-1;
    if (light < 0)
        light = 0;
    planezlight = zlight[light];

    for (y=miny; y<=maxy; y++)
    {
            x1 = rastertab[y].minx >> FRACBITS;
            x2 = rastertab[y].maxx >> FRACBITS;

        if (x1<0) x1 = 0;
        if (x2>=vid.width) x2 = vid.width-1;

        if (planeheight != cachedheight[y])
        {
            cachedheight[y] = planeheight;
            distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);
            ds_xstep = cachedxstep[y] = FixedMul (distance,basexscale);
            ds_ystep = cachedystep[y] = FixedMul (distance,baseyscale);
        }
        else
        {
            distance = cacheddistance[y];
            ds_xstep = cachedxstep[y];
            ds_ystep = cachedystep[y];
        }
        length = FixedMul (distance,distscale[x1]);
        angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;
        ds_xfrac = viewx + FixedMul(finecosine[angle], length);
        ds_yfrac = -viewy - FixedMul(finesine[angle], length);
        ds_xfrac -= offsetx;
        ds_yfrac += offsety;

        if (fixedcolormap)
            ds_colormap = fixedcolormap;
        else
        {
            index = distance >> LIGHTZSHIFT;
            if (index >= MAXLIGHTZ )
                index = MAXLIGHTZ-1;
            ds_colormap = planezlight[index];
        }

        ds_y = y;
        ds_x1 = x1;
        ds_x2 = x2;
        spanfunc ();

        // reset for next calls to edge rasterizer
            rastertab[y].minx = MAXINT;
            rastertab[y].maxx = MININT;
        }

#else

    for (y=miny; y<=maxy; y++)
    {
        x1 = rastertab[y].minx >> FRACBITS;
        x2 = rastertab[y].maxx >> FRACBITS;
        /*if ((unsigned)x1 >= vid.width)
            continue;
        if ((unsigned)x2 >= vid.width)
            continue;*/
        if (x1<0) x1 = 0;
        //if (x1>=vid.width) x1 = vid.width-1;
        //if (x2<0) x1 = 0;
        if (x2>=vid.width)
            x2 = vid.width-1;

        pDest = ylookup[y] + columnofs[x1];

        x = (x2-x1) + 1;

        //point de d‚part dans la texture
        tx = rastertab[y].tx1;
        ty = rastertab[y].ty1;

        // HORRIBLE BUG!!!
        if(x>0)
        {
             tdx = (rastertab[y].tx2 - tx) / x;
             tdy = (rastertab[y].ty2 - ty) / x;

             while (x-- > 0)
             {
                 *(pDest++) = (y&255);
                 tx += tdx;
                 ty += tdy;
             }
        }

        // r‚initialise les minimus maximus pour le prochain appel
        rastertab[y].minx = MAXINT;
        rastertab[y].maxx = MININT;
    }
#endif
}


// --------------------------------------------------------------------------
// R_DrawFloorSplats
// draw the flat floor/ceiling splats
// --------------------------------------------------------------------------
void R_DrawVisibleFloorSplats (void)
{
    floorsplat_t* pSplat;
    int           iCount = 0;
    
    fixed_t       tr_x;
    fixed_t       tr_y;
    fixed_t       rot_x;
    fixed_t       rot_y;
    fixed_t       rot_z;
    fixed_t       xscale;
    fixed_t       yscale;
    vertex_t*     v3d;
    vertex_t      v2d[4];
    int           i;

    pSplat = visfloorsplats;
    while (pSplat)
    {
        iCount++;

        // Draw a floor splat
        // 3--2
        // |  |
        // 0--1
        
        rot_z = pSplat->z - viewz;
        for (i=0; i<4; i++)
        {
            v3d = &pSplat->verts[i];
            
            // transform the origin point
            tr_x = v3d->x - viewx;
            tr_y = v3d->y - viewy;

            // rotation around vertical y axis
            rot_x = FixedMul(tr_x,viewsin) - FixedMul(tr_y,viewcos);
            rot_y = FixedMul(tr_x,viewcos) + FixedMul(tr_y,viewsin);

            if (rot_y < 4*FRACUNIT)
                goto skipit;

            // note: y from view above of map, is distance far away
            xscale = FixedDiv(projection, rot_y);
            yscale = -FixedDiv(projectiony, rot_y);

            // projection
            v2d[i].x = (centerxfrac + FixedMul (rot_x, xscale)) >>FRACBITS;
            v2d[i].y = (centeryfrac + FixedMul (rot_z, yscale)) >>FRACBITS;
        }
        /*
        pSplat->verts[3].x = 100 + iCount;
        pSplat->verts[3].y = 10 + iCount;
        pSplat->verts[2].x = 160 + iCount;
        pSplat->verts[2].y = 80 + iCount;
        pSplat->verts[1].x = 100 + iCount;
        pSplat->verts[1].y = 150 + iCount;
        pSplat->verts[0].x = 8 + iCount;
        pSplat->verts[0].y = 90 + iCount;
        */
        R_RenderFloorSplat (pSplat, v2d, NULL);
skipit:
        pSplat = pSplat->nextvis;
    }
    
    CONS_Printf ("%d floor splats in view\n", iCount);
}


static void prepare_rastertab (void)
{
        int iLine;
        for (iLine=0; iLine<vid.height; iLine++)
    {
         rastertab[iLine].minx = MAXINT;
             rastertab[iLine].maxx = MININT;
        }
}


#endif // FLOORSPLATS
