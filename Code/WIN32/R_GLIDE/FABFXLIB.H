
#ifndef _TESTLIB_H_
#define _TESTLIB_H_

#include <glide.h>

#define DEGREE (.01745328f)

/* The two most commonly defined macros in the known universe */
#ifndef MIN
#define MIN(__x, __y) (((__x) < (__y)) ? (__x) : (__y))
#endif

#ifndef MAX
#define MAX(__x, __y) (((__x) < (__y)) ? (__y) : (__x))
#endif


/*
** move the vertex layout defintion to application
*/
typedef struct {
  float  sow;                   /* s texture ordinate (s over w) */
  float  tow;                   /* t texture ordinate (t over w) */  
  float  oow;                   /* 1/w (used mipmapping - really 0xfff/w) */
}  GrTmuVertex;

typedef struct
{
  float x, y;         /* X and Y in screen space */
  float ooz;          /* 65535/Z (used for Z-buffering) */
  float oow;          /* 1/W (used for W-buffering, texturing) */
  float r, g, b, a;   /* R, G, B, A [0..255.0] */
  float z;            /* Z is ignored */
  GrTmuVertex  tmuvtx[GLIDE_NUM_TMU];
} GrVertex;

#define GR_VERTEX_X_OFFSET              0
#define GR_VERTEX_Y_OFFSET              1
#define GR_VERTEX_OOZ_OFFSET            2
#define GR_VERTEX_OOW_OFFSET            3
#define GR_VERTEX_R_OFFSET              4
#define GR_VERTEX_G_OFFSET              5
#define GR_VERTEX_B_OFFSET              6
#define GR_VERTEX_A_OFFSET              7
#define GR_VERTEX_Z_OFFSET              8
#define GR_VERTEX_SOW_TMU0_OFFSET       9
#define GR_VERTEX_TOW_TMU0_OFFSET       10
#define GR_VERTEX_OOW_TMU0_OFFSET       11
#define GR_VERTEX_SOW_TMU1_OFFSET       12
#define GR_VERTEX_TOW_TMU1_OFFSET       13
#define GR_VERTEX_OOW_TMU1_OFFSET       14
#if (GLIDE_NUM_TMU > 2)
#define GR_VERTEX_SOW_TMU2_OFFSET       15
#define GR_VERTEX_TOW_TMU2_OFFSET       16
#define GR_VERTEX_OOW_TMU2_OFFSET       17
#endif

void  tlSetScreen( float width, float height );

float tlScaleX( float coord );
float tlScaleY( float coord );

void tlConSet( float minX, float minY, 
               float maxX, float maxY,
               int columns, int rows,
               int color );
int  tlConOutput( const char *fmt, ... );
void tlConClear();
void tlConRender();
void tlPrintNumber (int frame);     //lame-o routine


// ----------------
// 3D stuff
// ----------------

// vertex structure as passed to hardware
// - these are projected textures
typedef struct {
	float	x,y;
	unsigned long   argb;
} myVertex2D;

// 3d model source coordinates,
// for original & transformed coordinates
typedef struct {
    float x;
    float y;
    float z;
    float w;
    /*float s;
    float t;
    float r;
    float g;
    float b;
    float a;*/
} myVertex3D;

typedef float TlMatrix[4][4];

extern  TlMatrix currentMatrix;

const float *tlInitMatrix( void );
const float *tlZRotation( float degrees );
const float *tlYRotation( float degrees );
const float *tlXRotation( float degrees );
const float *tlTranslation( float x, float y, float z );
void		tlSetMatrix( const float *m );
void		tlMultMatrix( const float *m );
void		tlTransformVertices( myVertex3D *dstList, 
					             myVertex3D *srcList, 
							     unsigned length );
void		tlProjectVertices( myVertex2D *dstList,
					           myVertex3D *srcList, 
							   unsigned length );


#endif