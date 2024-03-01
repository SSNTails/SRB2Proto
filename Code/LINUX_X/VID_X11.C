/* dummy X11 vid runtime for doom legacy 1.25 */
#include<stdlib.h>

int   VID_NumModes(void) {
  return 1;
}

char  *VID_GetModeName(int modenum) {
  if(modenum!=0) {
    return NULL;
  }
  return "320x200";
}

int  VID_SetMode(int modenum) { return 0; } /* dummy */

int VID_GetModeForSize( int w, int h) {
  return 0;
}
