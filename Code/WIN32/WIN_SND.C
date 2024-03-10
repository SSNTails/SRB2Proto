
// win_snd.c : interface level code for sound
//             uses the midiStream* Win32 functions to play MIDI data with low latency and low 
//             processor overhead.

#include "win_main.h"
#include <mmsystem.h>
#include <dsound.h>

#include "../command.h"
#include "../i_sound.h"
#include "../s_sound.h"
#include "../i_system.h"
#include "../doomdef.h"
#include "../m_argv.h"
#include "../w_wad.h"
#include "../z_zone.h"

#include "dx_error.h"

#include "../qmus2mid.h"
#include "mid2strm.h"


#define TESTCODE            // remove this for release version

/* briefly described here for convenience:
typedef struct { 
    WORD  wFormatTag;       // WAVE_FORMAT_PCM is the only format accepted for DirectSound: 
                            // this tag indicates Pulse Code Modulation (PCM), an uncompressed format
                            // in which each samples represents the amplitude of the signal at the time
                            // of sampling. 
    WORD  nChannels;        // either one (mono) or two (stereo)
    DWORD nSamplesPerSec;   // the sampling rate, or frequency, in hertz.
                            //  Typical values are 11,025, 22,050, and 44,100
    DWORD nAvgBytesPerSec;  // nAvgBytesPerSec is the product of nBlockAlign and nSamplesPerSec
    WORD  nBlockAlign;      // the number of bytes required for each complete sample, for PCM formats
                            // is equal to (wBitsPerSample * nChannels / 8). 
    WORD  wBitsPerSample;   // gives the size of each sample, generally 8 or 16 bits
    WORD  cbSize;           // cbSize gives the size of any extra fields required to describe a
                            // specialized wave format. This member is always zero for PCM formats.
} WAVEFORMATEX; 
*/

// defined in d_main.c
extern  boolean nosound;
extern  boolean nomusic;


// --------------------------------------------------------------------------
// DirectSound stuff
// --------------------------------------------------------------------------
LPDIRECTSOUND           DSnd = NULL;
LPDIRECTSOUNDBUFFER     DSndPrimary;

// Stack sounds means sounds put on top of each other, since DirectSound can't play
// the same sound buffer at different locations at the same time, we need to dupli-
// cate existing buffers to play multiple instances of the same sound in the same
// time frame. A duplicate sound is freed when it is no more used. The priority that
// comes from the s_sound engine, is kept so that the lowest priority sounds are
// stopped to make place for the new sound, unless the new sound has a lower priority
// than all playing sounds, in which case the sound is not started.
#define MAXSTACKSOUNDS      32          // this is the absolute number of sounds that
                                        // can play simultaneously, whatever the value
                                        // of cv_numChannels
typedef struct {
    LPDIRECTSOUNDBUFFER lpSndBuf;
    int                 priority;
    boolean             duplicate;
} StackSound_t;
StackSound_t    StackSounds[MAXSTACKSOUNDS];


// --------------------------------------------------------------------------
// Fill the DirectSoundBuffer with data from a sample, made separate so that
// sound data cna be reloaded if a sound buffer was lost.
// --------------------------------------------------------------------------
static boolean CopySoundData (LPDIRECTSOUNDBUFFER dsbuffer, byte* data)
{
    LPVOID  lpvAudio1;              // receives address of lock start
    DWORD   dwBytes1;               // receives number of bytes locked
    HRESULT hr;

    // Obtain memory address of write block.
    hr = dsbuffer->lpVtbl->Lock (dsbuffer, 0, 0, &lpvAudio1, &dwBytes1, NULL, NULL, DSBLOCK_ENTIREBUFFER);
    
    // If DSERR_BUFFERLOST is returned, restore and retry lock. 
    if (hr == DSERR_BUFFERLOST) 
    { 
        dsbuffer->lpVtbl->Restore (dsbuffer);
        hr = dsbuffer->lpVtbl->Lock (dsbuffer, 0, 0, &lpvAudio1, &dwBytes1, NULL, NULL, DSBLOCK_ENTIREBUFFER);
    } 
    
    if ( SUCCEEDED (hr) )
    {
        // copy wave data into the buffer (note: dwBytes1 should equal to dsbdesc->dwBufferBytes ...)
        CopyMemory (lpvAudio1, data, dwBytes1);
    
        // finally, unlock the buffer
        hr = dsbuffer->lpVtbl->Unlock (dsbuffer, lpvAudio1, dwBytes1, NULL, 0);

        if ( SUCCEEDED (hr) )
            return true;
    }

    I_Error ("CopySoundData() : Lock, Unlock, or Restore FAILED");
    return false;
}

// --------------------------------------------------------------------------
// raw2DS : convert a raw sound data, returns a LPDIRECTSOUNDBUFFER
// --------------------------------------------------------------------------
//   dsdata points a 4 unsigned short header:
//    +0 : value 3 what does it mean?
//    +2 : sample rate, either 11025 or 22050.
//    +4 : number of samples, each sample is a single byte since it's 8bit
//    +6 : value 0
static LPDIRECTSOUNDBUFFER raw2DS(unsigned char *dsdata, int len)
{
    HRESULT             hr;
    WAVEFORMATEX        wfm;
    DSBUFFERDESC        dsbdesc;
    LPDIRECTSOUNDBUFFER dsbuffer;

    // initialise WAVEFORMATEX structure describing the wave format
    ZeroMemory (&wfm, sizeof(WAVEFORMATEX));
    wfm.wFormatTag = WAVE_FORMAT_PCM;
    wfm.nChannels = 1;
    wfm.nSamplesPerSec = *((unsigned short*)dsdata+1);      //mostly 11025, but some at 22050.
    wfm.wBitsPerSample = 8;
    wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
    wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;

    // Set up DSBUFFERDESC structure.
    ZeroMemory (&dsbdesc, sizeof(DSBUFFERDESC) );
    dsbdesc.dwSize = sizeof (DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_CTRLPAN |
                      DSBCAPS_CTRLVOLUME |
                      DSBCAPS_STICKYFOCUS |
                      //DSBCAPS_LOCSOFTWARE |
                      DSBCAPS_STATIC;
    dsbdesc.dwBufferBytes = len-8;
    dsbdesc.lpwfxFormat = &wfm;             // pointer to WAVEFORMATEX structure

    // Create the sound buffer
    hr = DSnd->lpVtbl->CreateSoundBuffer (DSnd, &dsbdesc, &dsbuffer, NULL);
    if ( FAILED( hr ) )
        I_Error ("CreateSoundBuffer() FAILED: %s\n", DXErrorToString(hr));
 
    // fill the DirectSoundBuffer waveform data
    CopySoundData (dsbuffer, (byte*)dsdata + 8);

    return dsbuffer;
}


// --------------------------------------------------------------------------
// This function loads the sound data from the WAD lump, for single sound.
// --------------------------------------------------------------------------
void* I_GetSfx (sfxinfo_t*  sfx)
{
    byte*               dssfx;
    int                 size;

    if (sfx->lumpnum<0)
        sfx->lumpnum = S_GetSfxLumpNum (sfx);

    //CONS_Printf ("I_GetSfx(%d)\n", sfx->lumpnum);

    size = W_LumpLength (sfx->lumpnum);

    // PU_CACHE because the data is copied to the DIRECTSOUNDBUFFER, the one here will not be used
    dssfx = (byte*) W_CacheLumpNum (sfx->lumpnum, PU_CACHE);

    // return the LPDIRECTSOUNDBUFFER, which will be stored in S_sfx[].data
    return (void *)raw2DS (dssfx, size);
}


// --------------------------------------------------------------------------
// Free all allocated resources for a single sound
// --------------------------------------------------------------------------
void I_FreeSfx (sfxinfo_t* sfx)
{
    LPDIRECTSOUNDBUFFER dsbuffer;

    if (sfx->lumpnum<0)
        return;

    //CONS_Printf ("I_FreeSfx(%d)\n", sfx->lumpnum);

    // free DIRECTSOUNDBUFFER
    dsbuffer = (LPDIRECTSOUNDBUFFER) sfx->data;
    if( dsbuffer )
        dsbuffer->lpVtbl->Release (dsbuffer);

    sfx->data = NULL;
    sfx->lumpnum = -1;
}


// --------------------------------------------------------------------------
// Set the global volume for sound effects
// --------------------------------------------------------------------------
void I_SetSfxVolume(int volume)
{
    int     vol;
    HRESULT hr;

    if (nosound || !sound_started)
            return;
        
    // use the last quarter of volume range
    if (cv_soundvolume.value)
        vol = (cv_soundvolume.value * ((DSBVOLUME_MAX-DSBVOLUME_MIN)/4)) / 31 +
              (DSBVOLUME_MAX - ((DSBVOLUME_MAX-DSBVOLUME_MIN)/4));
    else
        vol = DSBVOLUME_MIN;    // make sure 0 is silence
    //CONS_Printf ("setvolume to %d\n", vol);
    hr = DSndPrimary->lpVtbl->SetVolume (DSndPrimary, vol);
    //if (FAILED(hr))
    //    CONS_Printf ("setvolumne failed\n");
}


// --------------------------------------------------------------------------
// Update the volume for a secondary buffer, make sure it was created with
// DSBCAPS_CTRLVOLUME
// --------------------------------------------------------------------------
static void I_UpdateSoundVolume (LPDIRECTSOUNDBUFFER lpSnd, int volume)
{
    HRESULT hr;
    volume = (volume * ((DSBVOLUME_MAX-DSBVOLUME_MIN)/4)) / 256 +
                        (DSBVOLUME_MAX - ((DSBVOLUME_MAX-DSBVOLUME_MIN)/4));
    hr = lpSnd->lpVtbl->SetVolume (lpSnd, volume);
    //if (FAILED(hr))
    //    CONS_Printf ("SetVolume FAILED\n");
}


// --------------------------------------------------------------------------
// Update the panning for a secondary buffer, make sure it was created with
// DSBCAPS_CTRLPAN
// --------------------------------------------------------------------------
#define DSBPAN_RANGE    (DSBPAN_RIGHT-(DSBPAN_LEFT))
#define SEP_RANGE       256     //Doom sounds pan range 0-255 (128 is centre)
static void I_UpdateSoundPanning (LPDIRECTSOUNDBUFFER lpSnd, int sep)
{
    HRESULT hr;
    hr = lpSnd->lpVtbl->SetPan (lpSnd, (sep * DSBPAN_RANGE)/SEP_RANGE - DSBPAN_RIGHT);
    //if (FAILED(hr))
    //    CONS_Printf ("SetPan FAILED for sep %d pan %d\n", sep, (sep * DSBPAN_RANGE)/SEP_RANGE - DSBPAN_RIGHT);
}


// --------------------------------------------------------------------------
// Start the given S_sfx[id] sound with given properties (panning, volume..)
// FIXME: if a specific sound Id is already being played, another instance
//        of that sound should be created with DuplicateSound()
// --------------------------------------------------------------------------
int I_StartSound (int            id,
                          int            vol,
                          int            sep,
                          int            pitch,
                          int            priority )
{
    HRESULT     hr;
    LPDIRECTSOUNDBUFFER     dsbuffer;
    byte*       dsdata;
    int         lumpnum;
    int         lowestpri,lowestprihandle;
    DWORD       dwStatus;
    int         handle = -1;
    int         i;

    if (nosound)
        return -1;

    //CONS_Printf ("I_StartSound:\n\t\tS_sfx[%d]\n", id);
    
    dsbuffer = (LPDIRECTSOUNDBUFFER)S_sfx[id].data;

    // DirectSound can't play multiple instances of the same sound buffer
    // unless they are duplicated, so if the sound buffer is in use, make a duplicate
    lowestpri = 256;
    lowestprihandle = 0;
    for (i=0; i<MAXSTACKSOUNDS; i++)
    {
        //remember lowest priority sound
        if (StackSounds[i].lpSndBuf)
            if (StackSounds[i].priority < lowestpri) {
                lowestpri = StackSounds[i].priority;
                lowestprihandle = i;
            }

        // find a free 'playing sound slot' to use
        if (StackSounds[i].lpSndBuf==NULL) {
            //CONS_Printf ("\t\tfound free slot %d\n", i);
            break;
        }
        else
            // check for sounds that finished playing, and can be freed
            if( !I_SoundIsPlaying(i) )
            {
                //CONS_Printf ("\t\tfinished sound in slot %d\n", i);
                //stop sound and free the 'slot'
                I_StopSound (i);
                // we can use this one since it's now freed
                break;
            }
    }
    
    // the maximum of sounds playing at the same time is reached, if we have at least
    // one sound playing with a lower priority, stop it and replace it with the new one
    if (i>=MAXSTACKSOUNDS)
    {
        //CONS_Printf ("\t\tall slots occupied..");
        if (priority >= lowestpri) {
            handle = lowestprihandle;
            //CONS_Printf (" kicking out lowest priority slot: %d pri: %d, my priority: %d\n",
            //             handle, lowestpri, priority);
        }
        else {
            //CONS_Printf (" not enough priority mine: %d lowest is: %d\n", priority, lowestpri);
            return -1;
        }
    }
    else {
        // we found a free playing sound slot
        handle = i;
    }

    //CONS_Printf ("\t\tusing handle %d\n", handle);

    // if the original buffer is playing, duplicate it (DirectSound specific)
    // else, use the original buffer
    dsbuffer = (LPDIRECTSOUNDBUFFER) S_sfx[id].data;
    dsbuffer->lpVtbl->GetStatus (dsbuffer, &dwStatus);
    if (dwStatus & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING))
    {
        //CONS_Printf ("\t\toriginal sound S_sfx[%d] is playing, duplicating.. ", id);
        hr = DSnd->lpVtbl->DuplicateSoundBuffer(DSnd,  (LPDIRECTSOUNDBUFFER) S_sfx[id].data, &dsbuffer);
        if (FAILED(hr))
        {
            //CONS_Printf ("Cound't duplicate sound buffer\n");
            // re-use the original then..
            dsbuffer = (LPDIRECTSOUNDBUFFER) S_sfx[id].data;
            // clean up stacksounds info
            for (i=0; i<MAXSTACKSOUNDS; i++)
                if (handle != i &&
                    StackSounds[i].lpSndBuf == dsbuffer)
                {
                    StackSounds[i].lpSndBuf = NULL;
                }
        }
        // stop the duplicate or the re-used original
        dsbuffer->lpVtbl->Stop (dsbuffer);
    }

    // store information on the playing sound
    StackSounds[handle].lpSndBuf = dsbuffer;
    StackSounds[handle].priority = priority;
    StackSounds[handle].duplicate = (dsbuffer != (LPDIRECTSOUNDBUFFER)S_sfx[id].data);

    //CONS_Printf ("StackSounds[%d].lpSndBuf is %s\n", handle, StackSounds[handle].lpSndBuf==NULL ? "Null":"valid");
    //CONS_Printf ("StackSounds[%d].priority is %d\n", handle, StackSounds[handle].priority);
    //CONS_Printf ("StackSounds[%d].duplicate is %s\n", handle, StackSounds[handle].duplicate ? "TRUE":"FALSE");

    I_UpdateSoundVolume (dsbuffer, vol);
    I_UpdateSoundPanning (dsbuffer, sep);

    dsbuffer->lpVtbl->SetCurrentPosition (dsbuffer, 0);

    hr = dsbuffer->lpVtbl->Play (dsbuffer, 0, 0, 0);
    if (hr == DSERR_BUFFERLOST)
    {
        // restores the buffer memory and all other settings for the buffer
        hr = dsbuffer->lpVtbl->Restore (dsbuffer);
        if ( SUCCEEDED ( hr ) )
        {
            // reload sample data here
            lumpnum = S_sfx[id].lumpnum;
            if (lumpnum<0)
                lumpnum = S_GetSfxLumpNum (&S_sfx[id]);
            dsdata = W_CacheLumpNum (lumpnum, PU_CACHE);
            CopySoundData (dsbuffer, (byte*)dsdata + 8);
            
            // play
            hr = dsbuffer->lpVtbl->Play (dsbuffer, 0, 0, 0);
        }
        else
            I_Error ("I_StartSound : ->Restore FAILED");
    }

    // Returns a handle
    return handle;
}


// --------------------------------------------------------------------------
// Stop a sound if it is playing,
// free the corresponding 'playing sound slot' in StackSounds[]
// --------------------------------------------------------------------------
void I_StopSound (int handle)
{
    LPDIRECTSOUNDBUFFER dsbuffer;
    HRESULT hr;

    if (nosound || handle<0)
        return;

    //CONS_Printf ("I_StopSound (%d)\n", handle);
    
    dsbuffer = StackSounds[handle].lpSndBuf;
    hr = dsbuffer->lpVtbl->Stop (dsbuffer);

    // free duplicates of original sound buffer (DirectSound hassles)
    if (StackSounds[handle].duplicate) {
        //CONS_Printf ("\t\trelease a duplicate..\n");
        dsbuffer->lpVtbl->Release (dsbuffer);
    }
    StackSounds[handle].lpSndBuf = NULL;
}


// --------------------------------------------------------------------------
// Returns whether the sound is currently playing or not
// --------------------------------------------------------------------------
int I_SoundIsPlaying(int handle)
{
    LPDIRECTSOUNDBUFFER dsbuffer;
    DWORD   dwStatus;
    
    if (nosound || handle == -1)
        return FALSE;

    dsbuffer = StackSounds[handle].lpSndBuf;
    if (dsbuffer) {
        dsbuffer->lpVtbl->GetStatus (dsbuffer, &dwStatus);
        if (dwStatus & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING))
            return TRUE;
    }
    
    return FALSE;
}


// --------------------------------------------------------------------------
// Update properties of a sound currently playing
// --------------------------------------------------------------------------
void I_UpdateSoundParams(int    handle,
                                     int        vol,
                                     int        sep,
                                     int        pitch)
{
    LPDIRECTSOUNDBUFFER     dsbuffer;

    if (nosound)
        return;

    dsbuffer = StackSounds[handle].lpSndBuf;
    if (dsbuffer) {
        I_UpdateSoundVolume (dsbuffer, vol);
        I_UpdateSoundPanning (dsbuffer, sep);
    }
}


//
// Shutdown DirectSound
//
void I_ShutdownSound(void)
{
    int i;

    CONS_Printf("I_ShutdownSound()\n");

    // release any temporary 'duplicated' secondary buffers
    for (i=0; i<MAXSTACKSOUNDS; i++)
        if (StackSounds[i].lpSndBuf)
            // stops the sound and release it if it is a duplicate
            I_StopSound (i);
    
    if (DSnd)
    {
        IDirectSound_Release(DSnd);
        DSnd = NULL;
    }
}


// ==========================================================================
// Startup DirectSound
// ==========================================================================
void I_StartupSound()
{
    HRESULT             hr;
    LPDIRECTSOUNDBUFFER lpDsb;
    DSBUFFERDESC        dsbdesc;
    WAVEFORMATEX        wfm;
    int                 cooplevel;
    int                 frequency;
    int                 p;

    sound_started = false;

    if (nosound)
        return;

    // Secure and configure sound device first.
    CONS_Printf("I_StartupSound: ");
    
    // Create DirectSound, use the default sound device
    hr = DirectSoundCreate( NULL, &DSnd, NULL);
    if ( FAILED( hr ) ) {
        CONS_Printf (" DirectSoundCreate FAILED\n"
                     " there is no sound device or the sound device is under\n"
                     " the control of another application\n" );
        nosound = true;
        return;
    }

    // register exit code, now that we have at least DirectSound to close
    I_AddExitFunc (I_ShutdownSound);

    // Set cooperative level
    // Cooperative sound with other applications can be requested at cmd-line
    if (M_CheckParm("-coopsound"))
        cooplevel = DSSCL_PRIORITY;
    else
        cooplevel = DSSCL_EXCLUSIVE;
    
    hr = DSnd->lpVtbl->SetCooperativeLevel (DSnd, hWndMain, cooplevel);
    if ( FAILED( hr ) ) {
        CONS_Printf (" SetCooperativeLevel FAILED\n");
        nosound = true;
        return;
    }

    // Set up DSBUFFERDESC structure.
    ZeroMemory (&dsbdesc, sizeof(DSBUFFERDESC) );
    dsbdesc.dwSize = sizeof (DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER |
                      DSBCAPS_CTRLVOLUME;
    dsbdesc.dwBufferBytes = 0;      // Must be 0 for primary buffer
    dsbdesc.lpwfxFormat = NULL;     // Must be NULL for primary buffer


    // frequency of primary buffer may be set at cmd-line
    p = M_CheckParm ("-freq");
    if (p && p < myargc-1) {
        frequency = atoi(myargv[p+1]);
        CONS_Printf (" requested frequency of %d hz\n", frequency);
    }
    else
        frequency = 22050;
    
    // Set up structure for the desired format
    ZeroMemory (&wfm, sizeof(WAVEFORMATEX));
    wfm.wFormatTag = WAVE_FORMAT_PCM;
    wfm.nChannels = 2;                              //STEREO SOUND!
    wfm.nSamplesPerSec = frequency;
    wfm.wBitsPerSample = 16;
    wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
    wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;

    // Gain access to the primary buffer
    hr = DSnd->lpVtbl->CreateSoundBuffer (DSnd, &dsbdesc, &lpDsb, NULL);

    // Set the primary buffer to the desired format,
    // but only if we are allowed to do it
    if (cooplevel >= DSSCL_PRIORITY)
    {
        if (SUCCEEDED ( hr ))
        {
            // Set primary buffer to the desired format. If this fails,
            // we'll just ignore and go with the default.
            hr = lpDsb->lpVtbl->SetFormat (lpDsb, &wfm);
            if (FAILED(hr))
                CONS_Printf ("I_StartupSound :  couldn't set primary buffer format.\n");
        }
    }

    // move any on-board sound memory into a contiguous block
    // to make the largest portion of free memory available.
    if (cooplevel >= DSSCL_PRIORITY)
    {
        CONS_Printf (" Compacting onboard sound-memory...");
        hr = DSnd->lpVtbl->Compact (DSnd);
        CONS_Printf (" %s\n", SUCCEEDED(hr) ? "done" : "FAILED");
    }

    // set the primary buffer to play continuously, for performance
    // "... this method will ensure that the primary buffer is playing even when no secondary
    // buffers are playing; in that case, silence will be played. This can reduce processing
    // overhead when sounds are started and stopped in sequence, because the primary buffer
    // will be playing continuously rather than stopping and starting between secondary buffers."
    hr = lpDsb->lpVtbl->Play (lpDsb, 0, 0, DSBPLAY_LOOPING);
    if ( FAILED ( hr ) )
        CONS_Printf (" Primary buffer continuous play FAILED\n");

#ifdef DEBUGSOUND
    {
    DSCAPS              DSCaps;
    DSCaps.dwSize = sizeof(DSCAPS);
    hr = DSnd->lpVtbl->GetCaps (DSnd, &DSCaps);
    if (SUCCEEDED (hr))
    {
        if (DSCaps.dwFlags & DSCAPS_CERTIFIED)
            CONS_Printf ("This driver has been certified by Microsoft\n");
        if (DSCaps.dwFlags & DSCAPS_EMULDRIVER)
            CONS_Printf ("No driver with DirectSound support installed (no hardware mixing)\n");
        if (DSCaps.dwFlags & DSCAPS_PRIMARY16BIT)
            CONS_Printf ("Supports 16-bit primary buffer\n");
        if (DSCaps.dwFlags & DSCAPS_PRIMARY8BIT)
            CONS_Printf ("Supports 8-bit primary buffer\n");
        if (DSCaps.dwFlags & DSCAPS_SECONDARY16BIT)
            CONS_Printf ("Supports 16-bit, hardware-mixed secondary buffers\n");
        if (DSCaps.dwFlags & DSCAPS_SECONDARY8BIT)
            CONS_Printf ("Supports 8-bit, hardware-mixed secondary buffers\n");

        CONS_Printf ("Maximum number of hardware buffers: %d\n", DSCaps.dwMaxHwMixingStaticBuffers);
        CONS_Printf ("Size of total hardware memory: %d\n", DSCaps.dwTotalHwMemBytes);
        CONS_Printf ("Size of free hardware memory= %d\n", DSCaps.dwFreeHwMemBytes);
        CONS_Printf ("Play Cpu Overhead (%% cpu cycles): %d\n", DSCaps.dwPlayCpuOverheadSwBuffers);
    }
    else
        CONS_Printf (" couldn't get sound device caps.\n");
    }
#endif

    // save pointer to the primary DirectSound buffer for volume changes
    DSndPrimary = lpDsb;

    ZeroMemory (StackSounds, sizeof(StackSounds));

    CONS_Printf("sound initialised.\n");
    sound_started = true;
}


// ==========================================================================
//
// MUSIC API using MidiStream
//
// ==========================================================================

#define MIDBUFFERSIZE   128*1024L       // buffer size for Mus2Midi conversion  (ugly code)
#define SPECIAL_HANDLE_CLEANMIDI  -1999 // tell I_StopSong() to do a full (slow) midiOutReset() on exit

static  BOOL            bMusicStarted;
static  char*           pMus2MidData;       // buffer allocated at program start for Mus2Mid conversion

static  UINT        uMIDIDeviceID = MIDI_MAPPER, uCallbackStatus;
static  HMIDISTRM   hStream;
static  HANDLE      hBufferReturnEvent; // for synch between the callback thread and main program thread
                                        // (we need to synch when we decide to stop/free stream buffers)

static  int         nCurrentBuffer = 0, nEmptyBuffers;

static  BOOL        bBuffersPrepared = FALSE;
static  DWORD       dwVolCache[MAX_MIDI_IN_TRACKS];
        DWORD       dwVolumePercent;    // accessed by win_main.c

        // this is accessed by mid2strm.c conversion code
        BOOL        bMidiLooped = FALSE, bMidiPlaying = FALSE, bMidiPaused = FALSE;
        CONVERTINFO ciStreamBuffers[NUM_STREAM_BUFFERS];

#define STATUS_KILLCALLBACK         100     // Signals that the callback should die
#define STATUS_CALLBACKDEAD         200     // Signals callback is done processing
#define STATUS_WAITINGFOREND    300     // Callback's waiting for buffers to play

#define DEBUG_CALLBACK_TIMEOUT 2000         // Wait 2 seconds for callback
                                        // faB: don't freeze the main code if we debug..

#define VOL_CACHE_INIT              127     // for dwVolCache[]

static BOOL bMidiCanSetVolume;          // midi caps

static void Mid2StreamFreeBuffers( void );
static void CALLBACK MidiStreamCallback (HMIDIIN hMidi, UINT uMsg, DWORD dwInstance,
                                                 DWORD dwParam1, DWORD dwParam2 );
static BOOL StreamBufferSetup( unsigned char* pMidiData, int iMidiSize );

// -------------------
// MidiErrorMessageBox
// Calls the midiOutGetErrorText() function and displays the text which
// corresponds to a midi subsystem error code.
// -------------------
static void MidiErrorMessageBox(MMRESULT mmr)
{
    char szTemp[256];

    /*szTemp[0] = '\2';   //white text to stand out*/
    midiOutGetErrorText (mmr, szTemp/*+1*/, sizeof(szTemp));
    CONS_Printf (szTemp);
    /*MessageBox (GetActiveWindow(), szTemp+1, "LEGACY",
                MB_OK | MB_ICONSTOP );*/
    //wsprintf( szDebug, "Midi subsystem error: %s", szTemp );
}


// ----------------
// I_InitAudioMixer
// ----------------
#ifdef TESTCODE
void I_InitAudioMixer (void)
{
    UINT        cMixerDevs;
    cMixerDevs = mixerGetNumDevs();
    CONS_Printf ("%d mixer devices available\n", cMixerDevs);
}
#endif


// -----------
// I_InitMusic
// Startup Midi device for streaming output
// -----------
void I_InitMusic(void)
{
    DWORD       idx;
    MMRESULT    mmrRetVal;
    UINT        cMidiDevs;
    MIDIOUTCAPS MidiOutCaps;
    char*       szTechnology;

    bMusicStarted = false;
    
    CONS_Printf("I_InitMusic()\n");
    
    if (nomusic)
            return;

    // check out number of MIDI devices available
    //
    cMidiDevs = midiOutGetNumDevs();
    if (!cMidiDevs) {
        CONS_Printf ("No MIDI devices available, music is disabled\n");
        nomusic = true;
        return;
    }
#ifdef DEBUGMIDISTREAM
    else {
        CONS_Printf ("%d MIDI devices available\n", cMidiDevs);
    }
#endif

    // get MIDI device caps
    //
    if ((mmrRetVal = midiOutGetDevCaps (uMIDIDeviceID, &MidiOutCaps, sizeof(MIDIOUTCAPS))) !=
        MMSYSERR_NOERROR) {
        CONS_Printf ("midiOutGetCaps FAILED : \n");
        MidiErrorMessageBox (mmrRetVal);
    }
    else
    {
        CONS_Printf ("MIDI product name: %s\n", MidiOutCaps.szPname);
        switch (MidiOutCaps.wTechnology) {
        case MOD_FMSYNTH:   szTechnology = "FM Synth"; break;
        case MOD_MAPPER:    szTechnology = "Microsoft MIDI Mapper"; break;
        case MOD_MIDIPORT:  szTechnology = "MIDI hardware port"; break;
        case MOD_SQSYNTH:   szTechnology = "Square wave synthesizer"; break;
        case MOD_SYNTH:     szTechnology = "Synthesizer"; break;
        default:            szTechnology = "unknown"; break;
        }
        CONS_Printf ("MIDI technology: %s\n", szTechnology);
        CONS_Printf ("MIDI caps:\n");
        if (MidiOutCaps.dwSupport & MIDICAPS_CACHE)
            CONS_Printf ("-Patch caching\n");
        if (MidiOutCaps.dwSupport & MIDICAPS_LRVOLUME)
            CONS_Printf ("-Separate left and right volume control\n");
        if (MidiOutCaps.dwSupport & MIDICAPS_STREAM)
            CONS_Printf ("-Direct support for midiStreamOut()\n");
        if (MidiOutCaps.dwSupport & MIDICAPS_VOLUME)
            CONS_Printf ("-Volume control\n");
        bMidiCanSetVolume = ((MidiOutCaps.dwSupport & MIDICAPS_VOLUME)!=0);
    }

#ifdef TESTCODE
    I_InitAudioMixer ();
#endif

    // initialisation of midicard by I_StartupSound
    pMus2MidData = (char *)Z_Malloc (MIDBUFFERSIZE,PU_STATIC,NULL);

    // ----------------------------------------------------------------------
    // Midi2Stream initialization
    // ----------------------------------------------------------------------
        // create event for synch'ing the callback thread to main program thread
    // when we will need it
    hBufferReturnEvent = CreateEvent( NULL, FALSE, FALSE,
                         "DoomLegacy Midi Playback: Wait For Buffer Return" );

    if ((mmrRetVal = midiStreamOpen(&hStream,
                                                    &uMIDIDeviceID,
                                    (DWORD)1, (DWORD)MidiStreamCallback/*NULL*/,
                                    (DWORD)0,
                                    CALLBACK_FUNCTION /*CALLBACK_NULL*/)) != MMSYSERR_NOERROR)
    {
        CONS_Printf ("I_RegisterSong: midiStreamOpen FAILED\n");
        MidiErrorMessageBox( mmrRetVal );
        nomusic = true;
        return;
    }

    // stream buffers are initially unallocated (set em NULL)
    for (idx = 0; idx < NUM_STREAM_BUFFERS; idx++ )
        ZeroMemory (&ciStreamBuffers[idx].mhBuffer, sizeof(MIDIHDR));
    // ----------------------------------------------------------------------
    
    // register exit code
    I_AddExitFunc (I_ShutdownMusic);

    bMusicStarted = true;
}


// ---------------
// I_ShutdownMusic
// ---------------
void I_ShutdownMusic(void)
{
    DWORD       idx;
    MMRESULT    mmrRetVal;

    if (!bMusicStarted)
                return;
        
    CONS_Printf("I_ShutdownMusic: \n");

    if (hStream)
    {
        I_StopSong (SPECIAL_HANDLE_CLEANMIDI);
    }
    
    Mid2StreamConverterCleanup();
    Mid2StreamFreeBuffers();

    // Free our stream buffers
    for( idx = 0; idx < NUM_STREAM_BUFFERS; idx++ ) {
            if( ciStreamBuffers[idx].mhBuffer.lpData )
            {
            GlobalFreePtr( ciStreamBuffers[idx].mhBuffer.lpData );
                ciStreamBuffers[idx].mhBuffer.lpData = NULL;
            }
    }

    if (hStream) {
                if(( mmrRetVal = midiStreamClose( hStream )) != MMSYSERR_NOERROR )
                    MidiErrorMessageBox( mmrRetVal );
        hStream = NULL;
    }
    
        CloseHandle( hBufferReturnEvent );

    //free (pMus2MidData);

    bMusicStarted = false;
}


// --------------------
// SetAllChannelVolumes
// Given a percent in tenths of a percent, sets volume on all channels to
// reflect the new value.
// --------------------
static void SetAllChannelVolumes( DWORD dwVolumePercent )
{
    DWORD       dwEvent, dwStatus, dwVol, idx;
    MMRESULT    mmrRetVal;

    if( !bMidiPlaying )
        return;

    for( idx = 0, dwStatus = MIDI_CTRLCHANGE; idx < MAX_MIDI_IN_TRACKS; idx++, dwStatus++ )
        {
            dwVol = ( dwVolCache[idx] * dwVolumePercent ) / 1000;
        //CONS_Printf ("channel %d vol %d\n", idx, dwVol);
        dwEvent = dwStatus | ((DWORD)MIDICTRL_VOLUME << 8)
                            | ((DWORD)dwVol << 16);
            if(( mmrRetVal = midiOutShortMsg( (HMIDIOUT)hStream, dwEvent ))
                                                                    != MMSYSERR_NOERROR )
            {
                MidiErrorMessageBox( mmrRetVal );
                return;
            }
        }
}


// ----------------
// I_SetMusicVolume
// Set the midi output volume
// ----------------
void I_SetMusicVolume(int volume)
{
    MMRESULT    mmrRetVal;
    int         iVolume;

    if (nomusic)
        return;
        
    if (bMidiCanSetVolume)
    {
        // method A
        // current volume is 0-31, we need 0-0xFFFF in each word (left/right channel)
        iVolume = (volume << 11) | (volume << 27);
        if ((mmrRetVal = midiOutSetVolume ((HMIDIOUT)uMIDIDeviceID, iVolume)) != MMSYSERR_NOERROR) {
            CONS_Printf ("I_SetMusicVolume: couldn't set volume\n");
            MidiErrorMessageBox(mmrRetVal);
        }
    }
    else
    {
        // method B
        dwVolumePercent = (volume * 1000) / 32;
        SetAllChannelVolumes (dwVolumePercent);
    }
}


// ----------
// I_PlaySong
// Note: doesn't use the handle, would be useful to switch between mid's after
//       some trigger (would do several RegisterSong, then PlaySong the chosen one)
// ----------
void I_PlaySong(int handle, int bLooping)
{
    MMRESULT        mmrRetVal;

    if (nomusic)
                return;
        
#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_PlaySong: looping %d\n", bLooping);
#endif

    // unpause the song first if it was paused
    if( bMidiPaused )
        I_PauseSong( handle );

        // Clear the status of our callback so it will handle
        // MOM_DONE callbacks once more
        uCallbackStatus = 0;
        if(( mmrRetVal = midiStreamRestart( hStream )) != MMSYSERR_NOERROR )
        {
                MidiErrorMessageBox( mmrRetVal );
        Mid2StreamFreeBuffers();
        Mid2StreamConverterCleanup();
        I_Error ("I_PlaySong: midiStreamRestart error");
    }
    bMidiPlaying = TRUE;
    bMidiLooped = bLooping;
}


// -----------
// I_PauseSong
// calls midiStreamPause() to pause the midi playback
// -----------
void I_PauseSong (int handle)
{
    if (nomusic)
                return;

#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_PauseSong: \n");
#endif

    if (!bMidiPaused) {
        midiStreamPause( hStream );
        bMidiPaused = true;
    }
}


// ------------
// I_ResumeSong
// un-pause the midi song with midiStreamRestart
// ------------
void I_ResumeSong (int handle)
{
    if (nomusic)
        return;

#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_ResumeSong: \n");
#endif

    if( bMidiPaused ) {
            midiStreamRestart( hStream );
        bMidiPaused = false;
    }
}


// ----------
// I_StopSong
// ----------
// faB: -1999 is a special handle here, it means we stop the midi when exiting
//      Legacy, this will do a midiOutReset() for a more 'sure' midi off.
void I_StopSong(int handle)
{
    MMRESULT        mmrRetVal;

    if (nomusic)
        return;
        
#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_StopSong: \n");
#endif

    if (bMidiPlaying || (uCallbackStatus != STATUS_CALLBACKDEAD) )
    {    
        bMidiPlaying = bMidiPaused = FALSE;
            if( uCallbackStatus != STATUS_CALLBACKDEAD &&
            uCallbackStatus != STATUS_WAITINGFOREND )
                    uCallbackStatus = STATUS_KILLCALLBACK;

        //CONS_Printf ("a: %d\n",I_GetTime());
        if(( mmrRetVal = midiStreamStop( hStream )) != MMSYSERR_NOERROR )
            {
                    MidiErrorMessageBox( mmrRetVal );
            return;
            }

        //faB: if we don't call midiOutReset() seems we have to stop the buffers
        //     ourselves (or it doesn't play anymore)
        if (!bMidiPaused && (handle != SPECIAL_HANDLE_CLEANMIDI))
        {
            midiStreamPause( hStream );
        }
        //CONS_Printf ("b: %d\n",I_GetTime());
        else
            //faB: this damn call takes 1 second and a half !!! still do it on exit
        //     to be sure everything midi is cleaned as much as possible
        if (handle == SPECIAL_HANDLE_CLEANMIDI) {
            //
            if(( mmrRetVal = midiOutReset( (HMIDIOUT)hStream )) != MMSYSERR_NOERROR )
                {
                        MidiErrorMessageBox( mmrRetVal );
                return;
                }
        }
        //CONS_Printf ("c: %d\n",I_GetTime());

        // Wait for the callback thread to release this thread, which it will do by
        // calling SetEvent() once all buffers are returned to it
            if( WaitForSingleObject( hBufferReturnEvent, DEBUG_CALLBACK_TIMEOUT )
                                                            == WAIT_TIMEOUT )
            {
            // Note, this is a risky move because the callback may be genuinely busy, but
            // when we're debugging, it's safer and faster than freezing the application,
            // which leaves the MIDI device locked up and forces a system reset...
                    CONS_Printf( "Timed out waiting for MIDI callback" );
                uCallbackStatus = STATUS_CALLBACKDEAD;
            }
        //CONS_Printf ("d: %d\n",I_GetTime());
    }

        if( uCallbackStatus == STATUS_CALLBACKDEAD )
        {
        uCallbackStatus = 0;
        Mid2StreamConverterCleanup();
        Mid2StreamFreeBuffers();
        //faB: we could close the stream here and re-open later to avoid
        //     a little quirk in mmsystem (see DirectX6 mstream note)
    }
}


// Is the song playing?
int I_QrySongPlaying (int handle)
{
    if (nomusic)
        return 0;

#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_QrySongPlaying: \n");
#endif
    return (bMidiPlaying);
}


void I_UnRegisterSong(int handle)
{
    if (nomusic)
        return;

    //faB: we might free here whatever is allocated per-music
    //     (but we don't cause I hate malloc's)
    Mid2StreamConverterCleanup();

#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_UnregisterSong: \n");
#endif
}


// --------------
// I_RegisterSong
// Prepare a song for playback
// - if MUS, convert it to MIDI format
// - setup midi stream buffers, and activate the callback procedure
//   which will continually fill the buffers with new data
// --------------
#ifdef DEBUGMIDISTREAM
void I_SaveMemToFile (unsigned char* pData, unsigned long iLength, char* sFileName);    //win_sys.c
#endif

int I_RegisterSong(void* data,int len)
{
    int             iErrorCode;
    char*           pMidiFileData = NULL;       // MIDI music buffer to be played or NULL
    int             iMus2MidSize;               // size of Midi output data

    if (nomusic)
        return 1;

#ifdef DEBUGMIDISTREAM
    CONS_Printf("I_RegisterSong: \n");
#endif
    if (!memcmp(data,"MUS",3))
    {
        // convert mus to mid with a wonderful function
        // thanks to S.Bacquet for the sources of qmus2mid
        // convert mus to mid and load it in memory
        if((iErrorCode = qmus2mid((char *)data,pMus2MidData,89,64,0,len,MIDBUFFERSIZE,
                                   &iMus2MidSize))!=0)
        {
            CONS_Printf("Cannot convert mus to mid, converterror :%d\n",iErrorCode);
            return 0;
        }
        pMidiFileData = pMus2MidData;
    }
    else
    // support mid file in WAD !!! (no conversion needed)
    if (!memcmp(data,"MThd",4))
    {
        pMidiFileData = data;
    }
    else
    {
        CONS_Printf ("Music lump is not MID or MUS music format\n");
        return 0;
    }

    if (pMidiFileData == NULL)
    {
        CONS_Printf ("Not a valid MIDI file : %d\n",iErrorCode);
        return 0;
    }
#ifdef DEBUGMIDISTREAM //EVENMORE
    else
    {
        I_SaveMemToFile (pMidiFileData, iMus2MidSize, "c:/temp/debug.mid");
    }
#endif
    
    // setup midi stream buffer
    if (StreamBufferSetup(pMidiFileData, iMus2MidSize)) {
        Mid2StreamConverterCleanup();
        I_Error ("I_RegisterSong: StreamBufferSetup FAILED");
    }

    return 1;
}


// -----------------
// StreamBufferSetup
// This function uses the filename stored in the global character array to
// open a MIDI file. Then it goes about converting at least the first part of
// that file into a midiStream buffer for playback.
// -----------------

//mid2strm.c - returns TRUE if an error occurs
BOOL Mid2StreamConverterInit( unsigned char* pMidiData, ULONG iMidiSize );
void Mid2StreamConverterCleanup( void );


// -----------------
// StreamBufferSetup
// - returns TRUE if a problem occurs
// -----------------
static BOOL StreamBufferSetup( unsigned char* pMidiData, int iMidiSize )
{
    MMRESULT            mmrRetVal;
    MIDIPROPTIMEDIV     mptd;
    BOOL    bFoundEnd = FALSE;
    int     dwConvertFlag;
    int     nChkErr;
    int     idx;

#ifdef DEBUGMIDISTREAM
    if (hStream == NULL)
        I_Error ("StreamBufferSetup: hStream is NULL!");
#endif
    
    // allocate the stream buffers (only once)
    for (idx = 0; idx < NUM_STREAM_BUFFERS; idx++ )
        {
            ciStreamBuffers[idx].mhBuffer.dwBufferLength = OUT_BUFFER_SIZE;
        if( ciStreamBuffers[idx].mhBuffer.lpData == NULL ) {
                if(( ciStreamBuffers[idx].mhBuffer.lpData = GlobalAllocPtr( GHND, OUT_BUFFER_SIZE )) == NULL )
            {
                return (FALSE);
            }
        }
        }

    // returns TRUE in case of conversion error
    if (Mid2StreamConverterInit( pMidiData, iMidiSize ))
        return( TRUE );

    // Initialize the volume cache array to some pre-defined value
    for( idx = 0; idx < MAX_MIDI_IN_TRACKS; idx++ )
        dwVolCache[idx] = VOL_CACHE_INIT;
   
    mptd.cbStruct = sizeof(mptd);
    mptd.dwTimeDiv = ifs.dwTimeDivision;
    if(( mmrRetVal = midiStreamProperty(hStream,(LPBYTE)&mptd,
                                        MIDIPROP_SET | MIDIPROP_TIMEDIV ))
                    != MMSYSERR_NOERROR )
    {
        MidiErrorMessageBox( mmrRetVal );
        return( TRUE );
    }

    nEmptyBuffers = 0;
    dwConvertFlag = CONVERTF_RESET;

    for( nCurrentBuffer = 0; nCurrentBuffer < NUM_STREAM_BUFFERS; nCurrentBuffer++ )
        {
        // Tell the converter to convert up to one entire buffer's length of output
        // data. Also, set a flag so it knows to reset any saved state variables it
        // may keep from call to call.
            ciStreamBuffers[nCurrentBuffer].dwStartOffset = 0;
            ciStreamBuffers[nCurrentBuffer].dwMaxLength = OUT_BUFFER_SIZE;
            ciStreamBuffers[nCurrentBuffer].tkStart = 0;
            ciStreamBuffers[nCurrentBuffer].bTimesUp = FALSE;

            if(( nChkErr = Mid2StreamConvertToBuffer( dwConvertFlag,
                                                                      &ciStreamBuffers[nCurrentBuffer] ))
                            != CONVERTERR_NOERROR )
            {
                if( nChkErr == CONVERTERR_DONE )
                    {
                    bFoundEnd = TRUE;
                    }
                else
                    {
                        CONS_Printf( "StreamBufferSetup: initial conversion pass failed" );
                        return( TRUE );
                    }
            }
            ciStreamBuffers[nCurrentBuffer].mhBuffer.dwBytesRecorded
                                = ciStreamBuffers[nCurrentBuffer].dwBytesRecorded;

            if( !bBuffersPrepared ) {
                if(( mmrRetVal = midiOutPrepareHeader( (HMIDIOUT)hStream,
                                            &ciStreamBuffers[nCurrentBuffer].mhBuffer,
                                            sizeof(MIDIHDR))) != MMSYSERR_NOERROR )
                    {
                        MidiErrorMessageBox( mmrRetVal );
                        return( TRUE );
                    }
        }
            if(( mmrRetVal = midiStreamOut( hStream,
                                        &ciStreamBuffers[nCurrentBuffer].mhBuffer,
                                        sizeof(MIDIHDR))) != MMSYSERR_NOERROR )
            {
                MidiErrorMessageBox( mmrRetVal );
                break;
            }
            dwConvertFlag = 0;

            if( bFoundEnd )
                break;
        }

    bBuffersPrepared = TRUE;
    nCurrentBuffer = 0;

    // MIDI volume
    dwVolumePercent = (cv_musicvolume.value * 1000) / 32;
    if( hStream )
        SetAllChannelVolumes( dwVolumePercent );
    
    return( FALSE );
}


// ----------------
// SetChannelVolume
// Call here delayed by MIDI stream callback, to adapt the volume event of the
// midi stream to our own set volume percentage.
// ----------------
void I_SetMidiChannelVolume( DWORD dwChannel, DWORD dwVolumePercent )
{
    DWORD       dwEvent, dwVol;
    MMRESULT    mmrRetVal;

    if( !bMidiPlaying )
            return;

    dwVol = ( dwVolCache[dwChannel] * dwVolumePercent ) / 1000;
    //CONS_Printf ("setvolchannel %d vol %d\n", dwChannel, dwVol);
    dwEvent = MIDI_CTRLCHANGE | dwChannel | ((DWORD)MIDICTRL_VOLUME << 8)
                                                        | ((DWORD)dwVol << 16);
    if(( mmrRetVal = midiOutShortMsg( (HMIDIOUT)hStream, dwEvent ))
                                                        != MMSYSERR_NOERROR )
        {
#ifdef DEBUGMIDISTREAM
        MidiErrorMessageBox( mmrRetVal );
#endif
            return;
        }
}



// ------------------
// MidiStreamCallback
// This is the callback handler which continually refills MIDI data buffers
// as they're returned to us from the audio subsystem.
// ------------------
static void CALLBACK MidiStreamCallback (HMIDIIN hMidi, UINT uMsg, DWORD dwInstance,
                                                 DWORD dwParam1, DWORD dwParam2 )
{
    //static int  nWaitingBuffers = 0;
    MMRESULT    mmrRetVal;
    int         nChkErr;
    MIDIEVENT   *pme;
    MIDIHDR     *pmh;

    switch( uMsg )
        {
        case MOM_DONE:
        //faB:  dwParam1 is LPMIDIHDR

            if( uCallbackStatus == STATUS_CALLBACKDEAD )
                return;

            nEmptyBuffers++;

            //faB: we reached end of song, but we wait until all the buffers are returned
        if( uCallbackStatus == STATUS_WAITINGFOREND )
                {
                    if( nEmptyBuffers < NUM_STREAM_BUFFERS )
                    {
                        return;
                    }
                    else
                    {
                // stop the song when end reached (was not looping)
                        uCallbackStatus = STATUS_CALLBACKDEAD;
                    SetEvent( hBufferReturnEvent );
                I_StopSong(0);
                        return;
                    }
                }

            // This flag is set whenever the callback is waiting for all buffers to
            // come back.
            if( uCallbackStatus == STATUS_KILLCALLBACK )
                {
                    // Count NUM_STREAM_BUFFERS-1 being returned for the last time
                    if( nEmptyBuffers < NUM_STREAM_BUFFERS )
                    {
                        return;
                    }
                    // Then send a stop message when we get the last buffer back...
                    else
                    {
                        // Change the status to callback dead
                        uCallbackStatus = STATUS_CALLBACKDEAD;
                    SetEvent( hBufferReturnEvent );
                        return;
                    }
                }

            dwProgressBytes += ciStreamBuffers[nCurrentBuffer].mhBuffer.dwBytesRecorded;

        // -------------------------------------------------
        // Fill an available buffer with audio data again...
        // -------------------------------------------------

            if( bMidiPlaying && nEmptyBuffers )
                {
                    ciStreamBuffers[nCurrentBuffer].dwStartOffset = 0;
                    ciStreamBuffers[nCurrentBuffer].dwMaxLength = OUT_BUFFER_SIZE;
                    ciStreamBuffers[nCurrentBuffer].tkStart = 0;
                    ciStreamBuffers[nCurrentBuffer].dwBytesRecorded = 0;
                    ciStreamBuffers[nCurrentBuffer].bTimesUp = FALSE;

                    if(( nChkErr = Mid2StreamConvertToBuffer( 0, &ciStreamBuffers[nCurrentBuffer] ))
                                                            != CONVERTERR_NOERROR )
                    {
                        if( nChkErr == CONVERTERR_DONE )
                            {
                                // Don't include this one in the count
                                //nWaitingBuffers = NUM_STREAM_BUFFERS - 1;
                                uCallbackStatus = STATUS_WAITINGFOREND;
                                return;
                            }
                        else
                            {
                    //faB: we're not in the main thread so we can't call I_Error() now
                    //     log the error message out, and post exit message.
                                CONS_Printf( "MidiStreamCallback(): conversion pass failed!" );
                    PostMessage(hWndMain, WM_CLOSE, 0, 0);
                                return;
                            }
                    }

                ciStreamBuffers[nCurrentBuffer].mhBuffer.dwBytesRecorded
                                    = ciStreamBuffers[nCurrentBuffer].dwBytesRecorded;

                if(( mmrRetVal = midiStreamOut( hStream,
                                                &ciStreamBuffers[nCurrentBuffer].mhBuffer,
                                                sizeof(MIDIHDR))) != MMSYSERR_NOERROR )
                    {
                    MidiErrorMessageBox( mmrRetVal );
                    Mid2StreamConverterCleanup();
                        return;
                    }
                nCurrentBuffer = ( nCurrentBuffer + 1 ) % NUM_STREAM_BUFFERS;
                nEmptyBuffers--;
                }

            break;

        case MOM_POSITIONCB:
            pmh = (MIDIHDR *)dwParam1;
            pme = (MIDIEVENT *)(pmh->lpData + pmh->dwOffset);
            if( MIDIEVENT_TYPE( pme->dwEvent ) == MIDI_CTRLCHANGE )
                {
#ifdef DEBUGMIDISTREAM
            if( MIDIEVENT_DATA1( pme->dwEvent ) == MIDICTRL_VOLUME_LSB )
                    {
                        CONS_Printf ( "Got an LSB volume event" );
                PostMessage (hWndMain, WM_CLOSE, 0, 0); //faB: can't I_Error() here
                        break;
                    }
#endif
                    // this is meant to respond to our own intention, from mid2strm.c
            if( MIDIEVENT_DATA1( pme->dwEvent ) != MIDICTRL_VOLUME )
                        break;

                    // Mask off the channel number and cache the volume data byte
                    //CONS_Printf ( "dwvolcache %d = %d\n", MIDIEVENT_CHANNEL( pme->dwEvent ),  MIDIEVENT_VOLUME( pme->dwEvent ));
                    dwVolCache[ MIDIEVENT_CHANNEL( pme->dwEvent )] = MIDIEVENT_VOLUME( pme->dwEvent );
            // call SetChannelVolume() later to adjust MIDI volume control message to our
            // own current volume level.
            PostMessage( hWndMain, WM_MSTREAM_UPDATEVOLUME,
                         MIDIEVENT_CHANNEL( pme->dwEvent ), 0L );
                }
            break;

        default:
            break;
        }

    return;
}


// ---------------------
// Mid2StreamFreeBuffers
// This function unprepares and frees all our buffers -- something we must
// do to work around a bug in MMYSYSTEM that prevents a device from playing
// back properly unless it is closed and reopened after each stop.
// ---------------------
static void Mid2StreamFreeBuffers( void )
{
    DWORD       idx;
    MMRESULT    mmrRetVal;

    if( bBuffersPrepared )
    {
        for( idx = 0; idx < NUM_STREAM_BUFFERS; idx++ ) {
                if(( mmrRetVal = midiOutUnprepareHeader( (HMIDIOUT)hStream,
                                        &ciStreamBuffers[idx].mhBuffer,
                                        sizeof(MIDIHDR)))
                                                != MMSYSERR_NOERROR )
                {
                    MidiErrorMessageBox( mmrRetVal );
                }
        }
        bBuffersPrepared = FALSE;
        }

    //faB: I don't free the stream buffers here, but rather allocate them
    //      once at startup, and free'em at shutdown
}
