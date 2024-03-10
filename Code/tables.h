// tables.h : lookup tables
//
//      int finetangent[4096]   - Tangens LUT.
//       Should work with BAM fairly well (12 of 16bit,
//      effectively, by shifting).
//
//      int finesine[10240]             - Sine lookup.
//
//      int tantoangle[2049]    - ArcTan LUT,
//        maps tan(angle) to angle fast. Gotta search.


#ifndef __TABLES__
#define __TABLES__

#ifdef LINUX
#include <math.h>
#else
//#define PI                              3.141592657
#endif

#include "m_fixed.h"

#define FINEANGLES              8192
#define FINEMASK                (FINEANGLES-1)
#define ANGLETOFINESHIFT        19      // 0x100000000 to 0x2000


// Effective size is 10240.
extern  fixed_t         finesine[5*FINEANGLES/4];

// Re-use data, is just PI/2 pahse shift.
extern  fixed_t*        finecosine;


// Effective size is 4096.
extern fixed_t          finetangent[FINEANGLES/2];

#define ANG45           0x20000000
#define ANG90           0x40000000
#define ANG180          0x80000000
#define ANG270          0xc0000000

#define ANGLE_45    0x20000000
#define ANGLE_90    0x40000000
#define ANGLE_180   0x80000000
#define ANGLE_MAX   0xffffffff
#define ANGLE_1     (ANGLE_45/45)
#define ANGLE_60    (ANGLE_180/3)

typedef unsigned angle_t;


// to get a global angle from cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system, then
// the y (<=x) is scaled and divided by x to get a tangent (slope) value
// which is looked up in the tantoangle[] table.
#define SLOPERANGE  2048
#define SLOPEBITS   11
#define DBITS       (FRACBITS-SLOPEBITS)

// The +1 size is to handle the case when x==y without additional checking.
extern  angle_t     tantoangle[SLOPERANGE+1];

// Utility function, called by R_PointToAngle.
int SlopeDiv ( unsigned      num,
               unsigned      den);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
