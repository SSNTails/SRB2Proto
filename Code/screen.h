//
// screen.h
//

#ifndef __SCREEN_H__
#define __SCREEN_H__

#include "command.h"

#ifdef __WIN32__
#include <windows.h>
#else
#define HWND    void*   //unused in DOS version
#endif

//added:26-01-98: quickhack for V_Init()... to be cleaned up
#define NUMSCREENS    4

// Size of statusbar.
#define ST_HEIGHT    32
#define ST_WIDTH     320

//added:20-01-98: used now as a maximum video mode size for extra vesa modes.

// we try to re-allocate a minimum of buffers for stability of the memory,
// so all the small-enough tables based on screen size, are allocated once
// and for all at the maximum size.

#define MAXVIDWIDTH     1024  //dont set this too high because actually
#define MAXVIDHEIGHT    768  // lots of tables are allocated with the MAX
                            // size.
#define BASEVIDWIDTH    320   //NEVER CHANGE THIS! this is the original
#define BASEVIDHEIGHT   200  // resolution of the graphics.

// global video state
typedef struct viddef_s
{
    int         modenum;         // vidmode num indexes videomodes list

    byte        *buffer;         // invisible screens buffer
    unsigned    rowbytes;        // bytes per scanline of the VIDEO mode
    int         width;           // PIXELS per scanline
    int         height;
    int         numpages;        // always 1, PAGE FLIPPING TODO!!!
    int         recalc;          // if true, recalc vid-based stuff
    byte        *direct;         // linear frame buffer, or vga base mem.
    int         dupx,dupy;       // scale 1,2,3 value for menus & overlays
    int         centerofs;       // centering for the scaled menu gfx
    int         bpp;             // BYTES per pixel: 1=256color, 2=highcolor

    // for Win32 version
    HWND        WndParent;       // handle of the application's window
} viddef_t;
#define VIDWIDTH    vid.width
#define VIDHEIGHT   vid.height


// internal additional info for vesa modes only
typedef struct {
    int         vesamode;         // vesa mode number plus LINEAR_MODE bit
    void        *plinearmem;      // linear address of start of frame buffer
} vesa_extra_t;
// a video modes from the video modes list,
// note: video mode 0 is always standard VGA320x200.
typedef struct vmode_s {

    struct vmode_s  *pnext;
    char         *name;
    unsigned int width;
    unsigned int height;
    unsigned int rowbytes;          //bytes per scanline
    unsigned int bytesperpixel;     // 1 for 256c, 2 for highcolor
    int          windowed;          // if true this is a windowed mode
    int          numpages;
    vesa_extra_t *pextradata;       //vesa mode extra data
    int          (*setmode)(viddef_t *lvid, struct vmode_s *pcurrentmode);
    int          misc;              //misc for display driver (r_glide.dll etc)
} vmode_t;

// ---------------------------------------------
// color mode dependent drawer function pointers
// ---------------------------------------------

extern void     (*skycolfunc) (void);
extern void     (*colfunc) (void);
#ifdef HORIZONTALDRAW
extern void     (*hcolfunc) (void);    //Fab 17-06-98
#endif
extern void     (*basecolfunc) (void);
extern void     (*fuzzcolfunc) (void);
extern void     (*transcolfunc) (void);
extern void     (*shadecolfunc) (void);
extern void     (*spanfunc) (void);
extern void     (*basespanfunc) (void);


// ----------------
// screen variables
// ----------------
extern viddef_t vid;
extern int      setmodeneeded;     // mode number to set if needed, or 0

extern boolean  fuzzymode;


extern int      scr_bpp;

extern int      scr_gamma;
extern int      scr_viewsize;      // (ex-screenblocks)

extern byte*    scr_borderpatch;   // patch used to fill the view borders


extern consvar_t cv_usegamma;
extern consvar_t cv_viewsize;
extern consvar_t cv_detaillevel;

extern consvar_t cv_scr_width;
extern consvar_t cv_scr_height;


// quick fix for tall/short skies, depending on bytesperpixel
void (*skydrawerfunc[2]) (void);

// from vid_vesa.c : user config video mode decided at VID_Init ();
extern int      vid_modenum;

// Change video mode, only at the start of a refresh.
void SCR_SetMode (void);
// Recalc screen size dependent stuff
void SCR_Recalc (void);
// Check parms once at startup
void SCR_CheckDefaultMode (void);
// Set the mode number which is saved in the config
void SCR_SetDefaultMode (void);

void SCR_Startup (void);

#endif //__SCREEN_H__
