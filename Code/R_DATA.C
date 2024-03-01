// r_data.c :
//      Preparation of data for rendering,
//      generation of lookups, caching, retrieval by name.

#include "doomdef.h"
#include "g_game.h"
#include "i_video.h"
#include "r_local.h"
#include "r_sky.h"
#include "p_local.h"
#include "r_data.h"
#include "w_wad.h"
#include "z_zone.h"

#ifdef __WIN32__
#include "malloc.h"
#endif

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//


int             firstflat;
int             lastflat;
int             numflats;

int             firstwaterflat; //added:18-02-98:WATER!

int             firstpatch;
int             lastpatch;
int             numpatches;

int             firstspritelump;
int             lastspritelump;
int             numspritelumps;



// textures
int             numtextures=0;      // total number of textures found,
// size of following tables

texture_t**     textures=NULL;
unsigned int**  texturecolumnofs;   // column offset lookup table for each texture
byte**          texturecache;       // graphics data for each generated full-size texture
int*            texturewidthmask;   // texture width is a power of 2, so it
                                    // can easily repeat along sidedefs using
                                    // a simple mask
fixed_t*        textureheight;      // needed for texture pegging

int*            texturetranslation=NULL; // for global animation


// needed for pre rendering
fixed_t*        spritewidth;
fixed_t*        spriteoffset;
fixed_t*        spritetopoffset;

lighttable_t    *colormaps;


//faB: for debugging/info purpose
int             flatmemory;
int             spritememory;
int             texturememory;


//faB: highcolor stuff
short    color8to16[256];       //remap color index to highcolor rgb value
short*   hicolormaps;           // test a 32k colormap remaps high -> high


//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//



//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
void R_DrawColumnInCache ( column_t*     patch,
                           byte*         cache,
                           int           originy,
                           int           cacheheight )
{
    int         count;
    int         position;
    byte*       source;
    byte*       dest;

    dest = (byte *)cache /*+ 3*/;

    while (patch->topdelta != 0xff)
    {
        source = (byte *)patch + 3;
        count = patch->length;
        position = originy + patch->topdelta;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
            memcpy (cache + position, source, count);

        patch = (column_t *)(  (byte *)patch + patch->length + 4);
    }
}



//
// R_GenerateTexture
//
//   Allocate space for full size texture, either single patch or 'composite'
//   Build the full textures from patches.
//   The texture caching system is a little more hungry of memory, but has
//   been simplified for the sake of highcolor, dynamic ligthing, & speed.
//   (oh.. and also 3dfx soon :)
//
//   This is not optimised, but it's supposed to be executed only once
//   per level, when enough memory is available.
//
byte* R_GenerateTexture (int texnum)
{
    byte*               block;
    byte*               blocktex;
    texture_t*          texture;
    texpatch_t*         patch;
    patch_t*            realpatch;
    int                 x;
    int                 x1;
    int                 x2;
    int                 i;
    column_t*           patchcol;
    unsigned int*       colofs;
    int                 blocksize;

    texture = textures[texnum];

    // allocate texture column offset lookup

    // single-patch textures can have holes in it and may be used on
    // 2sided lines so they need to be kept in 'packed' format
    if (texture->patchcount==1)
    {
        patch = texture->patches;
        blocksize = W_LumpLength (patch->patch);
#if 1
        realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);

        block = Z_Malloc (blocksize,
                          PU_STATIC,
                          &texturecache[texnum]);
        memcpy (block, realpatch, blocksize);
#else
        // FIXME: this version don't put the user z_block
        texturecache[texnum] = block = W_CacheLumpNum (patch->patch, PU_STATIC);
#endif
        //CONS_Printf ("R_GenTex SINGLE %.8s size: %d\n",texture->name,blocksize);
        texturememory+=blocksize;

        // use the patch's column lookup
        colofs = (unsigned int*)(block + 8);
        texturecolumnofs[texnum] = colofs;
                blocktex = block;
        for (i=0; i<texture->width; i++)
             colofs[i] += 3;
        goto done;
    }

    //
    // multi-patch textures (or 'composite')
    //

    blocksize = (texture->width * 4) + (texture->width * texture->height);

    //CONS_Printf ("R_GenTex MULTI  %.8s size: %d\n",texture->name,blocksize);
    texturememory+=blocksize;

    block = Z_Malloc (blocksize,
                      PU_STATIC,
                      &texturecache[texnum]);

    // columns lookup table
    colofs = (unsigned int*)block;
    texturecolumnofs[texnum] = colofs;

    // texture data before the lookup table
    blocktex = block + (texture->width*4);

    // Composite the columns together.
    patch = texture->patches;

    for (i=0 , patch = texture->patches;
         i<texture->patchcount;
         i++, patch++)
    {
        realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
        x1 = patch->originx;
        x2 = x1 + SHORT(realpatch->width);

        if (x1<0)
            x = 0;
        else
            x = x1;

        if (x2 > texture->width)
            x2 = texture->width;

        for ( ; x<x2 ; x++)
        {
            patchcol = (column_t *)((byte *)realpatch
                                    + LONG(realpatch->columnofs[x-x1]));

            // generate column ofset lookup
            colofs[x] = (x * texture->height) + (texture->width*4);

            R_DrawColumnInCache (patchcol,
                                 block + colofs[x],
                                 patch->originy,
                                 texture->height);
        }

    }

done:
    // Now that the texture has been built in column cache,
    //  it is purgable from zone memory.
    Z_ChangeTag (block, PU_CACHE);

    return blocktex;
}





//
// R_GetColumn
//


//
// new test version, short!
//
byte* R_GetColumn ( int           tex,
                    int           col )
{
    byte*       data;

    col &= texturewidthmask[tex];
    data = texturecache[tex];

    if (!data)
        data = R_GenerateTexture (tex);

    return data + texturecolumnofs[tex][col];
}


//  convert flat to hicolor as they are requested
//
//byte**  flatcache;

byte* R_GetFlat (int  flatlumpnum)
{
    return W_CacheLumpNum (flatlumpnum, PU_CACHE);

/*  // this code work but is useless
    byte*    data;
    short*   wput;
    int      i,j;

    //FIXME: work with run time pwads, flats may be added
    // lumpnum to flatnum in flatcache
    if ((data = flatcache[flatlumpnum-firstflat])!=0)
                return data;

    data = W_CacheLumpNum (flatlumpnum, PU_CACHE);
    i=W_LumpLength(flatlumpnum);

    Z_Malloc (i,PU_STATIC,&flatcache[flatlumpnum-firstflat]);
    memcpy (flatcache[flatlumpnum-firstflat], data, i);

    return flatcache[flatlumpnum-firstflat];
*/

/*  // this code don't work because it don't put a proper user in the z_block
    if ((data = flatcache[flatlumpnum-firstflat])!=0)
       return data;

    data = (byte *) W_CacheLumpNum(flatlumpnum,PU_LEVEL);
    flatcache[flatlumpnum-firstflat] = data;
    return data;

    flatlumpnum -= firstflat;

    if (scr_bpp==1)
    {
                flatcache[flatlumpnum] = data;
                return data;
    }

    // allocate and convert to high color

    wput = (short*) Z_Malloc (64*64*2,PU_STATIC,&flatcache[flatlumpnum]);
    //flatcache[flatlumpnum] =(byte*) wput;

    for (i=0; i<64; i++)
       for (j=0; j<64; j++)
                        wput[i*64+j] = ((color8to16[*data++]&0x7bde) + ((i<<9|j<<4)&0x7bde))>>1;

                //Z_ChangeTag (data, PU_CACHE);

                return (byte*) wput;
*/
}

//
// Empty the texture cache (used for load wad at runtime)
//
void R_FlushTextureCache (void)
{
    int i;

    if (numtextures>0)
        for (i=0; i<numtextures; i++)
        {
            if (texturecache[i])
                Z_Free (texturecache[i]);
        }
}

//
// R_InitTextures
// Initializes the texture list with the textures from the world map.
//
void R_LoadTextures (void)
{
    maptexture_t*       mtexture;
    texture_t*          texture;
    mappatch_t*         mpatch;
    texpatch_t*         patch;
    char*               pnames;

    int                 i;
    int                 j;

    int*                maptex;
    int*                maptex2;
    int*                maptex1;

    char                name[9];
    char*               name_p;

    int*                patchlookup;

    int                 nummappatches;
    int                 offset;
    int                 maxoff;
    int                 maxoff2;
    int                 numtextures1;
    int                 numtextures2;

    int*                directory;


    // free previous memory before numtextures change

    if (numtextures>0)
        for (i=0; i<numtextures; i++)
        {
            if (textures[i])
                Z_Free (textures[i]);
            if (texturecache[i])
                Z_Free (texturecache[i]);
        }

    // Load the patch names from pnames.lmp.
    name[8] = 0;
    pnames = W_CacheLumpName ("PNAMES", PU_STATIC);
    nummappatches = LONG ( *((int *)pnames) );
    name_p = pnames+4;
    patchlookup = alloca (nummappatches*sizeof(*patchlookup));

    for (i=0 ; i<nummappatches ; i++)
    {
        strncpy (name,name_p+i*8, 8);
        patchlookup[i] = W_CheckNumForName (name);
    }
    Z_Free (pnames);

    // Load the map texture definitions from textures.lmp.
    // The data is contained in one or two lumps,
    //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
    maptex = maptex1 = W_CacheLumpName ("TEXTURE1", PU_STATIC);
    numtextures1 = LONG(*maptex);
    maxoff = W_LumpLength (W_GetNumForName ("TEXTURE1"));
    directory = maptex+1;

    if (W_CheckNumForName ("TEXTURE2") != -1)
    {
        maptex2 = W_CacheLumpName ("TEXTURE2", PU_STATIC);
        numtextures2 = LONG(*maptex2);
        maxoff2 = W_LumpLength (W_GetNumForName ("TEXTURE2"));
    }
    else
    {
        maptex2 = NULL;
        numtextures2 = 0;
        maxoff2 = 0;
    }
    numtextures = numtextures1 + numtextures2;


    //faB : there is actually 5 buffers allocated in one for convenience
    if (textures)
        Z_Free (textures);

    textures         = Z_Malloc (numtextures*4*5, PU_STATIC, 0);

    texturecolumnofs = (void*)((int*)textures + numtextures);
    texturecache     = (void*)((int*)textures + numtextures*2);
    texturewidthmask = (void*)((int*)textures + numtextures*3);
    textureheight    = (void*)((int*)textures + numtextures*4);

    for (i=0 ; i<numtextures ; i++, directory++)
    {
        //only during game startup
        //if (!(i&63))
        //    CONS_Printf (".");

        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex = maptex2;
            maxoff = maxoff2;
            directory = maptex+1;
        }

        // offset to the current texture in TEXTURESn lump
        offset = LONG(*directory);

        if (offset > maxoff)
            I_Error ("R_LoadTextures: bad texture directory");

        // maptexture describes texture name, size, and
        // used patches in z order from bottom to top
        mtexture = (maptexture_t *) ( (byte *)maptex + offset);

        texture = textures[i] =
            Z_Malloc (sizeof(texture_t)
                      + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1),
                      PU_STATIC, 0);

        texture->width  = SHORT(mtexture->width);
        texture->height = SHORT(mtexture->height);
        texture->patchcount = SHORT(mtexture->patchcount);

        memcpy (texture->name, mtexture->name, sizeof(texture->name));
        mpatch = &mtexture->patches[0];
        patch = &texture->patches[0];

        for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
        {
            patch->originx = SHORT(mpatch->originx);
            patch->originy = SHORT(mpatch->originy);
            patch->patch = patchlookup[SHORT(mpatch->patch)];
            if (patch->patch == -1)
            {
                I_Error ("R_InitTextures: Missing patch in texture %s",
                         texture->name);
            }
        }

        j = 1;
        while (j*2 <= texture->width)
            j<<=1;

        texturewidthmask[i] = j-1;
        textureheight[i] = texture->height<<FRACBITS;
    }

    Z_Free (maptex1);
    if (maptex2)
        Z_Free (maptex2);


    //added:01-04-98: this takes 90% of texture loading time..
    // Precalculate whatever possible.
    for (i=0 ; i<numtextures ; i++)
        texturecache[i] = NULL;

    // Create translation table for global animation.
    if (texturetranslation)
        Z_Free (texturetranslation);

    texturetranslation = Z_Malloc ((numtextures+1)*4, PU_STATIC, 0);

    for (i=0 ; i<numtextures ; i++)
        texturetranslation[i] = i;
}



//
// R_InitFlats
//
void R_InitFlats (void)
{
    firstflat = W_GetNumForName ("F_START") + 1;
    lastflat = W_GetNumForName ("F_END") - 1;
    numflats = lastflat - firstflat + 1;

    //Fab: flattranslation is now per-level, see p_setup

    //added:18-02-98: WATER! flatnum of the first waterflat
    firstwaterflat = W_GetNumForName ("WATER0");

 //fab highcolor test
    //flatcache = Z_Malloc ((numflats+1)*4, PU_STATIC, 0);
    //memset (flatcache, 0, (numflats+1)*4);
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
int W_GetNumForNameFirst (char* name);  // modified version that scan forwards


//
//   allocate sprite lookup tables
//
void R_InitSpriteLumps (void)
{
    // the original Doom used to set numspritelumps from S_END-S_START+1

    //Fab:FIXME: find a better solution for adding new sprites dynamically
    numspritelumps = 0;

    spritewidth = Z_Malloc (MAXSPRITELUMPS*4, PU_STATIC, 0);
    spriteoffset = Z_Malloc (MAXSPRITELUMPS*4, PU_STATIC, 0);
    spritetopoffset = Z_Malloc (MAXSPRITELUMPS*4, PU_STATIC, 0);
}



//
// R_InitColormaps
//
void R_InitColormaps (void)
{
    int lump, length;

    // Load in the light tables,
    //  256 byte align tables.
    lump = W_GetNumForName("COLORMAP");
    length = W_LumpLength (lump) + 65535;
    // now 64k aligned for smokie test...
    colormaps = Z_Malloc (length, PU_STATIC, 0);
    colormaps = (byte *)( ((int)colormaps + 65535)&0xffff0000UL);
    W_ReadLump (lump,colormaps);
}


//
//  build a table for quick conversion from 8bpp to 15bpp
//
int makecol15(int r, int g, int b)
{
   return (((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3)));
}

void R_Init8to16 (void)
{
    byte*       palette;
    int         i;

    palette = W_CacheLumpName ("PLAYPAL",PU_CACHE);

    for (i=0;i<256;i++)
    {
                // doom PLAYPAL are 8 bit values
        color8to16[i] = makecol15 (palette[0],palette[1],palette[2]);
        palette += 3;
    }

    // test a big colormap
    hicolormaps = Z_Malloc (32768 /**34*/, PU_STATIC, 0);
    for (i=0;i<16384;i++)
         hicolormaps[i] = i<<1;
}


//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
    //fab highcolor
    if (highcolor)
    {
        CONS_Printf ("\nInitHighColor...");
        R_Init8to16 ();
    }

    CONS_Printf ("\nInitTextures...");
    R_LoadTextures ();
    CONS_Printf ("\nInitFlats...");
    R_InitFlats ();

    CONS_Printf ("\nInitSprites...\n");
    R_InitSpriteLumps ();
    R_InitSprites (sprnames);

    CONS_Printf ("\nInitColormaps...");
    R_InitColormaps ();
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName (char* name)
{
    int i;

    i = W_CheckNumForName (name);

    if (i == -1)
    {
        char namet[9];
        namet[8] = 0;
        memcpy (namet, name,8);
        I_Error ("R_FlatNumForName: %s not found",namet);
    }
    return i - firstflat;
}




//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int     R_CheckTextureNumForName (char *name)
{
    int         i;

    // "NoTexture" marker.
    if (name[0] == '-')
        return 0;

    for (i=0 ; i<numtextures ; i++)
        if (!strncasecmp (textures[i]->name, name, 8) )
            return i;

    return -1;
}



//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//
int     R_TextureNumForName (char* name)
{
    int         i;

    i = R_CheckTextureNumForName (name);

    if (i==-1)
    {
        I_Error ("R_TextureNumForName: %s not found",
                 name);
    }
    return i;
}




//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//

void R_PrecacheLevel (void)
{
    char*               flatpresent;
    char*               texturepresent;
    char*               spritepresent;

    int                 i;
    int                 j;
    int                 k;
    int                 lump;

    thinker_t*          th;
    spriteframe_t*      sf;

    //int numgenerated;  //faB:debug

    if (demoplayback)
        return;

    // do not flush the memory
    if (rendermode != render_soft)
        return;

    // Precache flats.
    flatpresent = alloca(numflats);
    memset (flatpresent,0,numflats);

    // Check for used flats
    for (i=0 ; i<numsectors ; i++)
    {
        flatpresent[sectors[i].floorpic] = 1;
        flatpresent[sectors[i].ceilingpic] = 1;
    }

    flatmemory = 0;

    for (i=0 ; i<numflats ; i++)
    {
        if (flatpresent[i])
        {
            lump = firstflat + i;
            if(devparm)
               flatmemory += W_LumpLength(lump);
            R_GetFlat (lump);
//            W_CacheLumpNum(lump, PU_CACHE);
        }
    }


    //
    // Precache textures.
    //
    // no need to precache all software textures in Glide mode
    // (note they are still used with the reference software view)
    if (rendermode == render_soft)
    {
    texturepresent = alloca(numtextures);
    memset (texturepresent,0, numtextures);

    for (i=0 ; i<numsides ; i++)
    {
        texturepresent[sides[i].toptexture] = 1;
        texturepresent[sides[i].midtexture] = 1;
        texturepresent[sides[i].bottomtexture] = 1;
    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to
    //  indicate a sky floor/ceiling as a flat,
    //  while the sky texture is stored like
    //  a wall texture, with an episode dependend
    //  name.
    texturepresent[skytexture] = 1;

    //if (devparm)
    //    CONS_Printf("Generating textures..\n");

    texturememory = 0;
    for (i=0 ; i<numtextures ; i++)
    {
        if (!texturepresent[i])
            continue;

        //texture = textures[i];
        if( texturecache[i]==NULL )
            R_GenerateTexture (i);
        //numgenerated++;

        // note: pre-caching individual patches that compose textures became
        //       obsolete since we now cache entire composite textures

        //for (j=0 ; j<texture->patchcount ; j++)
        //{
        //    lump = texture->patches[j].patch;
        //    texturememory += W_LumpLength(lump);
        //    W_CacheLumpNum(lump , PU_CACHE);
        //}
    }
    //CONS_Printf ("total mem for %d textures: %d k\n",numgenerated,texturememory>>10);
    }

    //
    // Precache sprites.
    //
    spritepresent = alloca(numsprites);
    memset (spritepresent,0, numsprites);

    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
        if (th->function.acp1 == (actionf_p1)P_MobjThinker)
            spritepresent[((mobj_t *)th)->sprite] = 1;
    }

    spritememory = 0;
    for (i=0 ; i<numsprites ; i++)
    {
        if (!spritepresent[i])
            continue;

        for (j=0 ; j<sprites[i].numframes ; j++)
        {
            sf = &sprites[i].spriteframes[j];
            for (k=0 ; k<8 ; k++)
            {
                //Fab: see R_InitSprites for more about lumppat,lumpid
                lump = /*firstspritelump +*/ sf->lumppat[k];
                if(devparm)
                   spritememory += W_LumpLength(lump);
                W_CachePatchNum(lump , PU_CACHE);
            }
        }
    }

    //FIXME: this is no more correct with glide render mode
    if (devparm)
    {
        CONS_Printf("Precache level done:\n"
                    "flatmemory:    %ld k\n"
                    "texturememory: %ld k\n"
                    "spritememory:  %ld k\n", flatmemory>>10, texturememory>>10, spritememory>>10 );
    }
}
