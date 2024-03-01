
// r_draw.c : span / column drawer functions, for 8bpp and 16bpp
//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// The frame buffer is a linear one, and we need only the base address.

#include "doomdef.h"
#include "r_local.h"
#include "st_stuff.h"   //added:24-01-98:need ST_HEIGHT
#include "i_video.h"
#include "vid_copy.h"   //added:05-02-98:
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"


// ==========================================================================
//                     COMMON DATA FOR 8bpp AND 16bpp
// ==========================================================================

byte*           viewimage;
int             viewwidth;
int             scaledviewwidth;
int             viewheight;
int             viewwindowx;
int             viewwindowy;

                // pointer to the start of each line of the screen,
byte*           ylookup[MAXVIDHEIGHT];

byte*           ylookup1[MAXVIDHEIGHT]; // for view1 (splitscreen)
byte*           ylookup2[MAXVIDHEIGHT]; // for view2 (splitscreen)

                 // x byte offset for columns inside the viewwindow
                // so the first column starts at (SCRWIDTH-VIEWWIDTH)/2
int             columnofs[MAXVIDWIDTH];

#ifdef HORIZONTALDRAW
//Fab 17-06-98: horizontal column drawer optimisation
byte*           yhlookup[MAXVIDWIDTH];
int             hcolumnofs[MAXVIDHEIGHT];
#endif

// =========================================================================
//                      COLUMN DRAWING CODE STUFF
// =========================================================================

lighttable_t*           dc_colormap;
lighttable_t*           dc_wcolormap;   //added:24-02-98:WATER!
int                     dc_x;
int                     dc_yl;
int                     dc_yh;
int                     dc_yw;          //added:24-02-98: WATER!

fixed_t                 dc_iscale;
fixed_t                 dc_texturemid;

byte*                   dc_source;


// -----------------------
// translucency stuff here
// -----------------------
#define NUMTRANSTABLES  5     // how many translucency tables are used

byte*                   transtables;    // translucency tables

// R_DrawTransColumn uses this
byte*                   dc_transmap;    // one of the translucency tables


// ----------------------
// translation stuff here
// ----------------------

byte*                   translationtables;

// R_DrawTranslatedColumn uses this
byte*                   dc_translation;


// =========================================================================
//                      SPAN DRAWING CODE STUFF
// =========================================================================

int                     ds_y;
int                     ds_x1;
int                     ds_x2;

lighttable_t*           ds_colormap;

fixed_t                 ds_xfrac;
fixed_t                 ds_yfrac;
fixed_t                 ds_xstep;
fixed_t                 ds_ystep;

byte*                   ds_source;      // start of a 64*64 tile image


// ==========================================================================
//                        OLD DOOM FUZZY EFFECT
// ==========================================================================

//
// Spectre/Invisibility.
//
#define FUZZTABLE     50
#define FUZZOFF       (1)

static  int fuzzoffset[FUZZTABLE] =
{
    FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF
};

static  int fuzzpos = 0;     // move through the fuzz table


//  fuzzoffsets are dependend of vid width, for optimising purpose
//  this is called by SCR_Recalc() whenever the screen size changes
//
void R_RecalcFuzzOffsets (void)
{
    int i;
    for (i=0;i<FUZZTABLE;i++)
    {
        fuzzoffset[i] = (fuzzoffset[i] < 0) ? -vid.width : vid.width;
    }
}


// =========================================================================
//                   TRANSLATION COLORMAP CODE
// =========================================================================

char *Color_Names[MAXSKINCOLORS]={
   "Green",
   "Gray" ,
   "Peach", // tails 02-19-2000
   "Red"  ,
   "Silver" , // tails 02-19-2000
   "Orange", // tails 02-19-2000
   "Pink"  , // tails 02-19-2000
   "Light blue" ,
   "Blue"       ,
   "Army"     , // tails 02-19-2000
   "Beige"
};

CV_PossibleValue_t Color_cons_t[]={{0,NULL},{1,NULL},{2,NULL},{3,NULL},
                                   {4,NULL},{5,NULL},{6,NULL},{7,NULL},
                                   {8,NULL},{9,NULL},{10,NULL},{0,NULL}};

//  Creates the translation tables to map the green color ramp to
//  another ramp (gray, brown, red, ...)
//
//  This is precalculated for drawing the player sprites in the player's
//  chosen color
//
void R_InitTranslationTables (void)
{
    int         i,j;

    //added:11-01-98: load here the transparency lookup tables 'TINTTAB'
    // NOTE: the TINTTAB resource MUST BE aligned on 64k for the asm optimised
    //       (in other words, transtables pointer low word is 0)
    transtables = Z_Malloc (0x10000*NUMTRANSTABLES+0xffff, PU_STATIC, 0);
    transtables = (byte *)(( (long)transtables + 0xffff ) & 0xffff0000UL);

    // load in translucency tables
    W_ReadLump( W_GetNumForName("TRANSMED"), transtables );
    W_ReadLump( W_GetNumForName("TRANSMOR"), transtables+0x10000 );
    W_ReadLump( W_GetNumForName("TRANSHI"),  transtables+0x20000 );
    W_ReadLump( W_GetNumForName("TRANSFIR"), transtables+0x30000 );
    W_ReadLump( W_GetNumForName("TRANSFX1"), transtables+0x40000 );

    translationtables = Z_Malloc (256*(MAXSKINCOLORS-1)+255, PU_STATIC, 0);
    translationtables = (byte *)(( (int)translationtables + 255 )& ~255);

    // translate just the 16 green colors
    for (i=0 ; i<256 ; i++)
    {
        if (i >= 0x70 && i<= 0x7f)
        {
            // map green ramp to gray, brown, red
            translationtables [i      ] = 0x60 + (i&0xf);
            translationtables [i+  256] = 0x30 + (i&0xf); // Peach // Tails 02-19-2000
            translationtables [i+2*256] = 0x20 + (i&0xf);

            // added 9-2-98
            translationtables [i+3*256] = 0x50 + (i&0xf); // silver // tails 02-19-2000
            translationtables [i+4*256] = 0xd0 + (i&0xf); // orange // tails 02-19-2000
            translationtables [i+5*256] = 0x10 + (i&0xf); // pink // tails 02-19-2000
            translationtables [i+6*256] = 0xc0 + (i&0xf); // light blue

            if ((i&0xf) <9)
               translationtables [i+7*256] = 0xc7 + (i&0xf);   // dark blue
            else
               translationtables [i+7*256] = 0xf0-9 + (i&0xf);

            if ((i&0xf) <8)
               translationtables [i+8*256] = 0x98 + (i&0xf);   // army // Tails 02-19-2000
            else
               translationtables [i+8*256] = 0x90-8 + (i&0xf);

            translationtables [i+9*256] = 0x80 + (i&0xf);     // beige

        }
        else
        {
            // Keep all other colors as is.
            for (j=0;j<(MAXSKINCOLORS-1)*256;j+=256)
                translationtables [i+j] = i;
        }
    }
}


// ==========================================================================
//               COMMON DRAWER FOR 8 AND 16 BIT COLOR MODES
// ==========================================================================

// in a perfect world, all routines would be compatible for either mode,
// and optimised enough
//
// in reality, the few routines that can work for either mode, are
// put here


// R_InitViewBuffer
// Creates lookup tables for getting the framebuffer address
//  of a pixel to draw.
//
void R_InitViewBuffer ( int   width,
                        int   height )
{
    int         i;
    int         bytesperpixel = vid.bpp;

    if (bytesperpixel<1 || bytesperpixel>2)
        I_Error ("R_InitViewBuffer : wrong bytesperpixel value %d\n",
                 bytesperpixel);

    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    viewwindowx = (vid.width-width) >> 1;

    // Column offset for those columns of the view window, but
    // relative to the entire screen
    for (i=0 ; i<width ; i++)
        columnofs[i] = (viewwindowx + i) * bytesperpixel;

    // Same with base row offset.
    if (width == vid.width)
        viewwindowy = 0;
    else
        viewwindowy = (vid.height-ST_HEIGHT-height) >> 1;

    // Precalculate all row offsets.
    for (i=0 ; i<height ; i++)
    {
        ylookup[i] = ylookup1[i] = vid.buffer + (i+viewwindowy)*vid.width*bytesperpixel;
                     ylookup2[i] = vid.buffer + (i+(vid.height>>1))*vid.width*bytesperpixel;
    }
        

#ifdef HORIZONTALDRAW
    //Fab 17-06-98
    // create similar lookup tables for horizontal column draw optimisation

    // (the first column is the bottom line)
    for (i=0; i<width; i++)
        yhlookup[i] = screens[2] + ((width-i-1) * bytesperpixel * height);

    for (i=0; i<height; i++)
        hcolumnofs[i] = i * bytesperpixel;
#endif
}


//
// Store the lumpnumber of the viewborder patches.
//
    int viewborderlump[8];
void R_InitViewBorder (void)
{
    viewborderlump[BRDR_T] = W_GetNumForName ("brdr_t");
    viewborderlump[BRDR_B] = W_GetNumForName ("brdr_b");
    viewborderlump[BRDR_L] = W_GetNumForName ("brdr_l");
    viewborderlump[BRDR_R] = W_GetNumForName ("brdr_r");
    viewborderlump[BRDR_TL] = W_GetNumForName ("brdr_tl");
    viewborderlump[BRDR_BL] = W_GetNumForName ("brdr_bl");
    viewborderlump[BRDR_TR] = W_GetNumForName ("brdr_tr");
    viewborderlump[BRDR_BR] = W_GetNumForName ("brdr_br");
}


//
// R_FillBackScreen
// Fills the back screen with a pattern for variable screen sizes
// Also draws a beveled edge.
//
void R_FillBackScreen (void)
{
    byte*       src;
    byte*       dest;
    int         x;
    int         y;
    patch_t*    patch;
    //faB: quickfix
    if (rendermode!=render_soft)
        return;


     //added:08-01-98:draw pattern around the status bar too (when hires),
    //                so return only when in full-screen without status bar.
    if ((scaledviewwidth == vid.width)&&(viewheight==vid.height))
        return;

    src  = scr_borderpatch;
    dest = screens[1];

    for (y=0 ; y<vid.height-ST_HEIGHT ; y++)
    {
        for (x=0 ; x<vid.width/64 ; x++)
        {
            memcpy (dest, src+((y&63)<<6), 64);
            dest += 64;
        }

        if (vid.width&63)
        {
            memcpy (dest, src+((y&63)<<6), vid.width&63);
            dest += (vid.width&63);
        }
    }

    //added:08-01-98:dont draw the borders when viewwidth is full vid.width.
    if (scaledviewwidth == vid.width)
       return;

    //faB: don't cache lumps in both modes
    if (rendermode!=render_soft)
        return;
    
    patch = W_CacheLumpNum (viewborderlump[BRDR_T],PU_CACHE);
    for (x=0 ; x<scaledviewwidth ; x+=8)
        V_DrawPatch (viewwindowx+x,viewwindowy-8,1,patch);
    patch = W_CacheLumpNum (viewborderlump[BRDR_B],PU_CACHE);
    for (x=0 ; x<scaledviewwidth ; x+=8)
        V_DrawPatch (viewwindowx+x,viewwindowy+viewheight,1,patch);
    patch = W_CacheLumpNum (viewborderlump[BRDR_L],PU_CACHE);
    for (y=0 ; y<viewheight ; y+=8)
        V_DrawPatch (viewwindowx-8,viewwindowy+y,1,patch);
    patch = W_CacheLumpNum (viewborderlump[BRDR_R],PU_CACHE);
    for (y=0 ; y<viewheight ; y+=8)
        V_DrawPatch (viewwindowx+scaledviewwidth,viewwindowy+y,1,patch);

    // Draw beveled corners.
    V_DrawPatch (viewwindowx-8,
                 viewwindowy-8,
                 1,
                 W_CacheLumpNum (viewborderlump[BRDR_TL],PU_CACHE));

    V_DrawPatch (viewwindowx+scaledviewwidth,
                 viewwindowy-8,
                 1,
                 W_CacheLumpNum (viewborderlump[BRDR_TR],PU_CACHE));

    V_DrawPatch (viewwindowx-8,
                 viewwindowy+viewheight,
                 1,
                 W_CacheLumpNum (viewborderlump[BRDR_BL],PU_CACHE));

    V_DrawPatch (viewwindowx+scaledviewwidth,
                 viewwindowy+viewheight,
                 1,
                 W_CacheLumpNum (viewborderlump[BRDR_BR],PU_CACHE));
}


//
// Copy a screen buffer.
//
void R_VideoErase (unsigned ofs, int count)
{
    // LFB copy.
    // This might not be a good idea if memcpy
    //  is not optiomal, e.g. byte by byte on
    //  a 32bit CPU, as GNU GCC/Linux libc did
    //  at one point.
    memcpy (screens[0]+ofs, screens[1]+ofs, count);
}


//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void R_DrawViewBorder (void)
{
    int         top;
    int         side;
    int         ofs;

#ifdef DEBUG
    fprintf(stderr,"RDVB: vidwidth %d vidheight %d scaledviewwidth %d viewheight %d\n",
             vid.width,vid.height,scaledviewwidth,viewheight);
#endif

     //added:08-01-98: draw the backtile pattern around the status bar too
    //                 (when statusbar width is shorter than vid.width)
    /*
    if( (vid.width>ST_WIDTH) && (vid.height!=viewheight) )
    {
        ofs  = (vid.height-ST_HEIGHT)*vid.width;
        side = (vid.width-ST_WIDTH)>>1;
        R_VideoErase(ofs,side);

        ofs += (vid.width-side);
        for (i=1;i<ST_HEIGHT;i++)
        {
            R_VideoErase(ofs,side<<1);  //wraparound right to left border
            ofs += vid.width;
        }
        R_VideoErase(ofs,side);
    }*/

    if (scaledviewwidth == vid.width)
        return;

    top  = (vid.height-ST_HEIGHT-viewheight) >>1;
    side = (vid.width-scaledviewwidth) >>1;

    // copy top and one line of left side
    R_VideoErase (0, top*vid.width+side);

    // copy one line of right side and bottom
    ofs = (viewheight+top)*vid.width-side;
    R_VideoErase (ofs, top*vid.width+side);

    // copy sides using wraparound
    ofs = top*vid.width + vid.width-side;
    side <<= 1;

    //added:05-02-98:simpler using our new VID_Blit routine
    VID_BlitLinearScreen(screens[1]+ofs, screens[0]+ofs,
                         side, viewheight-1, vid.width, vid.width);

    // useless, old dirty rectangle stuff
    //V_MarkRect (0,0,vid.width, vid.height-ST_HEIGHT);
}


// ==========================================================================
//                   INCLUDE 8bpp DRAWING CODE HERE
// ==========================================================================

#include "r_draw8.c"


// ==========================================================================
//                   INCLUDE 16bpp DRAWING CODE HERE
// ==========================================================================

#include "r_draw16.c"
