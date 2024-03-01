
// i_cdmus.c : cd music interface
//	       stub for compile
// define for boolean!!!
#include "doomtype.h"
#include "i_sound.h"
// added for 1.27 19990220 by Kin
#include "command.h"

consvar_t cd_volume = {"cd_volume","31",CV_SAVE};
consvar_t cdUpdate  = {"cd_update","1",CV_SAVE};

// pause cd music
void I_StopCD (void)
{
}

// continue after a pause
void I_ResumeCD (void)
{
}


void I_ShutdownCD (void)
{
}

void I_InitCD (void)
{
}



// loop/go to next track when track is finished (if cd_update var is true)
// update the volume when it has changed (from console/menu)
// TODO: check for cd change and restart music ?
//
void I_UpdateCD (void)
{
}


// play the cd...ooxx is it a bug of gcc?
void I_PlayCD (int track, boolean looping)
{
}


// volume : logical cd audio volume 0-31 (hardware is 0-255)
int I_SetVolumeCD (int volume)
{
    return 0;
}
