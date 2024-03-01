// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <math.h>

#include <sys/time.h>
#include <sys/types.h>

#if !defined(LINUX) && !defined(SCOOS5) && !defined(_AIX)
#include <sys/filio.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/msg.h>

// Linux voxware output.
#ifdef LINUX
#include <linux/soundcard.h>
#endif

// SCO OS5 and Unixware OSS sound output
#if defined(SCOOS5) || defined(SCOUW2) || defined(SCOUW7)
#include <sys/soundcard.h>
#endif

// Timer stuff. Experimental.
#include <time.h>
#include <signal.h>

// for IPC between xdoom and musserver
#include <sys/ipc.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"
// added for 1.27 19990203 by Kin
#include "doomstat.h"
#include "searchp.h"

//#include "cd_audio.h"

// Number of sound channels used as set in the config file
extern	consvar_t cv_numChannels;

// UNIX hack, to be removed.
#ifdef SNDSERV
FILE*	sndserver=0;
char*	sndserver_filename = "llsndserv";
int     sndcnt = 0; // sfx serial no. 19990201 by Kin
#elif SNDINTR

// Update all 30 millisecs, approx. 30fps synchronized.
// Linux resolution is allegedly 10 millisecs,
//  scale is microseconds.
#define SOUND_INTERVAL     10000

// Get the interrupt. Set duration in millisecs.
int I_SoundSetTimer( int duration_of_tick );
void I_SoundDelTimer( void );
#else
// None?
#endif

// UNIX hack too, unlikely to be removed.
#ifdef MUSSERV
FILE*	musserver=0;
char*	musserver_filename = "musserver";
int	msg_id = -1;
#endif

// Flags for the -nosound and -nomusic options
extern boolean nosound;
extern boolean nomusic;

// Flag to signal CD audio support to not play a title
//int playing_title;

// A quick hack to establish a protocol between
// synchronous mix buffer updates and asynchronous
// audio writes. Probably redundant with gametic.
volatile static int flag = 0;

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.


// Needed for calling the actual sound output.
#define SAMPLECOUNT		1024
#define NUM_CHANNELS		16
// It is 2 for 16bit, and 2 for two channels.
#define BUFMUL                  4
#define MIXBUFFERSIZE		(SAMPLECOUNT*BUFMUL)

#define SAMPLERATE		11025	// Hz
#define SAMPLESIZE		2   	// 16bit

// The actual lengths of all sound effects.
//int 		lengths[NUMSFX];

// The actual output device and a flag for using 8bit samples.
int	audio_fd;
int	audio_8bit_flag;

// The global mixing buffer.
// Basically, samples from all active internal channels
//  are modifed and added, and stored in the buffer
//  that is submitted to the audio device.
signed short	mixbuffer[MIXBUFFERSIZE];


// The channel step amount...
unsigned int	channelstep[NUM_CHANNELS];
// ... and a 0.16 bit remainder of last step.
unsigned int	channelstepremainder[NUM_CHANNELS];


// The channel data pointers, start and end.
unsigned char*	channels[NUM_CHANNELS];
unsigned char*	channelsend[NUM_CHANNELS];


// Time/gametic that the channel started playing,
//  used to determine oldest, which automatically
//  has lowest priority.
// In case number of active sounds exceeds
//  available channels.
int		channelstart[NUM_CHANNELS];

// The sound in channel handles,
//  determined on registration,
//  might be used to unregister/stop/modify,
int 		channelhandles[NUM_CHANNELS];

// SFX id of the playing sound effect.
// Used to catch duplicates (like chainsaw).
int		channelids[NUM_CHANNELS];			

// Pitch to stepping lookup, unused.
int		steptable[256];

// Volume lookups.
int		vol_lookup[128*256];

// Hardware left and right channel volume lookup.
int*		channelleftvol_lookup[NUM_CHANNELS];
int*		channelrightvol_lookup[NUM_CHANNELS];




//
// Safe ioctl, convenience.
//
void
myioctl
( int	fd,
  int	command,
  int*	arg )
{   
    int		rc;
    extern int	errno;
    
    rc = ioctl(fd, command, arg);  
    if (rc < 0)
    {
	fprintf(stderr, "ioctl(dsp,%d,arg) failed\n", command);
	fprintf(stderr, "errno=%d\n", errno);
	exit(-1);
    }
}





// dummy for now 19990220 by Kin
void I_FreeSfx(sfxinfo_t* sfx) {
}

//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void* I_GetSfx (sfxinfo_t*  sfx)
{
    byte*               dssfx;
    int                 size,i;
    unsigned char*      paddedsfx;
    int                 paddedsize;

    if (sfx->lumpnum<0)
    {
        sfx->lumpnum = S_GetSfxLumpNum (sfx);
    }

    size = W_LumpLength (sfx->lumpnum);

    dssfx = (byte*) W_CacheLumpNum (sfx->lumpnum, PU_STATIC);
    paddedsize = ((size-8 + (SAMPLECOUNT-1)) / SAMPLECOUNT) * SAMPLECOUNT;
    paddedsfx = (unsigned char*)Z_Malloc( paddedsize+8, PU_STATIC, 0 );
    memcpy(  paddedsfx, dssfx, size );
    for (i=size ; i<paddedsize+8 ; i++)
        paddedsfx[i] = 128;

    // Remove the cached lump.
    Z_Free( dssfx );
    *((int*)paddedsfx) = paddedsize;
    // convert raw data and header from Doom sfx to a SAMPLE for Allegro
    // write data to llsndserv 19990201 by Kin
#ifdef SNDSERV
    if(sndserver) {
        fputc('l',sndserver);
        fwrite(paddedsfx, sizeof(int), 1,sndserver);
        fwrite(paddedsfx+8, 1, paddedsize, sndserver);
        fflush(sndserver);
    }
    Z_Free(paddedsfx);
    paddedsfx = (unsigned char*)Z_Malloc(sizeof(int),PU_STATIC,0);
    *((int*)paddedsfx) = sndcnt;
    sndcnt++;
#endif
    return (void*)(paddedsfx);

}




//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
    static unsigned short	handlenums = 0;
 
    int		i;
    int		rc = -1;
    
    int		oldest = gametic;
    int		oldestnum = 0;
    int		slot;

    int		rightvol;
    int		leftvol;

    // Chainsaw troubles.
    // Play these sound effects only one at a time.
    if ( sfxid == sfx_sawup
	 || sfxid == sfx_sawidl
	 || sfxid == sfx_sawful
	 || sfxid == sfx_sawhit
	 || sfxid == sfx_stnmov
	 || sfxid == sfx_pistol	 )
    {
	// Loop all channels, check.
	for (i=0 ; i<cv_numChannels.value ; i++)
	{
	    // Active, and using the same SFX?
	    if ( (channels[i])
		 && (channelids[i] == sfxid) )
	    {
		// Reset.
		channels[i] = 0;
		// We are sure that iff,
		//  there will only be one.
		break;
	    }
	}
    }

    // Loop all channels to find oldest SFX.
    for (i=0; (i<cv_numChannels.value) && (channels[i]); i++)
    {
	if (channelstart[i] < oldest)
	{
	    oldestnum = i;
	    oldest = channelstart[i];
	}
    }

    // Tales from the cryptic.
    // If we found a channel, fine.
    // If not, we simply overwrite the first one, 0.
    // Probably only happens at startup.
    if (i == cv_numChannels.value)
	slot = oldestnum;
    else
	slot = i;

    // Okay, in the less recent channel,
    //  we will handle the new SFX.
    // Set pointer to raw data.
    channels[slot] = (unsigned char *) S_sfx[sfxid].data+8;
    // Set pointer to end of raw data.
    channelsend[slot] = channels[slot] + *((int*)S_sfx[sfxid].data);

    // Reset current handle number, limited to 0..100.
    //if (!handlenums)
    //  handlenums = 100;

    // Assign current handle number.
    // Preserved so sounds could be stopped (unused).
    channelhandles[slot] = rc = handlenums++;

    // Set stepping???
    // Kinda getting the impression this is never used.
    channelstep[slot] = step;
    // ???
    channelstepremainder[slot] = 0;
    // Should be gametic, I presume.
    channelstart[slot] = gametic;

    // Separation, that is, orientation/stereo.
    //  range is: 1 - 256
    seperation += 1;

    // Per left/right channel.
    //  x^2 seperation,
    //  adjust volume properly.
    leftvol =
	volume - ((volume*seperation*seperation) >> 16); ///(256*256);
    seperation = seperation - 257;
    rightvol =
	volume - ((volume*seperation*seperation) >> 16);	

    // Sanity check, clamp volume.
    if (rightvol < 0 || rightvol > 127)
	I_Error("rightvol out of bounds");
    
    if (leftvol < 0 || leftvol > 127)
	I_Error("leftvol out of bounds");
    
    // Get the proper lookup table piece
    //  for this volume level???
    channelleftvol_lookup[slot] = &vol_lookup[leftvol*256];
    channelrightvol_lookup[slot] = &vol_lookup[rightvol*256];

    // Preserve sound SFX id,
    //  e.g. for avoiding duplicates of chainsaw.
    channelids[slot] = sfxid;

    // You tell me.
    return rc;
}





//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
  // Init internal lookups (raw data, mixing buffer, channels).
  // This function sets up internal lookups used during
  //  the mixing process. 
  int		i;
  int		j;
    
  int*	steptablemid = steptable + 128;
  
  // Okay, reset internal mixing channels to zero.
  /*for (i=0; i<cv_numChannels.value; i++)
  {
    channels[i] = 0;
  }*/

  // This table provides step widths for pitch parameters.
  // I fail to see that this is currently used.
  for (i=-128 ; i<128 ; i++)
    steptablemid[i] = (int)(pow(2.0, (i/64.0))*65536.0);
  
  
  // Generates volume lookup tables
  //  which also turn the unsigned samples
  //  into signed samples.
  for (i=0 ; i<128 ; i++)
  {
    for (j=0 ; j<256 ; j++)
    {
      if (!audio_8bit_flag)
	vol_lookup[i*256+j] = (i*(j-128)*256)/127;
      else
	vol_lookup[i*256+j] = (i*(j-128)*256)/127*4;
    }
  }
}	

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
   // dummy for Linux 19990117 by Kin
//  snd_SfxVolume = volume;
}

// MUSIC API
void I_SetMusicVolume(int volume)
{
  // Internal state variable.
//  snd_MusicVolume = volume;
  // Now set volume on output device.
  // Whatever( snd_MusciVolume );

  if (nomusic)
    return;

#ifdef MUSSERV

  if (msg_id != -1) {
    struct {
      long msg_type;
      char msg_text[12];
    } msg_buffer;

    msg_buffer.msg_type = 6;
    memset(msg_buffer.msg_text, 0, 12);
    msg_buffer.msg_text[0] = 'v';
    msg_buffer.msg_text[1] = volume;
    msgsnd(msg_id, (struct msgbuf *) &msg_buffer, 12, IPC_NOWAIT);
  }
#endif
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{

  // UNUSED
  priority = 0;
  vol = vol >> 4; // xdoom only accept 0-15 19990124 by Kin
  
  if (nosound)
    return 0;

#ifdef SNDSERV 
    if (sndserver)
    {
        unsigned char scmd[4];
        scmd[0] = (unsigned char)*((int*)S_sfx[id].data);
        scmd[1] = (unsigned char)pitch;
        scmd[2] = (unsigned char)vol;
        scmd[3] = (unsigned char)sep;
        fputc('p',sndserver);
	fwrite(scmd,1,4,sndserver);
	fflush(sndserver);
    }
    // warning: control reaches end of non-void function.
    return id;
#else
    // Debug.
    //fprintf( stderr, "starting sound %d", id );
    
    // Returns a handle (not used).
    id = addsfx( id, vol, steptable[pitch], sep );

    // fprintf( stderr, "/handle is %d\n", id );
    
    return id;
#endif
}



void I_StopSound (int handle)
{
  // You need the handle returned by StartSound.
  // Would be looping all channels,
  //  tracking down the handle,
  //  an setting the channel to zero.
  
#ifndef SNDSERV
  int i;

  for (i=0; i<cv_numChannels.value; i++)
  {
    if (channelhandles[i] == handle)
    {
      channels[i] = 0;
      break;
    }
  }
#endif
}


int I_SoundIsPlaying(int handle)
{
    // Ouch.
    // return gametic < handle;

#ifndef SNDSERV
    int i;

    for (i=0; i<cv_numChannels.value; i++)
    {
      if (channelhandles[i] == handle)
	return 1;
    }
#endif
    return 0;
}




//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
void I_UpdateSound( void )
{
  // Debug. Count buffer misses with interrupt.
  static int misses = 0;

  // Flag. Will be set if the mixing buffer really gets updated.
  int updated = 0;
  
  // Mix current sound data.
  // Data, from raw sound, for right and left.
  register unsigned int	sample;
  register int		dl;
  register int		dr;
  unsigned short	sdl;
  unsigned short	sdr;
  
  // Pointers in global mixbuffer, left, right, end.
  signed short*		leftout;
  signed short*		rightout;
  signed short*		leftend;
  unsigned char*	bothout;
	
  // Step in mixbuffer, left and right, thus two.
  int			step;

  // Mixing channel index.
  int			chan;
    
    // Left and right channel
    //  are in global mixbuffer, alternating.
    leftout = mixbuffer;
    rightout = mixbuffer+1;
    bothout = (unsigned char *)mixbuffer;
    step = 2;

    // Determine end, for left channel only
    //  (right channel is implicit).
    leftend = mixbuffer + SAMPLECOUNT*step;

    // Mix sounds into the mixing buffer.
    // Loop over step*SAMPLECOUNT.
    while (leftout != leftend)
    {
	// Reset left/right value. 
	dl = 0;
	dr = 0;

	// Love thy L2 chache - made this a loop.
	// Now more channels could be set at compile time
	//  as well. Thus loop those  channels.
	for ( chan = 0; chan < cv_numChannels.value; chan++ )
	{
	    // Check channel, if active.
	    if (channels[ chan ])
	    {
		// we are updating the mixer buffer, set flag
		updated++;
		// Get the raw data from the channel. 
		sample = *channels[ chan ];
		// Add left and right part
		//  for this channel (sound)
		//  to the current data.
		// Adjust volume accordingly.
		dl += channelleftvol_lookup[ chan ][sample];
		dr += channelrightvol_lookup[ chan ][sample];
		// Increment index ???
		channelstepremainder[ chan ] += channelstep[ chan ];
		// MSB is next sample???
		channels[ chan ] += channelstepremainder[ chan ] >> 16;
		// Limit to LSB???
		channelstepremainder[ chan ] &= 65536-1;

		// Check whether we are done.
		if (channels[ chan ] >= channelsend[ chan ])
		    channels[ chan ] = 0;
	    }
	}
	
	// Clamp to range. Left hardware channel.
	// Has been char instead of short.
	// if (dl > 127) *leftout = 127;
	// else if (dl < -128) *leftout = -128;
	// else *leftout = dl;

	if (!audio_8bit_flag)
	{
	    if (dl > 0x7fff)
	        *leftout = 0x7fff;
	    else if (dl < -0x8000)
	        *leftout = -0x8000;
	    else
	        *leftout = dl;

	    // Same for right hardware channel.
	    if (dr > 0x7fff)
	        *rightout = 0x7fff;
	    else if (dr < -0x8000)
	        *rightout = -0x8000;
	    else
	        *rightout = dr;
	}
	else
	{
	    if (dl > 0x7fff)
		dl = 0x7fff;
	    else if (dl < -0x8000)
		dl = -0x8000;
	    sdl = dl ^ 0xfff8000;

	    if (dr > 0x7fff)
		dr = 0x7fff;
	    else if (dr < -0x8000)
		dr = -0x8000;
	    sdr = dr ^ 0xfff8000;
	
	    *bothout++ = (((sdr + sdl) / 2) >> 8);
	}

	// Increment current pointers in mixbuffer.
	leftout += step;
	rightout += step;
    }

    if (updated)
    {
	// Debug check.
	if ( flag )
	{
	  misses += flag;
	  flag = 0;
	}

	if ( misses > 10 )
	{
	  fprintf( stderr, "I_SoundUpdate: missed 10 buffer writes\n");
	  misses = 0;
	}
    
	// Increment flag for update.
	flag++;
    }
}


// 
// This is used to write out the mixbuffer
//  during each game loop update.
//
void
I_SubmitSound(void)
{
  // Write it to DSP device.
  if (flag)
  {
    if (!audio_8bit_flag)
      write(audio_fd, mixbuffer, SAMPLECOUNT*BUFMUL);
    else
      write(audio_fd, mixbuffer, SAMPLECOUNT);
    flag = 0;
  }
}



void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // I fail too see that this is used.
  // Would be using the handle to identify
  //  on which channel the sound might be active,
  //  and resetting the channel parameters.

  // UNUSED.
  handle = vol = sep = pitch = 0;
}




void I_ShutdownSound(void)
{    
#ifdef SNDSERV
  if (sndserver)
  {
    // Send a "quit" command.
    fputc('q',sndserver);
    fflush(sndserver);
  }

#else

  // Wait till all pending sounds are finished.
  int done = 0;
  int i;
  
  if (nosound)
    return;

#ifdef SNDINTR
  I_SoundDelTimer();
#endif
  
  while ( !done )
  {
    for( i=0 ; i<cv_numChannels.value && !channels[i] ; i++)
      ;
    if (i==cv_numChannels.value)
      done++;
    else
    {
	I_UpdateSound();
	I_SubmitSound();
    }
  }

  // Cleaning up -releasing the DSP device.
  close ( audio_fd );
#endif

  // Done.
  return;
}






void
I_StartupSound()
{ 
#ifdef SNDSERV
  char buffer[2048];
  char *fn_snd;
  
  if (nosound)
    return;

  fn_snd = searchpath(sndserver_filename);
  
  // start sound process
  if ( !access(fn_snd, X_OK) )
  {
    sprintf(buffer, "%s", fn_snd);
    strcat(buffer, " -quiet");
    sndserver = popen(buffer, "w");
  }
  else
    fprintf(stderr, "Could not start sound server [%s]\n", fn_snd);
#else
    
  int i, j;

  if (nosound)
    return;

  // Secure and configure sound device first.
  fprintf( stderr, "I_InitSound: ");
  
  audio_fd = open("/dev/dsp", O_WRONLY);
  if (audio_fd<0)
  {
    fprintf(stderr, "Could not open /dev/dsp\n");
    nosound++;
    return;
  }
                     
#ifndef OLD_SOUND_DRIVER
  myioctl(audio_fd, SNDCTL_DSP_RESET, 0);
#endif

  if (getenv("DOOM_SOUND_SAMPLEBITS") == NULL)
  {
    myioctl(audio_fd, SNDCTL_DSP_GETFMTS, &i);
    if (i&=AFMT_S16_LE)    
    {
      j = 11 | (2<<16);                                           
      myioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &j);
      myioctl(audio_fd, SNDCTL_DSP_SETFMT, &i);
      i=1;
      myioctl(audio_fd, SNDCTL_DSP_STEREO, &i);
    }
    else
    {
      i = 10 | (2<<16);                                           
      myioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &i);
      i=AFMT_U8;
      myioctl(audio_fd, SNDCTL_DSP_SETFMT, &i);
      audio_8bit_flag++;
    }
  }
  else
  {
    i = 10 | (2<<16);                                           
    myioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &i);
    i=AFMT_U8;
    myioctl(audio_fd, SNDCTL_DSP_SETFMT, &i);
    audio_8bit_flag++;
  }
  
  i=SAMPLERATE;
  myioctl(audio_fd, SNDCTL_DSP_SPEED, &i);
  
  fprintf(stderr, " configured %dbit audio device\n",
	  (audio_8bit_flag) ? 8 : 16 );

#ifdef SNDINTR
  fprintf( stderr, "I_SoundSetTimer: %d microsecs\n", SOUND_INTERVAL );
  I_SoundSetTimer( SOUND_INTERVAL );
#endif
    
  // Initialize external data (all sounds) at start, keep static.
  fprintf( stderr, "I_InitSound: ");

  // Do we have a sound lump for the chaingun?
  if (W_CheckNumForName("dschgun") == -1)
  {
    // No, so link it to the pistol sound
    //S_sfx[sfx_chgun].link = &S_sfx[sfx_pistol];
    //S_sfx[sfx_chgun].pitch = 150;
    //S_sfx[sfx_chgun].volume = 0;
    //S_sfx[sfx_chgun].data = 0;
    fprintf(stderr, "linking chaingun sound to pistol sound,");
  }
  else
  {
    fprintf(stderr, "found chaingun sound,");
  }
  
  //for (i=1 ; i<NUMSFX ; i++)
  //{ 
  //  lengths[i] = 0;
  //}

  fprintf( stderr, " pre-cached all sound data\n");
  
  // Now initialize mixbuffer with zero.
  for ( i = 0; i< MIXBUFFERSIZE; i++ )
    mixbuffer[i] = 0;
  
  // Finished initialization.
  fprintf(stderr, "I_InitSound: sound module ready\n");
    
#endif
}




//
// MUSIC API.
// Music done now, we'll use Michael Heasley's musserver.
//
void I_InitMusic(void)
{
#ifdef MUSSERV
  char buffer[2048];
  char *fn_mus;
#endif

  if (nomusic)
    return;

#ifdef MUSSERV
  fn_mus = searchpath(musserver_filename);

  // now try to start the music server process
  if (!access(fn_mus, X_OK)) {
    sprintf(buffer, "%s", fn_mus);
    strcat(buffer, " -t 20");
    fprintf(stderr, "Starting music server [%s]\n", buffer);
    musserver = popen(buffer, "w");
    msg_id = msgget(53075, IPC_CREAT | 0777);
  }
  else
    fprintf(stderr, "Could not start music server [%s]\n", fn_mus);
#endif
}

void I_ShutdownMusic(void)
{
  if (nomusic)
    return;

#ifdef MUSSERV
  if (musserver)
  {
    // send a "quit" command.
    if (msg_id != -1)
      msgctl(msg_id, IPC_RMID, (struct msgid_ds *) NULL);
  }
#endif
}

static int	looping=0;
static int	musicdies=-1;

void I_PlaySong(int handle, int looping)
{
  // UNUSED.
  handle = looping = 0;
  musicdies = gametic + TICRATE*30;

  if (nomusic)
    return;

}

void I_PauseSong (int handle)
{
  // UNUSED.
  handle = 0;

  if (nomusic)
    return;

}

void I_ResumeSong (int handle)
{
  // UNUSED.
  handle = 0;

  if (nomusic)
    return;

}

void I_StopSong(int handle)
{
  // UNUSED.
  handle = 0;
  looping = 0;
  musicdies = 0;

  if (nomusic)
    return;

}

void I_UnRegisterSong(int handle)
{
  // UNUSED.
  handle = 0;
}

// BP: len is unused but is just to have compatible api
int I_RegisterSong(void* data,int len)
{
#ifdef MUSSERV
  struct {
	long msg_type;
	char msg_text[12];
  } msg_buffer;
#endif

  if (nomusic) {
    data = NULL;
    return 1;
  }

#ifdef MUSSERV
  if (msg_id != -1) {
    msg_buffer.msg_type = 6;
    memset(msg_buffer.msg_text, 0, 12);
    sprintf(msg_buffer.msg_text, "d_%s", (char *) data);
    msgsnd(msg_id, (struct msgbuf *) &msg_buffer, 12, IPC_NOWAIT);
  }
#else
  data = NULL;
#endif

  return 1;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
  // UNUSED.
  handle = 0;
  return looping || musicdies > gametic;
}



//
// Experimental stuff.
// A Linux timer interrupt, for asynchronous
//  sound output.
// I ripped this out of the Timer class in
//  our Difference Engine, including a few
//  SUN remains...
//  
#ifdef sun
    typedef     sigset_t        tSigSet;
#else    
    typedef     int             tSigSet;
#endif


// We might use SIGVTALRM and ITIMER_VIRTUAL, if the process
//  time independend timer happens to get lost due to heavy load.
// SIGALRM and ITIMER_REAL doesn't really work well.
// There are issues with profiling as well.

//static int /*__itimer_which*/  itimer = ITIMER_REAL;
static int /*__itimer_which*/  itimer = ITIMER_VIRTUAL;

//static int sig = SIGALRM;
static int sig = SIGVTALRM;

// Interrupt handler.
void I_HandleSoundTimer( int ignore )
{
  // Debug.
  //fprintf( stderr, "%c", '+' ); fflush( stderr );
  
  // Feed sound device if necesary.
  if ( flag )
  {
    // See I_SubmitSound().
    // Write it to DSP device.
    if (!audio_8bit_flag)
	write(audio_fd, mixbuffer, SAMPLECOUNT*BUFMUL);
    else
	write(audio_fd, mixbuffer, SAMPLECOUNT);

    // Reset flag counter.
    flag = 0;
  }
  else
    return;
  
  // UNUSED, but required.
  ignore = 0;
  return;
}

// Get the interrupt. Set duration in millisecs.
int I_SoundSetTimer( int duration_of_tick )
{
  // Needed for gametick clockwork.
  struct itimerval    value;
  struct itimerval    ovalue;
  struct sigaction    act;
  struct sigaction    oact;

  int res;
  
  // This sets to SA_ONESHOT and SA_NOMASK, thus we can not use it.
  //     signal( _sig, handle_SIG_TICK );
  
  // Now we have to change this attribute for repeated calls.
  act.sa_handler = I_HandleSoundTimer;
#ifndef sun    
  //ac	t.sa_mask = _sig;
#endif
  act.sa_flags = SA_RESTART;
  
  sigaction( sig, &act, &oact );

  value.it_interval.tv_sec    = 0;
  value.it_interval.tv_usec   = duration_of_tick;
  value.it_value.tv_sec       = 0;
  value.it_value.tv_usec      = duration_of_tick;

  // Error is -1.
  res = setitimer( itimer, &value, &ovalue );

  // Debug.
  if ( res == -1 )
    fprintf( stderr, "I_SoundSetTimer: interrupt n.a.\n");
  
  return res;
}


// Remove the interrupt. Set duration to zero.
void I_SoundDelTimer()
{
  // Debug.
  if ( I_SoundSetTimer( 0 ) == -1)
    fprintf( stderr, "I_SoundDelTimer: failed to remove interrupt. Doh!\n");
}
