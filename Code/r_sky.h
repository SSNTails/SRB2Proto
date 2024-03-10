//      Sky rendering.

#ifndef __R_SKY__
#define __R_SKY__

#include "m_fixed.h"

#ifdef __GNUG__
#pragma interface
#endif

// SKY, store the number for name.
#define                 SKYFLATNAME  "F_SKY1"

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT         22

extern  int             skytexture;
extern int              skytexturemid;
extern fixed_t          skyscale;
extern int              skymode;  //current sky old (0) or new(1),
                                  // see SCR_SetMode

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.
extern int              skyflatnum;

//added:12-02-98: declare the asm routine which draws the sky columns
void R_DrawSkyColumn (void);

// Called once at startup.
void R_InitSkyMap (void);

// call after skytexture is set to adapt for old/new skies
void R_SetupSkyDraw (void);

void R_SetSkyScale (void);

#endif
