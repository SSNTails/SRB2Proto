// hardware renderer, using the standard HardWareRender driver DLL for Doom Legacy

#include "../doomdef.h"
#include "../command.h"
#include "../g_game.h"
#include "../g_input.h"
#include "../keys.h"
#include "../r_data.h"
#include "../r_draw.h"
#include "../r_local.h"
#include "../r_sky.h"
#include "../r_state.h"
#include "../p_local.h"
#include "../p_setup.h"
#include "../m_misc.h"
#include "../st_stuff.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../am_map.h"

#include "../tables.h"   //angle_t finesince, finecosine..

#include "hwr_main.h"
#include "hwr_data.h"
#include "hwr_defs.h"


// ==========================================================================
// the hardware driver object
// ==========================================================================
struct hwdriver_s hwdriver;


// height of status bar scaled to current resolution
#define STAT_HEIGHT         (ST_HEIGHT*vid.height/BASEVIDHEIGHT)

// the original aspect ratio of Doom graphics isn't square
#define ORIGINAL_ASPECT     (320.0f/200.0f)


// ==========================================================================
//                                                                     PROTOS
// ==========================================================================
boolean HWR_CheckBBox (fixed_t* bspcoord);

GlideTexture_t* HWR_GetTexture (int tex);
static void    HWR_GetFlat (int flatlumpnum);

static void    HWR_FoggingOn (void);
static void    HWR_FoggingOff (void);

static void CV_grPlug_OnChange (void);

static void HWR_ClearMipmapCache (void);

static void HWR_AddSprites (sector_t* sec);
static void HWR_ProjectSprite (mobj_t* thing);
static void HWR_DrawPlayerSprites (void);


// ==========================================================================
//                                          3D ENGINE COMMANDS & CONSOLE VARS
// ==========================================================================

//consvar_t cv_grsoftwareview =   {"gr_softwareview","0", 0,CV_OnOff};
consvar_t cv_grrounddown =      {"gr_rounddown","0", 0,CV_OnOff};
consvar_t cv_grcrappymlook =    {"gr_crappymlook","0", 0,CV_OnOff};
consvar_t cv_grclipwalls =      {"gr_clipwalls","0", 0,CV_OnOff};
consvar_t cv_grsky =            {"gr_sky","1", 0,CV_OnOff};
consvar_t cv_grfog =            {"gr_fog","1", 0,CV_OnOff};
consvar_t cv_grfiltermode =     {"gr_filtermode","1", 0,CV_OnOff};
consvar_t cv_grplug =           {"gr_plug","0", CV_CALL,CV_OnOff,CV_grPlug_OnChange};
consvar_t cv_grzbuffer =        {"gr_zbuffer","1", 0,CV_OnOff};

//development variables for diverse uses
consvar_t cv_gralpha = {"gr_alpha","160", 0, CV_Unsigned};
consvar_t cv_grbeta  = {"gr_beta","0", 0, CV_Unsigned};
consvar_t cv_grgamma = {"gr_gamma","0", 0, CV_Unsigned};


// ==========================================================================
//                                                               VIEW GLOBALS
// ==========================================================================
// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW             2048

angle_t                 gr_clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
int                     gr_viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t                 gr_xtoviewangle[MAXVIDWIDTH+1];


// ==========================================================================
//                                                                    GLOBALS
// ==========================================================================

// uncomment to remove the plane rendering
#define DOPLANES
//#define DOWALLS

static  boolean     drawtextured = true;

// base values set at SetViewSize
float       gr_basecentery;
float       gr_baseviewwindowy;
float       gr_basewindowcentery;

float       gr_viewwidth;           // viewport clipping boundaries (screen coords)
float       gr_viewheight;
float       gr_centerx;
float       gr_centery;
float       gr_viewwindowx;         // top left corner of view window
float       gr_viewwindowy;
float       gr_windowcenterx;       // center of view window, for projection
float       gr_windowcentery;

float       gr_pspritexscale;
float       gr_pspriteyscale;

seg_t*          gr_curline;
side_t*         gr_sidedef;
line_t*         gr_linedef;
sector_t*       gr_frontsector;
sector_t*       gr_backsector;

int         gr_segtextured;
int             gr_toptexture;
int             gr_bottomtexture;
int             gr_midtexture;

// --------------------------------------------------------------------------
//                                              STUFF FOR THE PROJECTION CODE
// --------------------------------------------------------------------------
#define crapmul (1.0f / 65536.0f)

// duplicates of the main code, set after R_SetupFrame() passed them into sharedstruct,
// copied here for local use
static  fixed_t dup_viewx;
static  fixed_t dup_viewy;
static  fixed_t dup_viewz;
static  fixed_t dup_viewangle;

static  float   gr_viewx;
static  float   gr_viewy;
static  float   gr_viewz;
static  float   gr_viewsin;
static  float   gr_viewcos;
static  float   gr_viewludsin;  //look up down kik test
static  float   gr_viewludcos;

static  float   gr_spriteludsin;  //look up down kik test
static  float   gr_spriteludcos;

static  float   gr_projectionx;
static  float   gr_projectiony;

static  float   gr_scalefrustum;    //scale 90degree y axis frustum to viewheight


// --------------------------------------------------------------------------
// This is global data for planes rendering
// --------------------------------------------------------------------------
// a vertex of a Doom 'plane' polygon
typedef struct {
    float   x;
    float   y;
} polyvertex_t;

// a convex 'plane' polygon, clockwise order
typedef struct {
    int          numpts;
    polyvertex_t pts[0];
} poly_t;


// holds extra info for 3D render, for each subsector in subsectors[]
typedef struct {
    poly_t*     planepoly;  // the generated convex polygon
} extrasubsector_t;

static  extrasubsector_t*   extrasubsectors=NULL;

// newsubsectors are subsectors without segs, added for the plane polygons
#define NEWSUBSECTORS       50
static  int                 totsubsectors;
static  int                 addsubsector;

// back to front drawing of subsectors, hold subsector number of each subsector
// while walking the BSP tree, then draw the subsectors from last number stored to the first
//static  short*              gr_drawsubsectors;
//static  short*              gr_drawsubsector_p;


// ==========================================================================
//                                                   CLIPPING TO NEAR Z PLANE
// ==========================================================================
#define ZCLIP_PLANE     4.0f
static  int HWR_ClipZ (wallVert3D* inVerts, wallVert3D* clipVerts, int numpoints)
{
    int     nrClipVerts = 0;
    int     a;
    byte    curin,nextin;
    int     nextvert;
    float   curdot, nextdot;
    wallVert3D *pinvert, *poutvert;
    float   scale;

    nrClipVerts = 0;
    pinvert = inVerts;
    poutvert = clipVerts;
    
    curdot = pinvert->z;
    curin = (curdot >= ZCLIP_PLANE);
    
    for(a=0; a<numpoints; a++)
    {
        nextvert = a + 1;
        if (nextvert == numpoints)
            nextvert = 0;
        
        if (curin) {
            *poutvert++ = *pinvert;
            nrClipVerts++;
        }

        nextdot = inVerts[nextvert].z;
        nextin = (nextdot >= ZCLIP_PLANE);
        
        if ( curin != nextin )
        {
            if (curdot >= nextdot)
            {
                scale = (float)(ZCLIP_PLANE - curdot)/(nextdot - curdot);
                poutvert->x = pinvert->x + scale * (inVerts[nextvert].x - pinvert->x);
                poutvert->y = pinvert->y + scale * (inVerts[nextvert].y - pinvert->y);
                poutvert->z = pinvert->z + scale * (inVerts[nextvert].z - pinvert->z);
                poutvert->w = pinvert->w + scale * (inVerts[nextvert].w - pinvert->w);

                poutvert->s = pinvert->s + scale * (inVerts[nextvert].s - pinvert->s);
                poutvert->t = pinvert->t + scale * (inVerts[nextvert].t - pinvert->t);
            }
            else
            {
                scale = (float)(ZCLIP_PLANE - nextdot)/(curdot - nextdot);
                poutvert->x = inVerts[nextvert].x + scale * (pinvert->x - inVerts[nextvert].x);
                poutvert->y = inVerts[nextvert].y + scale * (pinvert->y - inVerts[nextvert].y);
                poutvert->z = inVerts[nextvert].z + scale * (pinvert->z - inVerts[nextvert].z);
                poutvert->w = inVerts[nextvert].w + scale * (pinvert->w - inVerts[nextvert].w);
                
                poutvert->s = inVerts[nextvert].s + scale * (pinvert->s - inVerts[nextvert].s);
                poutvert->t = inVerts[nextvert].t + scale * (pinvert->t - inVerts[nextvert].t);
            }
            poutvert++;
            nrClipVerts++;
        }
        
        curdot = nextdot;
        curin = nextin;
        pinvert++;
    }

    return nrClipVerts;
}


// ==========================================================================
//                                                   3D VIEW FRUSTUM CLIPPING
// ==========================================================================

// faB: I used the 'Liang-Barsky' clipping algorithm, simply because that's
//      the one I found into a book called 'Procedural Elements for Computer
//      Graphics'.

// --------------------------------------------------------------------------
// ClipT performs trivial rejection tests and finds the maximum of the lower
// set of parameter values and the minimum of the upper set of parameter
// values
// --------------------------------------------------------------------------
__inline boolean ClipT (float d, float q, float *tl, float *tu)
{
    boolean visible;
    float   t;

    visible = true;

    if ( (d == 0) && (q < 0) )       // line is outside and parallel to edge
        visible = false;
    else if ( d < 0 )                   // looking for upper limit
    {   
        t = q / d;
        if ( t > *tu )                   // check for trivially invisible
            visible = false;
        else if ( t > *tl )              // find the minimum of the maximum
            *tl = t;
    }
    else if ( d > 0 )                   // looking for the lower limit
    {                 
        t = q / d;
        if ( t < *tl )                   // check for trivially invisible
            visible = false;
        else if ( t < *tu )              // find the maximum of the minimum
            *tu = t;
    }

    return visible;
}


// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WARNIIIIIIIIIIIIIIIIING!!!!
//             used by planes too, so make sure its large enough,
//              see 'planeVerts'
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!dirty code!!!!!!!!!!!!!!!!!!
static  wallVert3D tempVerts[256];

// Liang-Barsky three-dimensional clipping algorithm,
//  assumes a 90degree left to right, and top to bottom fov

// --------------------------------------------------------------------------
// In:
//   nrInVerts: nr of coordinates passed into inVerts[]
//   inVerts :  3D coordinates of polygon, in order, to clip
// Out:
//   outVerts : 3D coordinates of CLIPPED polygon, in order
// Returns:
//   nr of coordinates returned into outVerts[]
// --------------------------------------------------------------------------

// FIXME: this is a shame, had to do left/right, top/bottom clipping in two
// passes, or else we have a problem clipping to corners of the frustum, in
// some cases a point is not generated.. until then..

int ClipToFrustum (wallVert3D *inVerts, wallVert3D *outVerts, int nrInVerts)
{
    float       tl,tu;
    float       deltax, deltay, deltaz;

    int         i;
    int         xClipVerts, nrClipVerts;
    wallVert3D  *pinvert;
    wallVert3D  *poutvert;
    wallVert3D  *nextvert;


    // first pass left/right clipping
    xClipVerts = 0;
    pinvert = inVerts;
    poutvert = tempVerts;

    for (i=0; i<nrInVerts; i++)
    {
        nextvert = &inVerts[ i + 1 ];
        if (nextvert == &inVerts[nrInVerts])
            nextvert = inVerts;
    
        tl = 0; tu = 1;
        deltax = nextvert->x - pinvert->x;
        deltaz = nextvert->z - pinvert->z;
        if ( ClipT( -deltax - deltaz, pinvert->x + pinvert->z, &tl, &tu ) ) {
            if ( ClipT( deltax - deltaz, pinvert->z - pinvert->x, &tl, &tu ) ) {
                deltay = nextvert->y - pinvert->y;
                // clipped start point
                if ( tl > 0 ) {
                    
                    poutvert->x = pinvert->x + tl * deltax;
                    poutvert->y = pinvert->y + tl * deltay;
                    poutvert->z = pinvert->z + tl * deltaz;

                    poutvert->w = pinvert->w + tl * (nextvert->w - pinvert->w);
                    poutvert->s = pinvert->s + tl * (nextvert->s - pinvert->s);
                    poutvert->t = pinvert->t + tl * (nextvert->t - pinvert->t);
                }
                else {
                    // copy as it is
                    *poutvert = *pinvert;
                }
                poutvert++;
                xClipVerts++;
                
                // clipped end point
                if ( tu < 1 ) {
                    poutvert->x = pinvert->x + tu * deltax;
                    poutvert->y = pinvert->y + tu * deltay;
                    poutvert->z = pinvert->z + tu * deltaz;
                
                    poutvert->w = pinvert->w + tu * (nextvert->w - pinvert->w);
                    poutvert->s = pinvert->s + tu * (nextvert->s - pinvert->s);
                    poutvert->t = pinvert->t + tu * (nextvert->t - pinvert->t);
                    
                    poutvert++;
                    xClipVerts++;
                }
            }
        }
        pinvert++;
    }

    // SECOND pass top/bottom clipping
    pinvert = tempVerts;
    poutvert = outVerts;
    nrClipVerts = 0;

    for (i=0; i<xClipVerts; i++)
    {
        nextvert = &tempVerts[i+1];
        if (nextvert == &tempVerts[xClipVerts])
            nextvert = tempVerts;
    
        tl = 0; tu = 1;
        deltaz = nextvert->z - pinvert->z;
        deltay = nextvert->y - pinvert->y;
        if ( ClipT( -deltay - deltaz, pinvert->y + pinvert->z, &tl, &tu ) ) {
            if ( ClipT( deltay - deltaz, pinvert->z - pinvert->y, &tl, &tu ) ) {
                deltax = nextvert->x - pinvert->x;
                // clipped start point
                if ( tl > 0 ) {
                    poutvert->x = pinvert->x + tl * deltax;
                    poutvert->y = pinvert->y + tl * deltay;
                    poutvert->z = pinvert->z + tl * deltaz;

                    poutvert->w = pinvert->w + tl * (nextvert->w - pinvert->w);
                    poutvert->s = pinvert->s + tl * (nextvert->s - pinvert->s);
                    poutvert->t = pinvert->t + tl * (nextvert->t - pinvert->t);
                }
                else {
                    // copy as it is
                    *poutvert = *pinvert;
                }
                poutvert++;
                nrClipVerts++;
                
                // clipped end point
                if ( tu < 1 ) {
                    poutvert->x = pinvert->x + tu * deltax;
                    poutvert->y = pinvert->y + tu * deltay;
                    poutvert->z = pinvert->z + tu * deltaz;
                
                    poutvert->w = pinvert->w + tu * (nextvert->w - pinvert->w);
                    poutvert->s = pinvert->s + tu * (nextvert->s - pinvert->s);
                    poutvert->t = pinvert->t + tu * (nextvert->t - pinvert->t);
                    
                    poutvert++;
                    nrClipVerts++;
                }
            }
        }
        pinvert++;
    }

    return nrClipVerts;
}


// --------------------------------------------------------------------------
// Clip polygon to the view window
// In:          2D output coordinates,
// Out: CLIPPED 2D output coordinates
// --------------------------------------------------------------------------

// WARNING: we currently use the View clipping only for rectangular polygons,
//          with 4 corners, so we need a few temporary vertices here..
//          watchout if you use this to clip more than 4 points !!
static  wallVert2D temp2dVerts[8];

// WARNING: this routine considers inVerts[x]->oow is 1
static int ClipToView (wallVert2D *inVerts, wallVert2D *outVerts, int nrInVerts)
{
    float       tl,tu;
    float       deltax, deltay;

    float       clipleft,clipright,cliptop,clipbottom;

    int         i;
    int         xClipVerts, nrClipVerts;
    wallVert2D  *pinvert;
    wallVert2D  *poutvert;
    wallVert2D  *nextvert;

    //FIXME: compute this once at SetViewSize
    // set the clipping borders
    clipleft  = gr_viewwindowx;
    clipright = gr_viewwindowx + gr_viewwidth;
    cliptop   = gr_viewwindowy + gr_viewheight;
    clipbottom= gr_viewwindowy;
    
    // first pass left/right clipping
    xClipVerts = 0;
    pinvert = inVerts;
    poutvert = temp2dVerts;

    for (i=0; i<nrInVerts; i++)
    {
        nextvert = &inVerts[ i + 1 ];
        if (nextvert == &inVerts[nrInVerts])
            nextvert = inVerts;
    
        tl = 0; tu = 1;
        deltax = nextvert->x - pinvert->x;
        if ( ClipT( -deltax, pinvert->x - clipleft, &tl, &tu ) ) {
            if ( ClipT( deltax, clipright - pinvert->x, &tl, &tu) ) {
                deltay = nextvert->y - pinvert->y;
                
                // clipped start point
                if ( tl > 0 ) {
                    poutvert->x = pinvert->x + tl * deltax;
                    poutvert->y = pinvert->y + tl * deltay;
                    poutvert->oow = pinvert->oow;
                    poutvert->sow = pinvert->sow + tl * (nextvert->sow - pinvert->sow);
                    poutvert->tow = pinvert->tow + tl * (nextvert->tow - pinvert->tow);
                    //poutvert->argb = pinvert->argb;
                }
                else {
                    // copy as it is
                    *poutvert = *pinvert;
                }
                poutvert++;
                xClipVerts++;
            
                // clipped end point
                if ( tu < 1 ) {
                    poutvert->x = pinvert->x + tu * deltax;
                    poutvert->y = pinvert->y + tu * deltay;
                    poutvert->oow = pinvert->oow;
                    poutvert->sow = pinvert->sow + tu * (nextvert->sow - pinvert->sow);
                    poutvert->tow = pinvert->tow + tu * (nextvert->tow - pinvert->tow);
                    //poutvert->argb = pinvert->argb;

                    poutvert++;
                    xClipVerts++;
                }
            }
        }
        pinvert++;
    }

    // SECOND pass top/bottom clipping
    pinvert = temp2dVerts;
    poutvert = outVerts;
    nrClipVerts = 0;

    for (i=0; i<xClipVerts; i++)
    {
        nextvert = &temp2dVerts[i+1];
        if (nextvert == &temp2dVerts[xClipVerts])
            nextvert = temp2dVerts;
    
        tl = 0; tu = 1;
        deltay = nextvert->y - pinvert->y;
        if ( ClipT( -deltay, pinvert->y - clipbottom, &tl, &tu ) ) {
            if ( ClipT( deltay, cliptop - pinvert->y, &tl, &tu ) ) {
                deltax = nextvert->x - pinvert->x;
                
                // clipped start point
                if ( tl > 0 ) {
                    poutvert->x = pinvert->x + tl * deltax;
                    poutvert->y = pinvert->y + tl * deltay;
                    poutvert->oow = pinvert->oow;
                    poutvert->sow = pinvert->sow + tl * (nextvert->sow - pinvert->sow);
                    poutvert->tow = pinvert->tow + tl * (nextvert->tow - pinvert->tow);
                    //poutvert->argb = pinvert->argb;
                }
                else {
                    // copy as it is
                    *poutvert = *pinvert;
                }
                poutvert++;
                nrClipVerts++;
                
                // clipped end point
                if ( tu < 1 ) {
                    poutvert->x = pinvert->x + tu * deltax;
                    poutvert->y = pinvert->y + tu * deltay;
                    poutvert->oow = pinvert->oow;
                    poutvert->sow = pinvert->sow + tu * (nextvert->sow - pinvert->sow);
                    poutvert->tow = pinvert->tow + tu * (nextvert->tow - pinvert->tow);
                    //poutvert->argb = pinvert->argb;
                    
                    poutvert++;
                    nrClipVerts++;
                }
            }
        }
        pinvert++;
    }

    return nrClipVerts;
}


// ==========================================================================
//                                   FLOOR/CEILING GENERATION FROM SUBSECTORS
// ==========================================================================

#ifdef  DOPLANES

//what is the maximum number of verts around a convex floor/ceiling polygon?
#define MAXCLIPVERTS     256

static  wallVert3D  planeVerts[MAXCLIPVERTS];
static  wallVert3D  planeZVerts[MAXCLIPVERTS*2];
static  wallVert3D  planeFVerts[MAXCLIPVERTS*2];

static  wallVert2D  planeOutVerts[MAXCLIPVERTS*2];  //projected vertices

static void HWR_RenderPlane (extrasubsector_t *xsub, fixed_t fixedheight)
{
    polyvertex_t*   pv;
    float           height; //constant y for all points on the convex flat polygon
    wallVert3D      *v3d;
    wallVert2D      *pjv;
    int             nrPlaneVerts;   //verts original define of convex flat polygon
    int             nrClipVerts;     // clipped verts count after clippings
    int             i;
    float           tr_x,tr_y;
    float           flatxref,flatyref;
    
    int             lightnum;

    // no convex poly were generated for this subsector
    if (!xsub->planepoly)
        return;
    
    height = ((float)fixedheight) * crapmul;

    pv  = xsub->planepoly->pts;
    nrPlaneVerts = xsub->planepoly->numpts;

    if (nrPlaneVerts < 3)   //not even a triangle ?
        return;
    
    if (nrPlaneVerts)
    {
        //reference point for flat texture coord for each vertex around the polygon
        flatxref = (float) (((fixed_t)pv->x & (~63)) << 2);
        flatyref = (float) (((fixed_t)pv->y & (~63)) << 2);
    }

    v3d = planeVerts;
    for (i=0; i<nrPlaneVerts; i++)
    {
        v3d->x = pv->x;
        v3d->y = height;
        v3d->z = pv->y;        //distance close/away
        
        v3d->w = 1.0f;
        
        // use some modulo 64x64 to get the flat texture coord ?
        v3d->s = (v3d->x - flatxref) * 4;
        v3d->t = (flatyref - v3d->z) * 4;
            
        v3d++;
        pv++;
    }

    //tlConOutput ("nrplaneverts : %d\n", nrPlaneVerts);

    // transform
    v3d = planeVerts;
    for (i=0; i<nrPlaneVerts; i++,v3d++)
    {
        // translation
        tr_x = v3d->x - gr_viewx;
        tr_y = v3d->z - gr_viewy;
        v3d->y = (v3d->y - gr_viewz);

        // rotation around vertical y axis
        v3d->z = (tr_x * gr_viewcos) + (tr_y * gr_viewsin);
        v3d->x = (tr_x * gr_viewsin) - (tr_y * gr_viewcos);

        //test up/down ----TOTAL SUCKS!!!--- do the 2 in one!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        tr_x = v3d->z;
        tr_y = v3d->y;
        v3d->y = (tr_x * gr_viewludcos) + (tr_y * gr_viewludsin);
        v3d->z = (tr_x * gr_viewludsin) - (tr_y * gr_viewludcos);
        // ---------------------- mega lame test ----------------------------------
    
        //scale to enter the frustum clipping, so that 90deg frustum fit to screen height
        v3d->y = v3d->y * gr_scalefrustum;
    }

    // clip to near z plane
    nrClipVerts = HWR_ClipZ (planeVerts, planeZVerts, nrPlaneVerts);

    // -!!!- EXIT HERE if not enough points
    if (nrClipVerts<3)
        return;

    nrClipVerts = ClipToFrustum (planeZVerts, planeFVerts, nrClipVerts);

    // project clipped vertices
    v3d = planeFVerts;
    pjv = planeOutVerts;
    for (i=0; i<nrClipVerts; i++,v3d++,pjv++)
    {
        pjv->x = gr_windowcenterx + (v3d->x * gr_centerx / v3d->z);
        pjv->y = gr_windowcentery - (v3d->y * gr_centery / v3d->z);
        
        pjv->oow = 1.0f / v3d->z;
        pjv->sow = v3d->s * pjv->oow;
        pjv->tow = v3d->t * pjv->oow;

        //pjv->argb = 0xff804020;
    }

    //  use different light tables
    //  for horizontal / vertical / diagonal
    //  note: try to get the same visual feel as the original
    if (!fixedcolormap)
    {
        lightnum = gr_frontsector->lightlevel + (extralight<<4);
        if (lightnum > 255)
            lightnum = 255;
        HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, ((lightnum&0xff)<<24) | 0xf00000);
    }

    HWD.pfnDrawPolygon (planeOutVerts, nrClipVerts, 0xff804020);
    
    //debug
    /*
    pjv = planeOutVerts;
    for (i=0; i<nrPlaneVerts; i++,pjv++)
    {
        grDrawPoint (pjv);
    }*/
}
#endif


// ==========================================================================
//                                        WALL GENERATION FROM SUBSECTOR SEGS
// ==========================================================================


// v1,v2 : the start & end vertices along the original wall segment, that may have been
//         clipped so that only a visible portion of the wall seg is drawn.
// floorheight, ceilingheight : depend on wall upper/lower/middle, comes from the sectors.
// wallVerts are passed as:
//  3--2
//  | /|
//  |/ |
//  0--1
static void HWR_ProjectWall (wallVert3D* wallVerts)
{
    wallVert3D  trVerts[4];
    wallVert3D  clZVerts[8];
    wallVert3D  clVerts[8];
    wallVert2D  outVerts[8];
    int         i;
    int         wClipVerts;
    wallVert3D *wv;
    float       tr_x,tr_y;


    // transform
    wv = trVerts;
    for (i=0; i<4; i++,wv++)
    {
        // translation
        tr_x = wallVerts->x - gr_viewx;
        tr_y = wallVerts->z - gr_viewy;
        wv->y = (wallVerts->y - gr_viewz);

        // rotation around vertical y axis
        wv->z = (tr_x * gr_viewcos) + (tr_y * gr_viewsin);
        wv->x = (tr_x * gr_viewsin) - (tr_y * gr_viewcos);

        //test up/down ----TOTAL SUCKS!!!--- do the 2 in one!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        tr_x = wv->z;
        tr_y = wv->y;
        wv->y = (tr_x * gr_viewludcos) + (tr_y * gr_viewludsin);
        wv->z = (tr_x * gr_viewludsin) - (tr_y * gr_viewludcos);
        // ---------------------- mega lame test ----------------------------------

        //scale y before frustum so that frustum can be scaled to screen height
        wv->y = wv->y * gr_scalefrustum;

        if (drawtextured) {
            wv->s = wallVerts->s;
            wv->t = wallVerts->t;
        }
        wv->w = wallVerts->w;
        
        wallVerts++;
    }   
    
    // clip to near z plane    
    wClipVerts = HWR_ClipZ (trVerts, clZVerts, 4);

    if (wClipVerts>=4)
        wClipVerts = ClipToFrustum (clZVerts, clVerts, wClipVerts);
    else
        wClipVerts = 0;

    // project clipped vertices
    wv = clVerts;
    for (i=0; i<wClipVerts; i++,wv++)
    {
        outVerts[i].x = gr_windowcenterx + (wv->x * gr_centerx / wv->z);
        outVerts[i].y = gr_windowcentery - (wv->y * gr_centery / wv->z);
        
        outVerts[i].oow = 1.0f / wv->z;
        outVerts[i].sow = wv->s * outVerts[i].oow;
        outVerts[i].tow = wv->t * outVerts[i].oow;

        outVerts[i].argb = 0xff00ff00;
        //grDrawPoint (&outVerts[i]);
    }

    //1 sided walls
    HWD.pfnDrawPolygon (outVerts, wClipVerts, 0xffff0000 );
}


// ==========================================================================
//                                                          BSP , CULL, ETC..
// ==========================================================================

// return the frac from the interception of the clipping line
// (in fact a clipping plane that has a constant, so can clip with simple 2d)
// with the wall segment
//
static float HWR_ClipViewSegment (angle_t clipangle, vertex_t* v1, vertex_t* v2)
{
    float       num;
    float       den;
    float       v1x;
    float       v1y;
    float       v1dx;
    float       v1dy;
    float       v2dx;
    float       v2dy;

    // a segment of a polygon
    v1x  = (float)v1->x * crapmul;
    v1y  = (float)v1->y * crapmul;
    v1dx = (float)(v2->x - v1->x) * crapmul;
    v1dy = (float)(v2->y - v1->y) * crapmul;

    // the clipping line
    clipangle = clipangle + dup_viewangle; //back to normal angle (non-relative)
    v2dx = (float)finecosine[clipangle>>ANGLETOFINESHIFT] * crapmul;
    v2dy = (float)finesine[clipangle>>ANGLETOFINESHIFT] * crapmul;

    den = v2dy*v1dx - v2dx*v1dy;
    if (den == 0)
        return -1;         // parallel

    // calc the frac along the polygon segment,
    //num = (v2x - v1x)*v2dy + (v1y - v2y)*v2dx;
    //num = -v1x * v2dy + v1y * v2dx;
    num = (gr_viewx - v1x)*v2dy + (v1y - gr_viewy)*v2dx;

    return num / den;
}


//
// HWR_StoreWallRange
// A portion or all of a wall segment will be drawn, from startfrac to endfrac,
//  where 0 is the start of the segment, 1 the end of the segment
// Anything between means the wall segment has been clipped with solidsegs,
//  reducing wall overdraw to a minimum
//
static void HWR_StoreWallRange (float startfrac, float endfrac)
{
    wallVert3D  wallVerts[4];
    v2d_t       vs, ve;         // start, end vertices of 2d line (view from above)
    
    fixed_t     worldtop;
    fixed_t     worldbottom;
    fixed_t     worldhigh;
    fixed_t     worldlow;

    fixed_t     worldup;        // for 2sided lines
    fixed_t     worlddown;

    GlideTexture_t* grTex;

    float       cliplow,cliphigh;
    
    fixed_t     texturehpeg;
    fixed_t     texturevpeg;        //mid or top
    fixed_t     texturevpegtop;     //top
    fixed_t     texturevpegbottom;  //bottom
    
    int         lightnum;
                                    
    /*
    if (startfrac > 1 || startfrac > endfrac)
        I_Error ("HWR_RenderWallRange: startfrac %f to endfrac %f", startfrac , endfrac);
    */
    if (startfrac>endfrac)
        return;

    gr_sidedef = gr_curline->sidedef;
    gr_linedef = gr_curline->linedef;
        
    // mark the segment as visible for auto map
    gr_linedef->flags |= ML_MAPPED;

    worldtop    = gr_frontsector->ceilingheight;
    worldbottom = gr_frontsector->floorheight;

    gr_midtexture = gr_toptexture = gr_bottomtexture = 0;

    if (!gr_backsector)
    {
        // single sided line
        gr_midtexture = texturetranslation[gr_sidedef->midtexture];

        // PEGGING
        if (gr_linedef->flags & ML_DONTPEGBOTTOM)
            texturevpeg = gr_frontsector->floorheight + textureheight[gr_sidedef->midtexture] - worldtop;
        else
            // top of texture at top
            texturevpeg = 0;

        texturevpeg += gr_sidedef->rowoffset;
    }
    else
    {
        // two sided line

        worldhigh = gr_backsector->ceilingheight;
        worldlow = gr_backsector->floorheight;

        // hack to allow height changes in outdoor areas
        if (gr_frontsector->ceilingpic == skyflatnum &&
            gr_backsector->ceilingpic  == skyflatnum)
        {
            worldtop = worldhigh;
        }

        // possibly mid texture
        gr_midtexture = texturetranslation[gr_sidedef->midtexture];


        // find positioning of mid 2sided texture
        if (gr_midtexture) {
            // 2sided line mid texture uses lowest ceiling, highest floor
            worldup = worldhigh < worldtop ? worldhigh : worldtop;
            worlddown = worldlow > worldbottom ? worldlow : worldbottom;

            if (gr_linedef->flags & ML_DONTPEGBOTTOM) {
                worldup = worlddown + textureheight[gr_midtexture] + gr_sidedef->rowoffset;
                worlddown = worldup - textureheight[gr_midtexture];
                texturevpeg = 0;
                // 2sided textures don't repeat vertically
                /*
                
                if (worldup - worlddown > textureheight[gr_sidedef->midtexture])
                    worldup = worlddown + textureheight[gr_sidedef->midtexture];
                texturevpeg = worlddown + textureheight[gr_sidedef->midtexture] - worldup;*/
            }
            else {
                texturevpeg = 0;
                worldup += gr_sidedef->rowoffset;
                // 2sided textures don't repeat vertically
                //if (worldup - worlddown > textureheight[gr_sidedef->midtexture])
                    worlddown = worldup - textureheight[gr_midtexture];
            }
        }
       
        // check TOP TEXTURE
        if (worldhigh < worldtop)
        {
            // top texture
            gr_toptexture = texturetranslation[gr_sidedef->toptexture];

            // PEGGING
            if (gr_linedef->flags & ML_DONTPEGTOP)
            {
                // top of texture at top
                texturevpegtop = 0;
            }
            else
            {
                texturevpegtop = gr_backsector->ceilingheight + textureheight[gr_sidedef->toptexture] - worldtop;
            }
        }
        
        // check BOTTOM TEXTURE
        if (worldlow > worldbottom)     //seulement si VISIBLE!!!
        {
            // bottom texture
            gr_bottomtexture = texturetranslation[gr_sidedef->bottomtexture];
            
            // PEGGING
            if (gr_linedef->flags & ML_DONTPEGBOTTOM )
            {
                // bottom of texture at bottom
                // top of texture at top
                texturevpegbottom = worldtop - worldlow;
            }
            else
                // top of texture at top
                texturevpegbottom = 0;
        }

        texturevpegtop    += gr_sidedef->rowoffset;
        texturevpegbottom += gr_sidedef->rowoffset;
    }

    gr_segtextured = gr_midtexture | gr_toptexture | gr_bottomtexture;
        
    if (gr_segtextured) {
        // x offset the texture
        texturehpeg = gr_sidedef->textureoffset + gr_curline->offset;
    }

    // prepare common values for top/bottom and middle walls
    vs.x = ((float)gr_curline->v1->x) * crapmul;
    vs.y = ((float)gr_curline->v1->y) * crapmul;
    ve.x = ((float)gr_curline->v2->x) * crapmul;
    ve.y = ((float)gr_curline->v2->y) * crapmul;

    //
    // clip the wall segment to solidsegs
    //

    // clip start of segment
    if (startfrac > 0){
        if (startfrac>1)
            CONS_Printf ("startfrac %f\n", startfrac);
        else{
            vs.x = vs.x + (ve.x - vs.x) * startfrac;
            vs.y = vs.y + (ve.y - vs.y) * startfrac;
        }
    }

    // clip end of segment
    if (endfrac < 1){
        if (endfrac<0)
            CONS_Printf ("  endfrac %f\n", endfrac);
        else{
            ve.x = vs.x + (ve.x - vs.x) * endfrac;
            ve.y = vs.y + (ve.y - vs.y) * endfrac;
        }
    }

    // remember vertices ordering
    //  3--2
    //  | /|
    //  |/ |
    //  0--1
    // make a wall polygon (with 2 triangles), using the floor/ceiling heights,
    // and the 2d map coords of start/end vertices 
    wallVerts[0].x = wallVerts[3].x = vs.x;
    wallVerts[0].z = wallVerts[3].z = vs.y;
    wallVerts[2].x = wallVerts[1].x = ve.x;
    wallVerts[2].z = wallVerts[1].z = ve.y;
    wallVerts[0].w = wallVerts[1].w = wallVerts[2].w = wallVerts[3].w = 1.0f;


    //  use different light tables
    //  for horizontal / vertical / diagonal
    //  note: try to get the same visual feel as the original
    if (!fixedcolormap)
    {
            lightnum = gr_frontsector->lightlevel + (extralight<<4);
        if (lightnum > 255)
            lightnum = 255;
        
            if (gr_curline->v1->y == gr_curline->v2->y &&
            lightnum >= (256/LIGHTLEVELS) )
                lightnum -= (256/LIGHTLEVELS);
            else
            if (gr_curline->v1->x == gr_curline->v2->x &&
                lightnum < 256 - (256/LIGHTLEVELS) )
                    lightnum += (256/LIGHTLEVELS);

        HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, ((lightnum&0xff)<<24) | 0xf00000);
    }


    //
    // draw top texture
    //
    if (gr_toptexture)
    {
        if (drawtextured)
        {
            grTex = HWR_GetTexture (gr_toptexture);
        
            // clip texture s start/end coords with solidsegs
            if (startfrac > 0 && startfrac < 1)
                cliplow = (texturehpeg * grTex->scaleX) + (gr_curline->length * grTex->scaleX * startfrac);
            else
                cliplow = texturehpeg * grTex->scaleX;
        
            if (endfrac < 1 && endfrac>0)
                cliphigh = (texturehpeg * grTex->scaleX) + (gr_curline->length * grTex->scaleX * endfrac);
            else
                cliphigh = (texturehpeg + gr_curline->length) * grTex->scaleX;

            wallVerts[0].s = wallVerts[3].s = cliplow;
            wallVerts[2].s = wallVerts[1].s = cliphigh;

            wallVerts[3].t = wallVerts[2].t = texturevpegtop * grTex->scaleY;
            wallVerts[0].t = wallVerts[1].t = (texturevpegtop + worldtop - worldhigh) * grTex->scaleY;
        }

        // set top/bottom coords
        wallVerts[2].y = wallVerts[3].y = (float)worldtop * crapmul;
        wallVerts[0].y = wallVerts[1].y = (float)worldhigh * crapmul;
       
        HWR_ProjectWall (wallVerts);
    }

    //
    // draw bottom texture
    //
    if (gr_bottomtexture)
    {
        wallVerts[0].x = wallVerts[3].x = vs.x;
        wallVerts[0].z = wallVerts[3].z = vs.y;
        wallVerts[2].x = wallVerts[1].x = ve.x;
        wallVerts[2].z = wallVerts[1].z = ve.y;
        wallVerts[0].w = wallVerts[1].w = wallVerts[2].w = wallVerts[3].w = 1.0f;

        if (drawtextured) {
            grTex = HWR_GetTexture (gr_bottomtexture);

            // clip texture s start/end coords with solidsegs
            if (startfrac > 0 && startfrac < 1)
                cliplow = (texturehpeg * grTex->scaleX) + (gr_curline->length * grTex->scaleX * startfrac);
            else
                cliplow = texturehpeg * grTex->scaleX;
        
            if (endfrac < 1 && endfrac>0)
                cliphigh = (texturehpeg * grTex->scaleX) + (gr_curline->length * grTex->scaleX * endfrac);
            else
                cliphigh = (texturehpeg + gr_curline->length) * grTex->scaleX;

            wallVerts[0].s = wallVerts[3].s = cliplow;
            wallVerts[2].s = wallVerts[1].s = cliphigh;
            
            wallVerts[3].t = wallVerts[2].t = texturevpegbottom * grTex->scaleY;
            wallVerts[0].t = wallVerts[1].t = (texturevpegbottom + worldlow - worldbottom) * grTex->scaleY;
        }
        
        // set top/bottom coords
        wallVerts[2].y = wallVerts[3].y = (float)worldlow * crapmul;
        wallVerts[0].y = wallVerts[1].y = (float)worldbottom * crapmul;

        HWR_ProjectWall (wallVerts);
    }


    //
    // draw at last mid texture, ! we modify worldtop and worldbottom!
    // 
    if (gr_midtexture)
    {
        // 2 sided line
        if (gr_backsector) {
            worldtop = worldup;
            worldbottom = worlddown;
        }

        
        //
        // draw middle texture
        //
        if (drawtextured) {
            grTex = HWR_GetTexture (gr_midtexture);

            // clip texture s start/end coords with solidsegs
            if (startfrac > 0 && startfrac < 1)
                cliplow = (texturehpeg * grTex->scaleX) + (gr_curline->length * grTex->scaleX * startfrac);
            else
                cliplow = texturehpeg * grTex->scaleX;
            
            if (endfrac < 1 && endfrac>0)
                cliphigh = (texturehpeg * grTex->scaleX) + (gr_curline->length * grTex->scaleX * endfrac);
            else
                cliphigh = (texturehpeg + gr_curline->length) * grTex->scaleX;

            wallVerts[0].s = wallVerts[3].s = cliplow;
            wallVerts[2].s = wallVerts[1].s = cliphigh;

            wallVerts[3].t = wallVerts[2].t = texturevpeg * grTex->scaleY;
            wallVerts[0].t = wallVerts[1].t = (texturevpeg + worldtop - worldbottom) * grTex->scaleY;
        }
        
        // set top/bottom coords
        wallVerts[2].y = wallVerts[3].y = (float)worldtop * crapmul;
        wallVerts[0].y = wallVerts[1].y = (float)worldbottom * crapmul;
        
        HWR_ProjectWall (wallVerts);
    }


}


//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
typedef struct
{
    int first;
    int last;
    
} cliprange_t;


#define MAXSEGS         32

// newend is one past the last valid seg
cliprange_t*    newend;
cliprange_t     gr_solidsegs[MAXSEGS];


//
//
//
static void HWR_ClipSolidWallSegment (int   first,
                              int   last )
{
    cliprange_t*        next;
    cliprange_t*        start;
    float           lowfrac, highfrac;

    boolean poorhack=false;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = gr_solidsegs;
    while (start->last < first-1)
        start++;
    
    if (first < start->first)
    {
        if (last < start->first-1)
        {
            // Post is entirely visible (above start),
            //  so insert a new clippost.
            HWR_StoreWallRange (0, 1);
            next = newend;
            newend++;
            
            while (next != start)
            {
                *next = *(next-1);
                next--;
            }
            next->first = first;
            next->last = last;
            return;
        }
        
        // There is a fragment above *start.
        highfrac = HWR_ClipViewSegment (gr_xtoviewangle[start->first - 1], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
        HWR_StoreWallRange (0, highfrac);
        
        // Now adjust the clip size.
        start->first = first;
    }
    
    // Bottom contained in start?
    if (last <= start->last)
        return;
    
    next = start;
    while (last >= (next+1)->first-1)
    {
        // There is a fragment between two posts.
        lowfrac  = HWR_ClipViewSegment (gr_xtoviewangle[ next->last + 1 ], gr_curline->v1, gr_curline->v2);
        highfrac = HWR_ClipViewSegment (gr_xtoviewangle[(next+1)->first - 1 ], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
        HWR_StoreWallRange (lowfrac, highfrac);
        next++;
        
        if (last <= next->last)
        {
            // Bottom is contained in next.
            // Adjust the clip size.
            start->last = next->last;
            goto crunch;
        }
    }
    
    // There is a fragment after *next.
    lowfrac  = HWR_ClipViewSegment (gr_xtoviewangle[ next->last + 1 ], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
    HWR_StoreWallRange (lowfrac, 1);

    // Adjust the clip size.
    start->last = last;

    // Remove start+1 to next from the clip list,
    // because start now covers their area.
crunch:
    if (next == start)
    {
        // Post just extended past the bottom of one post.
        return;
    }


    while (next++ != newend)
    {
        // Remove a post.
        *++start = *next;
    }

    newend = start+1;
}


//
//  handle LineDefs with upper and lower texture (windows)
//
static void HWR_ClipPassWallSegment (int       first,
                             int        last )
{
    cliprange_t*        start;
    float           lowfrac, highfrac;

    boolean poorhack=false;   //to allow noclipwalls but still solidseg reject of non-visible walls

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = gr_solidsegs;
    while (start->last < first-1)
        start++;
    
    if (first < start->first)
    {
        if (last < start->first-1)
        {
            // Post is entirely visible (above start).
            HWR_StoreWallRange (0, 1);
            return;
        }
        
        // There is a fragment above *start.
        highfrac  = HWR_ClipViewSegment (gr_xtoviewangle[ start->first - 1 ], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
        HWR_StoreWallRange (0, highfrac);
    }
    
    // Bottom contained in start?
    if (last <= start->last)
        return;
    
    while (last >= (start+1)->first-1)
    {
        // There is a fragment between two posts.
        lowfrac  = HWR_ClipViewSegment (gr_xtoviewangle[ start->last + 1 ], gr_curline->v1, gr_curline->v2);
        highfrac = HWR_ClipViewSegment (gr_xtoviewangle[(start+1)->first - 1 ], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
        HWR_StoreWallRange (lowfrac, highfrac);
        start++;
        
        if (last <= start->last)
            return;
    }

    // There is a fragment after *next.
    lowfrac  = HWR_ClipViewSegment (gr_xtoviewangle[ start->last + 1 ], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
    HWR_StoreWallRange (lowfrac, 1);
}


//
// HWR_ClearClipSegs
//
static void HWR_ClearClipSegs (void)
{
    gr_solidsegs[0].first = -0x7fffffff;
    gr_solidsegs[0].last = -1;
    gr_solidsegs[1].first = VIDWIDTH;    //viewwidth;
    gr_solidsegs[1].last = 0x7fffffff;
    newend = gr_solidsegs+2;
}



//
// Clips the given segment
//          and adds any visible pieces to the line list.
//
static void  HWR_AddLine (seg_t*        line)
{
    int                 x1;
    int                 x2;
    angle_t             angle1;
    angle_t             angle2;
    angle_t             span;
    angle_t             tspan;

    gr_curline = line;

    // OPTIMIZE: quickly reject orthogonal back sides.
    angle1 = R_PointToAngle (line->v1->x, line->v1->y);
    angle2 = R_PointToAngle (line->v2->x, line->v2->y);

    // Clip to view edges.
    span = angle1 - angle2;

    // backface culling : span is < ANG180 if ang1 > ang2 : the seg is facing
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    //rw_angle1 = angle1;
    angle1 -= dup_viewangle;
    angle2 -= dup_viewangle;

    tspan = angle1 + gr_clipangle;
    if (tspan > 2*gr_clipangle)
    {
        tspan -= 2*gr_clipangle;
        
        // Totally off the left edge?
        if (tspan >= span)
            return;
        
        angle1 = gr_clipangle;
    }
    tspan = gr_clipangle - angle2;
    if (tspan > 2*gr_clipangle)
    {
        tspan -= 2*gr_clipangle;
        
        // Totally off the left edge?
        if (tspan >= span)
            return;
        angle2 = -gr_clipangle;
    }

    // The seg is in the view range,
    // but not necessarily visible.
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    x1 = gr_viewangletox[angle1];
    x2 = gr_viewangletox[angle2];
    
    // Does not cross a pixel?
    if (x1 == x2)
        return;
    
    gr_backsector = line->backsector;
    
    // Single sided line?
    if (!gr_backsector)
        goto clipsolid;
    
    // Closed door.
    if (gr_backsector->ceilingheight <= gr_frontsector->floorheight ||
        gr_backsector->floorheight >= gr_frontsector->ceilingheight)
        goto clipsolid;
    
    // Window.
    if (gr_backsector->ceilingheight != gr_frontsector->ceilingheight ||
        gr_backsector->floorheight != gr_frontsector->floorheight)
        goto clippass;
    
    // Reject empty lines used for triggers and special events.
    // Identical floor and ceiling on both sides,
    //  identical light levels on both sides,
    //  and no middle texture.
    if (gr_backsector->ceilingpic == gr_frontsector->ceilingpic
        && gr_backsector->floorpic == gr_frontsector->floorpic
        && gr_backsector->lightlevel == gr_frontsector->lightlevel
        && gr_curline->sidedef->midtexture == 0)
    {
        return;
    }
    
    
clippass:
    HWR_ClipPassWallSegment (x1, x2-1);
    return;
    
clipsolid:
    HWR_ClipSolidWallSegment (x1, x2-1);
}


// HWR_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
// modified to use local variables

extern  int     checkcoord[12][4];  //r_bsp.c

static boolean  HWR_CheckBBox (fixed_t*   bspcoord)
{
    int                 boxx;
    int                 boxy;
    int                 boxpos;

    fixed_t             x1;
    fixed_t             y1;
    fixed_t             x2;
    fixed_t             y2;

    angle_t             angle1;
    angle_t             angle2;
    angle_t             span;
    angle_t             tspan;

    cliprange_t*        start;

    int                 sx1;
    int                 sx2;

    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (dup_viewx <= bspcoord[BOXLEFT])
        boxx = 0;
    else if (dup_viewx < bspcoord[BOXRIGHT])
        boxx = 1;
    else
        boxx = 2;

    if (dup_viewy >= bspcoord[BOXTOP])
        boxy = 0;
    else if (dup_viewy > bspcoord[BOXBOTTOM])
        boxy = 1;
    else
        boxy = 2;

    boxpos = (boxy<<2)+boxx;
    if (boxpos == 5)
        return true;

    x1 = bspcoord[checkcoord[boxpos][0]];
    y1 = bspcoord[checkcoord[boxpos][1]];
    x2 = bspcoord[checkcoord[boxpos][2]];
    y2 = bspcoord[checkcoord[boxpos][3]];

    // check clip list for an open space
    angle1 = R_PointToAngle (x1, y1) - dup_viewangle;
    angle2 = R_PointToAngle (x2, y2) - dup_viewangle;

    span = angle1 - angle2;

    // Sitting on a line?
    if (span >= ANG180)
        return true;

    tspan = angle1 + gr_clipangle;

    if (tspan > 2*gr_clipangle)
    {
        tspan -= 2*gr_clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

        angle1 = gr_clipangle;
    }
    tspan = gr_clipangle - angle2;
    if (tspan > 2*gr_clipangle)
    {
        tspan -= 2*gr_clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

        angle2 = -gr_clipangle;
    }


    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    sx1 = gr_viewangletox[angle1];
    sx2 = gr_viewangletox[angle2];

    // Does not cross a pixel.
    if (sx1 == sx2)
        return false;
    sx2--;

    start = gr_solidsegs;
    while (start->last < sx2)
        start++;

    if (sx1 >= start->first
        && sx2 <= start->last)
    {
        // The clippost contains the new span.
        return false;
    }

    return true;
}


//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
static int doomwaterflat;  //set by R_InitFlats hack
static void HWR_Subsector ( int num )
{
    int                     count;
    seg_t*                  line;
    subsector_t*            sub;
    
    unsigned char           light;
    fixed_t                 wh;

//no risk while developing, enough debugging nights!
//#ifdef RANGECHECK
    if (num>=addsubsector)
        I_Error ("HWR_Subsector: ss %i with numss = %i",
                                num,
                                numsubsectors);
    
    /*if (num>=numsubsectors)
        I_Error ("HWR_Subsector: ss %i with numss = %i",
                         num,
                         numsubsectors);*/
//#endif

    if (num < numsubsectors)
    {
        sscount++;
        // subsector
        sub = &subsectors[num];
        // sector
        gr_frontsector = sub->sector;
        // how many linedefs
        count = sub->numlines;
        // first line seg
        line = &segs[sub->firstline];
    }
    else
    {
        // there are no segs but only planes
        sub = &subsectors[0];
        gr_frontsector = sub->sector;
        count = 0;
        line = NULL;
    }

    // sector lighting
    light = (unsigned char)sub->sector->lightlevel & 0xff;
    HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, (light<<24) | 0xf00000);

    // render floor ?
#ifdef DOPLANES
    // yeah, easy backface cull! :)
    if (gr_frontsector->floorheight < dup_viewz &&
        gr_frontsector->floorpic != skyflatnum) {
            //frontsector->floorheight, frontsector->floorpic, frontsector->lightlevel);
        HWR_GetFlat ( levelflats[gr_frontsector->floorpic].lumpnum );
        HWR_RenderPlane (&extrasubsectors[num], gr_frontsector->floorheight);
    }

    if (gr_frontsector->ceilingheight > dup_viewz &&
        gr_frontsector->ceilingpic != skyflatnum) {
            //frontsector->ceilingheight, frontsector->ceilingpic, frontsector->lightlevel
        HWR_GetFlat ( levelflats[gr_frontsector->ceilingpic].lumpnum );
        HWR_RenderPlane (&extrasubsectors[num], gr_frontsector->ceilingheight);
    }
#endif

    //GROSS test of sector lightlevel.. shoudl set it only when drawing something!
    if (line!=NULL)
    {
        // draw sprites first , coz they are clipped to the solidsegs of
        // subsectors more 'in front'
        HWR_AddSprites (gr_frontsector);

        while (count--)
        {
                HWR_AddLine (line);
                line++;
        }
    }

#ifdef DOPLANES
    // -------------------- WATER IN DEV. TEST ------------------------
    //dck hack : use abs(tag) for waterheight
    if (gr_frontsector->tag<0)
    {
        wh = ((-gr_frontsector->tag) <<16) + (FRACUNIT/2);
        if (wh > gr_frontsector->floorheight &&
            wh < gr_frontsector->ceilingheight )
        {
            HWR_GetFlat ( doomwaterflat );

            HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);
            HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, (0x60<<24) | 0x000000);
            
            HWD.pfnSetState (HWD_SET_DEPTHMASK, false);
            HWR_RenderPlane (&extrasubsectors[num], wh);
            HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
            HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_NONE);
        }
    }
    // -------------------- WATER IN DEV. TEST ------------------------
#endif
}


//
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.

#ifdef coolhack
//t;b;l;r
static fixed_t hackbbox[4];
//BOXTOP,
//BOXBOTTOM,
//BOXLEFT,
//BOXRIGHT
static boolean HWR_CheckHackBBox (fixed_t*   bb)
{
    if (bb[BOXTOP]<hackbbox[BOXBOTTOM]) //y up
        return false;
    if (bb[BOXBOTTOM]>hackbbox[BOXTOP])
        return false;
    if (bb[BOXLEFT]>hackbbox[BOXRIGHT])
        return false;
    if (bb[BOXRIGHT]<hackbbox[BOXLEFT])
        return false;
    return true;
}
#endif

static void HWR_RenderBSPNode (int bspnum)
{
    node_t*     bsp;
    int         side;

    // Found a subsector?
    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1) {
            //*(gr_drawsubsector_p++) = 0;
            HWR_Subsector (0);
        }
        else {
            //*(gr_drawsubsector_p++) = bspnum&(~NF_SUBSECTOR);
            HWR_Subsector (bspnum&(~NF_SUBSECTOR));
        }
        return;
    }

    bsp = &nodes[bspnum];

    // Decide which side the view point is on.
    side = R_PointOnSide (dup_viewx, dup_viewy, bsp);

    // Recursively divide front space.
    HWR_RenderBSPNode (bsp->children[side]);

    // Possibly divide back space.
    if (HWR_CheckBBox (bsp->bbox[side^1]))
        HWR_RenderBSPNode (bsp->children[side^1]);
}


/*
//
// Clear 'stack' of subsectors to draw
//
static void HWR_ClearDrawSubsectors (void)
{
    gr_drawsubsector_p = gr_drawsubsectors;
}


//
// Draw subsectors pushed on the drawsubsectors 'stack', back to front
//
static void HWR_RenderSubsectors (void)
{
    while (gr_drawsubsector_p > gr_drawsubsectors)
    {
        HWR_RenderBSPNode (
        lastsubsec->nextsubsec = bspnum & (~NF_SUBSECTOR);
    }
}
*/


// ==========================================================================
//                                                              FROM R_MAIN.C
// ==========================================================================

void HWR_InitTextureMapping (void)
{
    int                 i;
    int                 x;
    int                 t;
    fixed_t             focallength;
        
    fixed_t             grcenterx;
    fixed_t             grcenterxfrac;
    int                 grviewwidth;

    CONS_Printf ("HWR_InitTextureMapping()\n");

    grviewwidth = VIDWIDTH;
    grcenterx = grviewwidth/2;
    grcenterxfrac = grcenterx<<FRACBITS;
    
    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength = FixedDiv (grcenterxfrac,
                            finetangent[FINEANGLES/4+FIELDOFVIEW/2] );

    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
        if (finetangent[i] > FRACUNIT*2)
            t = -1;
        else if (finetangent[i] < -FRACUNIT*2)
            t = grviewwidth+1;
        else
        {
            t = FixedMul (finetangent[i], focallength);
            t = (grcenterxfrac - t+FRACUNIT-1)>>FRACBITS;

            if (t < -1)
                t = -1;
            else if (t>grviewwidth+1)
                t = grviewwidth+1;
        }
        gr_viewangletox[i] = t;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.
    for (x=0; x <= grviewwidth; x++)
    {
        i = 0;
        while (gr_viewangletox[i]>x)
            i++;
        gr_xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
        t = FixedMul (finetangent[i], focallength);
        t = grcenterx - t;

        if (gr_viewangletox[i] == -1)
            gr_viewangletox[i] = 0;
        else if (gr_viewangletox[i] == grviewwidth+1)
            gr_viewangletox[i]  = grviewwidth;
    }

    gr_clipangle = gr_xtoviewangle[0];
}


// ==========================================================================
// gr_things.c
// ==========================================================================

// sprites are drawn after all wall and planes are rendered, so that
// sprite translucency effects apply on the rendered view (instead of the background sky!!)
typedef struct gr_vissprite_s
{
    // Doubly linked list
    struct gr_vissprite_s* prev;
    struct gr_vissprite_s* next;
    float               x1;
    float               x2;
    float               tz;
    float               ty;
    int                 patch;
    boolean             flip;
    unsigned char       translucency;       //alpha level 0-255
    unsigned char       sectorlight;        // ...
} gr_vissprite_t;


gr_vissprite_t     gr_vissprites[MAXVISSPRITES];
gr_vissprite_t*    gr_vissprite_p;
int                gr_newvissprite;


// --------------------------------------------------------------------------
// HWR_ClearSprites
// Called at frame start.
// --------------------------------------------------------------------------
static void HWR_ClearSprites (void)
{
    gr_vissprite_p = gr_vissprites;
}


// --------------------------------------------------------------------------
// HWR_NewVisSprite
// --------------------------------------------------------------------------
gr_vissprite_t  gr_overflowsprite;

gr_vissprite_t* HWR_NewVisSprite (void)
{
    if (gr_vissprite_p == &gr_vissprites[MAXVISSPRITES])
        return &gr_overflowsprite;

    gr_vissprite_p++;
    return gr_vissprite_p-1;
}


// --------------------------------------------------------------------------
// Draw a Sprite
// --------------------------------------------------------------------------
static void HWR_DrawSprite (gr_vissprite_t* spr)
{
    float               tr_x;
    float               tr_y;

    wallVert3D          wallVerts[4];   //original
    wallVert3D          clZVerts[8];    //.. z clipped
    wallVert3D          clVerts[8];     //.. .. frustum clipped
    wallVert2D          outVerts[8];    //.. .. .. projected
    int                 i;
    int                 wClipVerts;
    wallVert3D          *wv;

    GlidePatch_t*       gpatch;      //sprite patch converted to 3Dfx

    // cache sprite graphics
    gpatch = W_CachePatchNum (spr->patch, PU_3DFXCACHE);

    // create the sprite billboard
    //    
    //  3--2
    //  | /|
    //  |/ |
    //  0--1
    wallVerts[0].x = wallVerts[3].x = spr->x1;
    wallVerts[2].x = wallVerts[1].x = spr->x2;
    wallVerts[2].y = wallVerts[3].y = spr->ty;
    wallVerts[0].y = wallVerts[1].y = spr->ty - gpatch->height;

    // make a wall polygon (with 2 triangles), using the floor/ceiling heights,
    // and the 2d map coords of start/end vertices 
    wallVerts[0].z = wallVerts[1].z = wallVerts[2].z = wallVerts[3].z = spr->tz;
    wallVerts[0].w = wallVerts[1].w = wallVerts[2].w = wallVerts[3].w = 1.0f;

    // transform
    wv = wallVerts;
    for (i=0; i<4; i++,wv++)
    {
        //test up/down ----TOTAL SUCKS!!!--- do the 2 in one!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        tr_x = wv->z;
        tr_y = wv->y;
        wv->y = (tr_x * gr_viewludcos) + (tr_y * gr_spriteludsin);
        wv->z = (tr_x * gr_viewludsin) - (tr_y * gr_spriteludcos);
        // ---------------------- mega lame test ----------------------------------

        //scale y before frustum so that frustum can be scaled to screen height
        wv->y = wv->y * gr_scalefrustum;
    }   

    // set the texture coords BEFORE clipping !!
    if (spr->flip) {
        wallVerts[0].s = wallVerts[3].s = gpatch->max_s;
        wallVerts[2].s = wallVerts[1].s = 0;
    }else{
        wallVerts[0].s = wallVerts[3].s = 0;
        wallVerts[2].s = wallVerts[1].s = gpatch->max_s;
    }
    wallVerts[3].t = wallVerts[2].t = 0;
    wallVerts[0].t = wallVerts[1].t = gpatch->max_t;

    // clip to near z plane (at 90deg lookup, a sprite billboard can look like a plane)
    wClipVerts = HWR_ClipZ (wallVerts, clZVerts, 4);

    if (wClipVerts>=4)
        wClipVerts = ClipToFrustum (clZVerts, clVerts, wClipVerts);
    else
        return;

    // project clipped vertices
    wv = clVerts;
    for (i=0; i<wClipVerts; i++,wv++)
    {
        outVerts[i].x = gr_windowcenterx + (wv->x * gr_centerx / wv->z);
        outVerts[i].y = gr_windowcentery - (wv->y * gr_centery / wv->z);
        outVerts[i].oow = 1.0f / wv->z;
        outVerts[i].sow = wv->s * outVerts[i].oow;
        outVerts[i].tow = wv->t * outVerts[i].oow;
        //outVerts[i].argb = 0xff00ff00;
    }

    // sprite lighting
    if (spr->translucency)
    {
        HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_CONSTANT);

        //HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);
        HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, (spr->translucency<<24) | 0x000000);
        HWD.pfnSetState (HWD_SET_ALPHATESTFUNC, GR_CMP_ALWAYS);

    }
    else{
        //grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
        HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_TEXTURE);
        HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, (spr->sectorlight<<24) | 0x000000);
        HWD.pfnSetState (HWD_SET_ALPHATESTFUNC, GR_CMP_GEQUAL);
    }

    // cache the patch in the graphics card memory
    //CONS_Printf ("downloading converted patch\n");
    HWD.pfnSetTexture (&gpatch->mipmap);
    HWD.pfnDrawPolygon (outVerts, wClipVerts, 0xffff0000 );
}


// --------------------------------------------------------------------------
// Sort vissprites by distance
// --------------------------------------------------------------------------
static gr_vissprite_t     gr_vsprsortedhead;

static void HWR_SortVisSprites (void)
{
    int                 i;
    int                 count;
    gr_vissprite_t*     ds;
    gr_vissprite_t*     best=NULL;      //shut up compiler
    gr_vissprite_t      unsorted;
    float               bestdist;

    count = gr_vissprite_p - gr_vissprites;

    unsorted.next = unsorted.prev = &unsorted;

    if (!count)
        return;

    for (ds=gr_vissprites ; ds<gr_vissprite_p ; ds++)
    {
        ds->next = ds+1;
        ds->prev = ds-1;
    }

    gr_vissprites[0].prev = &unsorted;
    unsorted.next = &gr_vissprites[0];
    (gr_vissprite_p-1)->next = &unsorted;
    unsorted.prev = gr_vissprite_p-1;

    // pull the vissprites out by scale
    //best = 0;         // shut up the compiler warning
    gr_vsprsortedhead.next = gr_vsprsortedhead.prev = &gr_vsprsortedhead;
    for (i=0 ; i<count ; i++)
    {
        bestdist = ZCLIP_PLANE-1;
        for (ds=unsorted.next ; ds!= &unsorted ; ds=ds->next)
        {
            if (ds->tz > bestdist)
            {
                bestdist = ds->tz;
                best = ds;
            }
        }
        best->next->prev = best->prev;
        best->prev->next = best->next;
        best->next = &gr_vsprsortedhead;
        best->prev = gr_vsprsortedhead.prev;
        gr_vsprsortedhead.prev->next = best;
        gr_vsprsortedhead.prev = best;
    }
}


// --------------------------------------------------------------------------
//  Draw all vissprites
// --------------------------------------------------------------------------
static void HWR_DrawSprites (void)
{
    gr_vissprite_t*        spr;

    // vissprites must be sorted, even with a zbuffer,
    // for the translucency effects to be correct
    HWR_SortVisSprites ();

    if (gr_vissprite_p > gr_vissprites)
    {
        // draw all vissprites back to front
        for (spr = gr_vsprsortedhead.next ;
             spr != &gr_vsprsortedhead ;
             spr = spr->next)
        {
            HWR_DrawSprite (spr);
        }
    }

    //back to default
    HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);
}


// --------------------------------------------------------------------------
// HWR_AddSprites
// During BSP traversal, this adds sprites by sector.
// --------------------------------------------------------------------------
static unsigned char sectorlight;
static void HWR_AddSprites (sector_t* sec)
{
    mobj_t*             thing;

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sec->validcount == validcount)
        return;

    // Well, now it will be done.
    sec->validcount = validcount;

    // sprite lighting
    sectorlight = (unsigned char)sec->lightlevel & 0xff;

    /*
    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+extralight;
    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS-1];
    else
        spritelights = scalelight[lightnum];
        */

    // Handle all things in sector.
    for (thing = sec->thinglist ; thing ; thing = thing->snext)
        HWR_ProjectSprite (thing);
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static boolean HWR_ClipSpriteToSolidSegs (int   first,
                                  int   last )
{
    cliprange_t*        start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = gr_solidsegs;
    while (start->last < first-1)
        start++;
    
    if (first < start->first)
    {
        if (last < start->first-1)
            // sprite is entirely visible
            return true;
        
        /* There is a fragment above *start.
        highfrac = HWR_ClipViewSegment (gr_xtoviewangle[start->first - 1], gr_curline->v1, gr_curline->v2);
        if (!cv_grclipwalls.value) {
            if (!poorhack) HWR_StoreWallRange (0,1);
            poorhack=true;
        }else
        HWR_StoreWallRange (0, highfrac);*/

        return true;
    }
    
    // Bottom contained in start?
    if (last <= start->last)
        return false;
    
    return true;
}


// --------------------------------------------------------------------------
// HWR_ProjectSprite
//  Generates a vissprite for a thing if it might be visible.
// --------------------------------------------------------------------------
static void HWR_ProjectSprite (mobj_t* thing)
{
    gr_vissprite_t*     vis;

    float               tr_x;
    float               tr_y;
    
    float               tx;
    float               ty;
    float               tz;

    float               x1;
    float               x2;

    spritedef_t*        sprdef;
    spriteframe_t*      sprframe;
    int                 lump;
    unsigned            rot;
    boolean             flip;
    angle_t             ang;

    // transform the origin point
    tr_x = ((float)thing->x * crapmul) - gr_viewx;
    tr_y = ((float)thing->y * crapmul) - gr_viewy;

    // rotation around vertical axis
    tz = (tr_x * gr_viewcos) + (tr_y * gr_viewsin); 

    // thing is behind view plane?
    if (tz < ZCLIP_PLANE)
        return;

    tx = (tr_x * gr_viewsin) - (tr_y * gr_viewcos);

    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if ((unsigned)thing->sprite >= numsprites)
        I_Error ("HWR_ProjectSprite: invalid sprite number %i ",
                 thing->sprite);
#endif

    //Fab:02-08-98: 'skin' override spritedef currently used for skin
    if (thing->skin)
        sprdef = &((skin_t *)thing->skin)->spritedef;
    else
        sprdef = &sprites[thing->sprite];

#ifdef RANGECHECK
    if ( (thing->frame&FF_FRAMEMASK) >= sprdef->numframes )
        I_Error ("HWR_ProjectSprite: invalid sprite frame %i : %i ",
                 thing->sprite, thing->frame);
#endif
    sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate) {
        // choose a different rotation based on player view
        ang = R_PointToAngle (thing->x, thing->y);          // uses viewx,viewy
        rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
        //Fab: lumpid is the index for spritewidth,spriteoffset... tables
        lump = sprframe->lumpid[rot];
        flip = (boolean)sprframe->flip[rot];
    } else {
        // use single rotation for all views
        rot = 0;                        //Fab: for vis->patch below
        lump = sprframe->lumpid[0];     //Fab: see note above
        flip = (boolean)sprframe->flip[0];
    }
    
    
    // calculate edges of the shape
    tx -= ((float)spriteoffset[lump] * crapmul);

    // project x
    x1 = gr_windowcenterx + (tx * gr_centerx / tz);

    // left edge off the right side?
    if (x1 > gr_viewwidth)
        return;

  //faB:tr_x doesnt matter
  tr_x = x1;
    
    x1 = tx;

    tx += ((float)spritewidth[lump] * crapmul);
    x2 = gr_windowcenterx + (tx * gr_centerx / tz);

    // right edge off the left side
    if (x2 < 0)
        return;

    // sprite completely hidden ?
    if (!HWR_ClipSpriteToSolidSegs((int)tr_x,(int)x2))
        return;

    //
    // store information in a vissprite
    //
    vis = HWR_NewVisSprite ();
    vis->x1 = x1;
    vis->x2 = tx;
    vis->tz = tz;
    vis->patch = sprframe->lumppat[rot];
    vis->flip = flip;

    // set top/bottom coords
    /*
    ty = ((float)thing->z * crapmul) - gr_viewz;
    wallVerts[0].y = wallVerts[1].y = ty;
    wallVerts[2].y = wallVerts[3].y = ty + ((float)spritetopoffset[lump] * crapmul);*/
    ty = ((float)thing->z * crapmul) - gr_viewz + ((float)spritetopoffset[lump] * crapmul);
    vis->ty = ty;

    //
    // determine the colormap (lightlevel & special effects)
    //
    if (thing->frame & (FF_TRANSMASK|FF_SMOKESHADE))
    {
        //translucency = (thing->frame & FF_TRANSMASK) >> 16;     //1-5
        vis->translucency = 0x80;
    }
    else if (thing->flags & MF_SHADOW)
    {
        vis->translucency = 0xC0;
    }
    else
    {
        vis->translucency = 0;
    }

    vis->sectorlight = sectorlight;
}


// --------------------------------------------------------------------------
// HWR_DrawPSprite
// --------------------------------------------------------------------------
#define BASEYCENTER           (BASEVIDHEIGHT/2)
static void HWR_DrawPSprite (pspdef_t* psp)
{
    spritedef_t*        sprdef;
    spriteframe_t*      sprframe;
    int                 lump;
    boolean             flip;

    wallVert3D          wallVerts[4];
    wallVert2D          projVerts[4];
    wallVert2D          outVerts[8];
    int                 wClipVerts;
    int                 i;
    wallVert3D          *wv;
    float               tx;
    float               ty;
    float               x1;
    float               x2;

    GlidePatch_t*       gpatch;      //sprite patch converted to 3Dfx

    // decide which patch to use
#ifdef RANGECHECK
    if ( (unsigned)psp->state->sprite >= numsprites)
        I_Error ("HWR_ProjectSprite: invalid sprite number %i ",
                 psp->state->sprite);
#endif
    sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
    if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
        I_Error ("HWR_ProjectSprite: invalid sprite frame %i : %i ",
                 psp->state->sprite, psp->state->frame);
#endif
    sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

    //Fab:debug
    //if (sprframe==NULL)
    //    I_Error("sprframes NULL for state %d\n", psp->state - states);

    lump = sprframe->lumpid[0];
    flip = (boolean)sprframe->flip[0];

    // calculate edges of the shape

    tx = (float)(psp->sx - ((BASEVIDWIDTH/2)<<FRACBITS)) * crapmul;
    tx -= ((float)spriteoffset[lump] * crapmul);
    x1 = gr_windowcenterx + (tx * gr_pspritexscale);
    
    wallVerts[3].x = wallVerts[0].x = tx;

    // off the right side
    if (x1 > gr_viewwidth)
        return;

    tx += ((float)spritewidth[lump] * crapmul);
    x2 = gr_windowcenterx + (tx * gr_pspritexscale) - 1;

    wallVerts[2].x = wallVerts[1].x = tx;

    // off the left side
    if (x2 < 0)
        return;

    //  3--2
    //  | /|
    //  |/ |
    //  0--1
    wallVerts[0].z = wallVerts[1].z = wallVerts[2].z = wallVerts[3].z = 1;
    wallVerts[0].w = wallVerts[1].w = wallVerts[2].w = wallVerts[3].w = 1;

    // cache sprite graphics
    gpatch = W_CachePatchNum (sprframe->lumppat[0], PU_CACHE);

    // set top/bottom coords
    ty = (float)(psp->sy - spritetopoffset[lump]) * crapmul;
    wallVerts[3].y = wallVerts[2].y = (float)BASEYCENTER - ty;

    ty += gpatch->height;
    wallVerts[0].y = wallVerts[1].y = (float)BASEYCENTER - ty;
    

    if (flip) {
        wallVerts[0].s = wallVerts[3].s = gpatch->max_s;
        wallVerts[2].s = wallVerts[1].s = 0.5f;
    }else{
        wallVerts[0].s = wallVerts[3].s = 0.5f;
        wallVerts[2].s = wallVerts[1].s = gpatch->max_s;
    }
    wallVerts[3].t = wallVerts[2].t = 0.5f;
    wallVerts[0].t = wallVerts[1].t = gpatch->max_t;

    // project clipped vertices
    wv = wallVerts;
    for (i=0; i<4; i++,wv++)
    {
        projVerts[i].x = gr_windowcenterx + (wv->x * gr_pspritexscale);
        projVerts[i].y = gr_windowcentery - (wv->y * gr_pspriteyscale);
        
        projVerts[i].oow = 1.0f;
        projVerts[i].sow = wv->s;
        projVerts[i].tow = wv->t;
    }

    // clip 2d polygon to view window
    wClipVerts = ClipToView (projVerts, outVerts, 4);

    // not even a triangle to draw ?
    if (wClipVerts < 3)
        return;
    
    // cache the patch in the graphics card memory
    //CONS_Printf ("downloading converted patch\n");

    HWD.pfnSetTexture (&gpatch->mipmap);
    HWD.pfnDrawPolygon (outVerts, 4, 0xffff0000 );

    /*
    vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid = 
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
    vis->scale = pspriteyscale;  //<<detailshift;

    if (flip) {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = spritewidth[lump]-1;
    }
    else {
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

    HWR_DrawVisSprite (vis, vis->x1, vis->x2);
    */
}


// --------------------------------------------------------------------------
// HWR_DrawPlayerSprites
// --------------------------------------------------------------------------
static void HWR_DrawPlayerSprites (void)
{
    int         i;
    unsigned char   light;
    pspdef_t*   psp;

    // set common states for rendering Player Sprites
    HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_TEXTURE);

    // get light level
    light = (unsigned char)viewplayer->mo->subsector->sector->lightlevel & 0xff;
    HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, (light<<24) | 0xf00000);

    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);

    // add all active psprites
    for (i=0, psp=viewplayer->psprites;
         i<NUMPSPRITES;
         i++,psp++)
    {
        if (psp->state)
            HWR_DrawPSprite (psp);
    }

    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
}


// ==========================================================================
//
// ==========================================================================
extern int skytexture;
static void    HWR_DrawSkyBackground (player_t* player)
{
    wallVert2D      v[4];
    int         angle;
    float f;

//  3--2
//  | /|
//  |/ |
//  0--1
    HWR_GetTexture (skytexture);

    v[0].x = v[3].x = gr_viewwindowx;
    v[2].x = v[1].x = gr_viewwindowx + gr_viewwidth - 1;
    v[3].y = v[2].y = gr_viewwindowy;
    v[0].y = v[1].y = gr_viewwindowy + gr_viewheight - 1;

    v[0].oow = v[1].oow = v[2].oow = v[3].oow = 1;

    angle = (dup_viewangle + gr_xtoviewangle[0])>>ANGLETOSKYSHIFT;
    angle &= 255;

    v[0].sow = v[3].sow = (float)angle;
    v[2].sow = v[1].sow = (float)angle - 255;

    f=20+80*FIXED_TO_FLOAT(finetangent[(2048-((int)player->aiming>>ANGLETOFINESHIFT)) & FINEMASK]);
//    CONS_Printf("f =%f\n",f);
    if(f<0) f=0;
    if(f>240-127) f=240-127;
    v[3].tow = v[2].tow = f;   //-player->aiming; // FIXME FAB
    v[0].tow = v[1].tow = f+127; //player->aiming + 127;      //suppose 256x128 sky...

    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);
    HWD.pfnDrawPolygon (v,4,0);    
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
}


// ==========================================================================
// HWR_ClearView : clear the viewwindow, with maximum z value
// ==========================================================================
//  3--2
//  | /|
//  |/ |
//  0--1
static void HWR_ClearView (void)
{
/*
    wallVert2D      v[4];

    v[0].x = v[3].x = gr_windowcenterx - (gr_viewwidth/2);
    v[2].x = v[1].x = gr_windowcenterx + (gr_viewwidth/2) - 1;
    v[3].y = v[2].y = gr_windowcentery - (gr_viewheight/2);
    v[0].y = v[1].y = gr_windowcentery + (gr_viewheight/2) - 1;

    v[0].oow = v[1].oow = v[2].oow = v[3].oow = 1.0f / gr_wrange[0];

        HWD.pfnSetState (HWD_SET_COLORSOURCE, HWD_COLORSOURCE_CONSTANT);
*/
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);  //just in case..
    HWD.pfnSetState (HWD_SET_COLORMASK, false);
    HWD.pfnClipRect ((int)gr_viewwindowx,
                     (int)gr_viewwindowy,
                     (int)(gr_viewwindowx + gr_viewwidth),
                     (int)(gr_viewwindowy + gr_viewheight));
    //HWD.pfnDrawPolygon (v,4,0);    
    HWD.pfnClearBuffer (0, 0, HWD_CLEARDEPTH_MAX);
    
    //disable clip window - set to full size
    HWD.pfnClipRect (0,0,VIDWIDTH,VIDHEIGHT);
    HWD.pfnSetState (HWD_SET_COLORMASK, true);
}


// ==========================================================================
// SetViewSize : set projection and scaling values depending on the
//               view window size
// ==========================================================================
void HWR_SetViewSize (int blocks)
{
    // setup view size

    // clamping viewsize is normally not needed coz it's done in R_ExecuteSetViewSize()
    // BEFORE calling here
    if ( blocks<3 || blocks>12 )
        blocks = 10;
    if ( blocks > 10 ) {
        gr_viewwidth = (float)VIDWIDTH;
        gr_viewheight = (float)VIDHEIGHT;
    } else {
        gr_viewwidth = (float) ((blocks*VIDWIDTH/10) & ~7);
        gr_viewheight = (float) ((blocks*(VIDHEIGHT-STAT_HEIGHT)/10) & ~1);
    }

    if( cv_splitscreen.value )
        gr_viewheight /= 2;

    gr_centerx = gr_viewwidth / 2;
    gr_basecentery = gr_viewheight / 2; //note: this is (gr_centerx * gr_viewheight / gr_viewwidth)
    //((VIDHEIGHT*gr_centerx*BASEVIDWIDTH)/BASEVIDHEIGHT)/VIDWIDTH;
    
    // top left corner of view window
    gr_viewwindowx = (VIDWIDTH - gr_viewwidth) / 2;
    if (gr_viewwidth == VIDWIDTH)
        gr_baseviewwindowy = 0;
    else
        gr_baseviewwindowy = (VIDHEIGHT-STAT_HEIGHT-gr_viewheight) / 2;

    //gr_basecentery = gr_viewheight / 2;// * VIDWIDTH / VIDHEIGHT; // * ORIGINAL_ASPECT;
    gr_windowcenterx = (float)(VIDWIDTH / 2);
    if (gr_viewwidth == VIDWIDTH)
        gr_basewindowcentery = gr_viewheight / 2;               // window top left corner at 0,0
    else
        gr_basewindowcentery = (float)((VIDHEIGHT-STAT_HEIGHT) / 2);

    gr_pspritexscale = gr_viewwidth / BASEVIDWIDTH; 
    gr_pspriteyscale = ((VIDHEIGHT*gr_pspritexscale*BASEVIDWIDTH)/BASEVIDHEIGHT)/VIDWIDTH;

    // scalefrustum : scales the frustum up/down to the viewheight, and does correct the
    //                aspect ratio so that it looks like the original Doom aspect ratio.
    gr_scalefrustum = VIDHEIGHT * ORIGINAL_ASPECT / VIDWIDTH      // original aspect ratio
                    * gr_viewwidth / gr_viewheight;                     // scale height to frustum height

}


//
// Activates texture draw mode
//
static void HWR_SetTexturedDraw (void)
{
    GrTextureFilterMode_t   grfilter;

    HWD.pfnSetState (HWD_SET_TEXTURECOMBINE, HWD_TEXTURECOMBINE_NORMAL);

    grfilter = cv_grfiltermode.value ? HWD_SET_TEXTUREFILTER_BILINEAR : HWD_SET_TEXTUREFILTER_POINTSAMPLED;
    HWD.pfnSetState (HWD_SET_TEXTUREFILTERMODE, grfilter);

    HWD.pfnSetState (HWD_SET_MIPMAPMODE, HWD_MIPMAP_DISABLE);

    HWD.pfnSetState (HWD_SET_COLORSOURCE, HWD_COLORSOURCE_CONSTANTALPHA_SCALE_TEXTURE);
    //note: use the alpha constant per-subsector, using the sector lightlevel as alpha constant!
    HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_CONSTANT);

    HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, 0xffffffff);
}


// ==========================================================================
//
// ==========================================================================
void HWR_RenderPlayerView (int viewnumber, player_t* player)
{
    float   viewangle_deg;
    float   tr_x, tr_z;
static    float   distance = BASEVIDWIDTH;
    angle_t saveangle;

    // note: sets viewangle, viewx, viewy, viewz
    R_SetupFrame (player);
    
    // copy view cam position for local use
    dup_viewx = viewx;
    dup_viewy = viewy;
    dup_viewz = viewz;
    dup_viewangle = viewangle;
    
    // set window position
    gr_centery = gr_basecentery;
    gr_viewwindowy = gr_baseviewwindowy;
    gr_windowcentery = gr_basewindowcentery;
    if ( cv_splitscreen.value && viewnumber==1 ) {
        //gr_centery += (VIDHEIGHT/2);
        gr_viewwindowy += (VIDHEIGHT/2);
        gr_windowcentery += (VIDHEIGHT/2);
    }

    // hmm solidsegs probably useless here
    //R_ClearDrawSegs ();
    // useless
    //R_ClearPlanes (player);
    //HWR_ClearSprites ();
        
    // check for new console commands.
    NetUpdate ();
        
    //gr_projection = 320.0f;
    viewangle_deg = ((float)(dup_viewangle>>ANGLETOFINESHIFT)) * 360.0f / ((float)FINEANGLES);
    tr_x = ((float)dup_viewx) * crapmul;
    tr_z = ((float)dup_viewy) * crapmul;
        
    gr_viewx = ((float)dup_viewx) * crapmul;
    gr_viewy = ((float)dup_viewy) * crapmul;
    gr_viewz = ((float)dup_viewz) * crapmul;
    gr_viewsin = (float)sin( viewangle_deg * DEGREE );
    gr_viewcos = (float)cos( viewangle_deg * DEGREE );

    //mega lame test of look up/down
    gr_viewludsin = (float)sin( ((player->aiming+ANG90)>>ANGLETOFINESHIFT) * 2*PI / FINEANGLES ); //(float)sin( (player->aiming+90) * DEGREE );
    gr_viewludcos = (float)cos( ((player->aiming+ANG90)>>ANGLETOFINESHIFT) * 2*PI / FINEANGLES ); //(float)cos( (player->aiming+90) * DEGREE );

    gr_spriteludsin = gr_viewludsin;
    gr_spriteludcos = gr_viewludcos;
    //gr_spriteludsin = (float)sin( ((((signed)player->aiming>>1)+ANG90)>>ANGLETOFINESHIFT) * 2*PI / FINEANGLES );
    //gr_spriteludcos = (float)cos( ((((signed)player->aiming>>1)+ANG90)>>ANGLETOFINESHIFT) * 2*PI / FINEANGLES );
    //gr_spriteludsin = (float)sin( ((player->aiming>>1)+90) * DEGREE );
    //gr_spriteludcos = (float)cos( ((player->aiming>>1)+90) * DEGREE );

    //------------------- z buffer for sorting easy ... ----------------------
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);

    //------------------------------------------------------------------------
    
    HWR_ClearView ();

    //------------------- shaded colour mode ---------------------------------
    if (!drawtextured)
    {
            HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, 0x00ffc060);
            HWD.pfnSetState (HWD_SET_COLORSOURCE, HWD_COLORSOURCE_CONSTANT);
            HWD.pfnSetState (HWD_SET_CULLMODE, false);
    }
    //------------------------------------------------------------------------

    //------------------- set texture mapped drawing -------------------------
    if (drawtextured)
    {
        HWR_SetTexturedDraw ();

        HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_NONE);
    }
    //------------------------------------------------------------------------
    if (cv_grfog.value)
        HWR_FoggingOn ();
    //------------------------------------------------------------------------

    //tlConClear ();

    // the default while the view is rendered is wrap around
    HWD.pfnSetState (HWD_SET_TEXTURECLAMP, HWD_TEXTURE_WRAP_XY);

    if (cv_grsky.value)
        HWR_DrawSkyBackground (player);

    if (!cv_grzbuffer.value)
        HWD.pfnSetState (HWD_SET_DEPTHMASK, false);

//hackbbox[0] = viewy + (256<<FRACBITS);
//hackbbox[1] = viewy - (256<<FRACBITS);
//hackbbox[2] = viewx - (256<<FRACBITS);
//hackbbox[3] = viewx + (256<<FRACBITS);
    HWR_ClearSprites ();

    saveangle = dup_viewangle;
    HWR_ClearClipSegs ();
    HWR_RenderBSPNode (numnodes-1);

    if (cv_grcrappymlook.value)
    {
        dup_viewangle += ANG90;
        HWR_ClearClipSegs ();
        HWR_RenderBSPNode (numnodes-1); //lefT

        dup_viewangle += ANG90;
        HWR_ClearClipSegs ();
        HWR_RenderBSPNode (numnodes-1); //back

        dup_viewangle += ANG90;
        HWR_ClearClipSegs ();
        HWR_RenderBSPNode (numnodes-1); //right
        dup_viewangle = saveangle;
    }

    //
    // reset defaults for non-drawing of view
    //
    // don't clamp or we get bilinear black crap wrap around
    HWD.pfnSetState (HWD_SET_TEXTURECLAMP, HWD_TEXTURE_CLAMP_XY);

    // translucency with alpha from patch
    HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);

    // get alpha from patch
    HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_TEXTURE);

    // reject all pixels too translucent, don't let too much blur
    HWD.pfnSetState (HWD_SET_ALPHATESTREFVALUE, cv_gralpha.value&0xff);
    HWD.pfnSetState (HWD_SET_ALPHATESTFUNC, GR_CMP_GEQUAL);


    //------------------------------------------------------------------------

        /*grTexFilterMode( GR_TMU0,
                     GR_TEXTUREFILTER_POINT_SAMPLED,             //POINT_SAMPLED
                     GR_TEXTUREFILTER_POINT_SAMPLED );           //POINT_SAMPLED ouch!
    */
    /*tlConOutput ("dummy\n");
    tlConOutput ("subsectors: %d/%d\n", sscount, numsubsectors);*/
    //HWR_RenderWithPortals ();

    // Check for new console commands.
    NetUpdate ();

    HWR_DrawSprites ();

    // Check for new console commands.
    NetUpdate ();
        
    //R_DrawMasked ();

    // draw the psprites on top of everything
    //  but does not draw on side views
    if (!viewangleoffset && !camera.chase && cv_psprites.value)
            HWR_DrawPlayerSprites ();
    
    //------------------------------------------------------------------------
    // put it off for menus etc
    if (cv_grfog.value)
        HWR_FoggingOff ();


        // Check for new console commands.
    //NetUpdate ();

    //-----------------------------------------------------------------------
    // Defaults for menu
    //-----------------------------------------------------------------------
    // translucency with alpha from patch
    HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);
    // get alpha from patch
    HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_TEXTURE);
    // reject all pixels too translucent, don't let too much blur
    HWD.pfnSetState (HWD_SET_ALPHATESTREFVALUE, cv_gralpha.value&0xff);
    HWD.pfnSetState (HWD_SET_ALPHATESTFUNC, GR_CMP_GEQUAL);
    // restore full bright for menus etc.
    HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, 0xffffffff);
}



// ==========================================================================
//                                    FLOOR & CEILING CONVEX POLYS GENERATION
// ==========================================================================


//debug counters
static int nobackpoly=0;
static int skipcut=0;
static int totalsubsecpolys=0;

// --------------------------------------------------------------------------
// Polygon fast alloc / free
// --------------------------------------------------------------------------
#define POLYPOOLSIZE   512000    // may be much over what is needed
                                //TODO: check out how much is used
static byte*    gr_polypool;
static byte*    gr_ppcurrent;
static int      gr_ppfree;

// allocate  pool for fast alloc of polys
static void HWR_InitPolyPool (void)
{
    CONS_Printf ("HWR_InitPolyPool() : allocating %d bytes\n", POLYPOOLSIZE);
    gr_polypool = (byte*) malloc (POLYPOOLSIZE);
    if (!gr_polypool)
        I_Error ("HWR_InitPolyPool() : couldn't malloc polypool\n");
    gr_ppfree = POLYPOOLSIZE;
    gr_ppcurrent = gr_polypool;
}

static void HWR_FreePolyPool (void)
{
    if (gr_polypool)
        free (gr_polypool);
}

// only between levels, clear poly pool
static void HWR_ClearPolys (void)
{
    gr_ppcurrent = gr_polypool;
    gr_ppfree = POLYPOOLSIZE;
}

static poly_t* HWR_AllocPoly (int numpts)
{
    poly_t*     p;
    int         size;

    size = sizeof(poly_t) + sizeof(polyvertex_t) * numpts;
    if (gr_ppfree < size)
        I_Error ("allocpoly() : no more memory %d bytes left, %d bytes needed\n", gr_ppfree, size);

    p = (poly_t*)gr_ppcurrent;
    p->numpts = numpts;
    gr_ppcurrent += size;
    gr_ppfree -= size;
    return p;
}

//TODO: polygons should be freed in reverse order for efficiency,
// for now don't free because it doenst' free in reverse order
static void HWR_FreePoly (poly_t* poly)
{
    int         size;
    
    size = sizeof(poly_t) + sizeof(polyvertex_t) * poly->numpts;
    //mempoly -= polysize;
}


// Return interception along bsp line,
// with the polygon segment
//
static float  bspfrac;
static polyvertex_t* fracdivline (node_t* bsp, polyvertex_t* v1, polyvertex_t* v2)
{
static polyvertex_t  pt;
    float       frac;
    float       num;
    float       den;
    float       v1x;
    float       v1y;
    float       v1dx;
    float       v1dy;
    float       v2x;
    float       v2y;
    float       v2dx;
    float       v2dy;

    // a segment of a polygon
    v1x  = v1->x;
    v1y  = v1->y;
    v1dx = v2->x - v1->x;
    v1dy = v2->y - v1->y;

    // the bsp partition line
    v2x  = (float)bsp->x * crapmul;
    v2y  = (float)bsp->y * crapmul;
    v2dx = (float)bsp->dx* crapmul;
    v2dy = (float)bsp->dy* crapmul;

    den = v2dy*v1dx - v2dx*v1dy;
    if (den == 0)
        return NULL;       // parallel

    // first check the frac along the polygon segment,
    // (do not accept hit with the extensions)
    num = (v2x - v1x)*v2dy + (v1y - v2y)*v2dx;
    frac = num / den;
    if (frac<0 || frac>1)
        return NULL;

    // now get the frac along the BSP line
    // which is useful to determine what is left, what is right
    den = v1dy*v2dx - v1dx*v2dy;
    if (den == 0)
        return NULL;       // parallel
    num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
    frac = num / den;
  bspfrac = frac;

    // find the interception point along the partition line
    pt.x = v2x + v2dx*frac;
    pt.y = v2y + v2dy*frac;

    return &pt;
}


// if two vertice coords have a x and/or y difference
// of less or equal than 1 FRACUNIT, they are considered the same
// point. Note: hardcoded value, 1.0f could be anything else.
boolean SameVertice (polyvertex_t* p1, polyvertex_t* p2)
{
    
    float diff;
    diff = p2->x - p1->x;
    if (diff < -1 || diff > 1)
       return false;
    diff = p2->y - p1->y;
    if (diff < -1 || diff > 1)
       return false;
       
    /*
    if (p1->x != p2->x)
        return false;
    if (p1->y != p2->y)
        return false;
    */

    // p1 and p2 are considered the same vertex
    return true;
}


// split a _CONVEX_ polygon in two convex polygons
// outputs:
//   frontpoly : polygon on right side of bsp line
//   backpoly  : polygon on left side
//
static void SplitPoly (node_t* bsp,                    //splitting parametric line
                poly_t* poly,                   //the convex poly we split
                poly_t** frontpoly,             //return one poly here
                poly_t** backpoly)              //return the other here
{
    int     i,j;
    polyvertex_t *pv;

    int          ps,pe;
    int          nptfront,nptback;
    polyvertex_t vs;
    polyvertex_t ve;
    polyvertex_t lastpv;
    float        fracs,frace;   //used to tell which poly is on
                                // the front side of the bsp partition line
    poly_t*     swappoly;
    int         psonline, peonline;

    ps = pe = -1;
    psonline = peonline = 0;

    for (i=0; i<poly->numpts; i++)
    {
        j=i+1;
        if (j==poly->numpts) j=0;

        // start & end points
        pv = fracdivline (bsp, &poly->pts[i], &poly->pts[j]);

        if (pv)
        {
            if (ps<0) {
                ps = i;
                vs = *pv;
                fracs = bspfrac;
            }
            else {
                //the partition line traverse a junction between two segments
                // or the two points are so close, they can be considered as one
                // thus, don't accept, since split 2 must be another vertex
                if (SameVertice(pv, &lastpv))
                {
                    if (pe<0) {
                        ps = i;
                        psonline = 1;
                    }
                    else {
                        pe = i;
                        peonline = 1;
                    }
                }else{
                    if (pe<0) {
                        pe = i;
                        ve = *pv;
                        frace = bspfrac;
                    }
                    else {
                    // a frac, not same vertice as last one
                    // we already got pt2 so pt 2 is not on the line,
                    // so we probably got back to the start point
                    // which is on the line
                        if (SameVertice(pv, &vs))
                            psonline = 1;
                        break;
                    }
                }
            }

            // remember last point intercept to detect identical points
            lastpv = *pv;
        }
    }

    // no split : the partition line is either parallel and
    // aligned with one of the poly segments, or the line is totally
    // out of the polygon and doesn't traverse it (happens if the bsp
    // is fooled by some trick where the sidedefs don't point to
    // the right sectors)
    if (ps<0)
    {
        //I_Error ("SplitPoly: did not split polygon (%d %d)\n"
        //         "debugpos %d",ps,pe,debugpos);

        // this eventually happens with 'broken' BSP's that accept
        // linedefs where each side point the same sector, that is:
        // the deep water effect with the original Doom

        //TODO: make sure front poly is to front of partition line?

        *frontpoly = poly;
        *backpoly  = NULL;
        return;
    }

    if (ps>=0 && pe<0)
    {
        //I_Error ("SplitPoly: only one point for split line (%d %d)",ps,pe);
        *frontpoly = poly;
        *backpoly  = NULL;
        return;
    }
    if (pe<=ps)
        I_Error ("SplitPoly: invalid splitting line (%d %d)",ps,pe);

    // number of points on each side, _not_ counting those
    // that may lie just one the line
    nptback  = pe - ps - peonline;
    nptfront = poly->numpts - peonline - psonline - nptback;

    if (nptback>0)
       *backpoly = HWR_AllocPoly (2 + nptback);
    else
       *backpoly = NULL;
    if (nptfront)
       *frontpoly = HWR_AllocPoly (2 + nptfront);
    else
       *frontpoly = NULL;

    // generate FRONT poly
    if (*frontpoly)
    {
        pv = (*frontpoly)->pts;
        *pv++ = vs;
        *pv++ = ve;
        i = pe;
        do {
            if (++i == poly->numpts)
               i=0;
            *pv++ = poly->pts[i];
        } while (i!=ps && --nptfront);
    }

    // generate BACK poly
    if (*backpoly)
    {
        pv = (*backpoly)->pts;
        *pv++ = ve;
        *pv++ = vs;
        i = ps;
        do {
            if (++i == poly->numpts)
               i=0;
            *pv++ = poly->pts[i];
        } while (i!=pe && --nptback);
    }

    // make sure frontpoly is the one on the 'right' side
    // of the partition line
    if (fracs>frace)
    {
        swappoly = *backpoly;
        *backpoly= *frontpoly;
        *frontpoly = swappoly;
    }
}


// use each seg of the poly as a partition line, keep only the
// part of the convex poly to the front of the seg (that is,
// the part inside the sector), the part behind the seg, is
// the void space and is cut out
//
static poly_t* CutOutSubsecPoly (seg_t* lseg, int count, poly_t* poly)
{
    int         i,j;
    
    polyvertex_t *pv;
    
    int          nump,ps,pe;
    polyvertex_t vs;
    polyvertex_t ve;
    float        fracs;
    
    fixed_t      cutseg[4];     //x,y,dx,dy as start of node_t struct
    
    poly_t*      temppoly;
    
    // for each seg of the subsector
    while (count--)
    {
        //x,y,dx,dy
        cutseg[0] = lseg->v1->x;
        cutseg[1] = lseg->v1->y;
        cutseg[2] = lseg->v2->x - lseg->v1->x;
        cutseg[3] = lseg->v2->y - lseg->v1->y;
        
        // see if it cuts the convex poly
        ps = -1;
        pe = -1;
        for (i=0; i<poly->numpts; i++)
        {
            j=i+1;
            if (j==poly->numpts)
                j=0;
            
            pv = fracdivline ((node_t*)cutseg, &poly->pts[i], &poly->pts[j]);
            
            if (pv)
            {
                if (ps<0) {
                    ps = i;
                    vs = *pv;
                    fracs = bspfrac;
                }
                else {
                    //frac 1 on previous segment,
                    //     0 on the next,
                    //the split line goes through one of the convex poly
                    // vertices, happens quite often since the convex
                    // poly is already adjacent to the subsector segs
                    // on most borders
                    if (SameVertice(pv, &vs))
                        continue;
                    
                    if (fracs<=bspfrac) {
                        nump = 2 + poly->numpts - (i-ps);
                        pe = ps;
                        ps = i;
                        ve = *pv;
                    }
                    else {
                        nump = 2 + (i-ps);
                        pe = i;
                        ve = vs;
                        vs = *pv;
                    }
                    //found 2nd point
                    break;
                }
            }
        }
        
        // there was a split
        if (ps>=0)
        {
            //need 2 points
            if (pe>=0)
            {
                // generate FRONT poly
                temppoly = HWR_AllocPoly (nump);
                pv = temppoly->pts;
                *pv++ = vs;
                *pv++ = ve;
                do {
                    if (++ps == poly->numpts)
                        ps=0;
                    *pv++ = poly->pts[ps];
                } while (ps!=pe);
                HWR_FreePoly(poly);
                poly = temppoly;
            }
            //hmmm... maybe we should NOT accept this, but this happens
            // only when the cut is not needed it seems (when the cut
            // line is aligned to one of the borders of the poly, and
            // only some times..)
            else
                skipcut++;
            //    I_Error ("CutOutPoly: only one point for split line (%d %d) %d",ps,pe,debugpos);
        }
        lseg++;
    }
    return poly;
}


// At this point, the poly should be convex and the exact
// layout of the subsector, it is not always the case,
// so continue to cut off the poly into smaller parts with
// each seg of the subsector.
//
static void HWR_SubsecPoly (int num, poly_t* poly)
{
    int          count;
    subsector_t* sub;
    extrasubsector_t* extrasub;
    seg_t*       lseg;

    sscount++;

    sub = &subsectors[num];
    count = sub->numlines;
    lseg = &segs[sub->firstline];

    if (poly) {
        poly = CutOutSubsecPoly (lseg,count,poly);
        if (poly) {
            totalsubsecpolys++;
            //extra data for this subsector
            extrasub = &extrasubsectors[num];
            extrasub->planepoly = poly;
        }
    }

}




// poly : the convex polygon that encloses all child subsectors
static void WalkBSPNode (int bspnum, poly_t* poly, unsigned short* leafnode)
{
    node_t*     bsp;

    poly_t*     backpoly;
    poly_t*     frontpoly;

    polyvertex_t*   pt;
    int     i;

    // Found a subsector?
    if (bspnum & NF_SUBSECTOR)
    {
            if (bspnum == -1)
            {
                // do we have a valid polygon ?
                if (poly && poly->numpts > 2) {
                    CONS_Printf ("Adding a new subsector !!!\n");
                    if (addsubsector == numsubsectors + NEWSUBSECTORS)
                        I_Error ("WalkBSPNode : not enough addsubsectors\n");
                    else if (addsubsector > 0x7fff)
                        I_Error ("WalkBSPNode : addsubsector > 0x7fff\n");
                    *leafnode = (unsigned short)addsubsector | NF_SUBSECTOR;
                    extrasubsectors[addsubsector].planepoly = poly;
                    addsubsector++;
                }
                
                //add subsectors without segs here?
                //HWR_SubsecPoly (0, NULL);
            }
            else
                HWR_SubsecPoly (bspnum&(~NF_SUBSECTOR), poly);
            HWR_FreePoly(poly);
            return;
    }

    bsp = &nodes[bspnum];
    SplitPoly (bsp, poly, &frontpoly, &backpoly);

    //debug
    if (!backpoly)
        nobackpoly++;

    // Recursively divide front space.
    // REMOVED TEMPORARY : IT SHOULD COMPLETE BBOX ONLY WITH SUBSECTOR 'CUTOUT' POLY
    if (frontpoly) {
        // Correct front bbox so it includes the space occupied by the convex polygon
        // (in software rendere it only needed to cover the space occupied by segs)
        for (i=0, pt=frontpoly->pts; i<frontpoly->numpts; i++,pt++)
            M_AddToBox (bsp->bbox[0], (fixed_t)(pt->x * 65536.0f), (fixed_t)(pt->y * 65536.0f));
    }
    else
        I_Error ("WalkBSPNode: no front poly ?");

    WalkBSPNode (bsp->children[0], frontpoly, &bsp->children[0]);

    // Recursively divide back space.
    if (backpoly) {
        // Correct back bbox to include floor/ceiling convex polygon
        for (i=0, pt=backpoly->pts; i<backpoly->numpts; i++,pt++)
            M_AddToBox (bsp->bbox[1], (fixed_t)(pt->x * 65536.0f), (fixed_t)(pt->y * 65536.0f));
        //   
        WalkBSPNode (bsp->children[1], backpoly, &bsp->children[1]);
    }

    // this frees the poly
    HWR_FreePoly (poly);
}


//FIXME: use Z_MAlloc() STATIC ?
static void HWR_FreeExtraSubsectors (void)
{
    /*if (gr_drawsubsectors)
        free(gr_drawsubsectors);*/
    if (extrasubsectors)
        free(extrasubsectors);
}


// call this routine after the BSP of a Doom wad file is loaded,
// and it will generate all the convex polys for the 3dfx renderer
void HWR_CreatePlanePolygons (int bspnum)
{
    poly_t*       rootp;
    polyvertex_t* rootpv;

    int     i;

    fixed_t     min_x,min_y;
    fixed_t     max_x,max_y;

    CONS_Printf ("HWR_CreatePlanePolygons() ...");

    HWR_ClearPolys ();
    
    // find min/max boundaries of map
    CONS_Printf ("Looking for boundaries of map...");
    min_x = min_y =  MAXINT;
    max_x = max_y = -MAXINT;
    for (i=0;i<numvertexes;i++)
    {
        if (vertexes[i].x < min_x)
            min_x = vertexes[i].x;
        else if (vertexes[i].x > max_x)
            max_x = vertexes[i].x;

        if (vertexes[i].y < min_y)
            min_y = vertexes[i].y;
        else if (vertexes[i].y > max_y)
            max_y = vertexes[i].y;
    }

    CONS_Printf ("Generating subsector polygons... %d subsectors present\n", numsubsectors);

    HWR_FreeExtraSubsectors ();
    // allocate extra data for each subsector present in map
    totsubsectors = numsubsectors + NEWSUBSECTORS;
    extrasubsectors = (extrasubsector_t*)malloc (sizeof(extrasubsector_t) * totsubsectors);
    if (!extrasubsectors)
        I_Error ("couldn't malloc extrasubsectors totsubsectors %d\n", totsubsectors);
    // set all data in to 0 or NULL !!!
    memset (extrasubsectors,0,sizeof(extrasubsector_t) * totsubsectors);

    // allocate table for back to front drawing of subsectors
    /*gr_drawsubsectors = (short*)malloc (sizeof(*gr_drawsubsectors) * totsubsectors);
    if (!gr_drawsubsectors)
        I_Error ("couldn't malloc gr_drawsubsectors\n");*/

    // number of the first new subsector that might be added
    addsubsector = numsubsectors;

    // construct the initial convex poly that encloses the full map
    rootp  = HWR_AllocPoly (4);
    rootpv = rootp->pts;

    rootpv->x = (float)min_x * crapmul;
    rootpv->y = (float)min_y * crapmul;  //lr
    rootpv++;
    rootpv->x = (float)min_x * crapmul;
    rootpv->y = (float)max_y * crapmul;  //ur
    rootpv++;
    rootpv->x = (float)max_x * crapmul;
    rootpv->y = (float)max_y * crapmul;  //ul
    rootpv++;
    rootpv->x = (float)max_x * crapmul;
    rootpv->y = (float)min_y * crapmul;  //ll
    rootpv++;

    WalkBSPNode (bspnum, rootp, NULL);

    //debug debug..
    if (nobackpoly)
        CONS_Printf ("no back polygon %d times\n"
                             "(should happen only with the deep water trick)",nobackpoly);

    if (skipcut)
        CONS_Printf ("%d cuts were skipped because of only one point\n",skipcut);


    CONS_Printf ("done : %d total subsector convex polygons\n", totalsubsecpolys);
}


// ==========================================================================
//                                                         TEXTURE HEAP CACHE
// ==========================================================================

static  GlideTexture_t*  gr_textures;       // for ALL Doom textures
static  GlideMipmap_t*   gr_flats;          // for ALL Doom flats

// --------------------------------------------------------------------------
// protos
// --------------------------------------------------------------------------
static void    HWR_GenerateTexture (int texnum, GlideTexture_t* grtex);

static void HWR_InitTextureCache (void)
{
    gr_textures = NULL;
    gr_flats = NULL;
}

static void HWR_FreeTextureCache (void)
{
    // first free all 3Dfx-converted graphics cached in the heap
    Z_FreeTags (PU_3DFXCACHE, PU_3DFXCACHE);

    // now the heap don't have any 'user' pointing to our 
    // texturecache info, we can free it
    if (gr_textures)
        free (gr_textures);
    if (gr_flats)
        free (gr_flats);
}

void HWR_PrepLevelCache (int numtextures, int numflats)
{
    // problem: the mipmap cache management hold a list of mipmaps.. but they are
    //           reallocated on each level..
    //sub-optimal, but 1) just need re-download stuff in 3Dfx cache VERY fast
    //   2) sprite/menu stuff mixed with level textures so can't do anything else
    //HWR_ClearMipmapCache ();
    HWR_FreeTextureCache ();

    // table of GLIDE texture info for each cachable texture
    gr_flats = malloc (sizeof(GlideMipmap_t) * numflats);
    if (!gr_flats)
        I_Error ("GLIDE can't alloc gr_flats");
    memset (gr_flats, 0, sizeof(GlideMipmap_t) * numflats);

    gr_textures = malloc (sizeof(GlideTexture_t) * numtextures);
    if (!gr_textures)
        I_Error ("GLIDE can't alloc gr_textures");
    memset (gr_textures, 0, sizeof(GlideTexture_t) * numtextures);
}


// --------------------------------------------------------------------------
// Make sure texture is downloaded and set it as the source
// --------------------------------------------------------------------------
GlideTexture_t* HWR_GetTexture (int tex)
{
    GlideTexture_t* grtex;

    grtex = &gr_textures[tex];

    if (!grtex->mipmap.downloaded &&
        !grtex->mipmap.grInfo.data )
        HWR_GenerateTexture (tex, grtex);
    
    HWD.pfnSetTexture (&grtex->mipmap);
    return grtex;
}


static void HWR_CacheFlat (GlideMipmap_t* grMipmap, int flatlumpnum)
{
    // setup the texture info
    grMipmap->grInfo.smallLodLog2 = GR_LOD_LOG2_64;
    grMipmap->grInfo.largeLodLog2 = GR_LOD_LOG2_64;
    grMipmap->grInfo.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
    grMipmap->grInfo.format = GR_TEXFMT_P_8;

    // the flat raw data needn't be converted with palettized textures
    grMipmap->grInfo.data = W_CacheLumpNum (flatlumpnum, PU_3DFXCACHE);
}


// Download a Doom 'flat' to the 3Dfx cache and make it ready for use
//FIXME: flats accross different pwads..
static void HWR_GetFlat (int flatlumpnum)
{
    GlideMipmap_t*     grmip;

    grmip = &gr_flats[flatlumpnum - firstflat];
    
    if ( !grmip->downloaded &&
         !grmip->grInfo.data )
         HWR_CacheFlat (grmip, flatlumpnum);

    HWD.pfnSetTexture (grmip);
}


static void HWR_SizePatchInCache (byte* block,
                          int   blockwidth,
                          int   blockheight,
                          int   blockmodulo,
                          int   texturewidth,
                          int   textureheight,
                          int   originx,        //where to draw the patch in the surface block
                          int   originy,
                          patch_t* realpatch)
{
    int                 x,x1,x2;
    int         col,ncols;
    fixed_t     xfrac, xfracstep;
    fixed_t     yfrac, yfracstep, position, count;
    fixed_t     scale_y;
        
    byte        *dest;
    byte        *source;
    column_t    *patchcol;

    x1 = originx;
    x2 = x1 + SHORT(realpatch->width);

    if (x1<0)
        x = 0;
    else
        x = x1;

    if (x2 > texturewidth)
        x2 = texturewidth;

    col  = x * blockwidth / texturewidth; 
    ncols= ((x2-x) * blockwidth) / texturewidth;

    
/*
    CONS_Printf("patch %dx%d texture %dx%d block %dx%d\n", SHORT(realpatch->width),
                                                            SHORT(realpatch->height),
                                                            texturewidth,
                                                            textureheight,
                                                            blockwidth,blockheight);
    CONS_Printf("      col %d ncols %d x %d\n", col, ncols, x);
  */  

    // source advance
    xfrac = 0;
    if (x1<0)
        xfrac = -x1<<16;

    xfracstep = (texturewidth << 16) / blockwidth;
    yfracstep = (textureheight<< 16) / blockheight;

    for (block += col; ncols--; block++, xfrac+=xfracstep)
    {
        patchcol = (column_t *)((byte *)realpatch
                                + LONG(realpatch->columnofs[xfrac>>16]));

        scale_y = (blockheight << 16) / textureheight;
        
        while (patchcol->topdelta != 0xff)
        {
            source = (byte *)patchcol + 3;
            count  = ((patchcol->length * scale_y) + (FRACUNIT/2)) >> 16;
            position = originy + patchcol->topdelta;

            yfrac = 0;
            //yfracstep = (patchcol->length << 16) / count;
            if (position < 0) {
                yfrac = -position<<16;
                count += (((position * scale_y) + (FRACUNIT/2)) >> 16);
                position = 0;
            }
            
            position = ((position * scale_y) + (FRACUNIT/2)) >> 16;
            if (position + count >= blockheight )
                count = blockheight - position;

            dest = block + (position*blockmodulo);
            while (count--)
            {
                *dest = source[yfrac>>16];
                dest += blockmodulo;
                yfrac += yfracstep;
            }
            patchcol = (column_t *)(  (byte *)patchcol + patchcol->length + 4);
        }
    }
}

// QUICKIE TEST ALPHA
static void HWR_SizePatchInCache2 (byte* block,
                          int   blockwidth,
                          int   blockheight,
                          int   blockmodulo,
                          int   texturewidth,
                          int   textureheight,
                          int   originx,        //where to draw the patch in the surface block
                          int   originy,
                          patch_t* realpatch)
{
    int                 x,x1,x2;
    int                 col,ncols;
    fixed_t             xfrac, xfracstep;
    fixed_t             yfrac, yfracstep, position, count;
    fixed_t             scale_y;
        
    byte        *dest;
    byte        *source;
    column_t    *patchcol;

    x1 = originx;
    x2 = x1 + SHORT(realpatch->width);

    if (x1<0)
        x = 0;
    else
        x = x1;

    if (x2 > texturewidth)
        x2 = texturewidth;

    col  = x * blockwidth / texturewidth; 
    ncols= ((x2-x) * blockwidth) / texturewidth;

/*    
    CONS_Printf("patch %dx%d texture %dx%d block %dx%d\n", SHORT(realpatch->width),
                                                            SHORT(realpatch->height),
                                                            texturewidth,
                                                            textureheight,
                                                            blockwidth,blockheight);
    CONS_Printf("      col %d ncols %d x %d\n", col, ncols, x);
  */  

    // source advance
    xfrac = 0;
    if (x1<0)
        xfrac = -x1<<16;

    xfracstep = (texturewidth << 16) / blockwidth;
    yfracstep = (textureheight<< 16) / blockheight;

    for (block += (col<<1); ncols--; block+=2, xfrac+=xfracstep)
    {
        patchcol = (column_t *)((byte *)realpatch
                                + LONG(realpatch->columnofs[xfrac>>16]));

        scale_y = (blockheight << 16) / textureheight;
        
        while (patchcol->topdelta != 0xff)
        {
            source = (byte *)patchcol + 3;
            count  = ((patchcol->length * scale_y) + (FRACUNIT/2)) >> 16;
            position = originy + patchcol->topdelta;

            yfrac = 0;
            //yfracstep = (patchcol->length << 16) / count;
            if (position < 0) {
                yfrac = -position<<16;
                count += (((position * scale_y) + (FRACUNIT/2)) >> 16);
                position = 0;
            }
            
            position = ((position * scale_y) + (FRACUNIT/2)) >> 16;
            if (position + count >= blockheight )
                count = blockheight - position;

            dest = block + (position*blockmodulo);
            while (count--)
            {
                // all opaque runs with alpha 0xff
                *((unsigned short*)dest) = (0xff<<8) | source[yfrac>>16];
                dest += blockmodulo;
                yfrac += yfracstep;
            }
            patchcol = (column_t *)(  (byte *)patchcol + patchcol->length + 4);
        }
    }
}

//   Build the full textures from patches.
static  GrLOD_t     gr_lods[9] = {
    GR_LOD_LOG2_256,
    GR_LOD_LOG2_128,
    GR_LOD_LOG2_64,
    GR_LOD_LOG2_32,
    GR_LOD_LOG2_16,
    GR_LOD_LOG2_8,
    GR_LOD_LOG2_4,
    GR_LOD_LOG2_2,
    GR_LOD_LOG2_1
};

typedef struct {
    GrAspectRatio_t aspect;
    float           max_s;
    float           max_t;
} booring_aspect_t;

static  booring_aspect_t gr_aspects[8] = {
    {GR_ASPECT_LOG2_1x1, 255, 255},
    {GR_ASPECT_LOG2_2x1, 255, 127},
    {GR_ASPECT_LOG2_4x1, 255,  63},
    {GR_ASPECT_LOG2_8x1, 255,  31},

    {GR_ASPECT_LOG2_1x1, 255, 255},
    {GR_ASPECT_LOG2_1x2, 127, 255},
    {GR_ASPECT_LOG2_1x4,  63, 255},
    {GR_ASPECT_LOG2_1x8,  31, 255}
};


// --------------------------------------------------------------------------
// Values set after a call to HWR_ResizeBlock()
// --------------------------------------------------------------------------
static  int     blocksize;
static  int     blockwidth;
static  int     blockheight;
static  float   max_s, max_t;


//note :  8bit (1 byte per pixel) palettized format
static void HWR_ResizeBlock ( int originalwidth,
                      int originalheight,
                      GrTexInfo*    grInfo )
{
    int     j,k;
    int     max,min;

    // find a power of 2 width/height
    if (cv_grrounddown.value)
    {
        blockwidth = 256;
        while (originalwidth < blockwidth)
            blockwidth >>= 1;
        if (blockwidth<1)
            I_Error ("GLIDE GenerateTexture : too small");

        blockheight = 256;
        while (originalheight < blockheight)
            blockheight >>= 1;
        if (blockheight<1)
            I_Error ("GLIDE GenerateTexture : too small");
    }
    else
    {
        //size up to nearest power of 2
        blockwidth = 1;
        while (blockwidth < originalwidth)
            blockwidth <<= 1;
        // scale down the original graphics to fit in 256
        if (blockwidth>256)
            blockwidth = 256;
            //I_Error ("GLIDE GenerateTexture : too big");

        //size up to nearest power of 2
        blockheight = 1;
        while (blockheight < originalheight)
            blockheight <<= 1;
        // scale down the original graphics to fit in 256
        if (blockheight>256)
            blockheight = 255;
            //I_Error ("GLIDE GenerateTexture : too big");
    }

    // do the boring LOD stuff.. blech!
    if (blockwidth >= blockheight) {
        max = blockwidth;
        min = blockheight;
    }else{
        max = blockheight;
        min = blockwidth;
    }
    
    for (k=256, j=0; k > max; j++)
        k>>=1;
    grInfo->smallLodLog2 = gr_lods[j];
    grInfo->largeLodLog2 = gr_lods[j];
        
    for (k=max, j=0; k>min && j<4; j++)
        k>>=1;
    // aspect ratio too small for 3Dfx (eg: 8x128 is 1x16 : use 1x8)
    if (j==4){
        j=3;
        //CONS_Printf ("HWR_ResizeBlock : bad aspect ratio %dx%d\n", blockwidth,blockheight);
        if (blockwidth<blockheight)
            blockwidth = max>>3;
        else
            blockheight = max>>3;
    }
    if (blockwidth<blockheight)
        j+=4;
    grInfo->aspectRatioLog2 = gr_aspects[j].aspect;
    max_s = gr_aspects[j].max_s;
    max_t = gr_aspects[j].max_t;

    blocksize = blockwidth * blockheight;
}


//
// Create a composite texture from patches, adapt the texture size to a power of 2
// height and width for the hardware texture cache.
//
static void HWR_GenerateTexture (int texnum, GlideTexture_t* grtex)
{
    byte*               block;
    texture_t*          texture;
    texpatch_t*         patch;
    patch_t*            realpatch;

    int         i;    
    boolean     skyspecial = false; //poor hack for Legacy large skies..

    texture = textures[texnum];

    // hack the Legacy skies.. texture size is 256x128 but patch size is larger..
    if ( texture->name[0] == 'S' &&
         texture->name[1] == 'K' &&
         texture->name[2] == 'Y' &&
         texture->name[4] == 0 )
    {
        skyspecial = true;
        texture->patchcount = 1;
    }

    //CONS_Printf ("Resizing texture %.8s from %dx%d", texture->name, texture->width, texture->height);
    HWR_ResizeBlock (texture->width, texture->height, &grtex->mipmap.grInfo);

    /*
    CONS_Printf ("to %dx%d\n"
                 "         LodLog2(%d) aspectRatio(%d)\n"
                 "         s: %f t: %f\n",
                 blockwidth, blockheight,
                 grtex->mipmap.grInfo.smallLodLog2, grtex->mipmap.grInfo.aspectRatioLog2,
                 max_s,max_t);
                 */

//    CONS_Printf ("R_GenTex MULTI  %.8s size: %d\n",texture->name,blocksize);
    block = Z_Malloc (blocksize,
                      PU_STATIC,
                      &grtex->mipmap.grInfo.data);

    /*
    for (x=0;x<blockwidth;x++) {
        for (i=0; i<blockheight; i++)
            *(block + (i*blockwidth) + x) = (unsigned char)texnum + (x ^ i);
    }*/

    // set the default transparent color as background
    memset (block, HWR_PATCHES_CHROMAKEY_COLORINDEX, blocksize);
    //for (i=0;i<blockheight;i++)
    //    memset (block+(i*blockwidth), i, blockwidth);

    // Composite the columns together.
    patch = texture->patches;

    for (i=0 , patch = texture->patches;
         i<texture->patchcount;
         i++, patch++)
    {
        realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
        // correct texture size for Legacy's large skies
        if (skyspecial) {
            texture->width = SHORT(realpatch->width);
            texture->height = SHORT(realpatch->height);
        }
        HWR_SizePatchInCache ( block,
                              blockwidth, blockheight,
                              blockwidth,
                              texture->width, texture->height,
                              patch->originx, patch->originy,
                              realpatch
                            );
    }

    // make it purgable from zone memory
    // use PU_PURGELEVEL so we can Z_FreeTags all at once
    Z_ChangeTag (block, PU_3DFXCACHE);

    // setup the texture info
    grtex->mipmap.grInfo.format = GR_TEXFMT_P_8;

    //used to find the texture coord, depends on the original texture size,
    // and the hardware cached texture size , which is not always the same)
    grtex->scaleX = max_s * crapmul / (float)texture->width;
    grtex->scaleY = max_t * crapmul / (float)texture->height;
}


// grTex : 3Dfx texture cache info
//         .data : address of converted patch in heap memory
//                 user for Z_Malloc(), becomes NULL if it is purged from the cache
void HWR_Make3DfxPatch (patch_t* patch, GlidePatch_t* grPatch)
{
    GlideMipmap_t*  grMipmap;
    byte*   block;
    int     i;    
    int     newwidth,newheight;

    // point the 3Dfx texture cache info from the GlidePatch
    grMipmap = &grPatch->mipmap;
    
    // save the original patch header so that the GlidePatch can be casted
    // into a standard patch_t struct and the existing code can get the
    // orginal patch dimensions and offsets.
    grPatch->width = SHORT(patch->width);
    grPatch->height = SHORT(patch->height);
    grPatch->leftoffset = SHORT(patch->leftoffset);
    grPatch->topoffset = SHORT(patch->topoffset);
    
    // allow the v_video drawing code to recognise a 3Dfx 'converted' patch_t
    grPatch->s3Dfx[0] = '3';
    grPatch->s3Dfx[1] = 'D';
    grPatch->s3Dfx[2] = 'f';
    grPatch->s3Dfx[3] = 'x';    
    
    // find a power of 2 width/height
    //CONS_Printf ("Resizing _PATCH_ from %dx%d", SHORT(patch->width), SHORT(patch->height));

    HWR_ResizeBlock (SHORT(patch->width), SHORT(patch->height), &grMipmap->grInfo);

    // comes handy to set the texture offsets
    grPatch->max_s = max_s;
    grPatch->max_t = max_t;

    /*
    CONS_Printf (" to %dx%d\n"
                 "         LodLog2(%d) aspectRatio(%d)\n"
                 "         s: %f t: %f\n",
                 blockwidth, blockheight,
                 grMipmap->grInfo.smallLodLog2, grMipmap->grInfo.aspectRatioLog2,
                 max_s,max_t);
                 */

    block = Z_Malloc (blocksize<<1,
                      PU_STATIC,
                      &grMipmap->grInfo.data);

    // set the chroma key color as background, for transparent areas
    //for (i=0;i<blockheight;i++)
    //    memset (block+(i*blockwidth), i, blockwidth);
    
    for (i=0;i<blocksize;i++)
        *((unsigned short*)block+i) = (0x80 <<8) | HWR_PATCHES_CHROMAKEY_COLORINDEX;
    //memset (block, HWR_PATCHES_CHROMAKEY_COLORINDEX, blocksize<<1);

    // if rounddown, rounddown patches as well as textures
    if (cv_grrounddown.value) {
        newwidth = blockwidth;
        newheight = blockheight;
    }else{
        // no rounddown, do not size up patches, so they don't look 'scaled'
        if (SHORT(patch->width) < blockwidth)
            newwidth = SHORT(patch->width);
        else
            // a rounddown was forced because patch->width was too large
            newwidth = blockwidth;

        if (SHORT(patch->height) < blockheight)
            newheight = SHORT(patch->height);
        else
            newheight = blockheight;
    }

    HWR_SizePatchInCache2 ( block,
                          newwidth, newheight,
                          blockwidth<<1,
                          SHORT(patch->width), SHORT(patch->height),
                          0,0,
                          patch
                        );

    // adapt texture offsets if the patch does not occupy all the surface
    // of the 3Dfx texture
    if (newwidth < blockwidth)
        grPatch->max_s = max_s * newwidth / blockwidth;
    if (newheight < blockheight)
        grPatch->max_t = max_t * newheight / blockheight;

    grPatch->max_s;
    grPatch->max_t;

    // Now that the texture has been built in cache, it is purgable from zone memory.
    Z_ChangeTag (block, PU_3DFXCACHE);

    // setup the texture info
    grMipmap->grInfo.format = GR_TEXFMT_AP_88;
}


// ==========================================================================
//                                                         2D 'PATCH' DRAWING
// ==========================================================================

static  BOOL    gr_scale_patch;     // HWR_DrawPatch() scaling state

static  float   gr_patch_scalex;
static  float   gr_patch_scaley;


//
// Download a patch to the 3Dfx cache and make it ready for use
//
static void HWR_GetPatch (GlidePatch_t* gpatch)
{
    GlideMipmap_t*      grmip;
    byte*               ptr;
    int                 lump;

    grmip = &gpatch->mipmap;
    
    // is it in hardware cache
    if ( !grmip->downloaded &&
         !grmip->grInfo.data )
    {
            // load the software patch, PU_STATIC or the Z_Malloc for 3Dfx patch will
        // flush the software patch before the conversion! oh yeah I suffered
        lump = gpatch->patchlump;
        ptr = Z_Malloc (W_LumpLength (lump), PU_STATIC, 0);
            W_ReadLumpHeader (lump, ptr, 0);   // read full
            
        HWR_Make3DfxPatch ( (patch_t*)ptr, gpatch );

        // this is inefficient.. but the 3Dfx patch in heap is purgeable so it
        // shouldn't fragment memory, and besides the REAL cache here is the 3Dfx memory
        Z_Free (ptr);
    }

    HWD.pfnSetTexture (grmip);
}


//
// Set current scaling state for HWR_DrawPatch()
//
void HWR_ScalePatch ( BOOL bScalePatch )
{
    gr_scale_patch = bScalePatch;
}

// Draw a 2D patch to the screen
// x,y : positions relative to the original Doom resolution
// 
void HWR_DrawPatch (GlidePatch_t* gpatch, int x, int y)
{
    wallVert2D      v[4];
    float           x1,y1,x2,y2;

    // make patch ready in 3Dfx cache
    HWR_GetPatch (gpatch);

//  3--2
//  | /|
//  |/ |
//  0--1
    
    x1 = (float)(x - gpatch->leftoffset);
    y1 = (float)(y - gpatch->topoffset);
    x2 = x1 + (float)gpatch->width;
    y2 = y1 + (float)gpatch->height;

    // scale BASEVIDWIDTH/BASEVIDHEIGHT coords to current resolution
    if ( gr_scale_patch ) {
        x1 *= gr_patch_scalex;
        x2 *= gr_patch_scalex;
        y1 *= gr_patch_scaley;
        y2 *= gr_patch_scaley;
    }
    
    v[0].x = v[3].x = x1;
    v[2].x = v[1].x = x2;
    v[3].y = v[2].y = y1;
    v[0].y = v[1].y = y2;

    v[0].oow = v[1].oow = v[2].oow = v[3].oow = 1;

    v[0].sow = v[3].sow = 0.5f;
    v[2].sow = v[1].sow = gpatch->max_s+0.5f;
    v[3].tow = v[2].tow = 0.5f;
    v[0].tow = v[1].tow = gpatch->max_t+0.5f;

    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);
    HWD.pfnDrawPolygon (v,4,0);
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
}


// this one doesn't scale at all
//
void HWR_DrawNonScaledPatch (GlidePatch_t* gpatch, int x, int y)
{
    wallVert2D      v[4];
    int             x2,y2;

    // make patch ready in 3Dfx cache
    HWR_GetPatch (gpatch);

//  3--2
//  | /|
//  |/ |
//  0--1
    
    x -= gpatch->leftoffset;
    y -= gpatch->topoffset;
    x2 = x + gpatch->width;
    y2 = y + gpatch->height;

    v[0].x = v[3].x = (float)x;
    v[2].x = v[1].x = (float)x2;
    v[3].y = v[2].y = (float)y;
    v[0].y = v[1].y = (float)y2;

    v[0].oow = v[1].oow = v[2].oow = v[3].oow = 1;

    v[0].sow = v[3].sow = 0.5f;
    v[2].sow = v[1].sow = gpatch->max_s+0.5f;
    v[3].tow = v[2].tow = 0.5f;
    v[0].tow = v[1].tow = gpatch->max_t+0.5f;

    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);
    HWD.pfnDrawPolygon (v,4,0);
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
}


// ==========================================================================
//                                                            V_VIDEO.C STUFF
// ==========================================================================


// --------------------------------------------------------------------------
// Fills a box of pixels using a flat texture as a pattern
// --------------------------------------------------------------------------
void HWR_DrawFlatFill (int x, int y, int w, int h, int flatlumpnum)
{
    wallVert2D  v[4];
    
//  3--2
//  | /|
//  |/ |
//  0--1
    
    v[0].x = v[3].x = (float)x * gr_patch_scalex;
    v[2].x = v[1].x = ((float)(x+w) * gr_patch_scalex);
    v[3].y = v[2].y = (float)y * gr_patch_scaley;
    v[0].y = v[1].y = ((float)(y+h) * gr_patch_scaley);

    v[0].oow = v[1].oow = v[2].oow = v[3].oow = 1;

    //<<2 (x4) because flat is 64x64 lod and texture offsets are 0-255 for each axis
    v[0].sow = v[3].sow = (float)((x & 63)<<2);
    v[2].sow = v[1].sow = v[0].sow + (float)(w<<2);
    v[3].tow = v[2].tow = (float)((y & 63)<<2);
    v[0].tow = v[1].tow = v[3].tow + (float)(h<<2);

    HWR_GetFlat (flatlumpnum);
    
    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);
    // the default out of renderview is clamping
    HWD.pfnSetState (HWD_SET_TEXTURECLAMP, HWD_TEXTURE_WRAP_XY);
    
    HWD.pfnDrawPolygon (v,4,0);

    HWD.pfnSetState (HWD_SET_TEXTURECLAMP, HWD_TEXTURE_CLAMP_XY);
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
}


// --------------------------------------------------------------------------
// Fade down the screen so that the menu drawn on top of it looks brighter
// --------------------------------------------------------------------------
//  3--2
//  | /|
//  |/ |
//  0--1
void HWR_FadeScreenMenuBack (unsigned long color, int height)
{
    wallVert2D  v[4];

    // setup some neat-o translucency effect
    
    //cool hack 0 height is full height
    if (!height)
        height = VIDHEIGHT - 1;

    v[0].x = v[3].x = 0;
    v[2].x = v[1].x = (float)(VIDWIDTH - 1);
    v[3].y = v[2].y = 0;
    v[0].y = v[1].y = (float)height;

    v[0].oow = v[1].oow = v[2].oow = v[3].oow = 1;
    //v[0].argb = v[1].argb = v[2].argb = v[3].argb = 0x0000ff00;

    HWD.pfnSetState (HWD_SET_COLORSOURCE, HWD_COLORSOURCE_CONSTANT);
    HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_CONSTANT);
    HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);
    HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, color);
    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);

    HWD.pfnDrawPolygon (v,4,0);
    
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
    HWD.pfnSetState (HWD_SET_COLORSOURCE, HWD_COLORSOURCE_TEXTURE);
    HWD.pfnSetState (HWD_SET_ALPHASOURCE, HWD_ALPHASOURCE_TEXTURE);
    HWD.pfnSetState (HWD_SET_CONSTANTCOLOR, 0xffffffff);

    // restore default menu blend
    HWD.pfnSetState (HWD_SET_ALPHABLEND, HWD_ALPHABLEND_TRANSLUCENT);
}


// ==========================================================================
//                                                             R_DRAW.C STUFF
// ==========================================================================


// --------------------------------------------------------------------------
// Fill the space around the view window with a Doom flat texture, draw the
// beveled edges.
// 'clearlines' is useful to clear the heads up messages, when the view
// window is reduced, it doesn't refresh all the view borders.
// --------------------------------------------------------------------------
extern int st_borderpatchnum;
void HWR_DrawViewBorder (int clearlines)
{
    int         x,y;
    int         top,side;
    int         baseviewwidth,baseviewheight;
    int         basewindowx,basewindowy;
    GlidePatch_t*   patch;

    if (gr_viewwidth == VIDWIDTH)
        return;

    if (!clearlines)
        clearlines = BASEVIDHEIGHT; //refresh all

    // calc view size based on original game resolution
    baseviewwidth  = (cv_viewsize.value * BASEVIDWIDTH/10)&~7;
    baseviewheight = (cv_viewsize.value * (BASEVIDHEIGHT-ST_HEIGHT)/10)&~1;

    // crap code, clean this up, use base resolution
    top  = (BASEVIDHEIGHT - ST_HEIGHT - baseviewheight) / 2;
    side = (BASEVIDWIDTH  - baseviewwidth) / 2;

    // top
    HWR_DrawFlatFill (0, 0,
                     BASEVIDWIDTH, (top<clearlines ? top : clearlines),
                     st_borderpatchnum);
    
    // left
    if (top<clearlines)
        HWR_DrawFlatFill (0, top,
                         side, (clearlines-top < baseviewheight ? clearlines-top : baseviewheight),
                         st_borderpatchnum);    
    
    // right
    if (top<clearlines)
        HWR_DrawFlatFill (side + baseviewwidth, top,
                         side, (clearlines-top < baseviewheight ? clearlines-top : baseviewheight),
                         st_borderpatchnum);    

    // bottom
    if (top+baseviewheight<clearlines)
        HWR_DrawFlatFill (0, top+baseviewheight,
                         BASEVIDWIDTH, (clearlines-baseviewheight-top < top ? clearlines-baseviewheight-top : top),
                         st_borderpatchnum);

    //
    // draw the view borders
    //
    basewindowx = (BASEVIDWIDTH - baseviewwidth)>>1;
    if (baseviewwidth==BASEVIDWIDTH)
        basewindowy = 0;
    else
        basewindowy = (BASEVIDHEIGHT - ST_HEIGHT - baseviewheight)>>1;

    // top edge
    if (clearlines > basewindowy-8) {
        patch = W_CachePatchNum (viewborderlump[BRDR_T],PU_CACHE);
        for (x=0 ; x<baseviewwidth; x+=8)
            HWR_DrawPatch (patch,basewindowx+x,basewindowy-8);
    }
    
    // bottom edge
    if (clearlines > basewindowy+baseviewheight) {
        patch = W_CachePatchNum (viewborderlump[BRDR_B],PU_CACHE);
        for (x=0 ; x<baseviewwidth ; x+=8)
            HWR_DrawPatch (patch,basewindowx+x,basewindowy+baseviewheight);
    }
    
    // left edge
    if (clearlines > basewindowy) {
        patch = W_CachePatchNum (viewborderlump[BRDR_L],PU_CACHE);
        for (y=0 ; y<baseviewheight && (basewindowy+y < clearlines); y+=8)
            HWR_DrawPatch (patch,basewindowx-8,basewindowy+y);
    }
    
    // right edge
    if (clearlines > basewindowy) {
        patch = W_CachePatchNum (viewborderlump[BRDR_R],PU_CACHE);
        for (y=0 ; y<baseviewheight && (basewindowy+y < clearlines); y+=8)
            HWR_DrawPatch (patch,basewindowx+baseviewwidth,basewindowy+y);
    }

    // Draw beveled corners.
    if (clearlines > basewindowy-8)
        HWR_DrawPatch (W_CachePatchNum (viewborderlump[BRDR_TL],PU_CACHE),
                      basewindowx-8,
                      basewindowy-8);

    if (clearlines > basewindowy-8)
        HWR_DrawPatch (W_CachePatchNum (viewborderlump[BRDR_TR],PU_CACHE),
                      basewindowx+baseviewwidth,
                      basewindowy-8);

    if (clearlines > basewindowy+baseviewheight)
        HWR_DrawPatch (W_CachePatchNum (viewborderlump[BRDR_BL],PU_CACHE),
                      basewindowx-8,
                      basewindowy+baseviewheight);

    if (clearlines > basewindowy+baseviewheight)
        HWR_DrawPatch (W_CachePatchNum (viewborderlump[BRDR_BR],PU_CACHE),
                      basewindowx+baseviewwidth,
                      basewindowy+baseviewheight);
}


//
// Enable chroma-keying
//
static void HWR_SetupChromakey (void)
{
    HWD.pfnSetState (HWD_SET_CHROMAKEY_MODE, HWD_CHROMAKEY_ENABLE);
    // note: this define is from me! see r_glide.h
    HWD.pfnSetState (HWD_SET_CHROMAKEY_VALUE, HWR_PATCHES_CHROMAKEY_COLORVALUE);
}


// ==========================================================================
//                                                     AM_MAP.C DRAWING STUFF
// ==========================================================================

// Clear the automap part of the screen
void HWR_clearAutomap (void)
{
    HWD.pfnSetState (HWD_SET_DEPTHMASK, false);
    HWD.pfnSetState (HWD_SET_COLORMASK, true);

    // minx,miny,maxx,maxy
    HWD.pfnClipRect (0, 0, VIDWIDTH,  VIDHEIGHT - STAT_HEIGHT);

    HWD.pfnClearBuffer (0, 0, HWD_CLEARDEPTH_MAX);
    
    //reset defaults
    HWD.pfnClipRect (0, 0, VIDWIDTH, VIDHEIGHT);
    HWD.pfnSetState (HWD_SET_DEPTHMASK, true);
}


// draw a line of the automap (the clipping is already done in automap code)
void HWR_drawAMline (fline_t* fl, int color)
{
    wallVert2D  v[2];

    v[0].x = (float)fl->a.x;
    v[0].y = (float)fl->a.y;
    v[1].x = (float)fl->b.x;
    v[1].y = (float)fl->b.y;
    v[0].argb = 0xffff0000;
    v[1].argb = 0xff00ff00;

    HWD.pfnDrawLine (&v[0], &v[1]);
}


// ==========================================================================
//                                                                        FOG
// ==========================================================================

   static GrFog_t *fogtable =NULL;

#define FOG_DENSITY .5
static void HWR_InitFog (void)
{
    int     nFog;
    GrFog_t *fog;
   
    int     i,j;
    //fixed_t scale,level;

    // allocate fog table
    HWD.pfnGetState (HWD_GET_FOGTABLESIZE, &nFog);

    fogtable = (GrFog_t*) malloc(nFog * sizeof(GrFog_t));

    if (!fogtable)
        I_Error ("could not allocate fog table\n");
    else
        CONS_Printf ("Fog table size: nFog %d\n", nFog);

    fog = fogtable;

    // the table is an exponential fog table. It computes q from i using guFogTableIndexToW()
    // and then computes the fog table entries as fog[i]=(1–e -kw )·255 where k is a user-defined
    // constant, FOG_DENSITY.
    for (i=0; i<nFog; i++) {
        /* Boris: coucou je suis venu mettre le bordel dans ton code (il n'y a pas que toi qui a le droit
                  de t'ammuser a faire l'alchimiste :)
                  donc tu trouvera en commentaire the old code et le reste c'est a moi
        CONS_Printf ("fuck %d\n", (1 - exp((- FOG_DENSITY) * guFogTableIndexToW(i))) * 255);    
        j = ( (1 - exp((- FOG_DENSITY) * guFogTableIndexToW(i))) * 255);
        if (j > 255)
            j = 255;
        fog[i]=j;
        */
        // bon voila sa a l'air pas trop mal mais j'ai bien chipoter avant de trouver le "7.0"
        // faux peu etre encore chipoter un peut pour trouver le juste bon parametre
        j = (int) (exp((float)i/7.7)-1); // pour voir les rai comme avec doom original enlever le float
        if (j > 250) // faux quand meme voir quelque chose meme si c'est loin
            j = 250;
        fog[i] = j;
        CONS_Printf ("phoque[%d]=%d\n", i, fog[i]);
    }
    fog[nFog-1] = 255;
    
    /*
    255 - (a/b)
    
    for (i=0; i<nFog; i++)
    {
        CONS_Printf ("dist fog[%d] = %f\n", i, guFogTableIndexToW(i));
        j = (fixed_t) guFogTableIndexToW(i) / 16;

        scale = FixedDiv ((BASEVIDWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
        scale >>= LIGHTSCALESHIFT;
        level = 8 - scale/2;

        if (level < 0)
            level = 0;
        if (level >= 32)
            level = 31;

        fog[i] = level*32;
        CONS_Printf ("fog[%#02d] = %d   j=%d scale=%d\n", i, fog[i], j, scale);
    }
    fog[0] = 0;
    fog[nFog-1] = 255;
    */
}

static void HWR_FreeFog (void)
{
    if (fogtable) {
        free (fogtable);
    }
}

static void HWR_FoggingOn (void)
{
    HWD.pfnSetState (HWD_SET_FOG_TABLE, (int)fogtable);
    HWD.pfnSetState (HWD_SET_FOG_COLOR, 0x00000000);
    HWD.pfnSetState (HWD_SET_FOG_MODE, HWD_FOG_ENABLE);
}


static void HWR_FoggingOff (void)
{
    HWD.pfnSetState (HWD_SET_FOG_MODE, HWD_FOG_DISABLE);
}


// ==========================================================================
//                                                      GLIDE ENGINE COMMANDS
// ==========================================================================

static void CV_grPlug_OnChange (void)
{
    HWD.pfnSetState (cv_grplug.value ? HWD_ENABLE : HWD_DISABLE, HWD_SHAMELESS_PLUG);
}

static void Command_GrStats_f (void)
{
    //debug
    Z_CheckHeap (-1);

    CONS_Printf ("GlidePatch_t info headers:       %d kb\n", Z_TagUsage(PU_3DFXPATCHINFO));
    CONS_Printf ("3Dfx-converted graphics in heap: %d kb\n", Z_TagUsage(PU_3DFXCACHE));
}


// --------------------------------------------------------------------------
// screen shot
// --------------------------------------------------------------------------
void HWR_Screenshot (void)
{
    int         i;
    char        lbmname[12];

    byte*   bufw;
    unsigned short* bufr;
    byte*   dest;
    unsigned short   rgb565;

    bufr = malloc(VIDWIDTH*VIDHEIGHT*2);
    if (!bufr)
        I_Error ("no bufr");
    bufw = malloc(VIDWIDTH*VIDHEIGHT*3);
    if (!bufw)
        I_Error ("no bufw");

    //returns 16bit 565 RGB
    HWD.pfnReadRect (0, 0, VIDWIDTH, VIDHEIGHT, VIDWIDTH*2, bufr);

    for (dest = bufw,i=0; i<VIDWIDTH*VIDHEIGHT; i++)
    {
        rgb565 = bufr[i];
        *(dest++) = ((rgb565 >> 11) & 31) <<3;
        *(dest++) = ((rgb565 >> 5) & 63) <<2;
        *(dest++) = (rgb565 & 31) <<3;
    }
    
    // find a file name to save it to
    strcpy(lbmname,"DOOM000.raw");

    for (i=0 ; i<=999 ; i++)
    {
            lbmname[4] = i/100 + '0';
            lbmname[5] = i/10  + '0';
            lbmname[6] = i%10  + '0';
            if (access(lbmname,0) == -1)
                break;  // file doesn't exist
    }
    if (i<100)
    {
        // save the file
        FIL_WriteFile (lbmname, bufw, VIDWIDTH*VIDHEIGHT*3);
    }

    free(bufw);
    free(bufr);
}


// **************************************************************************
//                                                         GLIDE ENGINE SETUP
// **************************************************************************

// --------------------------------------------------------------------------
// Add Glide Engine commands & consvars
// --------------------------------------------------------------------------
static void HWR_AddEngineCommands (void)
{
    // engine state variables
    CV_RegisterVar (&cv_grsky);
    CV_RegisterVar (&cv_grfog);
    CV_RegisterVar (&cv_grplug);
    CV_RegisterVar (&cv_grfiltermode);
    CV_RegisterVar (&cv_grzbuffer);
    //CV_RegisterVar (&cv_grsoftwareview);
    CV_RegisterVar (&cv_grclipwalls);
    CV_RegisterVar (&cv_grrounddown);

    CV_RegisterVar (&cv_grcrappymlook);

    // engine development mode variables
    // - usage may vary from version to version..
    CV_RegisterVar (&cv_gralpha);
    CV_RegisterVar (&cv_grbeta);
    CV_RegisterVar (&cv_grgamma);

    // engine commands
    COM_AddCommand ("gr_stats", Command_GrStats_f);
}


// --------------------------------------------------------------------------
// Setup the hardware renderer
// --------------------------------------------------------------------------
void HWR_Startup (void)
{
static BOOL startupdone = FALSE;
    
    CONS_Printf ("HWR_Startup()\n");

    // setup GlidePatch_t scaling
    gr_patch_scalex = (float)VIDWIDTH / BASEVIDWIDTH;
    gr_patch_scaley = (float)VIDHEIGHT / BASEVIDHEIGHT;

    // do this once
    if ( startupdone == FALSE )
    {
        // add console cmds & vars
        HWR_AddEngineCommands ();
    
        HWR_InitTextureCache ();
        HWR_InitPolyPool ();
        HWR_InitFog ();

        // for test water translucent surface
        doomwaterflat = W_GetNumForName ("FWATER1");
    }

    HWR_SetupChromakey ();

    // set the default texture clamp mode
    HWD.pfnSetState (HWD_SET_TEXTURECLAMP, HWD_TEXTURE_CLAMP_XY);

    // initially set textured draw for Console startup
    HWR_SetTexturedDraw ();
    
    startupdone = TRUE;
}


// --------------------------------------------------------------------------
// Free resources allocated by the hardware renderer
// --------------------------------------------------------------------------
void HWR_Shutdown (void)
{
    CONS_Printf ("HWR_Shutdown()\n");
    //CONS_Printf ("GLIDE memory cache used : %d bytes\n", gr_cachepos - gr_cachemin);
    
    HWR_FreeExtraSubsectors ();

    HWR_FreeFog ();
    HWR_FreePolyPool ();
    HWR_FreeTextureCache ();
}
