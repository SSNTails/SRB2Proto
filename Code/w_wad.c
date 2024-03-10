
// w_wad.c :   Handles WAD file header, directory, lump I/O.
// added for linux 19990220 by Kin
#ifdef LINUX
#define O_BINARY 0
#endif

#include <malloc.h>
#include <fcntl.h>
#ifndef __WIN32__
#include <unistd.h>
#endif

#include "doomdef.h"
#include "doomtype.h"
#include "w_wad.h"
#include "z_zone.h"

#include "i_video.h"        //rendermode
#include "d_netfil.h"

//===========================================================================
//                                                                    GLOBALS
//===========================================================================
int          numwadfiles;               // number of active wadfiles
wadfile_t*   wadfiles[MAX_WADFILES];    // 0 to numwadfiles-1 are valid


//===========================================================================
//                                                        LUMP BASED ROUTINES
//===========================================================================

// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// If filename starts with a tilde, the file is handled
//  specially to allow map reloads.
// But: the reload feature is a fragile hack...

int                     reloadlump;
char*                   reloadname;



//  Allocate a wadfile, setup the lumpinfo (directory) and
//  lumpcache, add the wadfile to the current active wadfiles
//
//  now returns index into wadfiles[], you can get wadfile_t*
//  with:
//       wadfiles[<return value>]
//
//  return -1 in case of problem
//
int W_LoadWadFile (char *filename)
{
    int              handle;
    wadinfo_t        header;
    int              numlumps;
    filelump_t*      fileinfo;
    lumpinfo_t*      lump_p;
    lumpinfo_t*      lumpinfo;
    lumpcache_t*     lumpcache;
    GlidePatch_t**   cache3Dfx;
    wadfile_t*       wadfile;
    int              length;
    int              i;
    struct stat      bufstat;
    time_t           timestamp;
//    GlidePatch_t*    grPatch;

    //
    // check if limit of active wadfiles
    //
    if (numwadfiles>=MAX_WADFILES)
    {
        CONS_Printf ("Maximum wad files reached\n");
        return -1;
    }

    // open wad file
    if ( (handle = open (filename,O_RDONLY|O_BINARY,0666)) == -1)
    {
        nameonly(filename); // leave full path here
        if(recsearch(filename,0))
        {
            if ( (handle = open (filename,O_RDONLY|O_BINARY,0666)) == -1)
            {
                CONS_Printf ("Can't open %s\n", filename);
                return -1;
            }
        }
        else
        {
        CONS_Printf ("Couldn't open %s\n", filename);
        return -1;
    }
    }

    fstat(handle,&bufstat);
    timestamp = bufstat.st_mtime;

    //CONS_Printf (" adding %s\n",filename);

    // read the header
    read (handle, &header, sizeof(header));

    if (strncmp(header.identification,"IWAD",4))
    {
        // Homebrew levels?
        if (strncmp(header.identification,"PWAD",4))
        {
            CONS_Printf ("%s doesn't have IWAD or PWAD id\n", filename);
            return -1;
        }
        // ???modifiedgame = true;
    }
    header.numlumps = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);

    // read wad file directory
    length = header.numlumps*sizeof(filelump_t);
    fileinfo = alloca (length);
    lseek (handle, header.infotableofs, SEEK_SET);
    read (handle, fileinfo, length);

    numlumps = header.numlumps;

    // fill in lumpinfo for this wad
    lump_p = lumpinfo = Z_Malloc (numlumps*sizeof(lumpinfo_t),PU_STATIC,NULL);

    for (i=0 ; i<numlumps ; i++,lump_p++, fileinfo++)
    {
        //lump_p->handle   = handle;
        lump_p->position = LONG(fileinfo->filepos);
        lump_p->size     = LONG(fileinfo->size);
        strncpy (lump_p->name, fileinfo->name, 8);
    }

    //
    //  link wad file to search files
    //
    wadfile = Z_Malloc (sizeof (wadfile_t),PU_STATIC,NULL);
    strncpy (wadfile->filename, filename, MAX_WADPATH);
    wadfile->handle = handle;
    wadfile->numlumps = numlumps;
    wadfile->lumpinfo = lumpinfo;
    wadfile->timestamp = timestamp;

    //
    //  set up caching
    //
    length = numlumps * sizeof(lumpcache_t);
    lumpcache = Z_Malloc (length,PU_STATIC,NULL);
    if (!lumpcache)
        I_Error ("Couldn't malloc lumpcache\n");
    memset (lumpcache, 0, length);
    wadfile->lumpcache = lumpcache;

    // TODO: get rid of this for software mode
    length = numlumps * sizeof(GlidePatch_t*);
    cache3Dfx = (GlidePatch_t**) Z_Malloc (length,PU_STATIC,NULL);
    if (!cache3Dfx)
        I_Error ("Couldn't malloc cache3Dfx\n");
    memset (cache3Dfx, 0, length);
    wadfile->cache3Dfx = cache3Dfx;

    //
    //  add the wadfile
    //
    wadfiles[numwadfiles++] = wadfile;

    CONS_Printf ("Added wadfile %s (%i lumps)\n", filename, numlumps);
    return numwadfiles-1;
}


// !!!NOT CHECKED WITH NEW WAD SYSTEM
//
// W_Reload
// Flushes any of the reloadable lumps in memory
//  and reloads the directory.
//
void W_Reload (void)
{
    wadinfo_t           header;
    int                 lumpcount;
    lumpinfo_t*         lump_p;
    int                 i;
    int                 handle;
    int                 length;
    filelump_t*         fileinfo;
    lumpcache_t*        lumpcache;

    if (!reloadname)
        return;

    if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
        I_Error ("W_Reload: couldn't open %s",reloadname);

    read (handle, &header, sizeof(header));
    lumpcount = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);
    length = lumpcount*sizeof(filelump_t);
    fileinfo = alloca (length);
    lseek (handle, header.infotableofs, SEEK_SET);
    read (handle, fileinfo, length);

    // Fill in lumpinfo
    lump_p = wadfiles[reloadlump>>16]->lumpinfo + (reloadlump&0xFFFF);

    lumpcache = wadfiles[reloadlump>>16]->lumpcache;

    for (i=reloadlump ;
         i<reloadlump+lumpcount ;
         i++,lump_p++, fileinfo++)
    {
        if (lumpcache[i])
            Z_Free (lumpcache[i]);

        lump_p->position = LONG(fileinfo->filepos);
        lump_p->size = LONG(fileinfo->size);
    }

    close (handle);
}


//
// W_InitMultipleFiles
// Pass a null terminated list of files to use.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
int W_InitMultipleFiles (char** filenames)
{
    int         rc=1;

    // open all the files, load headers, and count lumps
    numwadfiles = 0;

    // will be realloced as lumps are added
    for ( ; *filenames ; filenames++)
        rc &= (W_LoadWadFile (*filenames) != -1) ? 1 : 0;

    if (!numwadfiles)
        I_Error ("W_InitMultipleFiles: no files found");

    return rc;
}


// !!!NOT CHECKED WITH NEW WAD SYSTEM
//
// W_InitFile
// Just initialize from a single file.
//
/*
void W_InitFile (char* filename)
{
    char*       names[2];

    names[0] = filename;
    names[1] = NULL;
    W_InitMultipleFiles (names);
}*/


// !!!NOT CHECKED WITH NEW WAD SYSTEM
//
// W_NumLumps
//
/*
int W_NumLumps (void)
{
    return numlumps;
}*/



//
//  W_CheckNumForName
//  Returns -1 if name not found.
//

// this is normally always false, so external pwads take precedence,
//  this is set true temporary as W_GetNumForNameFirst() is called
static boolean scanforward = false;

int W_CheckNumForName (char* name)
{
    union {
                char    s[9];
                int             x[2];
    } name8;

    int         i,j;
    int         v1;
    int         v2;
    lumpinfo_t* lump_p;

    // make the name into two integers for easy compares
    strncpy (name8.s,name,8);

    // in case the name was a fill 8 chars
    name8.s[8] = 0;

    // case insensitive
    strupr (name8.s);

    v1 = name8.x[0];
    v2 = name8.x[1];

    if (!scanforward)
    {
        //
        // scan wad files backwards so patch lump files take precedence
        //
        for (i = numwadfiles-1 ; i>=0; i--)
        {
            lump_p = wadfiles[i]->lumpinfo;

            for (j = 0; j<wadfiles[i]->numlumps; j++,lump_p++)
            {
                if (    *(int *)lump_p->name == v1
                     && *(int *)&lump_p->name[4] == v2)
                {
                    // high word is the wad file number
                    return ((i<<16) + j);
                }
            }
        }
        // not found.
        return -1;
    }

    //
    // scan wad files forward, when original wad resources
    //  must take precedence
    //
    for (i = 0; i<numwadfiles; i++)
    {
        lump_p = wadfiles[i]->lumpinfo;
        for (j = 0; j<wadfiles[i]->numlumps; j++,lump_p++)
        {
            if (    *(int *)lump_p->name == v1
                 && *(int *)&lump_p->name[4] == v2)
            {
                return ((i<<16) + j);
            }
        }
    }
    // not found.
    return -1;
}


//
//  Same as the original, but checks in one pwad only
//  wadid is a wad number
//  (Used for sprites loading)
//
//  'startlump' is the lump number to start the search
//
int W_CheckNumForNamePwad (char* name, int wadid, int startlump)
{
    union {
                char    s[9];
                int             x[2];
    } name8;

    int         i;
    int         v1;
    int         v2;
    lumpinfo_t* lump_p;

    strncpy (name8.s,name,8);
    name8.s[8] = 0;
    strupr (name8.s);

    v1 = name8.x[0];
    v2 = name8.x[1];

    //
    // scan forward
    // start at 'startlump', useful parameter when there are multiple
    //                       resources with the same name
    //
    if (startlump < wadfiles[wadid]->numlumps)
    {
        lump_p = wadfiles[wadid]->lumpinfo + startlump;
        for (i = startlump; i<wadfiles[wadid]->numlumps; i++,lump_p++)
        {
            if ( *(int *)lump_p->name == v1
                 && *(int *)&lump_p->name[4] == v2)
            {
                return ((wadid<<16)+i);
            }
        }
    }

    // not found.
    return -1;
}



//
// W_GetNumForName
//   Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (char* name)
{
    int i;

    i = W_CheckNumForName (name);

    if (i == -1)
        I_Error ("W_GetNumForName: %s not found!\n", name);

    return i;
}


//
//  W_GetNumForNameFirst : like W_GetNumForName, but scans FORWARD
//                         so it gets resources from the original wad first
//  (this is used only to get S_START for now, in r_data.c)
int W_GetNumForNameFirst (char* name)
{
    int i;

    // 3am coding.. force a scan of resource name forward, for one call
    scanforward = true;
    i = W_CheckNumForName (name);
    scanforward = false;

    if (i == -1)
        I_Error ("W_GetNumForNameFirst: %s not found!", name);

    return i;
}


//
//  W_LumpLength
//   Returns the buffer size needed to load the given lump.
//
int W_LumpLength (int lump)
{
    if (lump<0) I_Error("W_LumpLenght: lump not exist\n");

    if ((lump&0xFFFF) >= wadfiles[lump>>16]->numlumps)
        I_Error ("W_LumpLength: %i >= numlumps",lump);

    return wadfiles[lump>>16]->lumpinfo[lump&0xFFFF].size;
}



//
// W_ReadLumpHeader : read 'size' bytes of lump
//                    sometimes just the header is needed
//
//Fab:02-08-98: now returns the number of bytes read (should == size)
int  W_ReadLumpHeader ( int           lump,
                        void*         dest,
                        int           size )
{
    int         bytesread;
    lumpinfo_t* l;
    int         handle;

    if (lump<0) I_Error("W_ReadLumpHeader: lump not exist\n");

    if ((lump&0xFFFF) >= wadfiles[lump>>16]->numlumps)
        I_Error ("W_ReadLumpHeader: %i >= numlumps",lump);

    l = wadfiles[lump>>16]->lumpinfo + (lump&0xFFFF);

    // the good ole 'loading' disc icon TODO: restore it :)
    // ??? I_BeginRead ();

    // empty resource (usually markers like S_START, F_END ..)
    if (l->size==0)
        return 0;

/*    if (l->handle == -1)
    {
        // reloadable file, so use open / read / close
        if ( (handle = open (reloadname,O_RDONLY|O_BINARY,0666)) == -1)
            I_Error ("W_ReadLumpHeader: couldn't open %s",reloadname);
    }
    else*/
        handle = wadfiles[lump>>16]->handle; //l->handle;

    // 0 size means read all the lump
    if (!size)
        size = l->size;

    lseek (handle, l->position, SEEK_SET);
    bytesread = read (handle, dest, size);

    // no more : check return value if needed
    //if (c < size)
    //    I_Error ("W_ReadLumpHeader: only read %i of %i on lump %i",
    //             c,size,lump);

    /*if (l->handle == -1)
        close (handle);*/

    // ??? I_EndRead ();
    return bytesread;
}


//
//  W_ReadLump
//  Loads the lump into the given buffer,
//   which must be >= W_LumpLength().
//
//added:06-02-98: now calls W_ReadLumpHeader() with full lump size.
//                0 size means the size of the lump, see W_ReadLumpHeader
void W_ReadLump ( int           lump,
                  void*         dest )
{
    W_ReadLumpHeader (lump, dest, 0);
}


// ==========================================================================
// W_CacheLumpNum
// ==========================================================================
void* W_CacheLumpNum ( int           lump,
                       int           tag )
{
    byte*         ptr;
    lumpcache_t*  lumpcache;

    // check return value of a previous W_CheckNumForName()
    if ( (lump==-1) ||
         ((lump&0xFFFF) >= wadfiles[lump>>16]->numlumps) )
        I_Error ("W_CacheLumpNum: %i >= numlumps", lump&0xffff );

    lumpcache = wadfiles[lump>>16]->lumpcache;
    if (!lumpcache[lump&0xFFFF]) {
        // read the lump in

        //printf ("cache miss on lump %i\n",lump);
        ptr = Z_Malloc (W_LumpLength (lump), tag, &lumpcache[lump&0xFFFF]);
        W_ReadLumpHeader (lump, lumpcache[lump&0xFFFF], 0);   // read full
    }
    else {
        //printf ("cache hit on lump %i\n",lump);
        Z_ChangeTag (lumpcache[lump&0xFFFF],tag);
    }

    return lumpcache[lump&0xFFFF];
}


// ==========================================================================
// W_CacheLumpName
// ==========================================================================
void* W_CacheLumpName ( char*         name,
                        int           tag )
{
    return W_CacheLumpNum (W_GetNumForName(name), tag);
}



// ==========================================================================
//                                         CACHING OF GRAPHIC PATCH RESOURCES
// ==========================================================================

// Graphic 'patches' are loaded, and if necessary, converted into the format
// the most useful for the current rendermode. For software renderer, the
// graphic patches are kept as is. For the 3Dfx renderer, graphic patches
// are 'unpacked', and are kept into the cache in that unpacked format, the
// heap memory cache then act as a 'level 2' cache just after the graphics
// card memory.

//
// Cache a patch into heap memory, convert the patch format as necessary
//

// --------------------------------------------------------------------------
// W_Profile
// --------------------------------------------------------------------------
//
/*     --------------------- UNUSED ------------------------
int             info[2500][10];
int             profilecount;

void W_Profile (void)
{
    int         i;
    memblock_t* block;
    void*       ptr;
    char        ch;
    FILE*       f;
    int         j;
    char        name[9];


    for (i=0 ; i<numlumps ; i++)
    {
        ptr = lumpcache[i];
        if (!ptr)
        {
            ch = ' ';
            continue;
        }
        else
        {
            block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
            if (block->tag < PU_PURGELEVEL)
                ch = 'S';
            else
                ch = 'P';
        }
        info[i][profilecount] = ch;
    }
    profilecount++;

    f = fopen ("waddump.txt","w");
    name[8] = 0;

    for (i=0 ; i<numlumps ; i++)
    {
        memcpy (name,lumpinfo[i].name,8);

        for (j=0 ; j<8 ; j++)
            if (!name[j])
                break;

        for ( ; j<8 ; j++)
            name[j] = ' ';

        fprintf (f,"%s ",name);

        for (j=0 ; j<profilecount ; j++)
            fprintf (f,"    %c",info[i][j]);

        fprintf (f,"\n");
    }
    fclose (f);
}

// --------------------------------------------------------------------------
// W_AddFile : the old code kept for reference
// --------------------------------------------------------------------------
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//

int filelen (int handle)
{
    struct stat fileinfo;

    if (fstat (handle,&fileinfo) == -1)
        I_Error ("Error fstating");

    return fileinfo.st_size;
}


int W_AddFile (char *filename)
{
    wadinfo_t           header;
    lumpinfo_t*         lump_p;
    unsigned            i;
    int                 handle;
    int                 length;
    int                 startlump;
    filelump_t*         fileinfo;
    filelump_t          singleinfo;
    int                 storehandle;

    // open the file and add to directory

    // handle reload indicator.
    if (filename[0] == '~')
    {
        filename++;
        reloadname = filename;
        reloadlump = numlumps;
    }

    if ( (handle = open (filename,O_RDONLY | O_BINARY)) == -1)
    {
        CONS_Printf (" couldn't open %s\n",filename);
        return 0;
    }

    CONS_Printf (" adding %s\n",filename);
    startlump = numlumps;

    if (stricmp (filename+strlen(filename)-3, "wad") )
    {
        // single lump file
        fileinfo = &singleinfo;
        singleinfo.filepos = 0;
        singleinfo.size = LONG(filelen(handle));
        FIL_ExtractFileBase (filename, singleinfo.name);
        numlumps++;
    }
    else
    {
        // WAD file
        read (handle, &header, sizeof(header));
        if (strncmp(header.identification,"IWAD",4))
        {
            // Homebrew levels?
            if (strncmp(header.identification,"PWAD",4))
            {
                I_Error ("Wad file %s doesn't have IWAD "
                         "or PWAD id\n", filename);
            }

            // ???modifiedgame = true;
        }
        header.numlumps = LONG(header.numlumps);
        header.infotableofs = LONG(header.infotableofs);
        length = header.numlumps*sizeof(filelump_t);
        fileinfo = alloca (length);
        lseek (handle, header.infotableofs, SEEK_SET);
        read (handle, fileinfo, length);
        numlumps += header.numlumps;
    }


    // Fill in lumpinfo
    lumpinfo = realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));

    if (!lumpinfo)
        I_Error ("Couldn't realloc lumpinfo");

    lump_p = &lumpinfo[startlump];

    storehandle = reloadname ? -1 : handle;

    for (i=startlump ; i<numlumps ; i++,lump_p++, fileinfo++)
    {
        lump_p->handle = storehandle;
        lump_p->position = LONG(fileinfo->filepos);
        lump_p->size = LONG(fileinfo->size);
        strncpy (lump_p->name, fileinfo->name, 8);
    }

    if (reloadname)
        close (handle);

    return 1;
}
*/

// --------------------------------------------------------------------------
