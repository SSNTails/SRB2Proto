// midi-2-stream conversion
// adapted from DirectX6 sample code
// note: a lot of code is duplicate : the standalone version
// simply converts in one pass the whole file, while the
// 'mid2streamXXXX' routines are called by win_snd.c and
// convert the midi data into stream buffers on the fly

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include "mid2strm.h"

#define LEGACY      // use Legacy's console output for error messages
                    // (does not compile stand-alone)


// seems like Allegro's midi player ran the qmus2mid file because
// it uses a running status of 'note off' as default on start of conversion
#define BAD_MIDI_FIX    0x80        //should be 0 (that is running status initially -none-)
                                    //but won't play qmus2mid output, or .. fix qmus2mid?

// faBBe's prefs..
#ifndef ULONG
typedef unsigned long   ULONG;
typedef long            LONG;
#endif
#ifndef UBYTE
typedef unsigned char   UBYTE;
#endif

#define CB_STREAMBUF    (4096)                          // Size of each stream buffer

#define MIDS_SHORTMSG   (0x00000000)
#define MIDS_TEMPO      (0x01000000)

// Macros for swapping hi/lo-endian data
//
#define WORDSWAP(w)     (((w) >> 8) | (((w) << 8) & 0xFF00))

#define LONGSWAP(dw)   (((dw) >> 24) |                 \
    (((dw) >> 8) & 0x0000FF00) |    \
    (((dw) << 8) & 0x00FF0000) |    \
(((dw) << 24) & 0xFF000000))

// In debug builds, TRACKERR will show us where the parser died
//
#ifdef DEBUGMIDISTREAM
#define TRACKERR(p,sz) ShowTrackError(p,sz);
#else
#define TRACKERR(p,sz)
#endif


// These structures are stored in MIDI files; they need to be UBYTE
// aligned.
//
#pragma pack(1)

// Chunk header. dwTag is either MTrk or MThd.
//
typedef struct
{
    LONG               dwTag;                  // Type
    LONG               cbChunk;                // Length (hi-lo)
} MIDICHUNK;

// Contents of MThd chunk.
typedef struct
{
    WORD                wFormat;                // Format (hi-lo)
    WORD                nTracks;                // # tracks (hi-lo)
    WORD                wTimeDivision;          // Time division (hi-lo)
} MIDIFILEHDR;

#pragma pack()

// Temporary event structure which stores event data until we're ready to
// dump it into a stream buffer
//
typedef struct
{
    LONG               tkEvent;                // Absolute time of event
    BYTE               abEvent[4];             // Event type and parameters if channel msg
    LONG               dwEventLength;          // Of data which follows if meta or sysex
    UBYTE*             pEvent;                 // -> Event data if applicable
} TEMPEVENT, *PTEMPEVENT;

// Description of a track open for read
//
#define ITS_F_ENDOFTRK  0x00000001


// Description of a stream buffer on the output side
//
typedef struct STREAMBUF *PSTREAMBUF;
typedef struct STREAMBUF
{
    UBYTE*             pBuffer;                // -> Start of actual buffer
    LONG               tkStart;                // Tick time just before first event
    UBYTE*             pbNextEvent;            // Where to write next event
    LONG               iBytesLeft;             // UBYTEs left in buffer
    LONG               iBytesLeftUncompressed; // UBYTEs left when uncompressed
    PSTREAMBUF         pNext;                  // Next buffer
} STREAMBUF;

// Description of output stream open for write
//
typedef struct
{
    LONG               tkTrack;                // Current tick position in track       
    PSTREAMBUF         pFirst;                 // First stream buffer
    PSTREAMBUF         pLast;                  // Last (current) stream buffer
} OUTSTREAMSTATE;

// Format of structs within a MSD file
//
// 'fmt ' chunk
//

#define MDS_F_NOSTREAMID        0x00000001      // Stream ID's skipped; reader inserts
typedef struct
{
    LONG               dwTimeFormat;           // Low word == time format in SMF format
    LONG               cbMaxBuffer;            // Guaranteed max buffer size
    LONG               dwFlags;                // Format flags
} MIDSFMT;

// 'data' chunk buffer header
//
typedef struct
{
    LONG               tkStart;                // Absolute tick offset at start of buffer
    LONG               cbBuffer;               // Bytes in this buffer
} MIDSBUFFER;

// A few globals
//
static HANDLE           hInFile = INVALID_HANDLE_VALUE;
static HANDLE           hOutFile= INVALID_HANDLE_VALUE;
static OUTSTREAMSTATE   ots;
static BOOL             fCompress = FALSE;

       INFILESTATE      ifs;
static DWORD            tkCurrentTime;

#ifdef DEBUGMIDISTREAM
static char gteBadRunStat[] =           "Reference to missing running status.";
static char gteRunStatMsgTrunc[] =      "Running status message truncated";
static char gteChanMsgTrunc[] =         "Channel message truncated";
static char gteSysExLenTrunc[] =        "SysEx event truncated (length)";
static char gteSysExTrunc[] =           "SysEx event truncated";
static char gteMetaNoClass[] =          "Meta event truncated (no class UBYTE)";
static char gteMetaLenTrunc[] =         "Meta event truncated (length)";
static char gteMetaTrunc[] =            "Meta event truncated";
#endif

// Prototypes
//
static BOOL             Init(LPSTR szInFile, LPSTR szOutFile);
static UBYTE*           GetInFileData(LONG cbToGet);
static void             Cleanup(void);
static BOOL             BuildNewTracks(void);
static BOOL             WriteStreamBuffers(void);
static BOOL             GetTrackVDWord(INTRACKSTATE* pInTrack, ULONG* lpdw);
static BOOL             GetTrackEvent(INTRACKSTATE* pInTrack, TEMPEVENT *pMe);
static BOOL             AddEventToStream(TEMPEVENT *pMe);
static UBYTE*           GetOutStreamBytes(LONG tkNow, LONG cbNeeded, LONG cbUncompressed);
#ifdef DEBUGMIDISTREAM
static void             ShowTrackError(INTRACKSTATE* pInTrack, char* szErr);
#endif
// either uses the local one (prints out on stderr) or Legacy's console output
void CONS_Printf (char *fmt, ...);
void I_Error (char *error, ...);



#ifndef LEGACY
// only for stand-alone version
//
void _cdecl main(int argc, char* argv[])
{
    UINT    idxFnames;
    
    if (argc < 3)
    {
        CONS_Printf ( "Format is mid2strm [-c] infile outfile\n");
        CONS_Printf ( "-c\tNo-stream-id compression\n");
        exit(1);
    }
    
    idxFnames = 1;
    if (argv[1][0] == '-')
    {
        ++idxFnames;
        if (argv[1][1] == 'c')
            fCompress = TRUE;
    }
    
    if (!Init(argv[idxFnames], argv[idxFnames+1]))
        exit(1);
    
    if (!BuildNewTracks())
        exit(1);
    
    if (!WriteStreamBuffers())
        exit(1);
    
    // Add cleanup code!!!
    //
    Cleanup();
    
    exit(0);
}
#endif


// ----
// Init (stand-alone version)
// 
// Open the input and output files
// Allocate and read the entire input file into memory
// Validate the input file structure
// Allocate the input track structures and initialize them
// Initialize the output track structures
//
// Return TRUE on success
// Prints its own error message if something goes wrong
//
// ----
static BOOL Init(char* szInFile, char* szOutFile)
{
    BOOL            fRet = FALSE;
    LONG            cbRead;
    ULONG*          pChunkID;
    ULONG*          pChunkSize;
    LONG            iChunkSize;
    MIDIFILEHDR*    pHeader;
    INTRACKSTATE*   pInTrack;
    UINT            iTrack;
    
    // Initialize things we'll try to free later if we fail
    //
    ifs.FileSize = 0;
    ifs.pFile = NULL;
    //ifs.pTracks = NULL;
    
    // Attempt to open the input and output files
    //
    hInFile = CreateFile(szInFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hInFile)
    {
        CONS_Printf ( "Could not open \"%s\" for read.\n", szInFile);
        goto Init_Cleanup;
    }
    
    hOutFile = CreateFile(szOutFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hOutFile)
    {
        CONS_Printf ( "Could not open \"%s\" for write.\n", szOutFile);
        goto Init_Cleanup;
    }
    
    // Figure out how big the input file is and allocate a chunk of memory big enough
    // to hold the whole thing. Read the whole file in at once.
    //
    if (((UINT)-1) == (ifs.FileSize = GetFileSize(hInFile, NULL)))
    {
        CONS_Printf ( "File system error on input file.\n");
        goto Init_Cleanup;
    }
    
    if (NULL == (ifs.pFile = GlobalAllocPtr(GPTR, ifs.FileSize)))
    {
        CONS_Printf ( "Out of memory.\n");
        goto Init_Cleanup;
    }
    
    if ((!ReadFile(hInFile, ifs.pFile, ifs.FileSize, &cbRead, NULL)) ||
        cbRead != ifs.FileSize)
    {
        CONS_Printf ( "Read error on input file.\n");
        goto Init_Cleanup;
    }
    
    // Set up to read from the memory buffer. Read and validate
    // - MThd header
    // - size of file header chunk
    // - file header itself
    //      
    ifs.iBytesLeft = ifs.FileSize;
    ifs.pFilePointer = ifs.pFile;
    
    // note: midi header size should always be 6
    if ((pChunkID = (ULONG*)GetInFileData(sizeof(*pChunkID))) == NULL ||
        *pChunkID != MThd ||
        (pChunkSize = (ULONG*)GetInFileData(sizeof(*pChunkSize))) == NULL ||
        (iChunkSize = LONGSWAP(*pChunkSize)) < sizeof(MIDIFILEHDR) ||
        (pHeader = (MIDIFILEHDR*)GetInFileData(iChunkSize)) == NULL )
    {
        CONS_Printf ( "Read error on MIDI header.\n");
        goto Init_Cleanup;
    }
    
    // File header is stored in hi-lo order. Swap this into Intel order and save
    // parameters in our native int size (32 bits)
    //
    ifs.dwFormat = (LONG)WORDSWAP(pHeader->wFormat);
    ifs.nTracks   = (LONG)WORDSWAP(pHeader->nTracks);
    ifs.dwTimeDivision = (LONG)WORDSWAP(pHeader->wTimeDivision);

#ifdef DEBUGMIDISTREAM
    CONS_Printf ("MIDI Header:\n"
                   "------------\n"
                   "format: %d\n"
                   "number of tracks: %d\n"
                   "time division: %d\n", ifs.dwFormat, ifs.nTracks, ifs.dwTimeDivision);
#endif

    // We know how many tracks there are; allocate the structures for them and parse
    // them. The parse merely looks at the MTrk signature and track chunk length
    // in order to skip to the next track header.
    /* faB: now static
    ifs.pTracks = (INTRACKSTATE*)GlobalAllocPtr(GPTR, ifs.nTracks*sizeof(INTRACKSTATE));
    if (ifs.pTracks==NULL)
    {
        CONS_Printf ( "Out of memory.\n");
        goto Init_Cleanup;
    }
    */

    // faB: made it static, but don't quit if there are more tracks, just skip'em
    // (this isn't really a limit, since 32 tracks are really the maximum for MIDI files)
    if (ifs.nTracks > MAX_MIDI_IN_TRACKS)
        ifs.nTracks = MAX_MIDI_IN_TRACKS;

    for (iTrack = 0, pInTrack = ifs.pTracks; iTrack < ifs.nTracks; ++iTrack, ++pInTrack)
    {
        if ((pChunkID = (ULONG*)GetInFileData(sizeof(*pChunkID))) == NULL ||
            *pChunkID!= MTrk ||
            (pChunkSize = (ULONG*)GetInFileData(sizeof(*pChunkSize))) == NULL)
        {
            CONS_Printf ( "Read error on track header.\n");
            goto Init_Cleanup;
        }
        
        iChunkSize = LONGSWAP(*pChunkSize);
        pInTrack->iTrackLen = iChunkSize;
        pInTrack->iBytesLeft = iChunkSize;
        pInTrack->pTrackData = GetInFileData(iChunkSize);
        if (pInTrack->pTrackData == NULL)
        {
            CONS_Printf ( "Read error while reading track data.\n");
            goto Init_Cleanup;
        }

#ifdef DEBUGMIDISTREAM
        CONS_Printf ("Track %d : length %d bytes\n", iTrack, iChunkSize);
        pInTrack->nTrack = iTrack;
#endif
        pInTrack->pTrackPointer = pInTrack->pTrackData;
        pInTrack->fdwTrack = 0;
        pInTrack->bRunningStatus = 0;
        
        // Handle bozo MIDI files which contain empty track chunks
        //
        if (!pInTrack->iBytesLeft)
        {
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            continue;
        }
        
        
        // We always preread the time from each track so the mixer code can
        // determine which track has the next event with a minimum of work
        //
        if (!GetTrackVDWord(pInTrack, &pInTrack->tkNextEventDue))
        {
            CONS_Printf ( "Read error while reading first delta time of track.\n");
            goto Init_Cleanup;
        }
    }
    
    ots.tkTrack = 0;
    ots.pFirst = NULL;
    ots.pLast = NULL;
    
    fRet = TRUE;
    
Init_Cleanup:
    if (!fRet)
        Cleanup();
    
    return fRet;
}


// -------------
// GetInFileData
//
// Gets the requested number of UBYTEs of data from the input file and returns
// a pointer to them.
// 
// Returns a pointer to the data or NULL if we'd read more than is
// there.
// -------------
static UBYTE* GetInFileData(LONG iBytesToGet)
{
    UBYTE*  pRet;
    if (ifs.iBytesLeft < iBytesToGet)       // requested more UBYTEs than there are left
        return NULL;
    pRet = ifs.pFilePointer;                // pointer to requested data
    ifs.iBytesLeft -= iBytesToGet;
    ifs.pFilePointer += iBytesToGet;
    return pRet;
}


// -------
// Cleanup
//
// Free anything we ever allocated
// -------
static void Cleanup(void)
{
    PSTREAMBUF   pCurr;
    PSTREAMBUF   pNext;
    
    if (hInFile != INVALID_HANDLE_VALUE)    
        CloseHandle(hInFile);
    if (hOutFile != INVALID_HANDLE_VALUE)   
        CloseHandle(hOutFile);
    if (ifs.pFile)
        GlobalFreePtr(ifs.pFile);
    /* faB: made pTracks static
    if (ifs.pTracks)                                                  
        GlobalFreePtr(ifs.pTracks);
    */

    pCurr = ots.pFirst;
    while (pCurr)
    {
        pNext = pCurr->pNext;
        GlobalFreePtr(pCurr);
        pCurr = pNext;
    }
}


// --------------
// BuildNewTracks
//
// This is where the actual work gets done.
//
// Until all tracks are done,
//  Scan the tracks to find the next due event
//  Figure out where the event belongs in the new mapping
//  Put it there
// Add end of track metas to all new tracks that now have any data
//
// Return TRUE on success
// Prints its own error message if something goes wrong
// --------------
static BOOL BuildNewTracks(void)
{
    INTRACKSTATE*   pInTrack;
    INTRACKSTATE*   pInTrackFound;
    UINT            idx;
    DWORD           tkNext;
    TEMPEVENT       me;
    
    for(;;)
    {
        // Find nearest event due
        //
        pInTrackFound = NULL;
        tkNext = 0xFFFFFFFFL;
        
        for (idx = 0, pInTrack = ifs.pTracks; idx < ifs.nTracks; ++idx, ++pInTrack) {
            if ( (!(pInTrack->fdwTrack & ITS_F_ENDOFTRK)) && (pInTrack->tkNextEventDue < tkNext))
            {
                tkNext = pInTrack->tkNextEventDue;
                pInTrackFound = pInTrack;
            }
        }
        
        // None found? We must be done
        //
        if (!pInTrackFound)
            break;
        
        // Ok, get the event header from that track
        //
        if (!GetTrackEvent(pInTrackFound, &me))
        {
            CONS_Printf ( "MIDI file is corrupt!\n");
            return FALSE;
        }
        
        // Don't add end of track event 'til we're done
        //
        if (me.abEvent[0] == MIDI_META && me.abEvent[1] == MIDI_META_EOT)
            continue;
        
        if (!AddEventToStream(&me))
        {
            CONS_Printf ( "Out of memory building tracks.\n");
            return FALSE;
        }
    }       
    
    return TRUE;
}


//
// WriteStreamBuffers
//
// Write stream buffers into an MDS file (RIFF MIDS format)
//
// Return TRUE on success
// Prints its own error message if something goes wrong
//
#define FOURCC_MIDS mmioFOURCC('M','I','D','S')
#define FOURCC_fmt  mmioFOURCC('f','m','t',' ')
#define FOURCC_data mmioFOURCC('d','a','t','a')

static BOOL WriteStreamBuffers(void)
{
    LONG                   cbFmt;
    LONG                   cbData;
    LONG                   cbRiff;
    PSTREAMBUF              psb;
    FOURCC                  fcc;
    FOURCC                  fcc2;
    MIDSFMT                 fmt;
    MIDSBUFFER              data;
    LONG                   cb;
    LONG                   cBuffers;
    
    // Walk buffer list to find entire size of data chunk
    //
    cbData = sizeof(cBuffers);
    cBuffers = 0;
    for (psb = ots.pFirst; psb; psb = psb->pNext, ++cBuffers)
        cbData += sizeof(MIDSBUFFER) + (CB_STREAMBUF - psb->iBytesLeft);
    
    cbFmt = sizeof(fmt);
    
    // Figure size of entire RIFF chunk
    //
    cbRiff = 
        sizeof(FOURCC) +                        // RIFF form type ('MIDS')
        sizeof(FOURCC) +                        // Format chunk type ('fmt ')
        sizeof(LONG) +                         // Format chunk size
        sizeof(MIDSFMT) +                       // Format chunk contents
        sizeof(FOURCC) +                        // Data chunk type ('data')
        sizeof(LONG) +                         // Data chunk size
        cbData;                                         // Data chunk contents
    
    fcc = FOURCC_RIFF;
    fcc2 = FOURCC_MIDS;
    if ((!WriteFile(hOutFile, &fcc, sizeof(fcc), &cb, NULL)) ||
        (!WriteFile(hOutFile, &cbRiff, sizeof(cbRiff), &cb, NULL)) ||
        (!WriteFile(hOutFile, &fcc2, sizeof(fcc2), &cb, NULL)))
        return FALSE;
    
    
    fmt.dwTimeFormat        = ifs.dwTimeDivision;
    fmt.cbMaxBuffer         = CB_STREAMBUF;
    fmt.dwFlags             = 0;
    if (fCompress)
        fmt.dwFlags             |= MDS_F_NOSTREAMID;
    
    fcc = FOURCC_fmt;
    if ((!WriteFile(hOutFile, &fcc, sizeof(fcc), &cb, NULL)) ||
        (!WriteFile(hOutFile, &cbFmt, sizeof(cbFmt), &cb, NULL)) ||
        (!WriteFile(hOutFile, &fmt, sizeof(fmt), &cb, NULL)))
        return FALSE;
    
    fcc = FOURCC_data;
    if ((!WriteFile(hOutFile, &fcc, sizeof(fcc), &cb, NULL)) ||
        (!WriteFile(hOutFile, &cbData, sizeof(cbData), &cb, NULL)) ||
        (!WriteFile(hOutFile, &cBuffers, sizeof(cBuffers), &cb, NULL)))
        return FALSE;
    
    for (psb = ots.pFirst; psb; psb = psb->pNext)
    {
        data.tkStart = psb->tkStart;
        data.cbBuffer = CB_STREAMBUF - psb->iBytesLeft;
        
        if ((!WriteFile(hOutFile, &data, sizeof(data), &cb, NULL)) ||
            (!WriteFile(hOutFile, psb->pBuffer, data.cbBuffer, &cb, NULL)))
            return FALSE;
    }
    
    return TRUE;
}


// --------------
// GetTrackVDWord
// Attempts to parse a variable length LONG from the given track. A VDWord
// in a MIDI file
//  (a) is in lo-hi format 
//  (b) has the high bit set on every UBYTE except the last
// Returns the LONG in *lpdw and TRUE on success; else
// FALSE if we hit end of track first. Sets ITS_F_ENDOFTRK
// if we hit end of track.
// --------------
static BOOL GetTrackVDWord(INTRACKSTATE* pInTrack, ULONG* lpdw)
{
    BYTE    b;
    LONG    dw = 0;
    
    if (pInTrack->fdwTrack & ITS_F_ENDOFTRK)
        return FALSE;
    
    do
    {
        if (!pInTrack->iBytesLeft)
        {
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            return FALSE;
        }
        
        b = *pInTrack->pTrackPointer++;
        --pInTrack->iBytesLeft;
        
        dw = (dw << 7) | (b & 0x7F);
    } while (b & 0x80);
    
    *lpdw = dw;
    
    return TRUE;
}


// -------------
// GetTrackEvent
//
// Fills in the event struct with the next event from the track
//
// pMe->tkEvent will contain the absolute tick time of the event
// pMe->abEvent[0] will contain
//  MIDI_META if the event is a meta event;
//   in this case pMe->abEvent[1] will contain the meta class
//  MIDI_SYSEX or MIDI_SYSEXEND if the event is a SysEx event
//  Otherwise, the event is a channel message and pMe->abEvent[1]
//   and pMe->abEvent[2] will contain the rest of the event.
//
// pMe->dwEventLength will contain
//  The total length of the channel message in pMe->abEvent if
//   the event is a channel message
//  The total length of the paramter data pointed to by
//   pMe->pEvent otherwise
//
// pMe->pEvent will point at any additional paramters if the 
//  event is a SysEx or meta event with non-zero length; else
//  it will contain NULL
//
// Returns TRUE on success or FALSE on any kind of parse error
// Prints its own error message ONLY in the debug version
//
// Maintains the state of the input track (i.e. pInTrack->iBytesLeft,
// pInTrack->pTrackPointers, and pInTrack->bRunningStatus).
// -------------
static BOOL GetTrackEvent(INTRACKSTATE* pInTrack, TEMPEVENT *pMe)
{
    BYTE     b;
    LONG     dwEventLength;
    
    // Clear out the temporary event structure to get rid of old data...
    ZeroMemory( pMe, sizeof(TEMPEVENT));
    
    // Already at end of track? There's nothing to read.
    //
    if ((pInTrack->fdwTrack & ITS_F_ENDOFTRK) || !pInTrack->iBytesLeft)
        return FALSE;
    
    // Get the first UBYTE, which determines the type of event.
    //
    b = *pInTrack->pTrackPointer++;
    --pInTrack->iBytesLeft;
    
    // If the high bit is not set, then this is a channel message
    // which uses the status UBYTE from the last channel message
    // we saw. NOTE: We do not clear running status across SysEx or
    // meta events even though the spec says to because there are
    // actually files out there which contain that sequence of data.
    //
    if (!(b & 0x80))
    {
        // No previous status UBYTE? We're hosed.
        //
        if (!pInTrack->bRunningStatus)
        {
            TRACKERR(pInTrack, gteBadRunStat);
            return FALSE;
        }
        
        //faB: the last midi command issued on that track
        pMe->abEvent[0] = pInTrack->bRunningStatus;
        pMe->abEvent[1] = b;        // the data !
        
        b = pMe->abEvent[0] & 0xF0;
        pMe->dwEventLength = 2;       //2 data bytes
        
        // Only program change and channel pressure events are 2 UBYTEs long;
        // the rest are 3 and need another UBYTE
        //
        if ((b != MIDI_PRGMCHANGE) && (b != MIDI_CHANPRESS))
        {
            if (!pInTrack->iBytesLeft)
            {
                TRACKERR(pInTrack, gteRunStatMsgTrunc);
                pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
                return FALSE;
            }
            
            pMe->abEvent[2] = *pInTrack->pTrackPointer++;
            --pInTrack->iBytesLeft;
            ++pMe->dwEventLength;
        }
    }
    else if ((b & 0xF0) != MIDI_SYSEX)
    {
        // Not running status, not in SysEx range - must be
        // normal channel message (0x80-0xEF)
        //
        pMe->abEvent[0] = b;
        pInTrack->bRunningStatus = b;
        
        // Strip off channel and just keep message type
        //
        b &= 0xF0;
        
        dwEventLength = (b == MIDI_PRGMCHANGE || b == MIDI_CHANPRESS) ? 1 : 2;
        pMe->dwEventLength = dwEventLength + 1;
        
        if (pInTrack->iBytesLeft < dwEventLength)
        {
            TRACKERR(pInTrack, gteChanMsgTrunc);
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            return FALSE;
        }
        
        pMe->abEvent[1] = *pInTrack->pTrackPointer++;
        if (dwEventLength == 2)
            pMe->abEvent[2] = *pInTrack->pTrackPointer++;
        
        pInTrack->iBytesLeft -= dwEventLength;
    } 
    else if (b == MIDI_SYSEX || b == MIDI_SYSEXEND)
    {
        // One of the SysEx types. (They are the same as far as we're concerned;
        // there is only a semantic difference in how the data would actually
        // get sent when the file is played. We must take care to put the correct
        // event type back on the output track, however.)
        //
        // Parse the general format of:
        //  BYTE        bEvent (MIDI_SYSEX or MIDI_SYSEXEND)
        //  VLONG      cbParms
        //  BYTE        abParms[cbParms]
        //
        pMe->abEvent[0] = b;
        if (!GetTrackVDWord(pInTrack, &pMe->dwEventLength))
        {
            TRACKERR(pInTrack, gteSysExLenTrunc);
            return FALSE;
        }
        
        if (pInTrack->iBytesLeft < pMe->dwEventLength)
        {
            TRACKERR(pInTrack, gteSysExTrunc);
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            return FALSE;
        }
        
        pMe->pEvent = pInTrack->pTrackPointer;
        pInTrack->pTrackPointer += pMe->dwEventLength;
        pInTrack->iBytesLeft -= pMe->dwEventLength;
    } 
    else if (b == MIDI_META)
    {
        // It's a meta event. Parse the general form:
        //  BYTE        bEvent  (MIDI_META)
        //      BYTE    bClass
        //  VLONG      cbParms
        //      BYTE    abParms[cbParms]
        //
        pMe->abEvent[0] = b;
        
        if (!pInTrack->iBytesLeft)
        {
            TRACKERR(pInTrack, gteMetaNoClass);
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            return FALSE;
        }
        
        pMe->abEvent[1] = *pInTrack->pTrackPointer++;
        --pInTrack->iBytesLeft;
        
        if (!GetTrackVDWord(pInTrack, &pMe->dwEventLength))
        {       
            TRACKERR(pInTrack, gteMetaLenTrunc);
            return FALSE;
        }
        
        // NOTE: Perfectly valid to have a meta with no data
        // In this case, dwEventLength == 0 and pEvent == NULL
        //
        if (pMe->dwEventLength)
        {               
            if (pInTrack->iBytesLeft < pMe->dwEventLength)
            {
                TRACKERR(pInTrack, gteMetaTrunc);
                pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
                return FALSE;
            }
            
            pMe->pEvent = pInTrack->pTrackPointer;
            pInTrack->pTrackPointer += pMe->dwEventLength;
            pInTrack->iBytesLeft -= pMe->dwEventLength;
        }
        
        if (pMe->abEvent[1] == MIDI_META_EOT)
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
    }
    else
    {
        // Messages in this range are system messages and aren't supposed to
        // be in a normal MIDI file. If they are, we've misparsed or the
        // authoring software is stpuid.
        //
#ifdef DEBUGMIDISTREAM
        CONS_Printf ("System message not supposed to be in MIDI file..\n");
#endif
        return FALSE;
    }
    
    // Event time was already stored as the current track time
    //
    pMe->tkEvent = pInTrack->tkNextEventDue;
    
    // Now update to the next event time. The code above MUST properly
    // maintain the end of track flag in case the end of track meta is
    // missing. 
    //
    if (!(pInTrack->fdwTrack & ITS_F_ENDOFTRK))
    {
        LONG                           tkDelta;
        
        if (!GetTrackVDWord(pInTrack, &tkDelta))
            return FALSE;
        
        pInTrack->tkNextEventDue += tkDelta;
    }
    
    return TRUE;
}


// ----------------
// AddEventToStream
//
// Put the given event onto the given output track.
// pMe must point to an event filled out in accordance with the
// description given in GetTrackEvent
//
// Returns TRUE on sucess or FALSE if we're out of memory
// ----------------
static BOOL AddEventToStream(TEMPEVENT *pMe)
{
    PLONG   pdw;
    LONG    tkNow, tkDelta;
    UINT    cdw;

    tkNow = ots.tkTrack;

    // Delta time is absolute event time minus absolute time
    // already gone by on this track
    //
    tkDelta = pMe->tkEvent - ots.tkTrack;
    
    // Event time is now current time on this track
    //
    ots.tkTrack = pMe->tkEvent;
    
    if (pMe->abEvent[0] < MIDI_SYSEX)
    {
        // Channel message. We know how long it is, just copy it. Need 3 LONG's: delta-t, 
        // stream-ID, event
        // 
        // TODO: Compress with running status
        //
        
        cdw = (fCompress ? 2 : 3);
        if (NULL == (pdw = (PLONG)GetOutStreamBytes(tkNow, cdw * sizeof(LONG), 3 * sizeof(LONG))))
            return FALSE;
        
        *pdw++ = tkDelta;
        if (!fCompress)
            *pdw++ = 0;
        *pdw =  (pMe->abEvent[0]) |
            (((LONG)pMe->abEvent[1]) << 8) |
            (((LONG)pMe->abEvent[2]) << 16) |
            MIDS_SHORTMSG;
    }
    else if (pMe->abEvent[0] == MIDI_SYSEX || pMe->abEvent[0] == MIDI_SYSEXEND)
    {
        CONS_Printf ( "NOTE: Ignoring SysEx for now.\n");
    }
    else
    {
        // Better be a meta event.
        //  BYTE                bEvent
        //  BYTE                bClass
        //      VLONG          cbParms
        //      BYTE            abParms[cbParms]
        //
        if (!(pMe->abEvent[0] == MIDI_META))
        {
            CONS_Printf ("Mid2Stream: error1\n");
            return FALSE;
        }
        
        // The only meta-event we care about is change tempo
        //
        if (pMe->abEvent[1] != MIDI_META_TEMPO)
            return TRUE;
        
        if (!(pMe->dwEventLength == 3))
        {
            CONS_Printf ("Mid2Stream: error2\n");
            return FALSE;
        }
        
        cdw = (fCompress ? 2 : 3);
        pdw = (PLONG)GetOutStreamBytes(tkNow, cdw * sizeof(LONG), 3 * sizeof(LONG));
        if (NULL == pdw)
            return FALSE;
        
        *pdw++ = tkDelta;
        if (!fCompress)
            *pdw++ = (LONG)-1;
        *pdw =  (pMe->pEvent[2]) |
            (((LONG)pMe->pEvent[1]) << 8) |
            (((LONG)pMe->pEvent[0]) << 16) |
            MIDS_TEMPO;
    }
    
    return TRUE;    
}


// -----------------
// GetOutStreamBytes
//
// This function performs the memory management and pseudo-file I/O for output
// tracks.
// 
// We build a linked list of stream buffers as they would exist if they were
// about to be played. Each buffer is CB_STREAMBUF UBYTEs long maximum. They are
// filled as full as possible; events are not allowed to cross buffers.
//
// Returns a pointer to the number of requested UBYTEs or NULL if we're out of memory
// -----------------
static UBYTE* GetOutStreamBytes(LONG tkNow, LONG cbNeeded, LONG cbUncompressed)
{
    UBYTE* pb;
    
    // Round request up to the next LONG boundry. This aligns the final output buffer correctly
    // and allows the above routines to deal with UBYTE-aligned data
    //
    cbNeeded = (cbNeeded + 3) & ~3;
    cbUncompressed = (cbUncompressed + 3) & ~3;
    
    if (!(cbUncompressed >= cbNeeded))
    {
        CONS_Printf ("GetOutStreamBytes: error\n");
        return NULL;
    }
    
    if (NULL == ots.pLast || cbUncompressed > ots.pLast->iBytesLeftUncompressed)
    {
        PSTREAMBUF pNew;
        
        pNew = GlobalAllocPtr(GHND, sizeof(*pNew) + CB_STREAMBUF);
        if (NULL == pNew)
            return NULL;
        
        pNew->pBuffer           = (UBYTE*)(pNew + 1);   //skip PSTRAMBUF struct header..
        pNew->tkStart           = tkNow;
        pNew->pbNextEvent       = pNew->pBuffer;
        pNew->iBytesLeft        = CB_STREAMBUF;
        pNew->iBytesLeftUncompressed = CB_STREAMBUF;
        pNew->pNext             = NULL;
        
        if (!ots.pLast)
        {
            ots.pFirst = pNew;
            ots.pLast  = pNew;
        }
        else
        {
            ots.pLast->pNext = pNew;
            ots.pLast        = pNew;
        }
    }
    
    // If there's STILL not enough room for the requested block, then an event is bigger than 
    // the buffer size -- this is unacceptable. 
    //
    if (cbNeeded > ots.pLast->iBytesLeft)
    {
        CONS_Printf ( "NOTE: An event requested %lu UBYTEs of memory; the\n", cbNeeded);
        CONS_Printf ( "      maximum configured buffer size is %lu.\n", (LONG)CB_STREAMBUF);
        
        return NULL;
    }
    
    pb = ots.pLast->pbNextEvent;
    
    ots.pLast->pbNextEvent += cbNeeded;
    ots.pLast->iBytesLeft -= cbNeeded;
    ots.pLast->iBytesLeftUncompressed -= cbUncompressed;
    
    return pb;
}

#ifdef DEBUGMIDISTREAM
static void ShowTrackError(INTRACKSTATE* pInTrack, char* szErr)
{
    unsigned char* data;
    int i;

    CONS_Printf ( "Track %u: %s\n", pInTrack->nTrack, szErr);
    CONS_Printf ( "Track offset %lu\n", (LONG)(pInTrack->pTrackPointer - pInTrack->pTrackData));
    CONS_Printf ( "Track total %lu  Track left %lu\n", pInTrack->iTrackLen, pInTrack->iBytesLeft);

    CONS_Printf (" Midi header: ");
    data = ifs.pFile;
    for (i = 0; i < 6; i++)
    {
        CONS_Printf ("%02x ", data[i]);
    }
    
    CONS_Printf ( "Track data: ");

    data = pInTrack->pTrackData;
    for (i = 0; i < 512; i++)
    {
        CONS_Printf ("%02x ", data[i]);
    }

}
#endif

#ifndef LEGACY
// if LEGACY is not defined, the stand-alone version will print out error messages to stderr
void CONS_Printf (char *fmt, ...)
{
    va_list     argptr;
    char        txt[512];

    va_start (argptr,fmt);
    vsprintf (txt,fmt,argptr);
    va_end   (argptr);

    fprintf  (stderr,"%s", txt);
}
#endif

// ==========================================================================
//                                 MIDI STREAM PLAYBACK (called by win_snd.c)
// ==========================================================================
// the following code is used with Legacy (not in stand-alone)
#ifdef LEGACY

static  DWORD   dwBufferTickLength;
        DWORD   dwProgressBytes;
// win_snd.c
extern  BOOL    bMidiLooped;


// --------------------------
// Mid2StreamConverterCleanup
// Free whatever was allocated before exiting program
// --------------------------
void Mid2StreamConverterCleanup (void)
{
    // hmm.. nothing to clean up.. since I made the INTRACKSTATE's static

    /* faB: made pTracks static
    if (ifs.pTracks)                                                  
        GlobalFreePtr(ifs.pTracks);
    */
}


// -----------------------
// Mid2StreamConverterInit
// 
// Validate the input file structure
// Allocate the input track structures and initialize them (faB: now STATIC)
// Initialize the output track structures
//
// Return TRUE on success
// -----------------------
BOOL Mid2StreamConverterInit( UBYTE* pMidiData, ULONG iMidiSize )
{
    BOOL            fRet = TRUE;
    ULONG*          pChunkID;
    ULONG*          pChunkSize;
    LONG            iChunkSize;
    MIDIFILEHDR*    pHeader;
    INTRACKSTATE*   pInTrack;
    UINT            iTrack;

    tkCurrentTime = 0;

    // Initialize things we'll try to free later if we fail
    ZeroMemory( &ifs, sizeof(INFILESTATE));
    //ifs.pTracks = NULL;   //faB: now static

    // Set up to read from the memory buffer. Read and validate
    // - MThd header
    // - size of file header chunk
    // - file header itself
    //      
    ifs.FileSize = iMidiSize;
    ifs.pFile = pMidiData;
    ifs.iBytesLeft = ifs.FileSize;
    ifs.pFilePointer = ifs.pFile;

#ifdef DEBUGMIDISTREAM
    CONS_Printf ("Midi file size: %d\n", iMidiSize);
#endif

    // note: midi header size should always be 6
    if ((pChunkID = (ULONG*)GetInFileData(sizeof(*pChunkID))) == NULL ||
        *pChunkID != MThd ||
        (pChunkSize = (ULONG*)GetInFileData(sizeof(*pChunkSize))) == NULL ||
        (iChunkSize = LONGSWAP(*pChunkSize)) < sizeof(MIDIFILEHDR) ||
        (pHeader = (MIDIFILEHDR*)GetInFileData(iChunkSize)) == NULL )
    {
        CONS_Printf ( "Read error on MIDI header.\n");
        goto Init_Cleanup;
    }

    ifs.dwFormat = (LONG)WORDSWAP(pHeader->wFormat);
    ifs.nTracks   = (LONG)WORDSWAP(pHeader->nTracks);
    ifs.dwTimeDivision = (LONG)WORDSWAP(pHeader->wTimeDivision);

#ifdef DEBUGMIDISTREAM
    CONS_Printf ("MIDI Header:\n"
                   "------------\n"
                   "format: %d\n"
                   "number of tracks: %d\n"
                   "time division: %d\n", ifs.dwFormat, ifs.nTracks, ifs.dwTimeDivision);
#endif

    /* faB: made static
    ifs.pTracks = (INTRACKSTATE*)GlobalAllocPtr(GPTR, ifs.nTracks*sizeof(INTRACKSTATE));
    if (ifs.pTracks==NULL)
    {
        CONS_Printf ( "Out of memory.\n");
        goto Init_Cleanup;
    }
    */

    // faB: made it static, but don't quit if there are more tracks, just skip'em
    if (ifs.nTracks > MAX_MIDI_IN_TRACKS)
        ifs.nTracks = MAX_MIDI_IN_TRACKS;

    for (iTrack = 0, pInTrack = ifs.pTracks; iTrack < ifs.nTracks; ++iTrack, ++pInTrack)
    {
        if ((pChunkID = (ULONG*)GetInFileData(sizeof(*pChunkID))) == NULL ||
            *pChunkID!= MTrk ||
            (pChunkSize = (ULONG*)GetInFileData(sizeof(*pChunkSize))) == NULL)
        {
            CONS_Printf ( "Read error on track header.\n");
            goto Init_Cleanup;
        }
        
        iChunkSize = LONGSWAP(*pChunkSize);
        pInTrack->iTrackLen = iChunkSize;       // Total track length
        pInTrack->iBytesLeft = iChunkSize;
        pInTrack->pTrackData = GetInFileData(iChunkSize);
        if (pInTrack->pTrackData == NULL)
        {
            CONS_Printf ( "Read error while reading track data.\n");
            goto Init_Cleanup;
        }

#ifdef DEBUGMIDISTREAM
        CONS_Printf ("Track %d : length %d bytes\n", iTrack, iChunkSize);
        pInTrack->nTrack = iTrack;
#endif
        // Setup pointer to the current position in the track
        pInTrack->pTrackPointer = pInTrack->pTrackData;

        pInTrack->fdwTrack = 0;
        pInTrack->bRunningStatus = BAD_MIDI_FIX;
        pInTrack->tkNextEventDue = 0;

        // Handle bozo MIDI files which contain empty track chunks
        if (!pInTrack->iBytesLeft)
        {
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            continue;
        }

        // We always preread the time from each track so the mixer code can
        // determine which track has the next event with a minimum of work
        if (!GetTrackVDWord(pInTrack, &pInTrack->tkNextEventDue))
        {
            CONS_Printf ( "Read error while reading first delta time of track.\n");
            goto Init_Cleanup;
        }
    }
    // End of track initialization code
    
    fRet = FALSE;

Init_Cleanup:
    if( fRet )
        Mid2StreamConverterCleanup();

    return( fRet );
}


// ----------------------
// AddEventToStreamBuffer
//
// Put the given event into the given stream buffer at the given location
// pMe must point to an event filled out in accordance with the
// description given in GetTrackEvent
//
// Returns FALSE on sucess or TRUE on an error condition
// Handles its own error notification by displaying to the appropriate
// output device (either our debugging window, or the screen).
// ----------------------
static int AddEventToStreamBuffer( PTEMPEVENT pMe, CONVERTINFO *lpciInfo )
{
    DWORD       tkNow, tkDelta;
    MIDIEVENT   *pmeEvent;

    pmeEvent = (MIDIEVENT *)( lpciInfo->mhBuffer.lpData
                                                + lpciInfo->dwStartOffset
                                                + lpciInfo->dwBytesRecorded );

    // When we see a new, empty buffer, set the start time on it...
    if( !lpciInfo->dwBytesRecorded )
        lpciInfo->tkStart = tkCurrentTime;

    // Use the above set start time to figure out how much longer we should fill
    // this buffer before officially declaring it as "full"
    if( tkCurrentTime - lpciInfo->tkStart > dwBufferTickLength )
    {
            if( lpciInfo->bTimesUp )
            {
                lpciInfo->bTimesUp = FALSE;
                return( CONVERTERR_BUFFERFULL );
            }
            else
                lpciInfo->bTimesUp = TRUE;
    }

    tkNow = tkCurrentTime;

    // Delta time is absolute event time minus absolute time
    // already gone by on this track
    tkDelta = pMe->tkEvent - tkCurrentTime;

    // Event time is now current time on this track
    tkCurrentTime = pMe->tkEvent;

    if( pMe->abEvent[0] < MIDI_SYSEX )
        {
            // Channel message. We know how long it is, just copy it.
            // Need 3 DWORD's: delta-t, stream-ID, event
            if( lpciInfo->dwMaxLength-lpciInfo->dwBytesRecorded < 3*sizeof(DWORD))
            {
                // Cleanup from our write operation
                return( CONVERTERR_BUFFERFULL );
            }

            pmeEvent->dwDeltaTime = tkDelta;
            pmeEvent->dwStreamID = 0;
            pmeEvent->dwEvent = ( pMe->abEvent[0] )
                                | (((DWORD)pMe->abEvent[1] ) << 8 )
                                | (((DWORD)pMe->abEvent[2] ) << 16 )
                                | MEVT_F_SHORT;

        //faB: control the volume with our own volume percentage
        if((( pMe->abEvent[0] & 0xF0) == MIDI_CTRLCHANGE ) &&
                ( pMe->abEvent[1] == MIDICTRL_VOLUME ))
            {
                // If this is a volume change, generate a callback so we can grab
                // the new volume for our cache
                pmeEvent->dwEvent |= MEVT_F_CALLBACK;
            }
        lpciInfo->dwBytesRecorded += 3 *sizeof(DWORD);
        }
    else if(( pMe->abEvent[0] == MIDI_SYSEX ) ||
                ( pMe->abEvent[0] == MIDI_SYSEXEND ))
        {
        CONS_Printf ( "NOTE: Ignoring SysEx for now.\n");
        }
    else
        {
        DWORD   dwCurrentTempo;
        
        // Better be a meta event.
            //  BYTE        byEvent
            //  BYTE        byEventType
            //  VDWORD      dwEventLength
            //  BYTE        pLongEventData[dwEventLength]
            //
        if (!(pMe->abEvent[0] == MIDI_META))
        {
            I_Error ("AddEventToStreamBuffer: not a META event\n");
        }

        // The only meta-event we care about is change tempo
            //
        if (pMe->abEvent[1] != MIDI_META_TEMPO)
                return( CONVERTERR_METASKIP );
        
            // We should have three bytes of parameter data...
        if (!(pMe->dwEventLength == 3))
        {
            I_Error ("AddEventToStreamBuffer: dwEventLength <> 3\n");
        }

            // Need 3 DWORD's: delta-t, stream-ID, event data
            if( lpciInfo->dwMaxLength - lpciInfo->dwBytesRecorded < 3 *sizeof(DWORD))
            {
                return( CONVERTERR_BUFFERFULL );
            }

            pmeEvent->dwDeltaTime = tkDelta;
            pmeEvent->dwStreamID = 0;
            // Note: this is backwards from above because we're converting a single
            //       data value from hi-lo to lo-hi format...
            pmeEvent->dwEvent = ( pMe->pEvent[2] )
                                        | (((DWORD)pMe->pEvent[1] ) << 8 )
                                        | (((DWORD)pMe->pEvent[0] ) << 16 );

        dwCurrentTempo = pmeEvent->dwEvent;
            pmeEvent->dwEvent |= (((DWORD)MEVT_TEMPO ) << 24 ) | MEVT_F_SHORT;

            dwBufferTickLength = ( ifs.dwTimeDivision * 1000 * BUFFER_TIME_LENGTH ) / dwCurrentTempo;
#ifdef DEBUGMIDISTREAM
            CONS_Printf("dwBufferTickLength = %lu", dwBufferTickLength );
#endif
            lpciInfo->dwBytesRecorded += 3 *sizeof(DWORD);
        }

    return( FALSE );
}


// -------------------------
// Mid2StreamRewindConverter
// This little function is an adaptation of the ConverterInit() code which
// resets the tracks without closing and opening the file, thus reducing the
// time it takes to loop back to the beginning when looping.
// -------------------------
static BOOL Mid2StreamRewindConverter( void )
{
    DWORD           iTrack;
    PINTRACKSTATE   pInTrack;
    BOOL    fRet;

    tkCurrentTime = 0;

    // reset to start of midi file
    ifs.iBytesLeft = ifs.FileSize;
    ifs.pFilePointer = ifs.pFile;

    for (iTrack = 0, pInTrack = ifs.pTracks; iTrack < ifs.nTracks; ++iTrack, ++pInTrack)
    {
        pInTrack->iBytesLeft = pInTrack->iTrackLen;

        // Setup pointer to the current position in the track
        pInTrack->pTrackPointer = pInTrack->pTrackData;

        pInTrack->fdwTrack = 0;
        pInTrack->bRunningStatus = BAD_MIDI_FIX;
        pInTrack->tkNextEventDue = 0;

        // Handle bozo MIDI files which contain empty track chunks
        if (!pInTrack->iBytesLeft)
        {
            pInTrack->fdwTrack |= ITS_F_ENDOFTRK;
            continue;
        }

        // We always preread the time from each track so the mixer code can
        // determine which track has the next event with a minimum of work
        if (!GetTrackVDWord(pInTrack, &pInTrack->tkNextEventDue))
        {
            CONS_Printf ( "Read error while reading first delta time of track.\n");
            goto Rewind_Cleanup;
        }
    }
    // End of track initialization code

    fRet = FALSE;

Rewind_Cleanup:
    if( fRet )
        return( TRUE );

    return( FALSE );
}


// -------------------------
// Mid2StreamConvertToBuffer
//
// This function converts MIDI data from the track buffers setup by a previous
// call to Mid2StreamConverterInit().  It will convert data until an error is
// encountered or the output buffer has been filled with as much event data
// as possible, not to exceed dwMaxLength. This function can take a couple
// bit flags, passed through dwFlags. Information about the success/failure
// of this operation and the number of output bytes actually converted will
// be returned in the CONVERTINFO structure pointed at by lpciInfo.
// -------------------------
int Mid2StreamConvertToBuffer( DWORD dwFlags, LPCONVERTINFO lpciInfo )
{
    static INTRACKSTATE *pInTrack, *pInTrackFound;
    static DWORD        dwStatus;
    static DWORD        tkNext;
    static TEMPEVENT    teTemp;

    int     nChkErr;
    DWORD   idx;

    lpciInfo->dwBytesRecorded = 0;

    if( dwFlags & CONVERTF_RESET )
        {
            dwProgressBytes = 0;
            dwStatus = 0;
            ZeroMemory( &teTemp, sizeof(TEMPEVENT));
            pInTrack = pInTrackFound = NULL;
        }

    // If we were already done, then return with a warning...
    if( dwStatus & CONVERTF_STATUS_DONE )
        {
            if( bMidiLooped )
            {
                Mid2StreamRewindConverter();
                dwProgressBytes = 0;
                dwStatus = 0;
            }
            else
                return( CONVERTERR_DONE );
        }
    // The caller is asking us to continue, but we're already hosed because we
    // previously identified something as corrupt, so complain louder this time.
    else if( dwStatus & CONVERTF_STATUS_STUCK )
        {
        return( CONVERTERR_STUCK );
        }
    else if( dwStatus & CONVERTF_STATUS_GOTEVENT )
        {
            // Turn off this bit flag
        dwStatus ^= CONVERTF_STATUS_GOTEVENT;
        
        // Don't add end of track event 'til we're done
        //
        if( teTemp.abEvent[0] == MIDI_META &&
            teTemp.abEvent[1] == MIDI_META_EOT )
        {
        }
        else if(( nChkErr = AddEventToStreamBuffer( &teTemp, lpciInfo ))
                != CONVERTERR_NOERROR )
        {
            if( nChkErr == CONVERTERR_BUFFERFULL )
            {
                // Do some processing and tell caller that this buffer's full
                dwStatus |= CONVERTF_STATUS_GOTEVENT;
                return( CONVERTERR_NOERROR );
            }
            else if( nChkErr == CONVERTERR_METASKIP )
            {
                // We skip by all meta events that aren't tempo changes...
            }
            else
            {
                I_Error ( "Unable to add event to stream buffer." );
            }
        }
    }
    
    for( ; ; )
    {
        pInTrackFound = NULL;
        tkNext = 0xFFFFFFFFL;

        // Find nearest event due
        //
        for (idx = 0, pInTrack = ifs.pTracks; idx < ifs.nTracks; ++idx, ++pInTrack) {
            if ( (!(pInTrack->fdwTrack & ITS_F_ENDOFTRK)) && (pInTrack->tkNextEventDue < tkNext))
            {
                tkNext = pInTrack->tkNextEventDue;
                pInTrackFound = pInTrack;
            }
        }
        
        // None found?  We must be done, so return to the caller with a smile.
        //
        if (!pInTrackFound)
        {
            dwStatus |= CONVERTF_STATUS_DONE;
            // Need to set return buffer members properly
            return( CONVERTERR_NOERROR );
        }
        
        // Ok, get the event header from that track
        //
        if( !GetTrackEvent( pInTrackFound, &teTemp ))
        {
            // Warn future calls that this converter is stuck at a corrupt spot
            // and can't continue
            dwStatus |= CONVERTF_STATUS_STUCK;
            return( CONVERTERR_CORRUPT );
        }
        
        // Don't add end of track event 'til we're done
        //
        if( teTemp.abEvent[0] == MIDI_META &&
            teTemp.abEvent[1] == MIDI_META_EOT )
            continue;
        
        if(( nChkErr = AddEventToStreamBuffer( &teTemp, lpciInfo ))
            != CONVERTERR_NOERROR )
        {
            if( nChkErr == CONVERTERR_BUFFERFULL )
            {
                // Do some processing and tell somebody this buffer is full...
                dwStatus |= CONVERTF_STATUS_GOTEVENT;
                return( CONVERTERR_NOERROR );
            }
            else if( nChkErr == CONVERTERR_METASKIP )
            {
                // We skip by all meta events that aren't tempo changes...
            }
            else
            {
                I_Error( "Unable to add event to stream buffer." );
            }
        }
    }       

    return( CONVERTERR_NOERROR );
    
}

#endif
