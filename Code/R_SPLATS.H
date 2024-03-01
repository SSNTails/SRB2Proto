// r_splats.h : flat sprites & blood splats effects

#ifndef __R_SPLATS_H__
#define __R_SPLATS_H__

#include "r_defs.h"

//#define WALLSPLATS      // comment this out to compile without splat effects
//#define FLOORSPLATS

#define MAXLEVELSPLATS      1024

// splat flags
#define SPLATDRAWMODE_MASK   0x03       // mask to get drawmode from flags
#define SPLATDRAWMODE_OPAQUE 0x00
#define SPLATDRAWMODE_SHADE  0x01
#define SPLATDRAWMODE_TRANS  0x02


// ==========================================================================
// DEFINITIONS
// ==========================================================================

// WALL SPLATS are patches drawn on top of wall segs
struct wallsplat_s {
    int         patch;      // lump id.
    vertex_t    v1;         // vertices along the linedef
    vertex_t    v2;
    fixed_t     top;
    fixed_t     offset;     // offset in columns<<FRACBITS from start of linedef to start of splat
    int         flags;
    //short       xofs, yofs;
    //int         tictime;
    line_t*     line;       // the parent line of the splat seg
    struct wallsplat_s * next;
};
typedef struct wallsplat_s wallsplat_t;

// FLOOR SPLATS are pic_t (raw horizontally stored) drawn on top of the floor or ceiling
struct floorsplat_s {
    int         pic;        // a pic_t lump id
    int         flags;
    vertex_t    verts[4];   // (x,y) as viewn from above on map
    fixed_t     z;          //     z (height) is constant for all the floorsplat
    subsector_t* subsector;       // the parent subsector
    struct floorsplat_s * next;
    struct floorsplat_s * nextvis;
};
typedef struct floorsplat_s floorsplat_t;



//p_setup.c
extern fixed_t P_SegLength (seg_t* seg);

// call at P_SetupLevel()
void R_ClearLevelSplats (void);

void R_AddWallSplat (line_t* wallline, char* patchname, fixed_t top, fixed_t wallfrac, int flags);
void R_AddFloorSplat (subsector_t* subsec, char* picname, fixed_t x, fixed_t y, fixed_t z, int flags);

void R_ClearVisibleFloorSplats (void);
void R_AddVisibleFloorSplats (subsector_t* subsec);
void R_DrawVisibleFloorSplats (void);


#endif __R_SPLATS_H__
