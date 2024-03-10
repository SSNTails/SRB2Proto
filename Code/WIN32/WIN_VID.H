
// win_vid.h

#ifndef __WIN_VID_H__
#define __WIN_VID_H__

#include "../command.h"
#include "../screen.h"

// wait for page flipping to end or not
extern consvar_t cv_vidwait;

// these are utilised among different display drivers (glide/ddraw/dosvesa/..)
extern int     numvidmodes;     //total number of DirectDraw display modes
extern vmode_t *pvidmodes;      //start of videomodes list.


// setup a video mode, this is to be called from the menu
int  VID_SetMode (int modenum);

BOOL    VID_FreeAndAllocVidbuffer (viddef_t *lvid);

// uses memcpy
void VID_BlitLinearScreen (void *srcptr, void *destptr,
                                                   int width, int height,
                                                   int srcrowbytes, int destrowbytes);

#endif //__WIN_VID_H__