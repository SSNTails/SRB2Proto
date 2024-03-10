// r_glide.c : 3Dfx Glide Render driver

#include <windows.h>

#define  _CREATE_DLL_
#include "../hwr_drv.h"

#include "../../screen.h"

#include <glide.h>

#define DEBUG_TO_FILE           //output debugging msgs to r_glide.log


// **************************************************************************
//                                                                     PROTOS
// **************************************************************************

// output all debugging messages to this file
#ifdef DEBUG_TO_FILE
static HANDLE  logstream;
#endif

static void ResetStates (void);
static void InitMipmapCache (void);
static void ClearMipmapCache (void);


// **************************************************************************
//                                                                    GLOBALS
// **************************************************************************

//0 means glide mode not set yet, this is returned by grSstWinOpen()
static GrContext_t grPreviousContext = 0;

// align boundary for textures in texture cache, set at Init()
static FxU32 gr_alignboundary;


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
#ifdef DEBUG_TO_FILE
            logstream = INVALID_HANDLE_VALUE;
            logstream = CreateFile ("r_glide.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH, NULL);
            if (logstream == INVALID_HANDLE_VALUE)
                return FALSE;
#endif
            break;

        case DLL_THREAD_ATTACH:
         // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
         // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
         // Perform any necessary cleanup.
#ifdef DEBUG_TO_FILE
            if ( logstream != INVALID_HANDLE_VALUE ) {
                CloseHandle ( logstream );
                logstream  = INVALID_HANDLE_VALUE;
            }
#endif
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}


// ----------
// DBG_Printf
// Output error messages to debug log if DEBUG_TO_FILE is defined,
// else do nothing
// ----------
void DBG_Printf (LPCTSTR lpFmt, ...)
{
#ifdef DEBUG_TO_FILE
    char    str[1999];
    va_list arglist;
    DWORD   bytesWritten;

    va_start (arglist, lpFmt);
    wvsprintf (str, lpFmt, arglist);
    va_end   (arglist);

    if ( logstream != INVALID_HANDLE_VALUE )
        WriteFile (logstream, str, lstrlen(str), &bytesWritten, NULL);
#endif
}


// ==========================================================================
// Initialise
// ==========================================================================
EXPORT BOOL HWRAPI( Init ) (void)
{
    FxU32 numboards;
    FxI32 fxret;

    DBG_Printf ("HWRAPI Init()\n");

    // check for Voodoo card
    // - the ONLY possible call before GlideInit
    grGet (GR_NUM_BOARDS,4,&numboards);
    DBG_Printf ("Num 3Dfx boards : %d\n", numboards);
    if (!numboards) {
        DBG_Printf ("3dfx chipset not detected\n");
        return FALSE;
    }

    DBG_Printf( "GR_VENDOR: %s\n", grGetString(GR_VENDOR) );
    DBG_Printf( "GR_EXTENSION: %s\n", grGetString(GR_EXTENSION) );
    DBG_Printf( "GR_HARDWARE: %s\n", grGetString(GR_HARDWARE) );
    DBG_Printf( "GR_RENDERER: %s\n", grGetString(GR_RENDERER) );
    DBG_Printf( "GR_VERSION: %s\n", grGetString(GR_VERSION) );
	
    // init
    grGlideInit();

    // select subsystem
    grSstSelect( 0 ); 

    // info
    grGet (GR_MAX_TEXTURE_SIZE, 4, &fxret);
    DBG_Printf ( "Max texture size : %d\n", fxret);
    grGet (GR_NUM_TMU, 4, &fxret);
    DBG_Printf ( "Number of TMU's : %d\n", fxret);
    grGet (GR_TEXTURE_ALIGN, 4, &fxret);
    DBG_Printf ( "Align boundary for textures : %d\n", fxret);
    // save for later!
    gr_alignboundary = fxret;
    if (fxret==0)
        gr_alignboundary = 16;  //hack, need to be > 0
   
    // setup mipmap download cache
    DBG_Printf ("InitMipmapCache()\n");
    InitMipmapCache ();
    
    return TRUE;
}


static viddef_t* viddef;
// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( Shutdown ) (void)
{
    DBG_Printf ("HWRAPI Shutdown()\n");
    grGlideShutdown();
}


// **************************************************************************
//                                                  3DFX DISPLAY MODES DRIVER
// **************************************************************************

static int Set3DfxMode (viddef_t *lvid, vmode_t *pcurrentmode) ;


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#define MAX_VIDEO_MODES		30
static  vmode_t         video_modes[MAX_VIDEO_MODES] = {{NULL, NULL}};


EXPORT void HWRAPI( GetModeList ) (void** pvidmodes, int* numvidmodes)
{
    GrResolution query;
    GrResolution *list;
    GrResolution *grRes;
    char*   resTxt;
    int     listSize;
    int     listPos;
    int     iPrevWidth, iPrevHeight;    //skip duplicate modes
    int     iMode, iWidth, iHeight;

    DBG_Printf ("HWRAPI GetModeList()\n");

    // find all possible modes that include a z-buffer
    query.resolution = GR_QUERY_ANY;
    query.refresh    = GR_QUERY_ANY;
    query.numColorBuffers = 2;  //GR_QUERY_ANY;
    query.numAuxBuffers = 1;
    listSize = grQueryResolutions (&query, NULL);
    list = _alloca(listSize);
    grQueryResolutions (&query, list);

    //dl.CONS_Printf ("GLIDE_GetExtraModes() ...\n");
    grRes = list;
    iPrevWidth = 0; iPrevHeight = 0;
    for (iMode=0,listPos=0; listPos<listSize; listPos+=sizeof(GrResolution))
    {
        switch (grRes->resolution)
        {
        case GR_RESOLUTION_320x200:
            resTxt = "320x200"; iWidth = 320; iHeight = 200; break;
        case GR_RESOLUTION_320x240:
            resTxt = "320x240"; iWidth = 320; iHeight = 240; break;
        case GR_RESOLUTION_400x256:
            resTxt = "400x256"; iWidth = 400; iHeight = 256; break;
        case GR_RESOLUTION_512x384:
            resTxt = "512x384"; iWidth = 512; iHeight = 384; break;
        case GR_RESOLUTION_640x200:
            resTxt = "640x200"; iWidth = 640; iHeight = 200; break;
        case GR_RESOLUTION_640x350:
            resTxt = "640x350"; iWidth = 640; iHeight = 350; break;
        case GR_RESOLUTION_640x400:
            resTxt = "640x400"; iWidth = 640; iHeight = 400; break;
        case GR_RESOLUTION_640x480:
            resTxt = "640x480"; iWidth = 640; iHeight = 480; break;
        case GR_RESOLUTION_800x600:
            resTxt = "800x600"; iWidth = 800; iHeight = 600; break;
        case GR_RESOLUTION_960x720:
            resTxt = "960x720"; iWidth = 960; iHeight = 720; break;
        case GR_RESOLUTION_856x480:
            resTxt = "856x480"; iWidth = 856; iHeight = 480; break;
        case GR_RESOLUTION_512x256:
            resTxt = "512x256"; iWidth = 512; iHeight = 256; break;
        case GR_RESOLUTION_1024x768:
            resTxt = "1024x768"; iWidth = 1024; iHeight = 768; break;
        case GR_RESOLUTION_1280x1024:
            resTxt = "1280x1024"; iWidth = 1280; iHeight = 1024; break;
        case GR_RESOLUTION_1600x1200:
            resTxt = "1600x1200"; iWidth = 1600; iHeight = 1200; break;
        case GR_RESOLUTION_400x300:
            resTxt = "400x300"; iWidth = 400; iHeight = 300; break;
        case GR_RESOLUTION_NONE:
            resTxt = "NONE"; iWidth = 0; iHeight = 0; break;
        default:
            iWidth = 0; iHeight = 0;
            resTxt = "unknown resolution"; break;
        }

	if (iMode>=MAX_VIDEO_MODES)
	{
	    //CONS_Printf ("mode skipped (too many)\n");
	    //return FALSE;
            goto skipmodes;
	}

        // skip duplicate resolutions where only the refresh rate changes
        if (iWidth!=iPrevWidth || iHeight!=iPrevHeight)
        {
            // disable too big modes until we get it fixed
            if ( iWidth <= MAXVIDWIDTH && iHeight <= MAXVIDHEIGHT )
            {
                // save video mode information for video modes menu
                video_modes[iMode].pnext = &video_modes[iMode+1];
                video_modes[iMode].windowed = 0;
                video_modes[iMode].misc = grRes->resolution;
                video_modes[iMode].name = resTxt;
                video_modes[iMode].width = iWidth;
                video_modes[iMode].height = iHeight;
                video_modes[iMode].rowbytes = iWidth * 2;   //framebuffer 16bit, but we don't use this anyway..
                video_modes[iMode].bytesperpixel = 2;
                video_modes[iMode].pextradata = NULL;       // for VESA, unused here
                video_modes[iMode].setmode = Set3DfxMode;
                /*dl.CONS_Printf ("Mode %d : %s numColorBuffers: %d numAuxBuffers: %d\n",
                             listPos,
                             resTxt,
                             grRes->numColorBuffers,
                             grRes->numAuxBuffers);*/
                iMode++;
            }
        }

        iPrevWidth = iWidth; iPrevHeight = iHeight;
        grRes++;
    }

skipmodes:
    // add video modes to the list
    if (iMode>0)
    {
	video_modes[iMode-1].pnext = NULL;
	(*(vmode_t**)pvidmodes) = &video_modes[0];
	(*numvidmodes) = iMode;
    }

    // VGA Init equivalent (add first defautlt mode)
    //glidevidmodes[NUMGLIDEVIDMODES-1].pnext = NULL;
    //*((vmode_t**)pvidmodes) = &glidevidmodes[0];
    //*numvidmodes = NUMGLIDEVIDMODES;
}


// Set3DfxMode 
// switch to one of the video modes querried at initialization
static int Set3DfxMode (viddef_t *lvid, vmode_t *pcurrentmode)
{
    int     iResolution;

    // we stored the GR_RESOLUTION_XXX number into the 'misc' field
    iResolution = pcurrentmode->misc;

    DBG_Printf ("Set3DfxMode() iResolution(%d)\n", iResolution);

    // this is normally not used (but we are actually double-buffering with glide)
    lvid->numpages = 2;

    // Change window attributes
    SetWindowLong (lvid->WndParent, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(lvid->WndParent, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE |
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    //cleanup previous mode
    if (grPreviousContext) {
        // close the current graphics subsystem
        DBG_Printf ("grSstWinClose()\n");
        grSstWinClose (grPreviousContext);
    }

    // create Glide window
    grPreviousContext = grSstWinOpen( (FxU32)lvid->WndParent,
                        iResolution,
                        GR_REFRESH_60Hz,      //note: we could always use the higher refresh?
                        GR_COLORFORMAT_ARGB,
                        GR_ORIGIN_UPPER_LEFT,
                        2, 1 );
    if ( !grPreviousContext ) {
        DBG_Printf ("grSstWinOpen() FAILED\n");
        return 0;
    }

    // for glide debug console
    //tlSetScreen (pcurrentmode->width, pcurrentmode->height);
	
    ResetStates ();
    
    // force reload of patches because the memory is trashed while we change the screen
    ClearMipmapCache();

    lvid->buffer = NULL;    //unless we use the software view
    lvid->direct = NULL;    //direct access to video memory, old DOS crap

    // remember lvid here, to free the software buffer if we used it (software view)
    viddef = lvid;

    return 1;
}


// ==========================================================================
// Set video mode routine for GLIDE display modes
// Out: 1 ok,
//		0 hardware could not set mode,
//     -1 no mem
// ==========================================================================

// current hack to allow the software view to co-exist for development
/*static BOOL VID_FreeAndAllocVidbuffer (viddef_t *lvid)
{
    int  vidbuffersize;
    
    vidbuffersize = (lvid->width * lvid->height * lvid->bpp * NUMSCREENS)
                    + (lvid->width * ST_HEIGHT * lvid->bpp);  //status bar
    
    // free allocated buffer for previous video mode
    if ( lvid->buffer )
        GlobalFree ( lvid->buffer );
    
    // allocate & clear the new screen buffer
    if ( (lvid->buffer = GlobalAlloc (GPTR, vidbuffersize) ) == NULL )
        return FALSE;

#ifdef DEBUG
    //CONS_Printf("VID_FreeAndAllocVidbuffer done, vidbuffersize: %x\n",vidbuffersize);
#endif
    return TRUE;
}*/


// --------------------------------------------------------------------------
// Swap front and back buffers
// --------------------------------------------------------------------------
static int glide_console = 1;
EXPORT void HWRAPI( FinishUpdate ) (void)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI FinishUpdate() : display not set\n");
        return;
    }

    /*
static int frame=0;
static int lasttic=0;
    int tic,fps;

    // draw silly stuff here
    if (glide_console & 1)
        tlPrintNumber (frame++);

    if (glide_console & 1)
    {
        tic = dl.I_GetTime();
        fps = TICRATE - (tic - lasttic) + 1;
        lasttic = tic;
        tlPrintNumber (fps);
    }
*/

    //if (cv_grsoftwareview.value)
    //    CrazyHouse ();

    // update screen
    //if (glide_console & 1)
    //    tlConRender();

    //DBG_Printf ("HWRAPI FinishUpdate()\n");

    // flip screen
    grBufferSwap( 0 );			//swap immediately (don't wait vertical synch)
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static unsigned long myPaletteData[256];  // 256 ARGB entries
//TODO: do the chroma key stuff out of here
EXPORT void HWRAPI( SetPalette ) (PALETTEENTRY* pal)
{
    int i;

    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI SetPalette() : display not set\n");
        return;
    }
    
    // create the palette in the format used for downloading to 3Dfx card
    for (i=0; i<256; i++)
        myPaletteData[i] =  (0xFF << 24) |          //alpha
                            (pal[i].peRed<<16) |
                            (pal[i].peGreen<<8) |
                             pal[i].peBlue;

    // make sure the chromakey color is always the same value
    myPaletteData[HWR_PATCHES_CHROMAKEY_COLORINDEX] = HWR_PATCHES_CHROMAKEY_COLORVALUE;

    grTexDownloadTable (GR_TEXTABLE_PALETTE, (void*)myPaletteData);
}


// **************************************************************************
//
// **************************************************************************

// store min/max w depth buffer values here, used to clear buffer
static  FxU32  gr_wrange[2];


// --------------------------------------------------------------------------
// Do a full buffer clear including color / alpha / and Z buffers
// --------------------------------------------------------------------------
static void BufferClear (void)
{
    if (!grPreviousContext) {
        DBG_Printf ("BufferClear() : display not set\n");
        return;
    }
    grDepthMask (FXTRUE);
    grColorMask (FXTRUE,FXTRUE);
    grBufferClear(0x00000000, 0, gr_wrange[1]);
}


//
// set initial state of 3d card settings
//
static void ResetStates (void)
{
    DBG_Printf ("ResetStates()\n");
    
    if (!grPreviousContext) {
        DBG_Printf ("ResetStates() : display not set\n");
        return;
    }
    //grSplash( 64, 64, SCREEN_WIDTH-64, SCREEN_HEIGHT-64, 0 );	//splash screen!

    // get min/max w buffer range values
    grGet (GR_WDEPTH_MIN_MAX, 8, gr_wrange);

    // my vertex format
    grVertexLayout(GR_PARAM_XY, 0, GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_PARGB, 8, GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_Q, 12, GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_ST0, 16, GR_PARAM_ENABLE);  //s and t for tmu0

    // initialize depth buffer type
    grCoordinateSpace(GR_WINDOW_COORDS);
    grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
    grDepthBufferFunction( GR_CMP_LESS );
    grDepthMask( FXTRUE );

    //tlConSet( 0.0f, 0.0f, 1.0f, 1.0f, 60, 30, 0x20ff30 );
    //tlConOutput( "Doom Legacy 3dfx Glide version pre-alpha v0.00001\n" );

    BufferClear();
}


// **************************************************************************
//                                               3DFX MEMORY CACHE MANAGEMENT
// **************************************************************************

#define TEXMEM_2MB_EDGE         (1<<21)

static  FxU32           gr_cachemin;
static  FxU32           gr_cachemax;

static  FxU32           gr_cachetailpos;    // manage a cycling cache for mipmaps
static  FxU32           gr_cachepos;        
static  GlideMipmap_t*  gr_cachetail;
static  GlideMipmap_t*  gr_cachehead;


// --------------------------------------------------------------------------
// This must be done once only for all program execution
// --------------------------------------------------------------------------
static void ClearMipmapCache (void)
{
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
    gr_cachemin = grTexMinAddress(GR_TMU0);
    gr_cachemax = grTexMaxAddress(GR_TMU0);

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
    FxU32   mipmapSize;
    FxU32   freespace;

    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI DownloadMipmap() : display not set\n");
        return;
    }
    if (grMipmap->downloaded)
        return;

    //if (grMipmap->grInfo.data == NULL)
    //    I_Error ("info.data is NULL\n");

    // (re-)download the texture data
    mipmapSize = grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &grMipmap->grInfo);

    // 3Dfx specs : a mipmap level can not straddle the 2MByte boundary
    if ((gr_cachepos < TEXMEM_2MB_EDGE) && (gr_cachepos+mipmapSize > TEXMEM_2MB_EDGE))
        gr_cachepos = TEXMEM_2MB_EDGE;

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

            /*CONS_Printf ("        tail: %#07d   pos: %#07d\n"
                         "        size: %#07d  free: %#07d\n",
                                  gr_cachetailpos, gr_cachepos, mipmapSize, freespace);*/

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
    grTexDownloadMipMap (GR_TMU0, grMipmap->startAddress, GR_MIPMAPLEVELMASK_BOTH, &grMipmap->grInfo);
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
}


// ==========================================================================
// The mipmap becomes the current texture source
// ==========================================================================
EXPORT void HWRAPI( SetTexture ) (GlideMipmap_t* grMipmap)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI SetTexture() : display not set\n");
        return;
    }
    if (!grMipmap->downloaded)
        DownloadMipmap (grMipmap);
    
    grTexSource (GR_TMU0, grMipmap->startAddress, GR_MIPMAPLEVELMASK_BOTH, &grMipmap->grInfo);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( SetState ) (hwdstate_t IdState, int Value)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI SetState() : display not set\n");
        return;
    }
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
}


// ==========================================================================
// Read a rectangle region of the truecolor framebuffer
// store pixels as 16bit 565 RGB
// ==========================================================================
EXPORT void HWRAPI( ReadRect ) (int x, int y, int width, int height,
                                int dst_stride, unsigned short * dst_data)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI ReadRect() : display not set\n");
        return;
    }
    grLfbReadRegion (GR_BUFFER_FRONTBUFFER,
                     x, y, width, height, dst_stride, dst_data);
}


// ==========================================================================
// Defines the 2D hardware clipping window
// ==========================================================================
EXPORT void HWRAPI( ClipRect ) (int minx, int miny, int maxx, int maxy)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI ClipRect() : display not set\n");
        return;
    }
    grClipWindow ( (FxU32)minx, (FxU32)miny, (FxU32)maxx, (FxU32)maxy);
}


// ==========================================================================
// Clear the color/alpha/depth buffer(s)
// ==========================================================================
EXPORT void HWRAPI( ClearBuffer ) (int color, int alpha, hwdcleardepth_t depth)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI ClearBuffer() : display not set\n");
        return;
    }
    grBufferClear ((GrColor_t)color, (GrAlpha_t)alpha, gr_wrange[depth]);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( DrawLine ) (wallVert2D* v1, wallVert2D* v2) 
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI DrawLine() : display not set\n");
        return;
    }
    grDrawLine (v1, v2);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI( GetState ) (hwdgetstate_t IdState, void* dest)
{
    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI GetState() : display not set\n");
        return;
    }
    if (IdState == HWD_GET_FOGTABLESIZE)
    {
        grGet (GR_FOG_TABLE_ENTRIES, 4, dest);
    }
}


// ==========================================================================
// Draw a triangulated polygon
// ==========================================================================
EXPORT void HWRAPI( DrawPolygon ) (wallVert2D *projVerts, int nClipVerts, unsigned long col)
{
    int nTris;
    int i;

    if (!grPreviousContext) {
        DBG_Printf ("HWRAPI DrawPolygon() : display not set\n");
        return;
    }

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
}
