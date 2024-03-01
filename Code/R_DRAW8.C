
// r_draw8.c : 8bpp span/column drawer functions
//
//       NOTE: no includes because this is included as part of r_draw.c



// ==========================================================================
// COLUMNS
// ==========================================================================

//  A column is a vertical slice/span of a wall texture that uses
//  a has a constant z depth from top to bottom.
//
#ifndef USEASM
void R_DrawColumn_8 (void)
{
    int                 count;
    byte*               dest;
    fixed_t             frac;
    fixed_t             fracstep;

    count = dc_yh - dc_yl;

    // Zero length, column does not exceed a pixel.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= vid.width
        || dc_yl < 0
        || dc_yh >= vid.height)
        I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?
    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do
    {
        // Re-map color indices from wall texture column
        //  using a lighting/special effects LUT.
        *dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];

        dest += vid.width;
        frac += fracstep;

    } while (count--);
}
#endif

#ifndef USEASM
void R_DrawSkyColumn_8 (void)
{
    int                 count;
    byte*               dest;
    fixed_t             frac;
    fixed_t             fracstep;
        
    count = dc_yh - dc_yl;
        
    // Zero length, column does not exceed a pixel.
    if (count < 0)
                return;
        
#ifdef RANGECHECK
    if ((unsigned)dc_x >= vid.width
                || dc_yl < 0
                || dc_yh >= vid.height)
                I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif
        
    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?
    dest = ylookup[dc_yl] + columnofs[dc_x];
        
    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;
        
    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do
    {
                // Re-map color indices from wall texture column
                //  using a lighting/special effects LUT.
                *dest = dc_colormap[dc_source[(frac>>FRACBITS)&255]];
                
                dest += vid.width;
                frac += fracstep;
                
    } while (count--);
}
#endif

//  The standard Doom 'fuzzy' (blur, shadow) effect
//  originally used for spectres and when picking up the blur sphere
//
//#ifndef USEASM // NOT IN ASSEMBLER, TO DO.. IF WORTH IT
void R_DrawFuzzColumn_8 (void)
{
    int                 count;
    byte*               dest;
    fixed_t             frac;
    fixed_t             fracstep;

    // Adjust borders. Low...
    if (!dc_yl)
        dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight-1)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;


#ifdef RANGECHECK
    if ((unsigned)dc_x >= vid.width
        || dc_yl < 0 || dc_yh >= vid.height)
    {
        I_Error ("R_DrawFuzzColumn: %i to %i at %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif


    // Does not work with blocky mode.
    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Looks familiar.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
        // Lookup framebuffer, and retrieve
        //  a pixel that is either one column
        //  left or right of the current one.
        // Add index from colormap to index.
        *dest = colormaps[6*256+dest[fuzzoffset[fuzzpos]]];

        // Clamp table lookup index.
        if (++fuzzpos == FUZZTABLE)
            fuzzpos = 0;

        dest += vid.width;

        frac += fracstep;
    } while (count--);
}
//#endif


//
// I've made an asm routine for the transparency, because it slows down
// a lot in 640x480 with big sprites (bfg on all screen, or transparent
// walls on fullscreen)
//
#ifndef USEASM
void R_DrawTranslucentColumn_8 (void)
{
    int                 count;
    byte*               dest;
    fixed_t             frac;
    fixed_t             fracstep;

static byte what;       //added01-01-98:
    //byte*               src;

    // check out coords for src*
    if((dc_yl<0)||(dc_x>=vid.width))
      return;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= vid.width
        || dc_yl < 0
        || dc_yh >= vid.height)
    {
        I_Error ( "R_DrawColumn: %i to %i at %i",
                  dc_yl, dc_yh, dc_x);
    }

#endif

    // FIXME. As above.
    //src  = ylookup[dc_yl] + columnofs[dc_x+2];
    dest = ylookup[dc_yl] + columnofs[dc_x];


    // Looks familiar.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    // Here we do an additional index re-mapping.
    do
    {
        *dest = *( dc_transmap + (dc_source[frac>>FRACBITS] <<8) + (*dest) );

        //*dest = dc_source[frac>>FRACBITS];
        dest += vid.width;
        //src  += vid.width;

        frac += fracstep;
    } while (count--);
}
#endif


//
//  Draw columns upto 128high but remap the green ramp to other colors
//
//#ifndef USEASM        // STILL NOT IN ASM, TO DO..
void R_DrawTranslatedColumn_8 (void)
{
    int                 count;
    byte*               dest;
    fixed_t             frac;
    fixed_t             fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= vid.width
        || dc_yl < 0
        || dc_yh >= vid.height)
    {
        I_Error ( "R_DrawColumn: %i to %i at %i",
                  dc_yl, dc_yh, dc_x);
    }

#endif


    // WATCOM VGA specific.
    /* Keep for fixing.
    if (detailshift)
    {
        if (dc_x & 1)
            outp (SC_INDEX+1,12);
        else
            outp (SC_INDEX+1,3);

        dest = destview + dc_yl*80 + (dc_x>>1);
    }
    else
    {
        outp (SC_INDEX+1,1<<(dc_x&3));

        dest = destview + dc_yl*80 + (dc_x>>2);
    }*/


    // FIXME. As above.
    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Looks familiar.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    // Here we do an additional index re-mapping.
    do
    {
        // Translation tables are used
        //  to map certain colorramps to other ones,
        //  used with PLAY sprites.
        // Thus the "green" ramp of the player 0 sprite
        //  is mapped to gray, red, black/indigo.
        *dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
        dest += vid.width;

        frac += fracstep;
    } while (count--);
}
//#endif


// ==========================================================================
// SPANS
// ==========================================================================


//  Draws the actual span.
//
#ifndef USEASM
void R_DrawSpan_8 (void)
{
    fixed_t             xfrac;
    fixed_t             yfrac;
    byte*               dest;
    int                 count;
    int                 spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1
        || ds_x1<0
        || ds_x2>=vid.width
        || (unsigned)ds_y>vid.height)
    {
        I_Error( "R_DrawSpan: %i to %i at %i",
                 ds_x1,ds_x2,ds_y);
    }
#endif

    xfrac = ds_xfrac;
    yfrac = ds_yfrac;

    dest = ylookup[ds_y] + columnofs[ds_x1];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

    do
    {
        // Current texture index in u,v.
        spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        *dest++ = ds_colormap[ds_source[spot]];

        // Next step in u,v.
        xfrac += ds_xstep;
        yfrac += ds_ystep;

    } while (count--);
}
#endif
