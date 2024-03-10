// hwr_data.h : defines structures and exports for the standard 3D driver DLL used by Doom Legacy

#ifndef _HWR_DATA_
#define _HWR_DATA_

#include "hwr_defs.h"

#include <windows.h>

//THIS MUST DISAPPEAR!!!
#include <glide.h>


// ==========================================================================
//                                                                 STRUCTURES
// ==========================================================================

// primary 2d output vertex layout (currently the only one we use)
// note: vertexlayout is set by vidglide.c
typedef struct {
        float   x,y;
        unsigned long   argb;
        float   oow;                    // one-over-w

    float   sow;            /* s texture ordinate (s over w) */
    float   tow;            /* t texture ordinate (t over w) */  
} wallVert2D;

// 3d model source coordinates,
// for original & transformed coordinates
typedef struct {
    float x;
    float y;
    float z;
    float w;
    float s;            /* s texture ordinate (s over w) */
    float t;            /* t texture ordinate (t over w) */  
} wallVert3D;

typedef struct {
    float   x;
    float   y;
} v2d_t;

typedef struct {
    float x,y,z,w,s,t;
} v3d_t;


// ==========================================================================
//                                                                       MISC
// ==========================================================================

// when Doom patches are unpacked to the 3Dfx format, the default background color
// is set to this color index, thus holes in the patches will use the value of this
// color index (with 8bit palettized textures)
#define HWR_PATCHES_CHROMAKEY_COLORINDEX     240

// this value is always set for the CHROMAKEY_COLORINDEX above, disregarding any
// color value of the Doom palette
#define HWR_PATCHES_CHROMAKEY_COLORVALUE     (0x00000000)    //ARGB format as in grSstWinOpen()



// ==========================================================================
//                                                               TEXTURE INFO
// ==========================================================================

// grInfo.data holds the address of the graphics data cached in heap memory
//                NULL if the texture is not in Doom heap cache.
struct GlideMipmap_s {
        GrTexInfo   grInfo;             //for TexDownloadMipMap
        FxU32       startAddress;       //offset in hardware cache
        FxU32       mipmapSize;         //size of mipmap
        BOOL        downloaded;

        struct GlideMipmap_s*  next;
        FxU32       cachepos;
};
typedef struct GlideMipmap_s GlideMipmap_t;


//
// Doom texture info, as cached for 3Dfx rendering
//
struct GlideTexture_s {
        GlideMipmap_t mipmap;
        float       scaleX;             //used for scaling textures on walls
        float       scaleY;
};
typedef struct GlideTexture_s GlideTexture_t;


// a cached patch as converted to 3Dfx format, holding the original patch_t
// header so that the existing code can retrieve ->width, ->height as usual
// This is returned by W_CachePatchNum()/W_CachePatchName(), when rendermode
// is 'render_glide'. Else it returns the normal patch_t data.

// for (unsigned long) compare with s3Dfx[] member
#define ID_3DFX_PATCH   (('x'<<24) | ('f'<<16) | ('D'<<8) | '3')

struct GlidePatch_s {
        // the 4 first fields come right away from the original patch_t
        short               width;
        short               height;
        short               leftoffset;     // pixels to the left of origin
        short               topoffset;      // pixels below the origin
        //
        char                s3Dfx[4];       // holds '3' 'D' 'f' 'x'
        float               max_s,max_t;
        int                 patchlump;      // the software patch lump num for when the 3Dfx patch
                                            // was flushed, and we need to re-create it
        GlideMipmap_t       mipmap;
};
typedef struct GlidePatch_s GlidePatch_t;


#endif //_HWR_DATA_