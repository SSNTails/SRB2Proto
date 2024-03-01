// qmus2mid.c : convert Doom MUS data to a MIDI music data
//              used by both DOS/WIN32

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "doomdef.h"
#include "i_system.h"
#include "byteptr.h"
#include "m_swap.h"

#include "qmus2mid.h"

ULONG  TRACKBUFFERSIZE = 65536UL;  /* 64 Ko */
struct Track track[16];

#define fwritemem(p,s,n,f)  memcpy(*f,p,n*s);*f+=(s*n)
#define fwriteshort(x,f)    WRITESHORT(*f,((x>>8) & 0xff) | (x<<8))
#define fwritelong(x,f)     WRITEULONG(*f,((x>>24) & 0xff) | ((x>>8) & 0xff00) | ((x<<8) & 0xff0000) | (x<<24))

/* this vc5 lamer don't compile right this code in debug or release mode !!

int fwritemem(void *p,int size,int number, byte **file)
{
    memcpy (*file,p,size*number);
    *file += size*number;
    return number;
}
#if 0
int fwrite2(USHORT *ptr, int size, byte **file)
{
    ULONG rev = 0;
    unsigned long i;

    for ( i = 0; i < size; i++ )
        rev = (rev << 8) + (((*ptr) >> (i*8)) & 0xFF);

    return fwritemem( &rev, size, 1, file );
}
#else
int fwrite2(byte *ptr, int size, byte **file)
{
    char    rev[4];
    unsigned long i, highbyte;

    highbyte = size - 1;

    for (i=0; i<size; i++, highbyte--) {
        rev[i] = ptr[highbyte];
    }

    // totally unreadable mess!
    //for ( i = 0; i < size; i++ )
    //    rev = (rev << 8) + ( ((*ptr) >> (i*8)) & 0xFF);

    return fwritemem( rev, size, 1, file );
}
#endif
*/
void FreeTracks ( void )
{
    int i;

    for (i = 0; i < 16; i++ )
      if (track[i].data )
        free( track[i].data );
}


void TWriteByte (byte MIDItrack, char bbyte)
{
    ULONG pos;

    pos = track[MIDItrack].current;
    if (pos < TRACKBUFFERSIZE )
        track[MIDItrack].data[pos] = bbyte;
    else
        I_Error("Mus Convert Error : Track buffer full.\n");

    track[MIDItrack].current++;
}


void TWriteVarLen (byte tracknum, ULONG value)
{
    ULONG buffer;

    buffer = value & 0x7f;
    while( (value >>= 7) )
    {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7f);
    }
    while( 1 )
    {
        TWriteByte( tracknum, (byte)buffer);
        if (buffer & 0x80 )
            buffer >>= 8;
        else
            break;
    }
}

int WriteMIDheader( USHORT ntrks, USHORT division, byte **file )
{
  fwritemem( MIDIMAGIC , 10, 1,file );
  fwriteshort( ntrks, file);
  fwriteshort( division, file );
  return 0;
}


#define last(e)         ((unsigned char)(e & 0x80))
#define event_type(e)   ((unsigned char)((e & 0x7F)>>4))
#define channel(e)      ((unsigned char)(e & 0x0F))


void WriteTrack (int tracknum, byte **file)
{
    USHORT size;
    size_t quot, rem;

    /* Do we risk overflow here ? */
    size = (USHORT)track[tracknum].current + 4;
    fwritemem( "MTrk", 4, 1, file );
    if (!tracknum )
        size += 33;

    fwritelong( size, file );
    if (!tracknum)
    {
        memset(*file,'\0',33);
        *file+=33;
    }
    quot = (size_t) (track[tracknum].current / 4096);
    rem = (size_t) (track[tracknum].current - quot*4096);
    fwritemem (track[tracknum].data, 4096, quot, file );
    fwritemem (((byte *) track[tracknum].data)+4096*quot, rem, 1, file );
    fwritemem (TRACKMAGIC2, 4, 1, file );
}


void WriteFirstTrack (byte **file)
{
    USHORT size;

    size = 43;
    fwritemem( "MTrk", 4, 1, file );
    fwritelong( size, file );
    fwritemem( TRACKMAGIC3 , 4, 1, file );
    memset(*file,'\0',22);
    *file+=22;
    fwritemem( TRACKMAGIC4, 6, 1, file );
    fwritemem( TRACKMAGIC5, 7, 1, file );
    fwritemem( TRACKMAGIC6, 4, 1, file );
}

ULONG ReadTime( byte **file )
{
    ULONG time = 0;
    int   bbyte;

    do
    {
        bbyte = *(*file)++;
        if (bbyte != EOF )
            time = (time << 7) + (bbyte & 0x7F);
    } while( (bbyte != EOF) && (bbyte & 0x80) );

    return time;
}

// return first MIDI channel available, except percussion channel 9
char FirstChannelAvailable (char MUS2MIDchannel[])
{
    int i;
    signed char max = -1;

    // note: skip channel 15 which is percussions
    for (i = 0; i < 15; i++ )
        if (MUS2MIDchannel[i] > max )
            max = MUS2MIDchannel[i];

    // MIDI channel 9 is used for percussions
    return (max == 8 ? 10 : max+1);
}

// MUS events
#define MUS_EV_SCOREEND     6

unsigned char MUS2MIDcontrol[15] =
{
    0,                  /* Program change - not a MIDI control change */
    0x00,               /* Bank select */
    0x01,               /* Modulation pot */
    0x07,               /* Volume */
    0x0A,               /* Pan pot */
    0x0B,               /* Expression pot */
    0x5B,               /* Reverb depth */
    0x5D,               /* Chorus depth */
    0x40,               /* Sustain pedal */
    0x43,               /* Soft pedal */
    0x78,               /* All sounds off */
    0x7B,               /* All notes off */
    0x7E,               /* Mono */
    0x7F,               /* Poly */
    0x79                /* Reset all controllers */
};

byte    MUSchannel;
byte    MIDItrack;

void MidiEvent(byte NewEvent)
{
    if ((NewEvent != track[MIDItrack].LastEvent) /*|| nocomp*/ )
    {
        TWriteByte( MIDItrack, NewEvent );
        track[MIDItrack].LastEvent = NewEvent;
    }
}

int qmus2mid (byte  *mus, byte *mid,     // buffers in memory
              USHORT division, int BufferSize, int nocomp,
              int    length, int midbuffersize,
              unsigned long* midilength)    //faB: returns midi file length in here
{
    byte*    file_mus;
    byte*    file_mid;     // pointer in memory

    static   MUSheader* MUSh;
    byte     MIDIchannel;
    byte     et;
    USHORT   TrackCnt=0;
    int      i, event, data;
    ULONG    DeltaTime;
    ULONG    TotalTime=0;

    byte     MIDIchan2track[16];
    char     MUS2MIDchannel[16];

    char ouch = 0;

    file_mus = mus;
    file_mid = mid;

    //r = ReadMUSheader (&MUSh, file_mus);
    MUSh = (MUSheader *)mus;
    if (strncmp (MUSh->ID, MUSMAGIC, 4))
        return NOTMUSFILE;

    //fseek( file_mus, MUSh.scoreStart, SEEK_SET );
    file_mus = (char *)mus + MUSh->scoreStart;

    if (MUSh->channels > 15)      /* <=> MUSchannels+drums > 16 */
        return TOOMCHAN;

    for (i=0; i<16; i++)
    {
        MUS2MIDchannel[i] = -1;
        track[i].current = 0;
        track[i].vel = 64;
        track[i].DeltaTime = 0;
        track[i].LastEvent = 0;
        track[i].data = NULL;
    }

    if (BufferSize)
        TRACKBUFFERSIZE = ((ULONG) BufferSize) << 10;

    event = *(file_mus++);
    et    = event_type (event);
    MUSchannel = channel (event);

    while ( (et != MUS_EV_SCOREEND) &&
            file_mus-mus < length   &&     /*!feof( file_mus )*/
            (event != EOF) )
    {
        if (MUS2MIDchannel[MUSchannel] == -1)
        {
            MIDIchannel = MUS2MIDchannel[MUSchannel] =
               // if percussion use channel 9
               (MUSchannel == 15 ? 9 : FirstChannelAvailable (MUS2MIDchannel) );
            MIDItrack = MIDIchan2track[MIDIchannel] = (byte)TrackCnt++;
            if (!(track[MIDItrack].data = (char *) malloc(TRACKBUFFERSIZE) ))
            {
                FreeTracks();
                return MEMALLOC;
            }
        }
        else
        {
            MIDIchannel = MUS2MIDchannel[MUSchannel];
            MIDItrack   = MIDIchan2track [MIDIchannel];
        }
        TWriteVarLen( MIDItrack, track[MIDItrack].DeltaTime );
        track[MIDItrack].DeltaTime = 0;
        switch (et)
        {
          case 0 :                /* release note */
            MidiEvent((byte)0x90 | MIDIchannel);
            
            data = *(file_mus++);
            TWriteByte( MIDItrack, (byte)(data & 0x7F));
              TWriteByte( MIDItrack, 0 );
              break;

          case 1 :
            MidiEvent((byte)0x90 | MIDIchannel);
            
            data = *(file_mus++);
            TWriteByte( MIDItrack, (byte)(data & 0x7F));
              if (data & 0x80)
                  track[MIDItrack].vel = (*file_mus++) & 0x7F;
              TWriteByte( MIDItrack, track[MIDItrack].vel );
              break;

          case 2 :
            MidiEvent((byte)0xE0 | MIDIchannel);

            data = *(file_mus++);
            TWriteByte( MIDItrack, (data & 1) << 6 );
            TWriteByte( MIDItrack, data >> 1 );
            break;
          case 3 :
            MidiEvent((byte)0xB0 | MIDIchannel);

            data = *(file_mus++);
            TWriteByte( MIDItrack, MUS2MIDcontrol[data] );
            if (data == 12 )
              TWriteByte( MIDItrack, MUSh->channels+1 );
            else
              TWriteByte( MIDItrack, 0 );
            break;
          case 4 :
            data = *(file_mus++);
            if (data )
            {
                MidiEvent((byte)0xB0 | MIDIchannel);

                TWriteByte( MIDItrack, MUS2MIDcontrol[data] );
            }
            else  /* program change */
                MidiEvent((byte)0xC0 | MIDIchannel);

            data = *(file_mus++);
            TWriteByte( MIDItrack, data & 0x7F );
            break;
          case 5 :
          case 7 :
            FreeTracks();
            return MUSFILECOR;
          default : break;
          }
        if (last( event ) )
        {
            DeltaTime = ReadTime( &file_mus );
            TotalTime += DeltaTime;
            for (i = 0; i < (int) TrackCnt; i++ )
                track[i].DeltaTime += DeltaTime;
        }
        event = *(file_mus++);
        if (event != EOF )
        {
            et = event_type( event );
            MUSchannel = channel( event );
        }
        else
            ouch = 1;
    }

    //if (ouch)
    //   printf( "WARNING : There are bytes missing at the end of %s.\n          "
    //           "The end of the MIDI file might not fit the original one.\n", mus );
    if (!division)
        division = 89;

    for (i = 0; i < 16; i++)
    {
      if (track[i].current && track[i].data)
      {
            TWriteByte(i, (byte)0x00); // midi end of track code
            TWriteByte(i, (byte)0xFF);
            TWriteByte(i, (byte)0x2F);
            TWriteByte(i, (byte)0x00);

      }
    }

    WriteMIDheader( TrackCnt+1, division, &file_mid );
    WriteFirstTrack( &file_mid );
    for (i = 0; i < (int) TrackCnt; i++ )
      WriteTrack( i, &file_mid );
    
    if (file_mid>mid+midbuffersize)
        return MIDTOLARGE;

    FreeTracks();

    //faB: return lenght of Midi data
    *midilength = (file_mid - mid);
    
    return 0;
}
