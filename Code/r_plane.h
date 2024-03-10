
// r_plane.h : Refresh, visplane stuff (floor, ceilings).

#ifndef __R_PLANE__
#define __R_PLANE__

#include "screen.h"     //needs MAXVIDWIDTH/MAXVIDHEIGHT
#include "r_data.h"

//
// Now what is a visplane, anyway?
// Simple : kinda floor/ceiling polygon optimised for Doom rendering.
// 4124 bytes!
//
typedef struct
{
  fixed_t               height;
  int                   picnum;
  int                   lightlevel;
  int                   minx;
  int                   maxx;

  // leave pads for [minx-1]/[maxx+1]

  //faB: words sucks .. should get rid of that.. but eats memory
  //added:08-02-98: THIS IS UNSIGNED! VERY IMPORTANT!!
  unsigned short         pad1;
  unsigned short         top[MAXVIDWIDTH];
  unsigned short         pad2;
  unsigned short         pad3;
  unsigned short         bottom[MAXVIDWIDTH];
  unsigned short         pad4;

} visplane_t;

extern visplane_t*    floorplane;
extern visplane_t*    ceilingplane;
extern visplane_t*    waterplane;

// Visplane related.
extern  short*          lastopening;

typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t  floorfunc;
extern planefunction_t  ceilingfunc_t;

extern short            floorclip[MAXVIDWIDTH];
extern short            ceilingclip[MAXVIDWIDTH];
extern short            waterclip[MAXVIDWIDTH];   //added:18-02-98:WATER!
extern fixed_t          yslopetab[MAXVIDHEIGHT*4];

extern fixed_t*         yslope;
extern fixed_t          distscale[MAXVIDWIDTH];

void R_InitPlanes (void);
void R_ClearPlanes (player_t *player);

void R_MapPlane
( int           y,
  int           x1,
  int           x2 );

void R_MakeSpans
( int           x,
  int           t1,
  int           b1,
  int           t2,
  int           b2 );

void R_DrawPlanes (void);

visplane_t* R_FindPlane
( fixed_t       height,
  int           picnum,
  int           lightlevel );

visplane_t* R_CheckPlane
( visplane_t*   pl,
  int           start,
  int           stop );



#endif
