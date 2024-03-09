
// console.h

#include "d_event.h"


// for debugging shopuld be replaced by nothing later.. so debug is inactive
#define LOG(x) CONS_Printf(x)

void CON_Init (void);

boolean CON_Responder (event_t *ev);

// set true when screen size has changed, to adapt console
extern boolean con_recalc;

extern boolean con_startup;

// top clip value for view render: do not draw part of view hidden by console
extern int     con_clipviewtop;

// 0 means console if off, or moving out
extern int     con_destlines;

void CON_ClearHUD (void);       // clear heads up messages

void CON_Ticker (void);
void CON_Drawer (void);
void CONS_Error (char *msg);       // print out error msg, and wait a key

// force console to move out
void CON_ToggleOff (void);
