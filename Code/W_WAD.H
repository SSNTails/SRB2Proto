
// w_wad.h : WAD I/O functions, wad resource definitions (some)


#ifndef __W_WAD__
#define __W_WAD__

typedef void GlidePatch_t;

#ifdef __GNUG__
#pragma interface
#endif

// ==============================================================
//               WAD FILE STRUCTURE DEFINITIONS
// ==============================================================


typedef long   lumpnum_t;           // 16:16 long (wad num: lump num)


// header of a wad file

typedef struct
{
    char       identification[4];   // should be "IWAD" or "PWAD"
    int        numlumps;            // how many resources
    int        infotableofs;        // the 'directory' of resources
} wadinfo_t;


// an entry of the wad directory

typedef struct
{
    int        filepos;             // file offset of the resource
    int        size;                // size of the resource
    char       name[8];             // name of the resource
} filelump_t;


// in memory : initted at game startup

typedef struct
{
    char        name[8];            // filelump_t name[]
    int         position;           // filelump_t filepos
    int         size;               // filelump_t size
} lumpinfo_t;


// =========================================================================
//                         DYNAMIC WAD LOADING
// =========================================================================

#define WADFILENUM(lump)       (lump>>16)   // wad file number in upper word
#define LUMPNUM(lump)          (lump&0xffff)    // lump number for this pwad

#define MAX_WADPATH   128
#define MAX_WADFILES  256      // Saxman - 02/13/2000
                               // maximum of wad files used at the same time
                               // (there is a max of simultaneous open files
                               // anyway, and this should be plenty)

#define lumpcache_t  void*

typedef struct wadfile_s
{
    char             filename[MAX_WADPATH+1];
    lumpinfo_t*      lumpinfo;
    lumpcache_t*     lumpcache;
    GlidePatch_t**   cache3Dfx;         // pacthes are cached in renderer's native format
    int              numlumps;          // this wad's number of resources
    int              handle;
    time_t           timestamp;         // check the timestamp in network
} wadfile_t;

extern  int          numwadfiles;
extern  wadfile_t*   wadfiles[MAX_WADFILES];


// =========================================================================

// load and add a wadfile to the active wad files, return wad file number
// (you can get wadfile_t pointer: the return value indexes wadfiles[])
int     W_LoadWadFile (char *filename);

//added 4-1-98 initmultiplefiles now return 1 if all ok 0 else
//             so that it stops with a message if a file was not found
//             but not if all is ok.
int     W_InitMultipleFiles (char** filenames);
void    W_Reload (void);

int     W_CheckNumForName (char* name);
// this one checks only in one pwad
int     W_CheckNumForNamePwad (char* name, int wadid, int startlump);
int     W_GetNumForName (char* name);

int     W_LumpLength (int lump);
//added:06-02-98: read all or a part of a lump
int     W_ReadLumpHeader (int lump, void* dest, int size);
//added:06-02-98: now calls W_ReadLumpHeader() with full lump size
void    W_ReadLump (int lump, void *dest);

void*   W_CacheLumpNum (int lump, int tag);
void*   W_CacheLumpName (char* name, int tag);

#define W_CachePatchNum(lump,tag)    W_CacheLumpNum(lump,tag)
#define W_CachePatchName(name,tag)   W_CacheLumpName(name,tag)

#endif // __W_WAD__
