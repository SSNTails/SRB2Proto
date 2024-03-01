#if !defined(QMUS2MID_H)
#define QMUS2MID_H

#define NOTMUSFILE      1       /* Not a MUS file */
#define COMUSFILE       2       /* Can't open MUS file */
#define COTMPFILE       3       /* Can't open TMP file */
#define CWMIDFILE       4       /* Can't write MID file */
#define MUSFILECOR      5       /* MUS file corrupted */
#define TOOMCHAN        6       /* Too many channels */
#define MEMALLOC        7       /* Memory allocation error */
#define MIDTOLARGE      8       /* If the mid don't fit in the buffer */

/* some (old) compilers mistake the "MUS\x1A" construct (interpreting
   it as "MUSx1A")      */

#define MUSMAGIC     "MUS\032"                    /* this seems to work */
#define MIDIMAGIC    "MThd\000\000\000\006\000\001"
#define TRACKMAGIC1  "\000\377\003\035"
#define TRACKMAGIC2  "\000\377\057\000"           /* end of track */
#define TRACKMAGIC3  "\000\377\002\026"
#define TRACKMAGIC4  "\000\377\131\002\000\000"
#define TRACKMAGIC5  "\000\377\121\003\011\243\032"
#define TRACKMAGIC6  "\000\377\057\000"


struct MUSheader_s
{
    char          ID[4];            // identifier "MUS" 0x1A
    USHORT        scoreLength;      // length of score in bytes
    USHORT        scoreStart;       // absolute file pos of the score
    USHORT        channels;         // count of primary channels
    USHORT        sec_channels;      // count of secondary channels
    USHORT        instrCnt;
    USHORT        dummy;
    // variable-length part starts here
    USHORT        instruments[0];
};
typedef struct MUSheader_s MUSheader;

struct Track
{
    unsigned long  current;
    char           vel;
    long           DeltaTime;
    unsigned char  LastEvent;
    char           *data;            /* Primary data */
};

int qmus2mid (byte  *mus, byte *mid,     // buffers in memory
              USHORT division, int BufferSize, int nocomp,
              int    length, int midbuffersize,
              unsigned long* midilength);    //faB: returns midi file length in here

#endif
