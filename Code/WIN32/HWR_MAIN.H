// hwr_main.h : 3D render mode functions

#include "hwr_drv.h"

#include "../am_map.h"
#include "../d_player.h"
#include "../r_defs.h"

// Startup & Shutdown the hardware mode renderer
void HWR_Startup (void);
void HWR_Shutdown (void);

void HWR_clearAutomap (void);
void HWR_drawAMline (fline_t* fl, int color);
void HWR_FadeScreenMenuBack (unsigned long color, int height);
void HWR_RenderPlayerView (int viewnumber, player_t* player);
void HWR_DrawViewBorder (int clearlines);
void HWR_DrawFlatFill (int x, int y, int w, int h, int flatlumpnum);
void HWR_Screenshot (void);
void HWR_InitTextureMapping (void);
void HWR_SetViewSize (int blocks);
void HWR_ScalePatch (BOOL bScalePatch);
void HWR_DrawPatch (GlidePatch_t* gpatch, int x, int y);
void HWR_Make3DfxPatch (patch_t* patch, GlidePatch_t* grPatch);
void HWR_CreatePlanePolygons (int bspnum);
void HWR_PrepLevelCache (int numtextures, int numflats);
