
// faB's 3dfx lib : handy debugging tlConOutput() console overlay, mainly.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <assert.h>

#include <math.h>

#include <glide.h>
#include "fabfxlib.h"


//-------------------------------------------------------------------
//Function: tlSetConsole
//-------------------------------------------------------------------
static unsigned char fontTable[128][2];
static GrTexInfo     fontInfo;
static unsigned long fontAddress;                       // download font to this start address in card mem.
static const char    fontString[] = "ABCDEFGHIJKLMN"
                                    "OPQRSTUVWXYZ01"
                                    "23456789.,;:*-"
                                    "+/_()<>|[]{}! ";

static const int fontWidth    = 9*2;
static const int fontHeight   = 12*2;
static const int charsPerLine = 14;

static int fontInitialized;

static void grabTex( FxU32 addr, void *storage );
static void putTex( FxU32 addr, void *storage );
static void consoleScroll( void );
static void drawChar( char character, float x, float y, float w, float h );

#include "fontdata.inc"


//---------------------------------------------------------------
//  Set up screen scaling
//
//  width - width of screen
//  height - height of screen
//-------------------------------------------------------------------
static float scrXScale;
static float scrYScale;

void tlSetScreen( float width, float height )
{
    scrXScale = width;
    scrYScale = height;
    return;
}


//-------------------------------------------------------------------
//  Initialize Console for printing.  The console will scroll text 
//  60 column text in the window described by minx, miny, maxx, maxy.
//
//  minX, minY - upper left corner of console
//  maxX, maxY - lower right corner of console
//  rows    - rows of text to display
//  columns - columns to display before scroll
//-------------------------------------------------------------------
static char *consoleGrid;           // text buffer of the console
static int   consoleRows;           // how many rows
static int   consoleColumns;        // how many columns
static int   consoleX;              // cursor pos in console text buffer
static int   consoleY;
static int   consoleColor;          // RGB color for the console text
static float consoleOriginX;        // minX, minY
static float consoleOriginY;
static float consoleCharWidth;      // character size in units of window coords (0.0f to 1.0f)
static float consoleCharHeight;

static void     *state = NULL;      // used to save current Glide state
static void     *vlstate = NULL;    // used to save current Glive vertex layout 


void tlConSet( float minX, float minY,
               float maxX, float maxY,
               int columns, int rows,
               int color )
{
    int entry;
    char xCoord;
    char yCoord;

    if (state == NULL)
    {
        FxI32 state_size;
        grGet (GR_GLIDE_STATE_SIZE, 4, &state_size);
        state = malloc(state_size);
    }
    if (vlstate == NULL)
    {
        FxI32 state_size;
        grGet (GR_GLIDE_VERTEXLAYOUT_SIZE, 4, &state_size);
        vlstate = malloc(state_size);        
    }
    
    fontInfo.smallLodLog2    = GR_LOD_LOG2_128;
    fontInfo.largeLodLog2    = GR_LOD_LOG2_128;
    fontInfo.aspectRatioLog2 = GR_ASPECT_LOG2_2x1;      // 128x64 texture
    fontInfo.format          = GR_TEXFMT_ALPHA_8;       // one byte per pixel 0 black 255 white
    fontInfo.data            = &fontData[0];

    if ( getenv( "FX_GLIDE_NO_FONT" ) )
    {
        fontInitialized = 0;
        return;
    }

    for (entry = 1; entry < 128; entry++)
    {
        char *hit = strchr( fontString, entry );
        if ( hit ) {
            int offset = hit - fontString;
            
            xCoord = ( offset % charsPerLine )  * fontWidth;
            yCoord = ( offset / charsPerLine )  * fontHeight; 

            fontTable[entry][0] = xCoord;
            fontTable[entry][1] = yCoord;
        }
    }
    
    if ( consoleGrid ) free( consoleGrid );

    consoleGrid = calloc( sizeof( char ), rows * columns );
    memset( consoleGrid, 32, rows*columns );
    consoleRows       = rows;
    consoleColumns    = columns;
    consoleX = consoleY = 0;

    consoleColor      = color;
    consoleOriginX    = minX;
    consoleOriginY    = minY;
    consoleCharWidth  = ( (maxX - minX)/(float)columns );
    consoleCharHeight = ( (maxY - minY)/(float)rows    );

    // where to download the font in card memory
    fontAddress = grTexMaxAddress( 0 ) - 
      grTexCalcMemRequired( fontInfo.smallLodLog2, fontInfo.largeLodLog2,
                            fontInfo.aspectRatioLog2, fontInfo.format );

    fontInitialized = 1;

    return; 
};

// -------------------------------------------------------------------
//  Output a printf style string to the console
//
//  fmt - format string
//  ... - other args
// Return:
//  int - number of chars printed
//  -------------------------------------------------------------------
int tlConOutput( const char *fmt, ... )
{
    int rv = 0;
    va_list argptr;

    if( fontInitialized ) {
        static char buffer[1024];
        const char *c;

        va_start( argptr, fmt );
        rv = vsprintf( buffer, fmt, argptr );
        va_end( argptr );

#if defined(__MWERKS__)
                                {
                                        char* temp = buffer;
                                        
                                        while(*temp != '\0') {
                                                *temp = toupper(*temp);
                                                temp++;
                                        }
                                }
#else
        strupr( buffer );
#endif
        
        c = buffer;

        /* update console grid */

        while( *c ) {
            switch( *c ) {
            case '\n':
                consoleY++;
            case '\r':
                consoleX = 0;
                if ( consoleY >= consoleRows ) {
                    consoleY = consoleRows - 1;
                    consoleScroll();
                }
                break;
            default:
                if ( consoleX >= consoleColumns ) {
                    consoleX = 0;
                    consoleY++;
                    if ( consoleY >= consoleRows ) {
                        consoleY = consoleRows - 1;
                        consoleScroll();
                    }
                }
                consoleGrid[(consoleY*consoleColumns)+consoleX]=*c;
                consoleX++;
                break;
            }
            c++;
        }
    }

    return rv;
}


// -------------------------------------------------------------------
// Clear the console
// -------------------------------------------------------------------
void tlConClear()
{
    memset( consoleGrid, 32, consoleRows*consoleColumns );
    consoleX = consoleY = 0;
    return;
}


// -------------------------------------------------------------------
// lame-o routine to show some number on top-left o'console
// (eg: frame) so I know the thing is alive and did not hang ..
// -------------------------------------------------------------------
void tlPrintNumber (int frame)
{
    int cx,cy;
    cx=consoleX; cy = consoleY;
    consoleX = consoleColumns - 16;
    consoleY = consoleRows - 1;
    tlConOutput("fps %#03d\n", frame);
    consoleX = cx; consoleY = cy;
}


// -------------------------------------------------------------------
// Render the console
// -------------------------------------------------------------------
void tlConRender( void )
{
    if( fontInitialized ) {
        int x, y;
        
        // save current state and vertex layout
        grGlideGetState( state );
        grGlideGetVertexLayout( vlstate );

        grVertexLayout(GR_PARAM_XY,  0, GR_PARAM_ENABLE);
        grVertexLayout(GR_PARAM_Q,   GR_VERTEX_OOW_OFFSET << 2, GR_PARAM_ENABLE);       //depth
        grVertexLayout(GR_PARAM_ST0, GR_VERTEX_SOW_TMU0_OFFSET << 2, GR_PARAM_ENABLE);  //s and t for tmu0

        grCoordinateSpace(GR_WINDOW_COORDS);
        grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,        // factor * Color other
                        GR_COMBINE_FACTOR_LOCAL,                // local color is factor
                        GR_COMBINE_LOCAL_CONSTANT,              // local is constant color
                        GR_COMBINE_OTHER_TEXTURE,               // color from texture map
                        FXFALSE );
        grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,        // factor * Alpha other
                        GR_COMBINE_FACTOR_ONE,                  // factor = 1
                        GR_COMBINE_LOCAL_NONE,                  // unspecified alpha
                        GR_COMBINE_OTHER_TEXTURE,               // alpha from texture map
                        FXFALSE );
        grTexCombine( GR_TMU0,
                      GR_COMBINE_FUNCTION_LOCAL,    // rgb func   : Color local
                      GR_COMBINE_FACTOR_NONE,       // rgb factor : none
                      GR_COMBINE_FUNCTION_LOCAL,    // alpha func   : Alpha local
                      GR_COMBINE_FACTOR_NONE,       // alpha factor : none
                      FXFALSE, 
                      FXFALSE );
        // make translucence with framebuffer
        grAlphaBlendFunction( GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA,
                              GR_BLEND_ONE, GR_BLEND_ZERO );
        grAlphaTestFunction( GR_CMP_ALWAYS );
        
		grTexFilterMode( GR_TMU0,
                         GR_TEXTUREFILTER_BILINEAR,
                         GR_TEXTUREFILTER_BILINEAR );
        
		grTexMipMapMode( GR_TMU0,
                         GR_MIPMAP_DISABLE,         // no mipmaps based on depth
                         FXFALSE );
        grDepthBufferFunction( GR_CMP_ALWAYS );
        grAlphaTestReferenceValue( 0x1 );
        grSstOrigin( GR_ORIGIN_UPPER_LEFT );
        grCullMode( GR_CULL_DISABLE );
        grTexDownloadMipMap( 0, fontAddress, GR_MIPMAPLEVELMASK_BOTH,
                             &fontInfo );
        grTexSource( 0, fontAddress, 
                     GR_MIPMAPLEVELMASK_BOTH, &fontInfo );
        grClipWindow( (int)tlScaleX(0.0f),(int)tlScaleY(0.0f),
                      (int)tlScaleX(1.0f),(int)tlScaleY(1.0f) );

        for( y = 0; y < consoleRows; y++ ) {
            float charX = consoleOriginX;
            float charY = consoleOriginY+(consoleCharHeight*y);
            for( x = 0; x < consoleColumns; x++ ) {
                drawChar( consoleGrid[(y*consoleColumns)+x],
                          charX, charY, 
                          consoleCharWidth, 
                          consoleCharHeight );
                charX += consoleCharWidth;
            }
        }

        grGlideSetState(state);
        grGlideSetVertexLayout( vlstate );
    }

    return;
}


static void readRegion( void *data, 
                        int x, int y,
                        int w, int h );
static void writeRegion( void *data, 
                         int x, int y,
                         int w, int h );

static void putTex( FxU32 addr, void *storage )
{
    GrTexInfo texInfo;

    texInfo.smallLodLog2    = GR_LOD_LOG2_256;
    texInfo.largeLodLog2    = GR_LOD_LOG2_256;
    texInfo.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
    texInfo.format      = GR_TEXFMT_RGB_565;
    texInfo.data        = storage;

    grTexDownloadMipMap( 0, addr, GR_MIPMAPLEVELMASK_BOTH, &fontInfo );
}


static void grabTex( FxU32 addr, void *storage )
{
    static FxU16 tmpSpace[256][256];
    GrTexInfo   texInfo;
    GrVertex    a, b, c, d;

    grGlideGetState( state );
    grDitherMode( GR_DITHER_DISABLE );
    grColorMask( FXTRUE, FXFALSE );
    grSstOrigin( GR_ORIGIN_UPPER_LEFT );
    grCullMode( GR_CULL_DISABLE );

    /* Grab Upper Left 256*256 of frame buffer */
    readRegion( tmpSpace, 0, 0, 256, 256 );

    /* Grab First 256x256 MM in Texture Ram */
    texInfo.smallLodLog2    = GR_LOD_LOG2_256;
    texInfo.largeLodLog2    = GR_LOD_LOG2_256;
    texInfo.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
    texInfo.format      = GR_TEXFMT_RGB_565;
    texInfo.data        = 0;
    grTexMipMapMode( 0, GR_MIPMAP_DISABLE, FXFALSE );
    grTexFilterMode( 0, 
                     GR_TEXTUREFILTER_POINT_SAMPLED, 
                     GR_TEXTUREFILTER_POINT_SAMPLED );
    grTexCombine( 0, 
                  GR_COMBINE_FUNCTION_LOCAL, 
                  GR_COMBINE_FACTOR_NONE,
                  GR_COMBINE_FUNCTION_LOCAL, 
                  GR_COMBINE_FACTOR_NONE,
                  FXFALSE,
                  FXFALSE );
    grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,
                    GR_COMBINE_FACTOR_ONE,
                    GR_COMBINE_LOCAL_NONE,
                    GR_COMBINE_OTHER_TEXTURE,
                    FXFALSE );
    grTexSource( 0, addr, GR_MIPMAPLEVELMASK_BOTH, &texInfo );
    grAlphaBlendFunction( GR_BLEND_ONE, GR_BLEND_ZERO,
                          GR_BLEND_ONE, GR_BLEND_ZERO);
    grDepthBufferFunction( GR_DEPTHBUFFER_DISABLE );
    grAlphaTestFunction( GR_CMP_ALWAYS );    
    grFogMode( GR_FOG_DISABLE );
    grCullMode( GR_CULL_DISABLE );
    grChromakeyMode( GR_CHROMAKEY_DISABLE );
    /*-------------------
      A---B
      | \ |
      C---D
      -------------------*/
    a.oow = a.tmuvtx[0].oow = 1.0f;
    b = c = d = a;
    a.x = c.x = a.y = b.y = 0.5f;
    b.x = d.x = c.y = d.y = 255.6f;
    a.tmuvtx[0].sow = c.tmuvtx[0].sow = a.tmuvtx[0].tow = b.tmuvtx[0].tow =
        0.5f;
    b.tmuvtx[0].sow = d.tmuvtx[0].sow = c.tmuvtx[0].tow = d.tmuvtx[0].tow =
        0.5f;
    grDrawTriangle( &a, &d, &c );
    grDrawTriangle( &a, &b, &d );
    readRegion( storage, 0, 0, 256, 256 );
    
    /* Restore The Upper Left Hand of Frame Buffer */
    writeRegion( tmpSpace, 0, 0, 256, 256 );
    grGlideSetState( state );
    return;
}


static void readRegion( void *data, 
                        int sx, int sy,
                        int w, int h ) {
    int x; int y;
    GrLfbInfo_t info;
    
    info.size = sizeof( info );

    assert( grLfbLock( GR_LFB_READ_ONLY,
                       GR_BUFFER_BACKBUFFER,
                       GR_LFBWRITEMODE_ANY,
                       GR_ORIGIN_UPPER_LEFT,
                       FXFALSE,
                       &info ) );

    for( y = 0; y < h; y++ ) {
        unsigned short *dst = ((unsigned short *)data+
                                (w*y));
        unsigned short *src = (unsigned short*)(((char*)info.lfbPtr)
                                                +(info.strideInBytes*(sy+y))
                                                +(sx<<1));
        for( x = 0; x < w; x++ ) {
            *dst++ = *src++;
        }
    }

    
    assert( grLfbUnlock( GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER ) );
    return;
}
static void writeRegion( void *data, 
                         int sx, int sy,
                         int w, int h ) {
    int x; int y;
    GrLfbInfo_t info;

    info.size = sizeof( info );

    assert( grLfbLock( GR_LFB_WRITE_ONLY,
                       GR_BUFFER_BACKBUFFER,
                       GR_LFBWRITEMODE_ANY,
                       GR_ORIGIN_UPPER_LEFT,
                       FXFALSE,
                       &info ) );

    for( y = 0; y < h; y++ ) {
        unsigned short *src = ((unsigned short *)data+
                                (w*y));
        unsigned short *dst = (unsigned short*)(((char*)info.lfbPtr)
                                                +(info.strideInBytes*(sy+y))
                                                +(sx<<1));
        for( x = 0; x < w; x++ ) {
            *dst++ = *src++;
        }
    }

    assert( grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER ) );
    return;
}


static void consoleScroll( void )
{
    // move console text one row up
    memmove( consoleGrid, 
             consoleGrid + consoleColumns,
             (consoleRows-1)*consoleColumns );
    // clear the new line that entered at bottom of console buffer
    memset( consoleGrid+(consoleRows-1)*consoleColumns,
            32, 
            consoleColumns );
}

static void drawChar( char character, float x, float y, float w, float h )
{
    GrVertex a, b, c, d;
    /* a---b
       |\  |    
       | \ |
       |  \|
       c---d */

    if ( character == 32 ) return;

    a.oow = b.oow = c.oow = d.oow = 1.0f;

    a.x = c.x = tlScaleX(x);
    a.y = b.y = tlScaleY(y);
    d.x = b.x = tlScaleX(x+w);
    d.y = c.y = tlScaleY(y+h);

    grConstantColorValue( consoleColor );

    a.tmuvtx[0].sow = c.tmuvtx[0].sow = 
        (float)fontTable[character][0];
    a.tmuvtx[0].tow = b.tmuvtx[0].tow = 
        (float)fontTable[character][1];
    d.tmuvtx[0].sow = b.tmuvtx[0].sow = 
        a.tmuvtx[0].sow + (float)fontWidth;
    d.tmuvtx[0].tow = c.tmuvtx[0].tow = 
        a.tmuvtx[0].tow + (float)fontHeight;

    grDrawTriangle( &a, &d, &c );
    grDrawTriangle( &a, &b, &d );
    return;
}


//-------------------------------------------------------------------
//  Scale X coordinates from normalized device coordinates [ 0.0, 1.0 )
//  to Screen Coordinates [ 0.0, WidthOfScreenInPixels )
//-------------------------------------------------------------------
float tlScaleX( float coord ) {
    return coord * scrXScale;
}

// -------------------------------------------------------------------
// Scale Y coordinates from normalized device coordinates [ 0.0, 1.0 )
// to Screen Coordinates [ 0.0, HeightOfScreenInPixels )
// -------------------------------------------------------------------
float tlScaleY( float coord ) {
    return coord *scrYScale;
}


// ==========================================================================
// 3D stuff
// ==========================================================================


/* -------------------------------------------------------------------
   Return an identity matrix
   ------------------------------------------------------------------- */
    TlMatrix currentMatrix;

const float *tlInitMatrix( void )
{
    static TlMatrix m;
    m[0][0] = 1.0f, m[0][1] = 0.0f, m[0][2] = 0.0f, m[0][3] = 0.0f;
    m[1][0] = 0.0f, m[1][1] = 1.0f, m[1][2] = 0.0f, m[1][3] = 0.0f;
    m[2][0] = 0.0f, m[2][1] = 0.0f, m[2][2] = 1.0f, m[2][3] = 0.0f;
    m[3][0] = 0.0f, m[3][1] = 0.0f, m[3][2] = 0.0f, m[3][3] = 1.0f;
    return &m[0][0];
}


/* -------------------------------------------------------------------
   Generate a rotation about the x axis
   Arguments:
    degrees - number of degrees to rotate
   Return:
    const point to rotation matrix
   ------------------------------------------------------------------- */
const float *tlXRotation( float degrees )
{
    static TlMatrix m;
    float c = (float)cos( degrees * DEGREE );
    float s = (float)sin( degrees * DEGREE );
    m[0][0] = 1.0f, m[0][1] = 0.0f, m[0][2] = 0.0f, m[0][3] = 0.0f;
    m[1][0] = 0.0f, m[1][1] =    c, m[1][2] =    s, m[1][3] = 0.0f;
    m[2][0] = 0.0f, m[2][1] =   -s, m[2][2] =    c, m[2][3] = 0.0f;
    m[3][0] = 0.0f, m[3][1] = 0.0f, m[3][2] = 0.0f, m[3][3] = 1.0f;
    return &m[0][0];
}


/* -------------------------------------------------------------------
   Generate a rotation about the y axis
   Arguments:
    degrees - number of degrees to rotate
   Return:
    const point to rotation matrix
   ------------------------------------------------------------------- */
const float *tlYRotation( float degrees )
{
    static TlMatrix m;
    float c = (float)cos( degrees * DEGREE );
    float s = (float)sin( degrees * DEGREE );
    m[0][0] =    c, m[0][1] = 0.0f, m[0][2] =   -s, m[0][3] = 0.0f;
    m[1][0] = 0.0f, m[1][1] = 1.0f, m[1][2] = 0.0f, m[1][3] = 0.0f;
    m[2][0] =    s, m[2][1] = 0.0f, m[2][2] =    c, m[2][3] = 0.0f;
    m[3][0] = 0.0f, m[3][1] = 0.0f, m[3][2] = 0.0f, m[3][3] = 1.0f;
    return &m[0][0];
}


/* -------------------------------------------------------------------
   Generate a rotation about the z axis
   Arguments:
    degrees - number of degrees to rotate
   Return:
    const point to rotation matrix
   ------------------------------------------------------------------- */
const float *tlZRotation( float degrees )
{
    static TlMatrix m;
    float c = (float)cos( degrees * DEGREE );
    float s = (float)sin( degrees * DEGREE );
    m[0][0] =    c, m[0][1] =    s, m[0][2] = 0.0f, m[0][3] = 0.0f;
    m[1][0] =   -s, m[1][1] =    c, m[1][2] = 0.0f, m[1][3] = 0.0f;
    m[2][0] = 0.0f, m[2][1] = 0.0f, m[2][2] = 1.0f, m[2][3] = 0.0f;
    m[3][0] = 0.0f, m[3][1] = 0.0f, m[3][2] = 0.0f, m[3][3] = 1.0f;
    return &m[0][0];
}


/* -------------------------------------------------------------------
   Generate a translation matrix
   Arguments:
    x, y, z - offsets to translate origin
   Return:
    const point to translation matrix
   ------------------------------------------------------------------- */
const float *tlTranslation( float x, float y, float z )
{
    static TlMatrix m;
    m[0][0] = 1.0f, m[0][1] = 0.0f, m[0][2] = 0.0f, m[0][3] = 0.0f;
    m[1][0] = 0.0f, m[1][1] = 1.0f, m[1][2] = 0.0f, m[1][3] = 0.0f;
    m[2][0] = 0.0f, m[2][1] = 0.0f, m[2][2] = 1.0f, m[2][3] = 0.0f;
    m[3][0] =    x, m[3][1] =    y, m[3][2] =    z, m[3][3] = 1.0f;
    return &m[0][0];
}


/* -------------------------------------------------------------------
   Set the current matrix.  This matrix translates the object into
   View space from local coordiantes during calls to transformVertices
   All spaces are considered to by -1.0->1.0 normalized.
   Arguments:
    m - pointer to matrix
   Return:
    none
   ------------------------------------------------------------------- */
void tlSetMatrix( const float *m ) {
    memcpy( currentMatrix, m, sizeof( TlMatrix ) );
    return;
}


/* -------------------------------------------------------------------
   Multiply the current matrix by the provided matrix
   Arguments:
    matrix to post-cat to the current matrix
   Return:
    none
   ------------------------------------------------------------------- */
void tlMultMatrix( const float *m )
{
    TlMatrix result;
    TlMatrix mat;
    int i, j;

    memcpy( mat, m, sizeof( TlMatrix ) );

    for( j = 0; j < 4; j++ ) {
        for( i = 0; i < 4; i++ ) {
            result[j][i] = 
                currentMatrix[j][0] * mat[0][i] +
                currentMatrix[j][1] * mat[1][i] +
                currentMatrix[j][2] * mat[2][i] +
                currentMatrix[j][3] * mat[3][i];
        }
    }
    memcpy( currentMatrix, result, sizeof( TlMatrix ) );
}


/* -------------------------------------------------------------------
   Transform a list of vertices from model space into view space
   Arguments:
    dstVerts - memory to store transformed vertices
    srcVerts - array of vertices to be transformed
    length - number of vertices to transform
   Return:
    none
   ------------------------------------------------------------------- */
void tlTransformVertices( myVertex3D *dstVerts, myVertex3D *srcVerts, unsigned length )
{
    myVertex3D tmp, v;
    TlMatrix m;
    unsigned i;

    memcpy( m, currentMatrix, sizeof( TlMatrix ) );
    for( i = 0; i < length; i++ ) {
        v = srcVerts[i];
        tmp = v;
        tmp.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + v.w * m[3][0];
        tmp.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + v.w * m[3][1];
        tmp.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + v.w * m[3][2];
        tmp.w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + v.w * m[3][3];
        dstVerts[i] = tmp;
    }
    return;
}

/* -------------------------------------------------------------------
   perspective project a set of vertices into normalized 2D space (0,1)
   Arguments:
    dstVerts - memory to store projected vertices
    srcVerts - array of vertices to be transformed
    length - number of vertices to transform
   Return:
    none
   ------------------------------------------------------------------- */
#define VP_OFFSET 1.0f
#define VP_SCALE  0.5f
#define VP_SCALEY  0.6f

void tlProjectVertices( myVertex2D *dstVerts, myVertex3D *srcVerts, unsigned length )
{
	myVertex3D tmp,v;
    //myVertex2D ;
    TlMatrix m;
    unsigned i;

    /* simplified perspective proj matrix assume unit clip volume */
    m[0][0] = 1.0f, m[0][1] = 0.0f, m[0][2] = 0.0f, m[0][3] = 0.0f;
    m[1][0] = 0.0f, m[1][1] = 1.0f, m[1][2] = 0.0f, m[1][3] = 0.0f;
    m[2][0] = 0.0f, m[2][1] = 0.0f, m[2][2] = 1.0f, m[2][3] = 1.0f;
    m[3][0] = 0.0f, m[3][1] = 0.0f, m[3][2] = 0.0f, m[3][3] = 0.0f;

    for( i = 0; i < length; i++ )
	{
        v = srcVerts[i];
        tmp.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + v.w * m[3][0];
        tmp.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + v.w * m[3][1];
        tmp.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + v.w * m[3][2];
        tmp.w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + v.w * m[3][3];
        if (tmp.w)
		{
		    tmp.x /= tmp.w, tmp.y /= tmp.w, tmp.z /= tmp.w;
		}
        dstVerts[i].x = (tmp.x + VP_OFFSET) * VP_SCALE;
        dstVerts[i].y = (tmp.y + VP_OFFSET) * VP_SCALEY;
    }
}
