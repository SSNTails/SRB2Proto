// r_plane.c :
//      Here is a core component: drawing the floors and ceilings,
//       while maintaining a per column clipping list only.
//      Moreover, the sky areas have to be determined.

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "r_data.h"
#include "r_local.h"
#include "r_state.h"
#include "r_splats.h"   //faB(21jan):testing
#include "r_sky.h"
#include "v_video.h"
#include "vid_copy.h"
#include "w_wad.h"
#include "z_zone.h"

#include "p_setup.h"    // levelflats

planefunction_t         floorfunc;
planefunction_t         ceilingfunc;

//
// opening
//

// Here comes the obnoxious "visplane".
#define MAXVISPLANES    512
visplane_t              visplanes[MAXVISPLANES];
visplane_t*             lastvisplane;
visplane_t*             floorplane;
visplane_t*             ceilingplane;
visplane_t*             waterplane;   //added:18-02-98:WATER!

// ?
#define MAXOPENINGS     MAXVIDWIDTH*64
short                   openings[MAXOPENINGS];
short*                  lastopening;


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short                   floorclip[MAXVIDWIDTH];
short                   ceilingclip[MAXVIDWIDTH];
short                   waterclip[MAXVIDWIDTH];   //added:18-02-98:WATER!

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int                     spanstart[MAXVIDHEIGHT];
//int                     spanstop[MAXVIDHEIGHT]; //added:08-02-98: Unused!!

//
// texture mapping
//
lighttable_t**          planezlight;
fixed_t                 planeheight;

//added:10-02-98: yslopetab is what yslope used to be,
//                yslope points somewhere into yslopetab,
//                now (viewheight/2) slopes are calculated above and
//                below the original viewheight for mouselook
//                (this is to calculate yslopes only when really needed)
//                (when mouselookin', yslope is moving into yslopetab)
//                Check R_SetupFrame, R_SetViewSize for more...
fixed_t                 yslopetab[MAXVIDHEIGHT*4];
fixed_t*                yslope;

fixed_t                 distscale[MAXVIDWIDTH];
fixed_t                 basexscale;
fixed_t                 baseyscale;

fixed_t                 cachedheight[MAXVIDHEIGHT];
fixed_t                 cacheddistance[MAXVIDHEIGHT];
fixed_t                 cachedxstep[MAXVIDHEIGHT];
fixed_t                 cachedystep[MAXVIDHEIGHT];


boolean itswater;       //added:24-02-98:WATER!

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
  // Doh!
}


//profile stuff ---------------------------------------------------------
//#define TIMING
#ifdef TIMING
#include "p5prof.h"
         long long mycount;
         long long mytotal = 0;
         unsigned long  nombre = 100000;
//static   char runtest[10][80];
#endif
//profile stuff ---------------------------------------------------------


//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
static int bgofs;
static int wtofs=0;

void R_MapPlane
( int           y,              // t1
  int           x1,
  int           x2 )
{
    angle_t     angle;
    fixed_t     distance;
    fixed_t     length;
    unsigned    index;
    int         fuck;


#ifdef RANGECHECK
    if (x2 < x1
        || x1<0
        || x2>=viewwidth
        || (unsigned)y>viewheight)
    {
        I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
    }
#endif

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

    if (itswater)
    {
        //ripples da water texture
        fuck = (wtofs + (distance>>10) ) & 8191;
        bgofs = FixedDiv(finesine[fuck],distance>>9)>>16;

        angle = (angle + 2048) & 8191;  //90ø
        ds_xfrac += FixedMul(finecosine[angle], (bgofs<<FRACBITS));
        ds_yfrac += FixedMul(finesine[angle], (bgofs<<FRACBITS));

        if (y+bgofs>=viewheight)
            bgofs = viewheight-y-1;
        if (y+bgofs<0)
            bgofs = -y;
    }

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
    // high or low detail

//added:16-01-98:profile hspans drawer.
#ifdef TIMING
  ProfZeroTimer();
#endif
    spanfunc ();
#ifdef TIMING
  RDMSR(0x10,&mycount);
  mytotal += mycount;   //64bit add
  if(nombre--==0)
  I_Error("spanfunc() CPU Spy reports: 0x%d %d\n", *((int*)&mytotal+1),
                                        (int)mytotal );
#endif

}

extern fixed_t waterheight;     //added:18-02-98:WATER!


//
// R_ClearPlanes
// At begining of frame.
//
//Fab:26-04-98:
// NOTE : uses con_clipviewtop, so that when console is on,
//        don't draw the part of the view hidden under the console
void R_ClearPlanes (player_t *player)
{
    int         i;
    angle_t     angle;

    int         waterz;

    // opening / clipping determination
    for (i=0 ; i<viewwidth ; i++)
    {
        floorclip[i] = viewheight;
        ceilingclip[i] = con_clipviewtop;       //Fab:26-04-98: was -1
    }

    //added:18-02-98:WATER! clear the waterclip
if(player->mo->subsector->sector->watersector)
      waterz = player->mo->subsector->sector->watersector->floorheight;
    else
        waterz = MININT;

    if (viewz>waterz)
    {
        for (i=0; i<viewwidth; i++)
            waterclip[i] = viewheight;
    }
    else
    {
        for (i=0; i<viewwidth; i++)
            waterclip[i] = -1;
    }

    lastvisplane = visplanes;
    lastopening = openings;

    // texture calculation
    memset (cachedheight, 0, sizeof(cachedheight));

    // left to right mapping
    angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;

    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedDiv (finecosine[angle],centerxfrac);
    baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}




//
// R_FindPlane : cherche un visplane ayant les valeurs identiques:
//               meme hauteur, meme flattexture, meme lightlevel.
//               Sinon en alloue un autre.
//
visplane_t* R_FindPlane( fixed_t height,
                         int     picnum,
                         int     lightlevel )
{
static int maxusedvisplanes=0;
    visplane_t* check;

    if (picnum == skyflatnum)
    {
        height = 0;                     // all skys map together
        lightlevel = 0;
    }

    for (check=visplanes; check<lastvisplane; check++)
    {
        if (height == check->height
            && picnum == check->picnum
            && lightlevel == check->lightlevel)
        {
            break;
        }
    }


    if (check < lastvisplane)
        return check;

    if (lastvisplane - visplanes == MAXVISPLANES)
        I_Error ("R_FindPlane: no more visplanes");

    if (devparm &&
        (lastvisplane-visplanes) > maxusedvisplanes)
    {
        maxusedvisplanes = lastvisplane - visplanes;
        CONS_Printf ("maxvisplane: %d\n", maxusedvisplanes);
    }


    lastvisplane++;

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = vid.width;
    check->maxx = -1;

    memset (check->top,0xff,sizeof(check->top));

    return check;
}


//
// R_CheckPlane : retourne le meme visplane, ou en alloue un autre
//                si necessaire
//
visplane_t*  R_CheckPlane( visplane_t*   pl,
                           int           start,
                           int           stop )
{
    int         intrl;
    int         intrh;
    int         unionl;
    int         unionh;
    int         x;

    if (start < pl->minx)
    {
        intrl = pl->minx;
        unionl = start;
    }
    else
    {
        unionl = pl->minx;
        intrl = start;
    }

    if (stop > pl->maxx)
    {
        intrh = pl->maxx;
        unionh = stop;
    }
    else
    {
        unionh = pl->maxx;
        intrh = stop;
    }

    //added 30-12-97 : 0xff ne vaut plus -1 avec un short...
    for (x=intrl ; x<= intrh ; x++)
        if (pl->top[x] != 0xffff)
            break;

    if (x > intrh)
    {
        pl->minx = unionl;
        pl->maxx = unionh;

        // use the same one
        return pl;
    }

    // make a new visplane
    lastvisplane->height = pl->height;
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;

    pl = lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;

    memset (pl->top,0xff,sizeof(pl->top));

    return pl;
}


//
// R_MakeSpans
//
void R_MakeSpans
( int           x,
  int           t1,
  int           b1,
  int           t2,
  int           b2 )
{
    while (t1 < t2 && t1<=b1)
    {
        R_MapPlane (t1,spanstart[t1],x-1);
        t1++;
    }
    while (b1 > b2 && b1>=t1)
    {
        R_MapPlane (b1,spanstart[b1],x-1);
        b1--;
    }

    while (t2 < t1 && t2<=b2)
    {
        spanstart[t2] = x;
        t2++;
    }
    while (b2 > b1 && b2>=t2)
    {
        spanstart[b2] = x;
        b2--;
    }
}


extern byte*    ylookup[];
extern int      columnofs[];

//extern byte*    transtables;

static int waterofs;

// la texture flat anim‚e de l'eau contient en fait des index
// de colormaps , plutot qu'une transparence, il s'agit d'ombrer et
// d'eclaircir pour donner l'effet de bosses de l'eau.
#ifdef couille
void R_DrawWaterSpan (void)
{
    fixed_t             xfrac;
    fixed_t             yfrac;
    byte*               dest;
    int                 count;
    int                 spot;

    byte*               brighten = transtables+(84<<8);
    //byte*               brighten = colormaps;

//#ifdef RANGECHECK
    if (ds_x2 < ds_x1
        || ds_x1<0
        || ds_x2>=vid.width
        || (unsigned)ds_y>=vid.height)
    {
        I_Error( "R_DrawWaterSpan: %i to %i at %i",
                 ds_x1,ds_x2,ds_y);
    }
//      dscount++;
//#endif


    xfrac = ds_xfrac;
    yfrac = (ds_yfrac + waterofs) & 0x3fffff;

    dest = ylookup[ds_y] + columnofs[ds_x1];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

// *dest++ = 192;
// --count;
    do //while(count--)
    {
        // Current texture index in u,v.
        spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        *dest++ = *( brighten + (ds_source[spot]<<8) + (*dest) );
        // Next step in u,v.
        xfrac += ds_xstep;
        yfrac += ds_ystep;

    } while(count--); //
// if (count==-1)
//     *dest = 200;
}
#endif

void R_DrawWaterSpan_8 (void)
{
    fixed_t             xfrac;
    fixed_t             yfrac;
    byte*               dest;
    byte*               dsrc;
    int                 count;
    int                 spot;

    byte*               brighten = transtables+(84<<8);
    //byte*               brighten = colormaps-(8*256);

//#ifdef RANGECHECK
    if (ds_x2 < ds_x1
        || ds_x1<0
        || ds_x2>=vid.width
        || ds_y>=vid.height)
    {
        I_Error( "R_DrawWaterSpan: %i to %i at %i",
                 ds_x1,ds_x2,ds_y);
    }
//      dscount++;
//#endif

    xfrac = ds_xfrac;
    yfrac = (ds_yfrac + waterofs) & 0x3fffff;

    // methode a : le fond est d‚form‚
    dest = ylookup[ds_y] + columnofs[ds_x1];
    dsrc = screens[2] + ((ds_y+bgofs)*vid.width) + columnofs[ds_x1];

    // m‚thode b : la surface est d‚form‚e !
    //dest = ylookup[ds_y+bgofs] + columnofs[ds_x1];
    //dsrc = screens[2] + (ds_y*vid.width) + columnofs[ds_x1];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

// *dest++ = 192;
// --count;

    /* debug show 0 offset lines
    if (ds_y==bgofs)
    {
      do
      {
        *dest++ = 247;
      } while(count--); //
      return;
    }*/

    do //while(count--)
    {
        // Current texture index in u,v.
        spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        *dest++ = *( brighten + (ds_source[spot]<<8) + (*dsrc++) );
        // Next step in u,v.
        xfrac += ds_xstep;
        yfrac += ds_ystep;

    } while(count--); //
// if (count==-1)
//     *dest = 200;
}



static int wateranim;

byte* R_GetFlat (int  flatnum);

void R_DrawPlanes (void)
{
    visplane_t*         pl;
    int                 light;
    int                 x;
    int                 stop;
    int                 angle;

    //added:18-02-98:WATER!
    boolean             watertodraw;

#ifdef RANGECHECK
    //faB: ugly realloc makes this test useless
    //if (ds_p - drawsegs > MAXDRAWSEGS)
    //    I_Error ("R_DrawPlanes: drawsegs overflow (%i)",
    //             ds_p - drawsegs);

    if (lastvisplane - visplanes > MAXVISPLANES)
        I_Error ("R_DrawPlanes: visplane overflow (%i)",
                 lastvisplane - visplanes);

    if (lastopening - openings > MAXOPENINGS)
        I_Error ("R_DrawPlanes: opening overflow (%i)",
                 lastopening - openings);
#endif

    //
    // DRAW NON-WATER VISPLANES FIRST
    //
    watertodraw = false;

    spanfunc = basespanfunc;

    itswater = false;
    for (pl = visplanes ; pl < lastvisplane ; pl++)
    {
        if (pl->minx > pl->maxx)
            continue;

        if (pl->picnum==1998)   //dont draw water visplanes now.
        {
            watertodraw = true;
            continue;
        }

        // sky flat
        if (pl->picnum == skyflatnum)
        {
            //added:12-02-98: use correct aspect ratio scale
            //dc_iscale = FixedDiv (FRACUNIT, pspriteyscale);
            dc_iscale = skyscale;

// Kik test non-moving sky .. weird
// cy = centery;
// centery = (viewheight/2);

            // Sky is allways drawn full bright,
            //  i.e. colormaps[0] is used.
            // Because of this hack, sky is not affected
            //  by INVUL inverse mapping.
            dc_colormap = colormaps;
            dc_texturemid = skytexturemid;
            for (x=pl->minx ; x <= pl->maxx ; x++)
            {
                dc_yl = pl->top[x];
                dc_yh = pl->bottom[x];

                if (dc_yl <= dc_yh)
                {
                    angle = (viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
                    dc_x = x;
                    dc_source = R_GetColumn(skytexture, angle);
                    skycolfunc ();
                }
            }
// centery = cy;
            continue;
        }

        ds_source = //(byte *)W_CacheLumpNum(levelflats[pl->picnum].lumpnum,PU_LEVEL);
                    (byte *) R_GetFlat (levelflats[pl->picnum].lumpnum);

        planeheight = abs(pl->height-viewz);

        light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

        if (light >= LIGHTLEVELS)
            light = LIGHTLEVELS-1;

        if (light < 0)
            light = 0;

        planezlight = zlight[light];


        //set the MAXIMUM value for unsigned
        pl->top[pl->maxx+1] = 0xffff;
        pl->top[pl->minx-1] = 0xffff;

        stop = pl->maxx + 1;

        for (x=pl->minx ; x<= stop ; x++)
        {
            R_MakeSpans(x,pl->top[x-1],
                        pl->bottom[x-1],
                        pl->top[x],
                        pl->bottom[x]);
        }

        //Z_ChangeTag (ds_source, PU_CACHE);


    }

    //
    // DRAW WATER VISPLANES AFTER
    //

    R_DrawSprites ();   //draw sprites before water. just a damn hack


 //added:24-02-98: SALE GROS HACK POURRI
 if (!watertodraw)
     goto skipwaterdraw;

    VID_BlitLinearScreen ( screens[0], screens[2],
                           vid.width, vid.height,
                           vid.width, vid.width );

    spanfunc = R_DrawWaterSpan_8;
    itswater = true;
    // always the same flat!!!
    ds_source = W_CacheLumpNum(firstwaterflat + ((wateranim>>3)&7), PU_STATIC);

    for (pl = visplanes ; pl < lastvisplane ; pl++)
    {
        if (pl->minx > pl->maxx)
            continue;

        if (pl->picnum!=1998)
            continue;

        planeheight = abs(pl->height-viewz);

        light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

        if (light >= LIGHTLEVELS)
            light = LIGHTLEVELS-1;

        if (light < 0)
            light = 0;

        // base light from sector light
        planezlight = zlight[light];

        //set the MAXIMUM value for unsigned
        pl->top[pl->maxx+1] = 0xffff;
        pl->top[pl->minx-1] = 0xffff;

        stop = pl->maxx + 1;

        for (x=pl->minx ; x<= stop ; x++)
        {
            R_MakeSpans(x,pl->top[x-1],
                        pl->bottom[x-1],
                        pl->top[x],
                        pl->bottom[x]);
        }

    }
    Z_ChangeTag (ds_source, PU_CACHE);
    itswater = false;
    spanfunc = basespanfunc;

skipwaterdraw:

    waterofs += (1<<14);
    wateranim++;
    wtofs += 75;
    //if (!wateranim)
    //    waterofs -= (32<<16);
}
