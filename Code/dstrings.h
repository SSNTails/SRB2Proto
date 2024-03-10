//      DOOM strings, by language.
#ifndef __DSTRINGS__
#define __DSTRINGS__


// All important printed strings.
// Language selection (message strings).
// Use -DFRENCH etc.

#ifdef FRENCH
#include "d_french.h"
#else
#include "d_englsh.h"
#endif

// Misc. other strings.
#define SAVEGAMENAME    "doomsav"

//
// File locations,
//  relative to current position.
// Path names are OS-sensitive.
//
#define DEVMAPS "devmaps"
#define DEVDATA "devdata"


// Not done in french?

// QuitDOOM messages
  //added:02-01-98: "22 messages - 7 fucking messages = 15 cool messages" !
#define NUM_QUITMESSAGES   15

extern char* endmsg[];


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
