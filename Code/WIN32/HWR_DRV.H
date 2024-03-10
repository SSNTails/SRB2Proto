// hwr_drv.h : macros for defining/using exports from the hardware renderer DLL

#ifndef __HWR_DRV_H__
#define __HWR_DRV_H__

#include "hwr_data.h"
#include "hwr_defs.h"

// Function declaration for exports from the DLL :
// EXPORT <return-type> HWRAPI(<function-name>) ( <arguments> ) ;
// If _CREATE_DLL_ is defined the above declaration translates to :
// __declspec(dllexport) <return-type> WINAPI <function-name> ( <arguments> ) ;
// If _CREATE_DLL_ is NOT DEFINED the above declaration translates to :
// __declspec(dllexport) <return->type> (WINAPI *<function-name>) ( <arguments> ) ;

#ifdef _CREATE_DLL_
    #ifdef __cplusplus
    #define EXPORT  extern "C" __declspec( dllexport )
    #else
    #define EXPORT  __declspec( dllexport )
    #endif
    #define HWRAPI(fn)  WINAPI fn
#else
    #define EXPORT      typedef
    #define HWRAPI(fn)  (WINAPI *fn)
#endif


// ==========================================================================
//                                                       STANDARD DLL EXPORTS
// ==========================================================================

EXPORT BOOL HWRAPI( Init ) (void) ;
EXPORT void HWRAPI( Shutdown ) (void) ;
EXPORT void HWRAPI( GetModeList ) (void** pvidmodes, int* numvidmodes) ;
EXPORT void HWRAPI( SetPalette ) (PALETTEENTRY* pal) ;
EXPORT void HWRAPI( FinishUpdate ) (void) ;

EXPORT void HWRAPI( DrawLine ) (wallVert2D* v1, wallVert2D* v2) ;
EXPORT void HWRAPI( DrawPolygon ) (wallVert2D *triVerts, int nVerts, unsigned long col) ;
EXPORT void HWRAPI( GetState ) (hwdgetstate_t IdState, void* dest) ;
EXPORT void HWRAPI( SetState ) (hwdstate_t IdState, int Value) ;
EXPORT void HWRAPI( ClearBuffer ) (int color, int alpha, hwdcleardepth_t depth) ;
EXPORT void HWRAPI( SetTexture ) (GlideMipmap_t* grMipmap) ;
EXPORT void HWRAPI( ReadRect ) (int x, int y, int width, int height,
                                int dst_stride, unsigned short * dst_data) ;
EXPORT void HWRAPI( ClipRect ) (int minx, int miny, int maxx, int maxy) ;


// ==========================================================================
//                                      HWR DRIVER OBJECT, FOR CLIENT PROGRAM
// ==========================================================================

#if !defined(_CREATE_DLL_)

struct hwdriver_s {
    Init            pfnInit;
    Shutdown        pfnShutdown;
    GetModeList     pfnGetModeList;
    SetPalette      pfnSetPalette;
    FinishUpdate    pfnFinishUpdate;
    DrawLine        pfnDrawLine;
    DrawPolygon     pfnDrawPolygon;
    GetState        pfnGetState;
    SetState        pfnSetState;
    ClearBuffer     pfnClearBuffer;
    SetTexture      pfnSetTexture;
    ReadRect        pfnReadRect;
    ClipRect        pfnClipRect;
};

extern struct hwdriver_s hwdriver;

#define HWD hwdriver

#endif //not defined _CREATE_DLL_

#endif __HWR_DRV_H__
