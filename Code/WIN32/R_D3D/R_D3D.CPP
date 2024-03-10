// r_d3d.c : Direct 3D Immediate mode driver

#include <windows.h>
#include <windowsx.h>
#define D3D_OVERLOADS
#include <d3d.h>

#define  _CREATE_DLL_
#include "../hwr_drv.h"

#include "../../screen.h"


#define SAFE_RELEASE(p) if(p != NULL) { p->Release(); p = NULL; }
#define SAFE_DELETE(p) if(p != NULL) { delete p; p = NULL; }


// **************************************************************************
//                                                                     PROTOS
// **************************************************************************

static BOOL InitD3DStates (LPDIRECT3DDEVICE3 pd3dDevice,
                           LPDIRECT3DVIEWPORT3 pvViewport);

static void InitMipmapCache (void);
static char* DDErr(HRESULT hresult);
static void BufferClear (void);


// **************************************************************************
//                                                                    GLOBALS
// **************************************************************************

#define DEVMODE                     // define this to create an output file
                                    // which log operations, to help development
#if defined(DEVMODE)
static HANDLE   fileHandle;         // for development debugging
#endif

LPDIRECTDRAW            lpDD;
LPDIRECTDRAW4           lpDD4;
LPDIRECTDRAWSURFACE4    lpDDSRender     = NULL;
LPDIRECTDRAWSURFACE4    lpDDSPrimary    = NULL;
LPDIRECTDRAWSURFACE4    lpDDSBackBuffer = NULL;
LPDIRECT3D3             lpD3D           = NULL;
LPDIRECT3DDEVICE3       lpD3DDevice     = NULL;
LPDIRECT3DVIEWPORT3     lpD3DViewport   = NULL;
RECT                    rcScreenRect;
RECT                    rcViewportRect;

//testing
D3DVERTEX               pvTriangleVertices[6];


// **************************************************************************
//                                                            DLL ENTRY POINT
// **************************************************************************
BOOL APIENTRY DllMain( HANDLE hModule,      // handle to DLL module
                       DWORD fdwReason,     // reason for calling function
                       LPVOID lpReserved )  // reserved
{
    // Perform actions based on the reason for calling.
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
         // Initialize once for each new process.
         // Return FALSE to fail DLL load.
            break;

        case DLL_THREAD_ATTACH:
         // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
         // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
         // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}



// --------------------------------------------------------------------------
// Directly output a formatted string to the errorlog file, using win32 funcs
// --------------------------------------------------------------------------
static void FPrintf (LPCTSTR lpFmt, ...)
{
    va_list arglist;
    char    str[1024];
    DWORD   bytesWritten;

    va_start (arglist, lpFmt);
    wvsprintf (str, lpFmt, arglist);
    va_end   (arglist);

#if defined(DEVMODE)
    WriteFile (fileHandle, str, strlen(str), &bytesWritten, NULL);
#endif
}


// ==========================================================================
// Initialise
// ==========================================================================
EXPORT BOOL HWRAPI( Init ) (void)
{
#if defined(DEVMODE)
    SYSTEMTIME  systemTime;
#endif
    HRESULT hr;

    // create file for development debugging
#if defined(DEVMODE)
    fileHandle = CreateFile ("r_d3dlog.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
        return FALSE;
    GetSystemTime (&systemTime);
#endif
    FPrintf ("r_d3d.DLL development mode log file\r\n");
    FPrintf ("r_d3d.dll Init() called at %02d/%02d/%04d %02d:%02d:%02d.\r\n",
             systemTime.wMonth, systemTime.wDay, systemTime.wYear,
             systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

    
    //-------------------------------------------------------------------------
	// Create DirectDraw
    //-------------------------------------------------------------------------

    hr = DirectDrawCreate (NULL, &lpDD, NULL); 
    if (FAILED (hr)) {
        FPrintf (DDErr(hr));
        return FALSE;
    }

    // get the DirectX 6 interface
    hr = lpDD->QueryInterface (IID_IDirectDraw4, (VOID**)&lpDD4 );
	if (FAILED(hr)) {
        FPrintf (DDErr(hr));
		return FALSE;
    }

    //-------------------------------------------------------------------------
    // Query DirectDraw for access to Direct3D
    //-------------------------------------------------------------------------
    lpDD4->QueryInterface( IID_IDirect3D3, (VOID**)&lpD3D );
    if (FAILED(hr)) {
        FPrintf (DDErr(hr));
        return FALSE;
    }
    
    // setup mipmap download cache
    //InitMipmapCache ();
    
    return TRUE;
}

static viddef_t* viddef;

// ==========================================================================
//
// ==========================================================================
#define RELEASE(p)    {if(p) p->Release();}
EXPORT void HWRAPI( Shutdown ) (void)
{
    if (viddef->buffer) {
        free (viddef->buffer);
        viddef->buffer = NULL;
    }
    
    // shutdown 3d display here
    // release DirectDraw object

	// Release the DDraw and D3D objects used by the app
    SAFE_RELEASE( lpD3DViewport )
	SAFE_RELEASE( lpD3D )
	SAFE_RELEASE( lpDDSBackBuffer )
	SAFE_RELEASE( lpDDSPrimary )
	SAFE_RELEASE( lpDD4 )

    // Do a safe check for releasing the D3DDEVICE. RefCount should be zero.
    if( lpD3DDevice )
        if( 0 < lpD3DDevice->Release() )
            FPrintf ("RefCount error releasing lpD3DDevice\r\n");

	// Do a safe check for releasing DDRAW. RefCount should be zero.
    if( lpDD )
        if( 0 < lpDD->Release() )
            FPrintf ("RefCount error releasing lpDD\r\n");

	lpD3DDevice    = NULL;
	lpDD           = NULL;

#if defined(DEVMODE)
    // close debug output file
    CloseHandle (fileHandle);
#endif
}


// **************************************************************************
//                                                   D3D DISPLAY MODES DRIVER
// **************************************************************************

int SetD3DMode (viddef_t *lvid, vmode_t *pcurrentmode) ;

#define NUMD3DVIDMODES    1
vmode_t	d3dvidmodes[NUMD3DVIDMODES] = {
  {
    NULL,
    "320x200",
    320, 200,  //(200.0/320.0)*(320.0/240.0),
    320, 1,    // rowbytes, bytes per pixel ... NOTE bpp is 1 but we never write to the LFB
    0, 2,
    NULL,
    &SetD3DMode
  }
};


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#define MAX_DEVICE_NAME     128
#define MAX_DEVICE_DESC     128

DWORD                   dwDeviceBitDepth = 0;
GUID                    guidDevice;
char                    szDeviceName[MAX_DEVICE_NAME+1];
char                    szDeviceDesc[MAX_DEVICE_DESC+1];
D3DDEVICEDESC           d3dHWDeviceDesc;

static HRESULT WINAPI GetModeListCallback( LPGUID          lpGUID, 
                                           LPSTR           lpszDeviceDesc, 
                                           LPSTR           lpszDeviceName, 
                                           LPD3DDEVICEDESC lpd3dHWDeviceDesc, 
                                           LPD3DDEVICEDESC lpd3dSWDeviceDesc, 
                                           LPVOID          lpUserArg )
{
    BOOL            fIsHardware; 
    LPD3DDEVICEDESC lpd3dDeviceDesc; 
 
    // If there is no hardware support the color model is zero.
    fIsHardware     = (lpd3dHWDeviceDesc->dcmColorModel != 0);
    lpd3dDeviceDesc = (fIsHardware ? lpd3dHWDeviceDesc :
                                     lpd3dSWDeviceDesc);

    FPrintf ("%s %s depth: %d\r\n", lpszDeviceDesc, lpszDeviceName, lpd3dDeviceDesc->dwDeviceRenderBitDepth);
                                     
    // Does the device render at the depth we want?
    if (!(lpd3dDeviceDesc->dwDeviceRenderBitDepth & dwDeviceBitDepth))
    {
        // If not, skip this device.
        return D3DENUMRET_OK;
    }
 
    // The device must support Gouraud-shaded triangles.
    if (D3DCOLOR_MONO == lpd3dDeviceDesc->dcmColorModel)
    {
        if (!(lpd3dDeviceDesc->dpcTriCaps.dwShadeCaps &
              D3DPSHADECAPS_COLORGOURAUDMONO))
        {
            // No Gouraud shading. Skip this device.
            return D3DENUMRET_OK;
        }
    }
    else
    {
        if (!(lpd3dDeviceDesc->dpcTriCaps.dwShadeCaps & 
              D3DPSHADECAPS_COLORGOURAUDRGB)) 
        { 
            // No Gouraud shading. Skip this device. 
             return D3DENUMRET_OK; 
        } 
    } 
 
    // Reject software devices, and monochromatic
    if (!fIsHardware || (lpd3dDeviceDesc->dcmColorModel != D3DCOLOR_RGB))
    { 
        return D3DENUMRET_OK; 
    } 
 
    //
    // This is a device we are interested in. Save the details.
    //
    *((BOOL*)lpUserArg) = TRUE;
    CopyMemory(&guidDevice, lpGUID, sizeof(GUID));
    strncpy(szDeviceDesc, lpszDeviceDesc, MAX_DEVICE_DESC);
    strncpy(szDeviceName, lpszDeviceName, MAX_DEVICE_NAME);
    CopyMemory(&d3dHWDeviceDesc, lpd3dHWDeviceDesc, sizeof(D3DDEVICEDESC));
    //CopyMemory(&d3dSWDeviceDesc, lpd3dSWDeviceDesc, sizeof(D3DDEVICEDESC));

    // If this is a hardware device, we have found
    // what we are looking for.
    if (fIsHardware)
        return D3DENUMRET_CANCEL;
 
    // Otherwise, keep looking.
    return D3DENUMRET_OK;
}


// --------------------------------------------------------------------------
// Get the list of available display modes
// --------------------------------------------------------------------------
EXPORT void HWRAPI( GetModeList ) (void** pvidmodes, int* numvidmodes)
{
    BOOL    fDeviceFound = FALSE; 
    HRESULT hr;

    FPrintf ("GetModeList(): ");
    
    // In this code fragment, the variable lpd3d contains a valid 
    // pointer to the IDirect3D3 interface that the application obtained
    // prior to executing this code.
    hr = lpD3D->EnumDevices(GetModeListCallback, &fDeviceFound);
    if (FAILED(hr)) 
    {
        // Code to handle the error goes here.
        FPrintf ("EnumDevices FAILED\r\n");
        FPrintf (DDErr(hr));
        *numvidmodes = 0;
        return;
    }
 
    /*if (!fDeviceFound) 
    { 
        // Code to handle the error goes here.
        FPrintf ("no device found\r\n");
        *numvidmodes = 0;
        return;
    } */

    // add first the default mode
    d3dvidmodes[NUMD3DVIDMODES-1].pnext = NULL;
    *((vmode_t**)pvidmodes) = &d3dvidmodes[0];
    *numvidmodes = NUMD3DVIDMODES;
}


// ==========================================================================
// Set video mode routine for GLIDE display modes
// Out: 1 ok,
//		0 hardware could not set mode,
//     -1 no mem
// ==========================================================================

// current hack to allow the software view to co-exist for development
static BOOL	VID_FreeAndAllocVidbuffer (viddef_t *lvid)
{
    int  vidbuffersize;

    vidbuffersize = (lvid->width * lvid->height * lvid->bpp * 4/*numscreens*/)
					+ (lvid->width * 32/*st_height*/ * lvid->bpp);  //status bar

    // allocate the new screen buffer
    if( (lvid->buffer = (byte *) malloc(vidbuffersize))==NULL )
		return FALSE;

    // initially clear the video buffer
    memset (lvid->buffer, 0, vidbuffersize);

    return TRUE;
}


// ==========================================================================
// CreateNewSurface
// ==========================================================================
LPDIRECTDRAWSURFACE4 CreateNewSurface(int dwWidth,
									  int dwHeight,
								      int dwSurfaceCaps)
{
    DDSURFACEDESC2      ddsd;
    HRESULT             hr;
//    DDCOLORKEY          ddck;
    LPDIRECTDRAWSURFACE4 psurf;
    
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT |DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = dwSurfaceCaps;
    ddsd.dwHeight = dwHeight;
    ddsd.dwWidth = dwWidth;

    hr = lpDD4->CreateSurface (&ddsd, &psurf, NULL);

    if (FAILED(hr)) {
        FPrintf (DDErr(hr));
        psurf = NULL;
    }
    else
    {
        psurf->Restore();

        //hr = ScreenReal->lpVtbl->GetColorKey(DDCKEY_SRCBLT, &ddck);
        //psurf->SetColorKey(DDCKEY_SRCBLT, &ddck);
    }

    return psurf;
}


// ==========================================================================
// Set display mode
// ==========================================================================
static boolean  d3d_display = false;
static BOOL     fullScreen = FALSE;
#define DIMW    640
#define DIMH    480
int SetD3DMode (viddef_t *lvid, vmode_t *pcurrentmode)
{
    DWORD   dwStyle;
    HRESULT hr;

    if (d3d_display)
        return 1;
    
    // say we're double-buffering, although this value isn't used..
    lvid->numpages = 2;

    //TODO: release stuff from previous Glide mode...
    if (!VID_FreeAndAllocVidbuffer (lvid))
        return -1;

    //-------------------------------------------------------------------------
    // Change window attributes
    //-------------------------------------------------------------------------
    HWND appWin = lvid->WndParent; //GetActiveWindow ();
    if (appWin == NULL) {
        FPrintf ("GetActiveWindow FAILED\r\n");
        return FALSE;
    }

    if (fullScreen)
    {
        dwStyle = WS_POPUP | WS_VISIBLE;
        SetWindowLong (appWin, GWL_STYLE, dwStyle);
        SetWindowPos (appWin, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
    else
	{
    	RECT rect;
		
        rect.top    = 0;
		rect.left   = 0;
		rect.bottom = DIMW - 1;
		rect.right  = DIMH - 1;

        dwStyle = GetWindowStyle(appWin);
        dwStyle &= ~WS_POPUP;
        dwStyle |= WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;

        SetWindowLong(appWin, GWL_STYLE, dwStyle);

        // Resize the window so that the client area is the requested width/height
        AdjustWindowRectEx (&rect, GetWindowStyle(appWin), GetMenu(appWin) != NULL,
							GetWindowExStyle(appWin));

        // Just in case the window was moved off the visible area of the screen.
        SetWindowPos(appWin, NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        SetWindowPos(appWin, HWND_NOTOPMOST, 0, 0, 0, 0,
					 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    }

    // just in case..
    ShowWindow(appWin, SW_SHOW);

    //-------------------------------------------------------------------------
    // Set the Windows cooperative level.
    //-------------------------------------------------------------------------
    hr = lpDD4->SetCooperativeLevel (appWin,
        (fullScreen ? DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT : DDSCL_NORMAL) );
    if (FAILED(hr)) {
        FPrintf ("SetCooperativeLevel FAILED\r\n");
        FPrintf (DDErr(hr));
        return FALSE;
    }

    //-------------------------------------------------------------------------
	// Create DirectDraw surfaces used for rendering
    //-------------------------------------------------------------------------
    DDSURFACEDESC2 ddsd;
	ZeroMemory (&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	// for fullscreen we use page flipping, for windowed mode, we blit the hidden surface to
	// the visible surface, in both cases we have a visible (or 'real') surface, and a hidden
	// (or 'virtual', or 'backbuffer') surface.
    if (fullScreen) {
        ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
    	ddsd.dwBackBufferCount = 1;
        ddsd.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    }

	hr = lpDD4->CreateSurface( &ddsd, &lpDDSPrimary, NULL );
    if (FAILED(hr)) {
        FPrintf ("CreateSurface Primary Screen FAILED");
        FPrintf (DDErr(hr));
        return FALSE;
    }
    
    if (fullScreen)
    {
        // Get a pointer to the back buffer
        DDSCAPS2 ddscaps;
	    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;// | DDSCAPS_3DDEVICE;
	    hr = lpDDSPrimary->GetAttachedSurface (&ddscaps, &lpDDSBackBuffer);
        if (FAILED(hr)) {
		    FPrintf ("GetAttachedSurface FAILED");
            FPrintf (DDErr(hr));
            return FALSE;
        }
    }
    else
    {
	    GetClientRect( appWin, &rcScreenRect );
	    GetClientRect( appWin, &rcViewportRect );
	    ClientToScreen( appWin, (POINT*)&rcScreenRect.left );
	    ClientToScreen( appWin, (POINT*)&rcScreenRect.right );
        
        // Create a back buffer for offscreen rendering, this will be used to
        // blt to the primary
        lpDDSBackBuffer = CreateNewSurface(DIMW, DIMH, DDSCAPS_OFFSCREENPLAIN |
													   DDSCAPS_3DDEVICE);
        if (lpDDSBackBuffer == NULL) {
            FPrintf ("CreateSurface Secondary Screen FAILED");
            return FALSE;
        }
    }

    if (!fullScreen)
    {
	    LPDIRECTDRAWCLIPPER pcClipper;
	    hr = lpDD4->CreateClipper( 0, &pcClipper, NULL );
        if (FAILED(hr)) {
            FPrintf (DDErr(hr));
		    return FALSE;
        }
	    pcClipper->SetHWnd( 0, appWin );
	    lpDDSPrimary->SetClipper( pcClipper );
	    pcClipper->Release();
    }


    //-------------------------------------------------------------------------
	// Create the Direct3D interfaces
    //-------------------------------------------------------------------------

    // Query DirectDraw for access to Direct3D
    // see Init()

    // Before creating the device, check that we are NOT in a palettized
	// display. That case will cause CreateDevice() to fail, don't bother
	// with palettes..
	ddsd.dwSize = sizeof(ddsd);
	lpDD4->GetDisplayMode( &ddsd );
	if( ddsd.ddpfPixelFormat.dwRGBBitCount <= 8 )
    {
        FPrintf ("Screen palettized format not supported\r\n");
		return FALSE;
    }
   
 	// Create the device. The GUID is hardcoded for now, but should come from
	// device enumeration, which is the topic of a future tutorial. The device
	// is created off of our back buffer, which becomes the render target for
	// the newly created device.
    hr = lpD3D->CreateDevice( IID_IDirect3DHALDevice, lpDDSBackBuffer,
                               &lpD3DDevice, NULL );
	if( FAILED( hr ) ) {
        FPrintf ("Create Device FAILED\r\n");
        FPrintf (DDErr(hr));
        return FALSE;
    }

    //-------------------------------------------------------------------------
	// Create the viewport
    //-------------------------------------------------------------------------

    // Set up the viewport data parameters
    D3DVIEWPORT2 vdData;
    ZeroMemory( &vdData, sizeof(vdData) );
    vdData.dwSize       = sizeof(vdData);
	vdData.dwWidth      = DIMW;
	vdData.dwHeight     = DIMH;
    vdData.dvClipX      = -1.0f;
    vdData.dvClipWidth  = 2.0f;
    vdData.dvClipY      = 1.0f;
    vdData.dvClipHeight = 2.0f;
    vdData.dvMaxZ       = 1.0f;

    // Create the viewport
    hr = lpD3D->CreateViewport( &lpD3DViewport, NULL );
    if( FAILED( hr ) ) {
        FPrintf ("lpD3D->CreateViewport FAILED\r\n");
        FPrintf (DDErr(hr));
        return FALSE;
    }

    // Associate the viewport with the D3DDEVICE object
    lpD3DDevice->AddViewport( lpD3DViewport );

    // Set the parameters to the new viewport
    lpD3DViewport->SetViewport2( &vdData );

    // Set the viewport as current for the device
    lpD3DDevice->SetCurrentViewport( lpD3DViewport );

    //-------------------------------------------------------------------------
	// We're done and ready to set up our scene
    //-------------------------------------------------------------------------

    // set initial state of d3d
    InitD3DStates (lpD3DDevice, lpD3DViewport);

    lvid->direct = NULL;
    d3d_display = true;

    //save lvid to free software vidbuffer on shutdown
    viddef = lvid;

    FPrintf("SetD3DMode() succesfull\r\n");

    return 1;
}


// --------------------------------------------------------------------------
// Checks for lost surfaces and restores them if lost.
// --------------------------------------------------------------------------
void RestoreSurfaces()
{
    // Check/restore the primary surface
    if( lpDDSPrimary )
        if( lpDDSPrimary->IsLost() )
            lpDDSPrimary->Restore();
    
    // Check/restore the back buffer
    if( lpDDSBackBuffer )
        if( lpDDSBackBuffer->IsLost() )
            lpDDSBackBuffer->Restore();

}


// --------------------------------------------------------------------------
// Swap front and back buffers
// --------------------------------------------------------------------------
EXPORT void HWRAPI( FinishUpdate ) (void)
{
    HRESULT hr;
    
    if (!lpDDSPrimary)
		return;

    // show fps?
    // draw software view ?
    // page flip

    BufferClear ();

    if ( FAILED( lpD3DDevice->BeginScene() ) )
		return;

    lpD3DDevice->DrawPrimitive( D3DPT_TRIANGLELIST, D3DFVF_VERTEX,
                                pvTriangleVertices, 6, NULL );

    // End the scene.
    lpD3DDevice->EndScene();

    // We are in windowed mode, so perform a blit from the backbuffer to the
	// correct position on the primary surface
    if (fullScreen)
    {
    }
    else
    {
        hr = lpDDSPrimary->Blt( &rcScreenRect, lpDDSBackBuffer, 
                                &rcViewportRect, DDBLT_WAIT, NULL );
        if (hr == DDERR_SURFACELOST)
            RestoreSurfaces ();
    }
}


// --------------------------------------------------------------------------
// Moves the screen rect for windowed renderers
// --------------------------------------------------------------------------
void OnWindowMove( int x, int y )
{
	DWORD dwWidth  = rcScreenRect.right - rcScreenRect.left;
	DWORD dwHeight = rcScreenRect.bottom - rcScreenRect.top;
    SetRect( &rcScreenRect, x, y, x + dwWidth, y + dwHeight );
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static unsigned long myPaletteData[256];  // 256 ARGB entries
//TODO: do the chroma key stuff out of here
EXPORT void HWRAPI( SetPalette ) (PALETTEENTRY* pal)
{
    int i;

    if (!d3d_display)
        return;

    // create the palette in the format used for downloading to 3Dfx card
    for (i=0; i<256; i++)
        myPaletteData[i] =  (0xFF << 24) |          //alpha
                            (pal[i].peRed<<16) |
                            (pal[i].peGreen<<8) |
                             pal[i].peBlue;

    // make sure the chromakey color is always the same value
    myPaletteData[HWR_PATCHES_CHROMAKEY_COLORINDEX] = HWR_PATCHES_CHROMAKEY_COLORVALUE;

    // download palette
}


// **************************************************************************
//
// **************************************************************************


// --------------------------------------------------------------------------
// Do a full buffer clear including color / alpha / and Z buffers
// --------------------------------------------------------------------------
static void BufferClear (void)
{
    // clear color/alpha and depth buffers
    if (lpD3DViewport)
        lpD3DViewport->Clear2( 1UL, (D3DRECT*)&rcViewportRect, D3DCLEAR_TARGET, 0x000000ff,
		                       0L, 0L );
}


// --------------------------------------------------------------------------
// Set initial state of 3d card settings
// --------------------------------------------------------------------------
static BOOL InitD3DStates (LPDIRECT3DDEVICE3 pd3dDevice,
                           LPDIRECT3DVIEWPORT3 pvViewport)
{
    FPrintf ("InitD3DStates()...\r\n");

    // Get a ptr to the ID3D object to create materials and/or lights.
    LPDIRECT3D3 pD3D;
    if( FAILED( pd3dDevice->GetDirect3D( &pD3D ) ) )
        return FALSE;
    pD3D->Release();
    

	// Data for the geometry of the triangle. Note that this tutorial only
	// uses ambient lighting, so the vertices' normals are not actually used.
	D3DVECTOR p1( 0.0f, 3.0f, 0.0f );
	D3DVECTOR p2( 3.0f,-3.0f, 0.0f );
	D3DVECTOR p3(-3.0f,-3.0f, 0.0f );
	D3DVECTOR vNormal( 0.0f, 0.0f, 1.0f );
    // Initialize the 3 vertices for the front of the triangle
	pvTriangleVertices[0] = D3DVERTEX( p1, vNormal, 0, 0 );
	pvTriangleVertices[1] = D3DVERTEX( p2, vNormal, 0, 0 );
	pvTriangleVertices[2] = D3DVERTEX( p3, vNormal, 0, 0 );
	// Initialize the 3 vertices for the back of the triangle
	pvTriangleVertices[3] = D3DVERTEX( p1, -vNormal, 0, 0 );
	pvTriangleVertices[4] = D3DVERTEX( p3, -vNormal, 0, 0 );
	pvTriangleVertices[5] = D3DVERTEX( p2, -vNormal, 0, 0 );

    
    // get min/max w buffer range values

    // set my vertex format (the one of wallVert2D hwr_data.h)

    // set coord space
    
    // set W buffer

    // depth buffer func cmp_less

    // enable depth buffer

    // Ambient lighting on to full white.
    pd3dDevice->SetLightState(  D3DLIGHTSTATE_AMBIENT,  0xffffffff );

    BufferClear();

    return TRUE;
}


// **************************************************************************
//                                                   TEXTURE CACHE MANAGEMENT
// **************************************************************************

static  DWORD           gr_cachemin;
static  DWORD           gr_cachemax;

static  DWORD           gr_cachetailpos;    // manage a cycling cache for mipmaps
static  DWORD           gr_cachepos;        
static  GlideMipmap_t*  gr_cachetail;
static  GlideMipmap_t*  gr_cachehead;


// --------------------------------------------------------------------------
// This must be done once only for all program execution
// --------------------------------------------------------------------------
static void ClearMipmapCache (void)
{
    FPrintf ("ClearMipmapCache() \r\n");
    while (gr_cachetail)
    {
        gr_cachetail->downloaded = false;
        gr_cachetail = gr_cachetail->next;
    }
    
    gr_cachetail = NULL;
    gr_cachehead = NULL;
    gr_cachetailpos = gr_cachepos = gr_cachemin;
}


static void InitMipmapCache (void)
{
    FPrintf ("InitMipmapCache() \r\n");
    /*
    gr_cachemin = grTexMinAddress(GR_TMU0);
    gr_cachemax = grTexMaxAddress(GR_TMU0);
    */

    //testing..
    //gr_cachemax = gr_cachemin + (1024<<10);

    gr_cachetail = NULL;
    ClearMipmapCache();

    //CONS_Printf ("HWR_InitMipmapCache() : %d kb\n", (gr_cachemax-gr_cachemin)>>10);
}


static void FlushMipmap (GlideMipmap_t* mipmap)
{
    mipmap->downloaded = false;
    gr_cachetail = mipmap->next;
    // should never happen
    if (!mipmap->next)
    //    I_Error ("This just CAN'T HAPPEN!!! So you think you're different eh ?");
        ClearMipmapCache ();
    else
        gr_cachetailpos = mipmap->next->cachepos;
}


// --------------------------------------------------------------------------
// Download a 'surface' into the graphics card memory
// --------------------------------------------------------------------------
void DownloadMipmap (GlideMipmap_t* grMipmap)
{
    DWORD   mipmapSize;
    DWORD   freespace;

    if (grMipmap->downloaded)
        return;

    FPrintf ("Downloading mipmap\r\n");

    return;
/*
    //if (grMipmap->grInfo.data == NULL)
    //    I_Error ("info.data is NULL\n");

    // (re-)download the texture data
    //mipmapSize = grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &grMipmap->grInfo);

    //CONS_Printf ("DOWNLOAD\n");

    // flush out older mipmaps to make space
    //CONS_Printf ("HWR_DownloadTexture: texture too big for TMU0\n");
    if (gr_cachehead)
    {
        while (1)
        {
            if (gr_cachetailpos >= gr_cachepos)
                freespace = gr_cachetailpos - gr_cachepos;
            else
                freespace = gr_cachemax - gr_cachepos;
    
            if (freespace >= mipmapSize)
                break;

            //CONS_Printf ("        tail: %#07d   pos: %#07d\n"
            //             "        size: %#07d  free: %#07d\n",
            //                      gr_cachetailpos, gr_cachepos, mipmapSize, freespace);

            // not enough space in the end of the buffer
            if (gr_cachepos > gr_cachetailpos) {
                gr_cachepos = gr_cachemin;
                //CONS_Printf ("        cycle over\n");
            }
            else{
                FlushMipmap (gr_cachetail);
            }
        }
    }

    grMipmap->startAddress = gr_cachepos;
    //gr_cachepos += mipmapSize;
    grMipmap->mipmapSize = mipmapSize;
    //grTexDownloadMipMap (GR_TMU0, grMipmap->startAddress, GR_MIPMAPLEVELMASK_BOTH, &grMipmap->grInfo);
    grMipmap->downloaded = true;

    // store pointer to downloaded mipmap
    if (gr_cachehead)
        gr_cachehead->next = grMipmap;
    else {
        gr_cachetail = gr_cachehead = grMipmap;
    }
    grMipmap->cachepos = gr_cachepos;
    grMipmap->next = NULL;  //debug (not used)
    gr_cachepos += mipmapSize;
    gr_cachehead = grMipmap;
*/
}


// ==========================================================================
// The mipmap becomes the current texture source
// ==========================================================================
EXPORT void HWRAPI( SetTexture ) (GlideMipmap_t* grMipmap)
{
    if (!grMipmap->downloaded)
        DownloadMipmap (grMipmap);
    
    //grTexSource (GR_TMU0, grMipmap->startAddress, GR_MIPMAPLEVELMASK_BOTH, &grMipmap->grInfo);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( SetState ) (hwdstate_t IdState, int Value)
{
/*
    switch (IdState)
    {
        // set depth buffer on/off
        case HWD_SET_DEPTHMASK:
            grDepthMask (Value);
            break;

        case HWD_SET_COLORMASK:
            grColorMask (Value, FXTRUE);    //! alpha buffer true
            break;

        case HWD_SET_CULLMODE:
            grCullMode (Value ? GR_CULL_POSITIVE : GR_CULL_DISABLE);
            break;

        case HWD_SET_CONSTANTCOLOR:
            grConstantColorValue (Value);
            break;

        case HWD_SET_COLORSOURCE:
            if (Value == HWD_COLORSOURCE_CONSTANT)
            {
                grColorCombine( GR_COMBINE_FUNCTION_ZERO,
                                GR_COMBINE_FACTOR_ZERO,
                                GR_COMBINE_LOCAL_CONSTANT,
                                GR_COMBINE_OTHER_NONE,
                                FXFALSE );
            }
            else if (Value == HWD_COLORSOURCE_ITERATED)
            {
	            grColorCombine(GR_COMBINE_FUNCTION_LOCAL,
				               GR_COMBINE_FACTOR_NONE,
				               GR_COMBINE_LOCAL_ITERATED,
				               GR_COMBINE_OTHER_NONE,
				               FXFALSE );
            }
            else if (Value == HWD_COLORSOURCE_TEXTURE)
            {
                grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,
                                GR_COMBINE_FACTOR_ONE,
                                GR_COMBINE_LOCAL_NONE,
                                GR_COMBINE_OTHER_TEXTURE,
                                FXFALSE );
            }
            else if (Value == HWD_COLORSOURCE_CONSTANTALPHA_SCALE_TEXTURE)
            {
                grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,        // factor * Color other
                                GR_COMBINE_FACTOR_LOCAL_ALPHA,
                                GR_COMBINE_LOCAL_CONSTANT, //ITERATED    // local is constant color
                                GR_COMBINE_OTHER_TEXTURE,               // color from texture map
                                FXFALSE );
            }
            break;

        case HWD_SET_ALPHABLEND:
            if (Value == HWD_ALPHABLEND_NONE)
            {
                grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
                grAlphaTestFunction (GR_CMP_ALWAYS);
            }
            else
            if (Value == HWD_ALPHABLEND_TRANSLUCENT)
            {
                grAlphaBlendFunction (GR_BLEND_SRC_ALPHA,
                                      GR_BLEND_ONE_MINUS_SRC_ALPHA,
                                      GR_BLEND_ONE,
                                      GR_BLEND_ZERO );
            }
            break;

        case HWD_SET_TEXTURECLAMP:
            if (Value == HWD_TEXTURE_CLAMP_XY)
            {
                grTexClampMode (GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
            }
            else if (Value == HWD_TEXTURE_WRAP_XY)
            {
                grTexClampMode (GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
            }
            break;

        case HWD_SET_TEXTURECOMBINE:
            if (Value == HWD_TEXTURECOMBINE_NORMAL)
            {
                grTexCombine (GR_TMU0, GR_COMBINE_FUNCTION_LOCAL,
                                   GR_COMBINE_FACTOR_NONE,
                                   GR_COMBINE_FUNCTION_LOCAL,
                                   GR_COMBINE_FACTOR_NONE,
                                   FXFALSE, FXFALSE );
            }
            break;

        case HWD_SET_TEXTUREFILTERMODE:
            if (Value == HWD_SET_TEXTUREFILTER_BILINEAR)
            {
                grTexFilterMode (GR_TMU0, GR_TEXTUREFILTER_BILINEAR,
                                          GR_TEXTUREFILTER_BILINEAR);
            }
            else if (Value == HWD_SET_TEXTUREFILTER_POINTSAMPLED)
            {
                grTexFilterMode (GR_TMU0, GR_TEXTUREFILTER_POINT_SAMPLED,
                                          GR_TEXTUREFILTER_POINT_SAMPLED);
            }
            break;

        case HWD_SET_MIPMAPMODE:
            if (Value == HWD_MIPMAP_DISABLE)
            {
                grTexMipMapMode (GR_TMU0,
                                 GR_MIPMAP_DISABLE,         // no mipmaps based on depth
                                 FXFALSE );
            }
            break;

        //!!!hardcoded 3Dfx Value!!!
        case HWD_SET_ALPHATESTFUNC:
            grAlphaTestFunction (Value);
            break;

        //!!!hardcoded 3Dfx Value!!!
        case HWD_SET_ALPHATESTREFVALUE:
            grAlphaTestReferenceValue ((unsigned char)Value);
            break;
        
        case HWD_SET_ALPHASOURCE:
            if (Value == HWD_ALPHASOURCE_CONSTANT)
            {
                // constant color alpha
                grAlphaCombine( GR_COMBINE_FUNCTION_LOCAL_ALPHA,
                                GR_COMBINE_FACTOR_NONE,
                                GR_COMBINE_LOCAL_CONSTANT,
                                GR_COMBINE_OTHER_NONE,
                                FXFALSE );
            }
            else if (Value == HWD_ALPHASOURCE_TEXTURE)
            {
                // 1 * texture alpha channel
                grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,
                    GR_COMBINE_FACTOR_ONE,
                    GR_COMBINE_LOCAL_NONE,
                    GR_COMBINE_OTHER_TEXTURE,
                    FXFALSE );
            }
            break;

        case HWD_ENABLE:
            if (Value == HWD_SHAMELESS_PLUG)
                grEnable (GR_SHAMELESS_PLUG);
            break;

        case HWD_DISABLE:
            if (Value == HWD_SHAMELESS_PLUG)
                grDisable (GR_SHAMELESS_PLUG);
            break;

        case HWD_SET_FOG_TABLE:
            grFogTable ((unsigned char *)Value);
            break;

        case HWD_SET_FOG_COLOR:
            grFogColorValue (Value);
            break;

        case HWD_SET_FOG_MODE:
            if (Value == HWD_FOG_DISABLE)
                grFogMode (GR_FOG_DISABLE);
            else if (Value == HWD_FOG_ENABLE)
                grFogMode (GR_FOG_WITH_TABLE_ON_Q);
            break;

        case HWD_SET_CHROMAKEY_MODE:
            if (Value == HWD_CHROMAKEY_ENABLE)
                grChromakeyMode (GR_CHROMAKEY_ENABLE);
            break;

        case HWD_SET_CHROMAKEY_VALUE:
            grChromakeyValue (Value);
            break;

        default:
            break;
    }
*/
}


// ==========================================================================
// Read a rectangle region of the truecolor framebuffer
// store pixels as 16bit 565 RGB
// ==========================================================================
EXPORT void HWRAPI( ReadRect ) (int x, int y, int width, int height,
                                int dst_stride, unsigned short * dst_data)
{
    FPrintf ("ReadRect()\r\n");
    //grLfbReadRegion (GR_BUFFER_FRONTBUFFER,
    //                 x, y, width, height, dst_stride, dst_data);
}


// ==========================================================================
// Defines the 2D hardware clipping window
// ==========================================================================
EXPORT void HWRAPI( ClipRect ) (int minx, int miny, int maxx, int maxy)
{
    FPrintf ("ClipRect()\r\n");
    //grClipWindow ( (DWORD)minx, (DWORD)miny, (DWORD)maxx, (DWORD)maxy);
}


// ==========================================================================
// Clear the color/alpha/depth buffer(s)
// ==========================================================================
EXPORT void HWRAPI( ClearBuffer ) (int color, int alpha, hwdcleardepth_t depth)
{
    FPrintf ("ClearBuffer()\r\n");
    //grBufferClear ((GrColor_t)color, (GrAlpha_t)alpha, gr_wrange[depth]);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( DrawLine ) (wallVert2D* v1, wallVert2D* v2) 
{
    //grDrawLine (v1, v2);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( GetState ) (hwdgetstate_t IdState, void* dest)
{
    FPrintf ("GetState()\r\n");
/*
    if (IdState == HWD_GET_FOGTABLESIZE)
    {
        grGet (GR_FOG_TABLE_ENTRIES, 4, dest);
    }
*/
}


// ==========================================================================
// Draw a triangulated polygon
// ==========================================================================
EXPORT void HWRAPI( DrawPolygon ) (wallVert2D *projVerts, int nClipVerts, unsigned long col)
{
/*
    int nTris;
    int i;

    if (nClipVerts < 3)
        return;

    nTris = nClipVerts-2;
    
    // triangles fan
    for(i=0; i < nTris; i++)
    {
        // set some weird colours
        projVerts[0].argb = col;
        projVerts[i+1].argb = col | 0x80;
        projVerts[i+2].argb = col | 0xf0;

        //FIXME: 3Dfx : check for 'crash' coordinates if not clipped
        if (projVerts[0].x > 1280.0f ||
            projVerts[0].x < -640.0f)
            goto skip;
        if (projVerts[0].y > 900.0f ||
            projVerts[0].y < -500.0f)
            goto skip;
        if (projVerts[i+1].x > 1280.0f ||
            projVerts[i+1].x < -640.0f)
            goto skip;
        if (projVerts[i+1].y > 900.0f ||
            projVerts[i+1].y < -500.0f)
            goto skip;
        if (projVerts[i+2].x > 1280.0f ||
            projVerts[i+2].x < -640.0f)
            goto skip;
        if (projVerts[i+2].y > 900.0f ||
            projVerts[i+2].y < -500.0f)
            goto skip;
        
        grDrawTriangle ( projVerts,
                         &projVerts[i + 1],
                         &projVerts[i + 2] );
skip:
        col = col + 0x002000;
    }
*/
}

/*
//---------------------------------------------------------------------------
// Set to flat shading.
// This code fragment assumes that lpDev3 is a valid pointer to 
// an IDirect3DDevice3 interface.
    hr = lpDev3->SetRenderState(D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT);
    if(FAILED(hr))
    {
        // Code to handle the error goes here.
    }
 
    // Set to Gouraud shading (this is the default for Direct3D).
    hr = lpDev3->SetRenderState(D3DRENDERSTATE_SHADEMODE,
                                D3DSHADE_GOURAUD);
    if(FAILED(hr))
    {
        // Code to handle the error goes here.
    }


//---------------------------------------------------------------------------
    D3DVERTEX lpVertices[3];
 
    // A vertex can be specified one structure member at a time.
    lpVertices[0].x = 0;
    lpVertices[0].y = 5;
    lpVertices[0].z = 5;
    lpVertices[0].nx = 0;    // X component of the normal vector.
    lpVertices[0].ny = 0;    // Y component of the normal vector.
    lpVertices[0].nz = -1;   // Points the normal back at the origin.
    lpVertices[0].tu = 0;    // Only used if a texture is being used.
    lpVertices[0].tv = 0;    // Only used if a texture is being used.
 
    // Vertices can also by specified on one line of code for each vertex
    // by using some of the D3DOVERLOADS macros.
    lpVertices[1] = D3DVERTEX(D3DVECTOR(-5,-5,5),D3DVECTOR(0,0,-1),0,0);
    lpVertices[2] = D3DVERTEX(D3DVECTOR(5,-5,5),D3DVECTOR(0,0,-1),0,0);

//----------------------------------------------------------------------------
    An application uses the dwShadeCaps member of the D3DPRIMCAPS structure to
        determine what forms of interpolation the current device driver supports. 

//----------------------------------------------------------------------------
    DirectDrawCreate
    You can call the IDirect3D3::CreateDevice method to create a Direct3DDevice
    object and retrieve an IDirect3DDevice3 interface.

//----------------------------------------------------------------------------

The Direct3D Immediate Mode API consists primarily of the following COM interfaces: 

IDirect3D3	            Root interface, used to obtain other interfaces
IDirect3DDevice	        3D Device for execute-buffer based programming
IDirect3DDevice3	    3D Device for DrawPrimitive-based programming
IDirect3DLight	        Interface used to work with lights
IDirect3DMaterial3	    Surface-material interface
IDirect3DTexture2	    Texture-map interface
IDirect3DVertexBuffer	Interface used to work with vertex buffers.
IDirect3DViewport3	    Interface to define the viewport's characteristics.
IDirect3DExecuteBuffer	Interface for working with execute buffers

*/

static char* DDErr(HRESULT hresult)
{
    static char errmsg[128];
    static char msgtxt[128];
    switch(hresult)
    {
        case DD_OK:
             strcpy(errmsg,"The request completed successfully.");
             break;

        case DDERR_ALREADYINITIALIZED:
             strcpy(errmsg,"The object has already been initialized.");
             break;

        case DDERR_BLTFASTCANTCLIP:
             strcpy(errmsg,"A DirectDrawClipper object is attached to a source surface that has passed into a call to the IDirectDrawSurface2::BltFast method.");
             break;

        case DDERR_CANNOTATTACHSURFACE:
             strcpy(errmsg,"A surface cannot be attached to another requested surface.");
             break;

        case DDERR_CANNOTDETACHSURFACE:
             strcpy(errmsg,"A surface cannot be detached from another requested surface.");
             break;

        case DDERR_CANTCREATEDC:
             strcpy(errmsg,"Windows cannot create any more device contexts (DCs).");
             break;

        case DDERR_CANTDUPLICATE:
             strcpy(errmsg,"Primary and 3D surfaces, or surfaces that are implicitly created, cannot be duplicated.");
             break;

        case DDERR_CANTLOCKSURFACE:
             strcpy(errmsg,"Access to this surface is refused because an attempt was made to lock the primary surface without DCI support.");
             break;

        case DDERR_CANTPAGELOCK:
             strcpy(errmsg,"An attempt to page lock a surface failed. Page lock will not work on a display-memory surface or an emulated primary surface.");
             break;

        case DDERR_CANTPAGEUNLOCK:
             strcpy(errmsg,"An attempt to page unlock a surface failed. Page unlock will not work on a display-memory surface or an emulated primary surface.");
             break;

        case DDERR_CLIPPERISUSINGHWND:
             strcpy(errmsg,"An attempt was made to set a clip list for a DirectDrawClipper object that is already monitoring a window handle.");
             break;

        case DDERR_COLORKEYNOTSET:
             strcpy(errmsg,"No source color key is specified for this operation.");
             break;

        case DDERR_CURRENTLYNOTAVAIL:
             strcpy(errmsg,"No support is currently available.");
             break;

        case DDERR_DCALREADYCREATED:
             strcpy(errmsg,"A device context (DC) has already been returned for this surface. Only one DC can be retrieved for each surface.");
             break;

        case DDERR_DIRECTDRAWALREADYCREATED:
             strcpy(errmsg,"A DirectDraw object representing this driver has already been created for this process.");
             break;

        case DDERR_EXCEPTION:
             strcpy(errmsg,"An exception was encountered while performing the requested operation.");
             break;

        case DDERR_EXCLUSIVEMODEALREADYSET:
             strcpy(errmsg,"An attempt was made to set the cooperative level when it was already set to exclusive.");
             break;

        case DDERR_GENERIC:
             strcpy(errmsg,"There is an undefined error condition.");
             break;

        case DDERR_HEIGHTALIGN:
             strcpy(errmsg,"The height of the provided rectangle is not a multiple of the required alignment.");
             break;

        case DDERR_HWNDALREADYSET:
             strcpy(errmsg,"The DirectDraw cooperative level window handle has already been set. It cannot be reset while the process has surfaces or palettes created.");
             break;

        case DDERR_HWNDSUBCLASSED:
             strcpy(errmsg,"DirectDraw is prevented from restoring state because the DirectDraw cooperative level window handle has been subclassed.");
             break;

        case DDERR_IMPLICITLYCREATED:
             strcpy(errmsg,"The surface cannot be restored because it is an implicitly created surface.");
             break;

        case DDERR_INCOMPATIBLEPRIMARY:
             strcpy(errmsg,"The primary surface creation request does not match with the existing primary surface.");
             break;

        case DDERR_INVALIDCAPS:
             strcpy(errmsg,"One or more of the capability bits passed to the callback function are incorrect.");
             break;

        case DDERR_INVALIDCLIPLIST:
             strcpy(errmsg,"DirectDraw does not support the provided clip list.");
             break;

        case DDERR_INVALIDDIRECTDRAWGUID:
             strcpy(errmsg,"The globally unique identifier (GUID) passed to the DirectDrawCreate function is not a valid DirectDraw driver identifier.");
             break;

        case DDERR_INVALIDMODE:
             strcpy(errmsg,"DirectDraw does not support the requested mode.");
             break;

        case DDERR_INVALIDOBJECT:
             strcpy(errmsg,"DirectDraw received a pointer that was an invalid DirectDraw object.");
             break;

        case DDERR_INVALIDPARAMS:
             strcpy(errmsg,"One or more of the parameters passed to the method are incorrect.");
             break;

        case DDERR_INVALIDPIXELFORMAT:
             strcpy(errmsg,"The pixel format was invalid as specified.");
             break;

        case DDERR_INVALIDPOSITION:
             strcpy(errmsg,"The position of the overlay on the destination is no longer legal.");
             break;

        case DDERR_INVALIDRECT:
             strcpy(errmsg,"The provided rectangle was invalid.");
             break;

        case DDERR_INVALIDSURFACETYPE:
             strcpy(errmsg,"The requested operation could not be performed because the surface was of the wrong type.");
             break;

        case DDERR_LOCKEDSURFACES:
             strcpy(errmsg,"One or more surfaces are locked, causing the failure of the requested operation.");
             break;

        case DDERR_NO3D:
             strcpy(errmsg,"No 3D hardware or emulation is present.");
             break;

        case DDERR_NOALPHAHW:
             strcpy(errmsg,"No alpha acceleration hardware is present or available, causing the failure of the requested operation.");
             break;

        case DDERR_NOBLTHW:
             strcpy(errmsg,"No blitter hardware is present.");
             break;

        case DDERR_NOCLIPLIST:
             strcpy(errmsg,"No clip list is available.");
             break;

        case DDERR_NOCLIPPERATTACHED:
             strcpy(errmsg,"No DirectDrawClipper object is attached to the surface object.");
             break;

        case DDERR_NOCOLORCONVHW:
             strcpy(errmsg,"The operation cannot be carried out because no color-conversion hardware is present or available.");
             break;

        case DDERR_NOCOLORKEY:
             strcpy(errmsg,"The surface does not currently have a color key.");
             break;

        case DDERR_NOCOLORKEYHW:
             strcpy(errmsg,"The operation cannot be carried out because there is no hardware support for the destination color key.");
             break;

        case DDERR_NOCOOPERATIVELEVELSET:
             strcpy(errmsg,"A create function is called without the IDirectDraw2::SetCooperativeLevel method being called.");
             break;

        case DDERR_NODC:
             strcpy(errmsg,"No DC has ever been created for this surface.");
             break;

        case DDERR_NODDROPSHW:
             strcpy(errmsg,"No DirectDraw raster operation (ROP) hardware is available.");
             break;

        case DDERR_NODIRECTDRAWHW:
             strcpy(errmsg,"Hardware-only DirectDraw object creation is not possible; the driver does not support any hardware.");
             break;

        case DDERR_NODIRECTDRAWSUPPORT:
             strcpy(errmsg,"DirectDraw support is not possible with the current display driver.");
             break;

        case DDERR_NOEMULATION:
             strcpy(errmsg,"Software emulation is not available.");
             break;

        case DDERR_NOEXCLUSIVEMODE:
             strcpy(errmsg,"The operation requires the application to have exclusive mode, but the application does not have exclusive mode.");
             break;

        case DDERR_NOFLIPHW:
             strcpy(errmsg,"Flipping visible surfaces is not supported.");
             break;

        case DDERR_NOGDI:
             strcpy(errmsg,"No GDI is present.");
             break;

        case DDERR_NOHWND:
             strcpy(errmsg,"Clipper notification requires a window handle, or no window handle has been previously set as the cooperative level window handle.");
             break;

        case DDERR_NOMIPMAPHW:
             strcpy(errmsg,"The operation cannot be carried out because no mipmap texture mapping hardware is present or available.");
             break;

        case DDERR_NOMIRRORHW:
             strcpy(errmsg,"The operation cannot be carried out because no mirroring hardware is present or available.");
             break;

        case DDERR_NOOVERLAYDEST:
             strcpy(errmsg,"The IDirectDrawSurface2::GetOverlayPosition method is called on an overlay that the IDirectDrawSurface2::UpdateOverlay method has not been called on to establish a destination.");
             break;

        case DDERR_NOOVERLAYHW:
             strcpy(errmsg,"The operation cannot be carried out because no overlay hardware is present or available.");
             break;

        case DDERR_NOPALETTEATTACHED:
             strcpy(errmsg,"No palette object is attached to this surface.");
             break;

        case DDERR_NOPALETTEHW:
             strcpy(errmsg,"There is no hardware support for 16- or 256-color palettes.");
             break;

        case DDERR_NORASTEROPHW:
             strcpy(errmsg,"The operation cannot be carried out because no appropriate raster operation hardware is present or available.");
             break;

        case DDERR_NOROTATIONHW:
             strcpy(errmsg,"The operation cannot be carried out because no rotation hardware is present or available.");
             break;

        case DDERR_NOSTRETCHHW:
             strcpy(errmsg,"The operation cannot be carried out because there is no hardware support for stretching.");
             break;

        case DDERR_NOT4BITCOLOR:
             strcpy(errmsg,"The DirectDrawSurface object is not using a 4-bit color palette and the requested operation requires a 4-bit color palette.");
             break;

        case DDERR_NOT4BITCOLORINDEX:
             strcpy(errmsg,"The DirectDrawSurface object is not using a 4-bit color index palette and the requested operation requires a 4-bit color index palette.");
             break;

        case DDERR_NOT8BITCOLOR:
             strcpy(errmsg,"The DirectDrawSurface object is not using an 8-bit color palette and the requested operation requires an 8-bit color palette.");
             break;

        case DDERR_NOTAOVERLAYSURFACE:
             strcpy(errmsg,"An overlay component is called for a non-overlay surface.");
             break;

        case DDERR_NOTEXTUREHW:
             strcpy(errmsg,"The operation cannot be carried out because no texture-mapping hardware is present or available.");
             break;

        case DDERR_NOTFLIPPABLE:
             strcpy(errmsg,"An attempt has been made to flip a surface that cannot be flipped.");
             break;

        case DDERR_NOTFOUND:
             strcpy(errmsg,"The requested item was not found.");
             break;

        case DDERR_NOTINITIALIZED:
             strcpy(errmsg,"An attempt was made to call an interface method of a DirectDraw object created by CoCreateInstance before the object was initialized.");
             break;

        case DDERR_NOTLOCKED:
             strcpy(errmsg,"An attempt is made to unlock a surface that was not locked.");
             break;

        case DDERR_NOTPAGELOCKED:
             strcpy(errmsg,"An attempt is made to page unlock a surface with no outstanding page locks.");
             break;

        case DDERR_NOTPALETTIZED:
             strcpy(errmsg,"The surface being used is not a palette-based surface.");
             break;

        case DDERR_NOVSYNCHW:
             strcpy(errmsg,"The operation cannot be carried out because there is no hardware support for vertical blank synchronized operations.");
             break;

        case DDERR_NOZBUFFERHW:
             strcpy(errmsg,"The operation to create a z-buffer in display memory or to perform a blit using a z-buffer cannot be carried out because there is no hardware support for z-buffers.");
             break;

        case DDERR_NOZOVERLAYHW:
             strcpy(errmsg,"The overlay surfaces cannot be z-layered based on the z-order because the hardware does not support z-ordering of overlays.");
             break;

        case DDERR_OUTOFCAPS:
             strcpy(errmsg,"The hardware needed for the requested operation has already been allocated.");
             break;

        case DDERR_OUTOFMEMORY:
             strcpy(errmsg,"DirectDraw does not have enough memory to perform the operation.");
             break;

        case DDERR_OUTOFVIDEOMEMORY:
             strcpy(errmsg,"DirectDraw does not have enough display memory to perform the operation.");
             break;

        case DDERR_OVERLAYCANTCLIP:
             strcpy(errmsg,"The hardware does not support clipped overlays.");
             break;

        case DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
             strcpy(errmsg,"An attempt was made to have more than one color key active on an overlay.");
             break;

        case DDERR_OVERLAYNOTVISIBLE:
             strcpy(errmsg,"The IDirectDrawSurface2::GetOverlayPosition method is called on a hidden overlay.");
             break;

        case DDERR_PALETTEBUSY:
             strcpy(errmsg,"Access to this palette is refused because the palette is locked by another thread.");
             break;

        case DDERR_PRIMARYSURFACEALREADYEXISTS:
             strcpy(errmsg,"This process has already created a primary surface.");
             break;

        case DDERR_REGIONTOOSMALL:
             strcpy(errmsg,"The region passed to the IDirectDrawClipper::GetClipList method is too small.");
             break;

        case DDERR_SURFACEALREADYATTACHED:
             strcpy(errmsg,"An attempt was made to attach a surface to another surface to which it is already attached.");
             break;

        case DDERR_SURFACEALREADYDEPENDENT:
             strcpy(errmsg,"An attempt was made to make a surface a dependency of another surface to which it is already dependent.");
             break;

        case DDERR_SURFACEBUSY:
             strcpy(errmsg,"Access to the surface is refused because the surface is locked by another thread.");
             break;

        case DDERR_SURFACEISOBSCURED:
             strcpy(errmsg,"Access to the surface is refused because the surface is obscured.");
             break;

        case DDERR_SURFACELOST:
             strcpy(errmsg,"Access to the surface is refused because the surface memory is gone. The DirectDrawSurface object representing this surface should have the IDirectDrawSurface2::Restore method called on it.");
             break;

        case DDERR_SURFACENOTATTACHED:
             strcpy(errmsg,"The requested surface is not attached.");
             break;

        case DDERR_TOOBIGHEIGHT:
             strcpy(errmsg,"The height requested by DirectDraw is too large.");
             break;

        case DDERR_TOOBIGSIZE:
             strcpy(errmsg,"The size requested by DirectDraw is too large. However, the individual height and width are OK.");
             break;

        case DDERR_TOOBIGWIDTH:
             strcpy(errmsg,"The width requested by DirectDraw is too large.");
             break;

        case DDERR_UNSUPPORTED:
             strcpy(errmsg,"The operation is not supported.");
             break;

        case DDERR_UNSUPPORTEDFORMAT:
             strcpy(errmsg,"The FourCC format requested is not supported by DirectDraw.");
             break;

        case DDERR_UNSUPPORTEDMASK:
             strcpy(errmsg,"The bitmask in the pixel format requested is not supported by DirectDraw.");
             break;

        case DDERR_UNSUPPORTEDMODE:
             strcpy(errmsg,"The display is currently in an unsupported mode.");
             break;

        case DDERR_VERTICALBLANKINPROGRESS:
             strcpy(errmsg,"A vertical blank is in progress.");
             break;

        case DDERR_WASSTILLDRAWING:
             strcpy(errmsg,"The previous blit operation that is transferring information to or from this surface is incomplete.");
             break;

        case DDERR_WRONGMODE:
             strcpy(errmsg,"This surface cannot be restored because it was created in a different mode.");
             break;

        case DDERR_XALIGN:
             strcpy(errmsg,"The provided rectangle was not horizontally aligned on a required boundary.");
             break;

        default:
             wsprintf(errmsg, "Unknown Error Code : %04X", hresult);
             break;
    }

    wsprintf(msgtxt, "DDraw Error: %s\n", errmsg);
	return msgtxt;
}
