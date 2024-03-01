// v_video.c :
//      Gamma correction LUT stuff.
//      Functions to draw patches (by post) directly to screen.
//      Functions to blit a block to the screen.

#include "doomdef.h"
#include "r_local.h"
#include "v_video.h"
#include "hu_stuff.h"

#include "i_video.h"            //rendermode

// Each screen is [vid.width*vid.height];
byte*      screens[5];

consvar_t cv_ticrate={"vid_ticrate","0",0,CV_OnOff,NULL};

// Now where did these came from?
byte gammatable[5][256] =
{
    {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
     17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
     33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
     49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
     65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
     81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
     97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
     113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
     144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
     160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
     176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
     192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
     208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
     224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
     240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},

    {2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,24,25,26,27,29,30,31,
     32,33,34,36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52,54,55,
     56,57,58,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,77,
     78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,
     99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
     115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,129,
     130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
     146,147,148,148,149,150,151,152,153,154,155,156,157,158,159,160,
     161,162,163,163,164,165,166,167,168,169,170,171,172,173,174,175,
     175,176,177,178,179,180,181,182,183,184,185,186,186,187,188,189,
     190,191,192,193,194,195,196,196,197,198,199,200,201,202,203,204,
     205,205,206,207,208,209,210,211,212,213,214,214,215,216,217,218,
     219,220,221,222,222,223,224,225,226,227,228,229,230,230,231,232,
     233,234,235,236,237,237,238,239,240,241,242,243,244,245,245,246,
     247,248,249,250,251,252,252,253,254,255},

    {4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,33,35,36,38,39,40,42,
     43,45,46,47,48,50,51,52,54,55,56,57,59,60,61,62,63,65,66,67,68,69,
     70,72,73,74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,91,92,93,
     94,95,96,97,98,100,101,102,103,104,105,106,107,108,109,110,111,112,
     113,114,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
     129,130,131,132,133,133,134,135,136,137,138,139,140,141,142,143,144,
     144,145,146,147,148,149,150,151,152,153,153,154,155,156,157,158,159,
     160,160,161,162,163,164,165,166,166,167,168,169,170,171,172,172,173,
     174,175,176,177,178,178,179,180,181,182,183,183,184,185,186,187,188,
     188,189,190,191,192,193,193,194,195,196,197,197,198,199,200,201,201,
     202,203,204,205,206,206,207,208,209,210,210,211,212,213,213,214,215,
     216,217,217,218,219,220,221,221,222,223,224,224,225,226,227,228,228,
     229,230,231,231,232,233,234,235,235,236,237,238,238,239,240,241,241,
     242,243,244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,
     255},

    {8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,47,49,50,52,53,55,
     57,58,60,61,63,64,65,67,68,70,71,72,74,75,76,77,79,80,81,82,84,85,
     86,87,88,90,91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
     108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
     125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,139,140,
     141,142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,
     155,156,157,158,159,160,160,161,162,163,164,165,165,166,167,168,169,
     169,170,171,172,173,173,174,175,176,176,177,178,179,180,180,181,182,
     183,183,184,185,186,186,187,188,189,189,190,191,192,192,193,194,195,
     195,196,197,197,198,199,200,200,201,202,202,203,204,205,205,206,207,
     207,208,209,210,210,211,212,212,213,214,214,215,216,216,217,218,219,
     219,220,221,221,222,223,223,224,225,225,226,227,227,228,229,229,230,
     231,231,232,233,233,234,235,235,236,237,237,238,238,239,240,240,241,
     242,242,243,244,244,245,246,246,247,247,248,249,249,250,251,251,252,
     253,253,254,254,255},

    {16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,66,68,69,71,73,75,76,
     78,80,81,83,84,86,87,89,90,92,93,94,96,97,98,100,101,102,103,105,106,
     107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,
     125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
     142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
     156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
     169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,181,
     182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,
     193,194,195,195,196,196,197,198,198,199,200,200,201,202,202,203,203,
     204,205,205,206,207,207,208,208,209,210,210,211,211,212,213,213,214,
     214,215,216,216,217,217,218,219,219,220,220,221,221,222,223,223,224,
     224,225,225,226,227,227,228,228,229,229,230,230,231,232,232,233,233,
     234,234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
     243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,
     251,252,252,253,254,254,255,255}
};



//added:18-02-98: this is an offset added to the destination address,
//                for all SCALED graphics. When the menu is displayed,
//                it is TEMPORARILY set to vid.centerofs, the rest of
//                the time it should be zero.
//                The menu is scaled, a round multiple of the original
//                pixels to keep the graphics clean, then it is centered
//                a little, but excepeted the menu, scaled graphics don't
//                have to be centered. Set by m_menu.c, and SCR_Recalc()
int     scaledofs;


// V_MarkRect : this used to refresh only the parts of the screen
//              that were modified since the last screen update
//              it is useless today
//
int        dirtybox[4];
void V_MarkRect ( int           x,
                  int           y,
                  int           width,
                  int           height )
{
    M_AddToBox (dirtybox, x, y);
    M_AddToBox (dirtybox, x+width-1, y+height-1);
}


//
// V_CopyRect
//
void V_CopyRect
( int           srcx,
  int           srcy,
  int           srcscrn,
  int           width,
  int           height,
  int           destx,
  int           desty,
  int           destscrn )
{
    byte*       src;
    byte*       dest;

#ifdef RANGECHECK
    if (srcx<0
        ||srcx+width >vid.width
        || srcy<0
        || srcy+height>vid.height
        ||destx<0||destx+width >vid.width
        || desty<0
        || desty+height>vid.height
        || (unsigned)srcscrn>4
        || (unsigned)destscrn>4)
    {
        I_Error ("Bad V_CopyRect");
    }
#endif
    V_MarkRect (destx, desty, width, height);

#ifdef DEBUG
    CONS_Printf("V_CopyRect: vidwidth %d screen[%d]=%x to screen[%d]=%x\n",
             vid.width,srcscrn,screens[srcscrn],destscrn,screens[destscrn]);
    CONS_Printf("..........: srcx %d srcy %d width %d height %d destx %d desty %d\n",
            srcx,srcy,width,height,destx,desty);
#endif

    src = screens[srcscrn]+vid.width*srcy+srcx;
    dest = screens[destscrn]+vid.width*desty+destx;

    for ( ; height>0 ; height--)
    {
        memcpy (dest, src, width);
        src += vid.width;
        dest += vid.width;
    }
}


//
//  V_DrawMappedPatch : like V_DrawScaledPatch, but with a colormap.
//
//
//added:05-02-98:
void V_DrawMappedPatch ( int           x,
                         int           y,
                         int           scrn,
                         patch_t*      patch,
                         byte*         colormap )
{
    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    int         dupx,dupy;
    int         ofs;
    int         colfrac,rowfrac;

    dupx = vid.dupx;
    dupy = vid.dupy;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (!scrn)
        V_MarkRect (x, y, SHORT(patch->width)*dupx, SHORT(patch->height)*dupy);

    col = 0;
    colfrac  = FixedDiv (FRACUNIT, dupx<<FRACBITS);
    rowfrac  = FixedDiv (FRACUNIT, dupy<<FRACBITS);

    desttop = screens[scrn] + (y*dupy*vid.width) + (x*dupx) + scaledofs;

    w = SHORT(patch->width)<<FRACBITS;

    for ( ; col<w ; col+=colfrac, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col>>FRACBITS]));

        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest   = desttop + column->topdelta*dupy*vid.width;
            count  = column->length*dupy;

            ofs = 0;
            while (count--)
            {
                *dest = *(colormap + source[ofs>>FRACBITS] );
                dest += vid.width;
                ofs += rowfrac;
            }

            column = (column_t *)( (byte *)column + column->length + 4 );
        }
    }

}


//
// V_DrawScaledPatch
//   like V_DrawPatch, but scaled 2,3,4 times the original size and position
//   this is used for menu and title screens, with high resolutions
//
//added:05-02-98:
void V_DrawScaledPatch ( int           x,
                         int           y,
                         int           scrn,    // hacked flags in it...
                         patch_t*      patch )
{
    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    int         dupx,dupy;
    int         ofs;
    int         colfrac,rowfrac;

    dupx = vid.dupx;
    dupy = vid.dupy;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    col = 0;
    colfrac  = FixedDiv (FRACUNIT, dupx<<FRACBITS);
    rowfrac  = FixedDiv (FRACUNIT, dupy<<FRACBITS);

    desttop = screens[scrn&0xFF];
    if (scrn&V_NOSCALESTART)
        desttop += (y*vid.width) + x;
    else
        desttop += (y*dupy*vid.width) + (x*dupx) + scaledofs;

    w = SHORT(patch->width)<<FRACBITS;

    for ( ; col<w ; col+=colfrac, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col>>FRACBITS]));

        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest   = desttop + column->topdelta*dupy*vid.width;
            count  = column->length*dupy;

            ofs = 0;
            while (count--)
            {
                *dest = source[ofs>>FRACBITS];
                dest += vid.width;
                ofs += rowfrac;
            }

            column = (column_t *)( (byte *)column + column->length + 4 );
        }
    }
}


// Same as V_DrawScaledPatch, but x-mirrored - just for f_finale.c
//
void V_DrawScaledPatchFlipped ( int           x,
                                int           y,
                                int           scrn,    // hacked flags in it...
                                patch_t*      patch )
{
    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    int         dupx,dupy;
    int         ofs;
    int         colfrac,rowfrac;

    dupx = vid.dupx;
    dupy = vid.dupy;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    col = 0;
    colfrac  = FixedDiv (FRACUNIT, dupx<<FRACBITS);
    rowfrac  = FixedDiv (FRACUNIT, dupy<<FRACBITS);

    desttop = screens[scrn&0xFF];
    if (scrn&V_NOSCALESTART)
        desttop += (y*vid.width) + x;
    else
        desttop += (y*dupy*vid.width) + (x*dupx) + scaledofs;

    w = SHORT(patch->width)<<FRACBITS;

    for (col=w-colfrac; col>=0; col-=colfrac, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col>>FRACBITS]));

        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest   = desttop + column->topdelta*dupy*vid.width;
            count  = column->length*dupy;

            ofs = 0;
            while (count--)
            {
                *dest = source[ofs>>FRACBITS];
                dest += vid.width;
                ofs += rowfrac;
            }

            column = (column_t *)( (byte *)column + column->length + 4 );
        }
    }
}




//added:16-02-98: now used for crosshair
//
//  This draws a patch over a background with translucency...SCALED
//  DOESNT SCALE THE STARTING COORDS!!
//
extern byte* transtables;       //will use the first one, tr_transmed
void V_DrawTranslucentPatch ( int           x,
                              int           y,
                              int           scrn,
                              patch_t*      patch )
{
    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    int         dupx,dupy;
    int         ofs;
    int         colfrac,rowfrac;

    dupx = vid.dupx;
    dupy = vid.dupy;

    y -= SHORT(patch->topoffset)*dupy;
    x -= SHORT(patch->leftoffset)*dupx;

    if (!scrn)
        V_MarkRect (x, y, SHORT(patch->width)*dupx, SHORT(patch->height)*dupy);

    col = 0;
    colfrac  = FixedDiv (FRACUNIT, dupx<<FRACBITS);
    rowfrac  = FixedDiv (FRACUNIT, dupy<<FRACBITS);

    desttop = screens[scrn] + (y*vid.width) + x;

    w = SHORT(patch->width)<<FRACBITS;

    for ( ; col<w ; col+=colfrac, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col>>FRACBITS]));

        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest   = desttop + column->topdelta*dupy*vid.width;
            count  = column->length*dupy;

            ofs = 0;
            while (count--)
            {
                *dest = *( transtables + ((source[ofs>>FRACBITS]<<8)&0xFF00) + (*dest&0xFF) );
                dest += vid.width;
                ofs += rowfrac;
            }

            column = (column_t *)( (byte *)column + column->length + 4 );
        }
    }
}


//
//  Masks a column based masked pic to the screen, USING A COLORMAP
//
//  This is just like V_DrawMappedPatch, but it doesn't scale since it's
//  used for the status bar right now...
//
//  This is called 'V_DrawTranslationPatch' because it is used to draw
//  the facebacks of statusbar and intermission screens with the right
//  player colors.
//
void V_DrawTranslationPatch ( int           x,
                              int           y,
                              int           scrn,
                              patch_t*      patch,
                              byte*         colormap )
{
    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
#ifdef RANGECHECK
    if (x<0
        ||x+SHORT(patch->width) >vid.width
        || y<0
        || y+SHORT(patch->height)>vid.height
        || (unsigned)scrn>4)
    {
      fprintf( stderr, "Patch at %d,%d exceeds LFB\n", x,y );
      // No I_Error abort - what is up with TNT.WAD?
      fprintf( stderr, "V_DrawPatch: bad patch (ignored)\n");
      return;
    }
#endif

    if (!scrn)
        V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = screens[scrn]+y*vid.width+x;

    w = SHORT(patch->width);

    for ( ; col<w ; x++, col++, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta*vid.width;
            count = column->length;

            while (count--)
            {
                *dest = *(colormap + *(source++) );
                dest += vid.width;
            }
            column = (column_t *)(  (byte *)column + column->length + 4 );
        }
    }
}


//
// V_DrawPatch
// Masks a column based masked pic to the screen. NO SCALING!!!
//
void V_DrawPatch ( int           x,
                   int           y,
                   int           scrn,
                   patch_t*      patch )
{

    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
#ifdef RANGECHECK
    if (x<0
        ||x+SHORT(patch->width) >vid.width
        || y<0
        || y+SHORT(patch->height)>vid.height
        || (unsigned)scrn>4)
    {
      fprintf( stderr, "Patch at %d,%d exceeds LFB\n", x,y );
      // No I_Error abort - what is up with TNT.WAD?
      fprintf( stderr, "V_DrawPatch: bad patch (ignored)\n");
      return;
    }
#endif

    if (!scrn)
        V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = screens[scrn]+y*vid.width+x;

    w = SHORT(patch->width);

    for ( ; col<w ; x++, col++, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta*vid.width;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += vid.width;
            }
            column = (column_t *)(  (byte *)column + column->length
                                    + 4 );
        }
    }
}


//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
void
V_DrawPatchFlipped
( int           x,
  int           y,
  int           scrn,
  patch_t*      patch )
{

    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
#ifdef RANGECHECK
    if (x<0
        ||x+SHORT(patch->width) >vid.width
        || y<0
        || y+SHORT(patch->height)>vid.height
        || (unsigned)scrn>4)
    {
      fprintf( stderr, "Patch origin %d,%d exceeds LFB\n", x,y );
      I_Error ("Bad V_DrawPatch in V_DrawPatchFlipped");
    }
#endif

    if (!scrn)
        V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = screens[scrn]+y*vid.width+x;

    w = SHORT(patch->width);

    for ( ; col<w ; x++, col++, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[w-1-col]));

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta*vid.width;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += vid.width;
            }
            column = (column_t *)(  (byte *)column + column->length
                                    + 4 );
        }
    }
}



//
// V_DrawPatchDirect
// Draws directly to the screen on the pc.
//
void
V_DrawPatchDirect
( int           x,
  int           y,
  int           scrn,
  patch_t*      patch )
{
    V_DrawPatch (x,y,scrn, patch);

    /*
    int         count;
    int         col;
    column_t*   column;
    byte*       desttop;
    byte*       dest;
    byte*       source;
    int         w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

#ifdef RANGECHECK
    if (x<0
        ||x+SHORT(patch->width) >vid.width
        || y<0
        || y+SHORT(patch->height)>vid.height
        || (unsigned)scrn>4)
    {
        I_Error ("Bad V_DrawPatchDirect");
    }
#endif

    //  V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));
    desttop = destscreen + y*vid.width/4 + (x>>2);

    w = SHORT(patch->width);
    for ( col = 0 ; col<w ; col++)
    {
        outp (SC_INDEX+1,1<<(x&3));
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta*vid.width/4;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += vid.width/4;
            }
            column = (column_t *)(  (byte *)column + column->length
                                    + 4 );
        }
        if ( ((++x)&3) == 0 )
            desttop++;  // go to next byte, not next plane
    }*/
}



//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void V_DrawBlock ( int           x,
                   int           y,
                   int           scrn,
                   int           width,
                   int           height,
                   byte*         src )
{
    byte*       dest;

#ifdef RANGECHECK
    if (x<0
        ||x+width >vid.width
        || y<0
        || y+height>vid.height
        || (unsigned)scrn>4 )
    {
        I_Error ("Bad V_DrawBlock");
    }
#endif

    //V_MarkRect (x, y, width, height);

    dest = screens[scrn] + y*vid.width + x;

    while (height--)
    {
        memcpy (dest, src, width);
        src += width;
        dest += vid.width;
    }
}



//
// V_GetBlock
// Gets a linear block of pixels from the view buffer.
//
void
V_GetBlock
( int           x,
  int           y,
  int           scrn,
  int           width,
  int           height,
  byte*         dest )
{
    byte*       src;
    if (rendermode!=render_soft)
        I_Error ("V_GetBlock: called in non-software mode");


#ifdef RANGECHECK
    if (x<0
        ||x+width >vid.width
        || y<0
        || y+height>vid.height
        || (unsigned)scrn>4 )
    {
        I_Error ("Bad V_GetBlock");
    }
#endif

    src = screens[scrn] + y*vid.width+x;

    while (height--)
    {
        memcpy (dest, src, width);
        src += vid.width;
        dest += width;
    }
}


//  Draw a linear pic, scaled, TOTALLY CRAP CODE!!! OPTIMISE AND ASM!!
//  CURRENTLY USED FOR StatusBarOverlay, scale pic but not starting coords
//
void V_DrawScalePic ( int           x1,
                      int           y1,
                      int           scrn,
                      pic_t*        pic )
{
    int         dupx,dupy;
    int         i;
    byte        *src, *dest;
    int         x, stepx;
    int         y, stepy;
    int         width,height;

    dupx = vid.dupx;
    dupy = vid.dupy;
    stepx= FixedDiv (1<<16, dupx<<16);
    stepy= FixedDiv (1<<16, dupy<<16);

    width = pic->width * dupx;
    height= pic->height* dupy;

    src  = pic->data;
    dest = screens[scrn] + (y1*vid.width) + (x1);

    for (y=0 ; height-- ; y+=stepy, dest += vid.width)
    {
        src = pic->data + (y>>16)*pic->width;
        if (dupx==1)
            memcpy (dest, src, pic->width);
        else
        {
            // scale pic to resolution
            x=0;
            for (i=0 ; i<width ; i++)
            {
                dest[i] = src[x>>16];
                x += stepx;
            }
        }
    }

}



//
//  Fills a box of pixels with a single color, NOTE: scaled to screen size
//
//added:05-02-98:
void V_DrawFill (int x, int y, int w, int h, int c)
{
    byte      *dest;
    int       u, v;
    int       dupx,dupy;
    //faB: should put equivalent hardware code here
    if (rendermode!=render_soft)
        return;


    dupx = vid.dupx;
    dupy = vid.dupy;

    dest = screens[0] + y*dupy*vid.width + x*dupx + scaledofs;

    w *= dupx;
    h *= dupy;

    for (v=0 ; v<h ; v++, dest += vid.width)
        for (u=0 ; u<w ; u++)
            dest[u] = c;
}



//
//  Fills a box of pixels using a flat texture as a pattern,
//  scaled to screen size.
//
//added:06-02-98:
void V_DrawFlatFill (int x, int y, int w, int h, byte* flat)
{
    byte      *dest;
    int       u, v;
    int       dupx,dupy;
    fixed_t   dx,dy,xfrac,yfrac;
    byte      *src;

    dupx = vid.dupx;
    dupy = vid.dupy;

    //psrc = W_CacheLumpName (flat, PU_CACHE);

    dest = screens[0] + y*dupy*vid.width + x*dupx + scaledofs;

    w *= dupx;
    h *= dupy;

    dx = FixedDiv(FRACUNIT,dupx<<FRACBITS);
    dy = FixedDiv(FRACUNIT,dupy<<FRACBITS);

    yfrac = 0;
    for (v=0; v<h ; v++, dest += vid.width)
    {
        xfrac = 0;
        src = flat + (((yfrac>>FRACBITS)&63)<<6);
        for (u=0 ; u<w ; u++)
        {
            dest[u] = src[(xfrac>>FRACBITS)&63];
            xfrac += dx;
        }
        yfrac += dy;
    }
}



//
//  Fade all the screen buffer, so that the menu is more readable,
//  especially now that we use the small hufont in the menus...
//
void V_DrawFadeScreen (void)
{
    int         x,y,w;
    int         *buf;
    unsigned    quad;
    byte        p1, p2, p3, p4;
    byte*       fadetable = (byte *) colormaps + 16*256;

    //if (scr_bpp==1)
    //{

    w = vid.width>>2;
    for (y=0 ; y<vid.height ; y++)
    {
        buf = (int *)(screens[0] + vid.width*y);
        for (x=0 ; x<w ; x++)
        {
            quad = buf[x];
            p1 = fadetable[quad&255];
            p2 = fadetable[(quad>>8)&255];
            p3 = fadetable[(quad>>16)&255];
            p4 = fadetable[quad>>24];
            buf[x] = (p4<<24) | (p3<<16) | (p2<<8) | p1;
        }
    }
    //}

#ifdef _16bitcrapneverfinished
 else
 {
    w = vid.width;
    for (y=0 ; y<vid.height ; y++)
    {
        wput = (short*) (vid.buffer + vid.width*y);
        for (x=0 ; x<w ; x++)
        {
            *wput++ = (*wput>>1) & 0x3def;
        }
    }
 }
#endif
}


// Simple translucence with one color, coords are resolution dependent
//
//added:20-03-98: console test
extern byte* greenmap;    //added:20-03-98:initted at CONS_Init()
void V_DrawFadeConsBack (int x1, int y1, int x2, int y2)
{
    int         x,y,w;
    int         *buf;
    unsigned    quad;
    byte        p1, p2, p3, p4;
    short*      wput;

 if (scr_bpp==1)
 {
    x1 >>=2;
    x2 >>=2;
    for (y=y1 ; y<y2 ; y++)
    {
        buf = (int *)(screens[0] + vid.width*y);
        for (x=x1 ; x<x2 ; x++)
        {
            quad = buf[x];
            p1 = greenmap[quad&255];
            p2 = greenmap[(quad>>8)&255];
            p3 = greenmap[(quad>>16)&255];
            p4 = greenmap[quad>>24];
            buf[x] = (p4<<24) | (p3<<16) | (p2<<8) | p1;
        }
    }
 }
 else
 {
    w = x2-x1;
    for (y=y1 ; y<y2 ; y++)
    {
        wput = (short*)(vid.buffer + vid.width*y) + x1;
        for (x=0 ; x<w ; x++)
        {
            *wput++ = ((*wput&0x7bde) + (15<<5)) >>1;
        }
    }
 }
}


extern byte* whitemap;  //m_menu.c


// Writes a single character (draw WHITE if bit 7 set)
//
//added:20-03-98:
void V_DrawCharacter (int x, int y, int c)
{
    int         w;
    boolean     white;

    white = c & 0x80;
    c &= 0x7f;

    c = toupper(c) - HU_FONTSTART;
    if (c < 0 || c>= HU_FONTSIZE)
        return;

    w = SHORT (hu_font[c]->width);
    if (x+w > vid.width /*BASEVIDWIDTH*/)
        return;

//    if (scalemode)
//        V_DrawScaledPatch(x, y, 0, hu_font[c]);
//    else
    if (white)
        // draw with colormap, WITHOUT scale
        V_DrawTranslationPatch(x, y, 0, hu_font[c], whitemap);
    else
        V_DrawPatch(x, y, 0, hu_font[c]);
}



//
//  Write a string using the hu_font
//  NOTE: the text is centered for screens larger than the base width
//
//added:05-02-98:
void V_DrawString (int x, int y, char* string)
{
    int         w;
    char*       ch;
    int         c;
    int         cx;
    int         cy;

    ch = string;
    cx = x;
    cy = y;

    while(1)
    {
        c = *ch++;
        if (!c)
            break;
        if (c == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c>= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);
        if (cx+w > BASEVIDWIDTH)
            break;
        V_DrawScaledPatch(cx, cy, 0, hu_font[c]);
        cx+=w;
    }
}


//
//added:03-02-98: V_DrawString, using a colormap to display the text in
//                brighter colors.
void V_DrawStringWhite (int x, int y, char* string)
{
    int         w;
    char*       ch;
    int         c;
    int         cx;
    int         cy;

    ch = string;
    cx = x;
    cy = y;
    while(1)
    {
        c = *ch++;
        if (!c)
            break;
        if (c == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c>= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);
        if (cx+w > vid.width)
            break;
        V_DrawMappedPatch(cx, cy, 0, hu_font[c], whitemap);
        cx+=w;
    }
}


//
// Find string width from hu_font chars
//
int V_StringWidth (char* string)
{
    int             i;
    int             w = 0;
    int             c;

    for (i = 0;i < (int)strlen(string);i++)
    {
        c = toupper(string[i]) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE)
            w += 4;
        else
            w += SHORT (hu_font[c]->width);
    }

    return w;
}
// V_Init
// olf software stuff, buffers are allocated at video mode setup
// here we set the screens[x] pointers accordingly
// WARNING :
// - called at runtime (don't init cvar here)


void V_Init (void)
{
    int         i;
    byte*       base;
    int         screensize;
#ifdef __WIN32__
    // hardware modes do not use screens[] pointers
    if (rendermode != render_soft)
    {
        // be sure to cause a NULL read/write error so we detect it, in case of..
        for (i=0 ; i<NUMSCREENS ; i++)
            screens[i] = NULL;
        return;
    }
#endif
    

    //added:26-01-98:start address of NUMSCREENS * width*height vidbuffers
    base = vid.buffer;

    screensize = vid.width * vid.height * vid.bpp;

    for (i=0 ; i<NUMSCREENS ; i++)
        screens[i] = base + i*screensize;

    //added:26-01-98: statusbar buffer
    screens[4] = base + NUMSCREENS*screensize;

 //!debug
#ifdef DEBUG
 CONS_Printf("V_Init done:\n");
 for(i=0;i<NUMSCREENS+1;i++)
     CONS_Printf(" screens[%d] = %x\n",i,screens[i]);
#endif

}


//
//
//
typedef struct {
    int    px;
    int    py;
} modelvertex_t;

static modelvertex_t vertex[4];

void R_DrawSpanNoWrap (void);   //tmap.S

//
//  Tilts the view like DukeNukem...
//
//added:12-02-98:

#ifdef __WIN32__
void V_DrawTiltView (byte *viewbuffer)  // don't touch direct video I'll find something..
{}
#else

void V_DrawTiltView (byte *viewbuffer)
{
    fixed_t    leftxfrac;
    fixed_t    leftyfrac;
    fixed_t    xstep;
    fixed_t    ystep;

    int        y;

    vertex[0].px = 0;   // tl
    vertex[0].py = 53;
    vertex[1].px = 212; // tr
    vertex[1].py = 0;
    vertex[2].px = 264; // br
    vertex[2].py = 144;
    vertex[3].px = 53;  // bl
    vertex[3].py = 199;

    // resize coords to screen
    for (y=0;y<4;y++)
    {
        vertex[y].px = (vertex[y].px * vid.width) / BASEVIDWIDTH;
        vertex[y].py = (vertex[y].py * vid.height) / BASEVIDHEIGHT;
    }

    ds_colormap = fixedcolormap;
    ds_source = viewbuffer;

    // starting points top-left and top-right
    leftxfrac  = vertex[0].px <<FRACBITS;
    leftyfrac  = vertex[0].py <<FRACBITS;

    // steps
    xstep = ((vertex[3].px - vertex[0].px)<<FRACBITS) / vid.height;
    ystep = ((vertex[3].py - vertex[0].py)<<FRACBITS) / vid.height;

    ds_y  = (int) vid.direct;
    ds_x1 = 0;
    ds_x2 = vid.width-1;
    ds_xstep = ((vertex[1].px - vertex[0].px)<<FRACBITS) / vid.width;
    ds_ystep = ((vertex[1].py - vertex[0].py)<<FRACBITS) / vid.width;


//    I_Error("ds_y %d ds_x1 %d ds_x2 %d ds_xstep %x ds_ystep %x \n"
//            "ds_xfrac %x ds_yfrac %x ds_source %x\n", ds_y,
//                      ds_x1,ds_x2,ds_xstep,ds_ystep,leftxfrac,leftyfrac,
//                      ds_source);

    // render spans
    for (y=0; y<vid.height; y++)
    {
        // FAST ASM routine!
        ds_xfrac = leftxfrac;
        ds_yfrac = leftyfrac;
        R_DrawSpanNoWrap ();
        ds_y += vid.rowbytes;

        // move along the left and right edges of the polygon
        leftxfrac += xstep;
        leftyfrac += ystep;
    }

}
#endif

//
// Test 'scrunch perspective correction' tm (c) ect.
//
//added:05-04-98:

#ifdef __WIN32__
void V_DrawPerspView (byte *viewbuffer, int aiming)
{}
#else

void V_DrawPerspView (byte *viewbuffer, int aiming)
{

     byte*      source;
     byte*      dest;
     int        y;
     int        x1,w;
     int        offs;

     fixed_t    topfrac,bottomfrac,scale,scalestep;
     fixed_t    xfrac,xfracstep;

    source = viewbuffer;

    //+16 to -16 fixed
    offs = ((aiming*20)<<16) / 100;

    topfrac    = ((vid.width-40)<<16) - (offs*2);
    bottomfrac = ((vid.width-40)<<16) + (offs*2);

    scalestep  = (bottomfrac-topfrac) / vid.height;
    scale      = topfrac;

    for (y=0; y<vid.height; y++)
    {
        x1 = ((vid.width<<16) - scale)>>17;
        dest = ((byte*) vid.direct) + (vid.rowbytes*y) + x1;

        xfrac = (20<<FRACBITS) + ((!x1)&0xFFFF);
        xfracstep = FixedDiv((vid.width<<FRACBITS)-(xfrac<<1),scale);
        w = scale>>16;
        while (w--)
        {
            *dest++ = source[xfrac>>FRACBITS];
            xfrac += xfracstep;
        }
        scale += scalestep;
        source += vid.width;
    }

}
#endif
