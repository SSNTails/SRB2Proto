
// r_draw.h : low level span/column drawer functions
//

#ifndef __R_DRAW__
#define __R_DRAW__

#include "r_defs.h"

// -------------------------------
// COMMON STUFF FOR 8bpp AND 16bpp
// -------------------------------
extern byte*            ylookup[MAXVIDHEIGHT];
extern byte*            ylookup1[MAXVIDHEIGHT];
extern byte*            ylookup2[MAXVIDHEIGHT];
extern int              columnofs[MAXVIDWIDTH];

#ifdef HORIZONTALDRAW
//Fab 17-06-98
extern byte*            yhlookup[MAXVIDWIDTH];
extern int              hcolumnofs[MAXVIDHEIGHT];
#endif

// -------------------------
// COLUMN DRAWING CODE STUFF
// -------------------------

extern lighttable_t*    dc_colormap;
extern lighttable_t*    dc_wcolormap;   //added:24-02-98:WATER!
extern int              dc_x;
extern int              dc_yl;
extern int              dc_yh;
extern int              dc_yw;          //added:24-02-98:WATER!
extern fixed_t          dc_iscale;
extern fixed_t          dc_texturemid;

extern byte*            dc_source;      // first pixel in a column

// translucency stuff here
extern byte*            transtables;    // translucency tables
extern byte*            dc_transmap;

// translation stuff here

extern byte*            translationtables;
extern byte*            dc_translation;


// -----------------------
// SPAN DRAWING CODE STUFF
// -----------------------

extern int              ds_y;
extern int              ds_x1;
extern int              ds_x2;

extern lighttable_t*    ds_colormap;

extern fixed_t          ds_xfrac;
extern fixed_t          ds_yfrac;
extern fixed_t          ds_xstep;
extern fixed_t          ds_ystep;

extern byte*            ds_source;      // start of a 64*64 tile image


// viewborder patches lump numbers
#define BRDR_T      0
#define BRDR_B      1
#define BRDR_L      2
#define BRDR_R      3
#define BRDR_TL     4
#define BRDR_TR     5
#define BRDR_BL     6
#define BRDR_BR     7

extern int viewborderlump[8];

// ------------------------------------------------
// r_draw.c COMMON ROUTINES FOR BOTH 8bpp and 16bpp
// ------------------------------------------------

//added:26-01-98: called by SCR_Recalc() when video mode changes
void    R_RecalcFuzzOffsets (void);
// Initialize color translation tables, for player rendering etc.
void    R_InitTranslationTables (void);

void    R_InitViewBuffer ( int   width,
                           int   height );

void    R_InitViewBorder (void);

void    R_VideoErase ( unsigned      ofs,
                       int           count );

// Rendering function.
void    R_FillBackScreen (void);

// If the view size is not full screen, draws a border around it.
void    R_DrawViewBorder (void);


// -----------------
// 8bpp DRAWING CODE
// -----------------

#ifdef HORIZONTALDRAW
//Fab 17-06-98
void    R_DrawHColumn_8 (void);
#endif

void    ASMCALL R_DrawColumn_8 (void);
void    ASMCALL R_DrawSkyColumn_8 (void);
void    ASMCALL R_DrawShadeColumn_8 (void);             //smokie test..
void    ASMCALL R_DrawFuzzColumn_8 (void);
void    ASMCALL R_DrawTranslucentColumn_8 (void);
void    ASMCALL R_DrawTranslatedColumn_8 (void);
void    ASMCALL R_DrawSpan_8 (void);


// ------------------
// 16bpp DRAWING CODE
// ------------------

void    ASMCALL R_DrawColumn_16 (void);
void    ASMCALL R_DrawSkyColumn_16 (void);
void    ASMCALL R_DrawFuzzColumn_16 (void);
void    ASMCALL R_DrawTranslucentColumn_16 (void);
void    ASMCALL R_DrawTranslatedColumn_16 (void);
void    ASMCALL R_DrawSpan_16 (void);


// =========================================================================
#endif  // __R_DRAW__
