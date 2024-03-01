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
// DESCRIPTION:
//      The not so system specific sound interface.
//
//-----------------------------------------------------------------------------


#ifndef __S_SOUND__
#define __S_SOUND__

#include "sounds.h"

extern consvar_t stereoreverse;

extern consvar_t cv_soundvolume;
extern consvar_t cv_musicvolume;
extern consvar_t cv_numChannels;

extern CV_PossibleValue_t soundvolume_cons_t[];
//part of i_cdmus.c
extern consvar_t cd_volume;
extern consvar_t cdUpdate;

// register sound vars and commands at game startup
void S_RegisterSoundStuff (void);


//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init (int sfxVolume, int musicVolume);


//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start(void);

//
// basicaly a wgetnumforname with adding "ds" at the begin of string
// return a lumpnum
//
int S_GetSfxLumpNum (sfxinfo_t* sfx);

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//
void
S_StartSound
( void*         origin,
  int           sound_id );



// Will start a sound at a given volume.
void
S_StartSoundAtVolume
( void*         origin,
  int           sound_id,
  int           volume );


// Stop sound for thing at <origin>
void S_StopSound(void* origin);


// Start music using <music_id> from sounds.h
void S_StartMusic(int music_id);

// Start music using <music_id> from sounds.h,
//  and set whether looping
void S_ChangeMusic (int music_id, int looping);

// Stops the music fer sure.
void S_StopMusic(void);

// Stop and resume music, during game PAUSE.
void S_PauseSound(void);
void S_ResumeSound(void);


//
// Updates music & sounds
//
void S_UpdateSounds(void);

void S_SetMusicVolume(int volume);
void S_SetSfxVolume(int volume);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
