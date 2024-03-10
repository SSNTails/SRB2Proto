
// d_netfil.c :
//      Transfer a file using HSendPacket

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef LINUX
#include <io.h>
#include <direct.h>
#endif

#ifndef __WIN32__
#include <unistd.h>
#endif
#ifdef __DJGPP__
#include <dir.h>
#endif

#include "doomdef.h"
#include "doomstat.h"
#include "d_clisrv.h"
#include "g_game.h"
#include "i_net.h"
#include "i_system.h"
#include "m_argv.h"
#include "d_net.h"
#include "d_netfil.h"
#include "z_zone.h"

#ifndef __WIN32__
#define min(a,b)  (a<b ? a : b)
#endif

extern  doomdata_t*   netbuffer;        // points inside doomcom

typedef struct filetx_s {
    int      ram;
    char     *filename;
    ULONG    size;
    int      node;        // destination
    struct filetx_s *next; // a queu
} filetx_t;

// current transfer
filetx_t  txlist; //  next is the first 
ULONG     position=0;
FILE*     currentfile;

// read time of file : stat _stmtime
// write time of file : utime

void SendFile(int node,char *filename)
{}

void SendRam(int node,byte *data, ULONG size,freemethode_t freemethode)
{
    filetx_t *p;
    p=&txlist;
    while(p->next) p=p->next;
    p->next=(filetx_t *)malloc(sizeof(filetx_t));
    if(!p->next)
       I_Error("SendRam : No more ram\n");
    p=p->next;
    p->ram=freemethode;
    p->filename=data;
    p->size=size;
    p->node=node;
    p->next=NULL; // end of list
}

void FiletxTicker(void)
{
    filetx_pak *p;
    ULONG      size;
    int        ram=txlist.next->ram;

    if(!txlist.next)
        return;

    if(!ram && !currentfile) // file not allready open
    {
        currentfile=fopen(txlist.next->filename,"rb");
        if(!currentfile)
            I_Error("File %s not exist",txlist.next->filename);
    }

    p=&netbuffer->u.filetxpak;
    size=min(TXPACKETSIZE,txlist.size-position);
    if(ram)
        memcpy(p->data,&txlist.next->filename[position],size);
    else
    {
        if( fread(p->data,TXPACKETSIZE,1,currentfile) != 1 )
            I_Error("FiletxTicker : can't get %d byte on %s at %d",size,txlist.next->filename,position);
    }
    p->position = position;
    p->fileid   = -1;
    p->size=size;

    if (!HSendPacket(txlist.next->node,true,0)) // reliable SEND
    { // not sent for some od reason
      // retry at next call
         if( !ram )
             fseek(currentfile,position,SEEK_SET);
    }
    else // success
    {
        position+=size;
        if(position==txlist.next->size) // youhou finish
        {
            switch (ram) {
               case file:
                    fclose(currentfile);
                    currentfile=NULL;
                    free(txlist.next->filename);
                    break;
               case z_free_methode:
                    Z_Free(txlist.next->filename);
                    break;
               case free_methode:
                    free(txlist.next->filename);
               case nofree:
                    break;
            }
            txlist.filename = (char *)txlist.next;
            txlist.next = txlist.next->next;
            free(txlist.filename);
        }
    }
}

int packetrestend;

void Got_Filetxpak(void)
{

}
// function cut and pasted from doomatic :)

void nameonly(char *s)
{
  int j;

  for(j=strlen(s);(j>=0) && (s[j]!='\\') && (s[j]!=':') && (s[j]!='/');j--);

  if(j>=0)
    strcpy(s,&(s[j+1]));
}

#ifdef LINUX
#define O_BINARY 0
#endif

boolean fileexist(char *filename,time_t time)
{
   int handel;
   handel=open(filename,O_RDONLY|O_BINARY);
   if(handel!=-1)
   {
         close(handel);
         if(time!=0)
         {
            struct stat bufstat;
            stat(filename,&bufstat);
            if( time!=bufstat.st_mtime )
                return false;
         }
         return true;
   }
   else
         return true;
}

// s1=s2+s3+s1
void strcatbf(char *s1,char *s2,char *s3)
{
  char tmp[512];

  strcpy(tmp,s1);
  strcpy(s1,s2);
  strcat(s1,s3);
  strcat(s1,tmp);
}

// ATTENTION : make sure there is enouth space in filename to put a full path (255 or 512)
#ifdef __WIN32__

int recsearch(char *filename,time_t timestamp)
{
  struct _finddata_t dta;
  int    searchhandle;

  searchhandle=_findfirst(filename,&dta);
  if(searchhandle!=-1)
  {
      _findclose(searchhandle);
      if(timestamp)
      {
          if (dta.time_write==timestamp)
              return 1;
          else
              return 2; // found with differant date
      }
      return 1;
  }

  if((searchhandle=_findfirst("*.*",&dta))!=-1)
  {
    do {
        if((dta.name[0]!='.') && (dta.attrib & _A_SUBDIR ))
        {
            int found;
            chdir(dta.name);
            found = recsearch(filename,timestamp);
            chdir("..");
            if( found )
            {
                strcatbf(filename,dta.name,"\\");
                _findclose(searchhandle);
                return found;
            }
        }
    } while(_findnext(searchhandle,&dta)==0);
  }
  _findclose(searchhandle);
  return 0;
}

#else
#ifdef __DJGPP__
int recsearch(char *filename,time_t timestamp)
{
  struct ffblk dta;

  if(findfirst(filename,&dta,0)==0)
  {
      if(timestamp)
      {
          struct stat statbuf;
          stat(filename,&statbuf);
          if (statbuf.st_mtime==timestamp)
              return 1;
          else
              return 2; // found with differant date
      }
      return 1;
  }

  if(findfirst("*.*",&dta,FA_DIREC)==0)
  {
    do {
      if((dta.ff_name[0]!='.') && (dta.ff_attrib & FA_DIREC))
      {
        int found;
        chdir(dta.ff_name);
        found = recsearch(filename,timestamp);
        chdir("..");
        if(found)
        {
          strcatbf(filename,dta.ff_name,"\\");

          return found;
        }
      }
    } while(findnext(&dta)==0);
  }
  return 0;
}
#else
#ifdef LINUX
int recsearch(char *filename,time_t timestamp)
{
    return 0;
}
#endif
#endif
#endif