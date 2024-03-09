
// m_misc.h :
//      Default Config File.
//      PCX Screenshots.
//      File i/o
//      Common used routines


#ifndef __M_MISC__
#define __M_MISC__


#include "doomtype.h"
#include "w_wad.h"

// the file where all game vars and settings are saved
#define CONFIGFILENAME   "config.cfg"


//
// MISC
//
//===========================================================================

boolean FIL_WriteFile ( char const*   name,
                        void*         source,
                        int           length );

int  FIL_ReadFile ( char const*   name,
                    byte**        buffer );

void FIL_DefaultExtension (char *path, char *extension);

//added:11-01-98:now declared here for use by G_DoPlayDemo(), see there...
void FIL_ExtractFileBase (char* path, char* dest);

boolean FIL_CheckExtension (char *in);

//===========================================================================

void M_ScreenShot (void);

//===========================================================================

extern char   configfile[MAX_WADPATH];

void Command_SaveConfig_f (void);
void Command_LoadConfig_f (void);
void Command_ChangeConfig_f (void);

void M_FirstLoadConfig(void);
//Fab:26-04-98: save game config : cvars, aliases..
void M_SaveConfig (char *filename);

//===========================================================================

int M_DrawText ( int           x,
                 int           y,
                 boolean       direct,
                 char*         string );

#endif
