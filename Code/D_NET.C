

// d_net.c :
//      DOOM Network game communication and protocol,
//      all OS independend parts.

#include "doomdef.h"
#include "d_clisrv.h"
#include "g_game.h"
#include "i_net.h"
#include "i_system.h"
#include "m_argv.h"
#include "d_net.h"

//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// server:
//   maketic is the tic that hasn't had control made for it yet
//   nettics : is the tic for eatch node
//   firsttictosend : is the lowest value of nettics
// client:
//   neededtic : is the tic needed by the client for run the game
//   firsttictosend : is used to optimize a condition
// normaly maketic>=gametic>0,


doomcom_t*    doomcom;
doomdata_t*   netbuffer;        // points inside doomcom

// UNUSED
int           ticdup;

FILE*         debugfile;        // put some net info in a file
                                // during the game

#define       MAXREBOUND 8
doomdata_t    reboundstore[MAXREBOUND];
int           rebound_head,rebound_tail;

// -----------------------------------------------------------------
//  Some stuct and function for acknowledgment of packets
// -----------------------------------------------------------------
#define MAXACKPACKETS 32 // minimum nomber of nodes
#define MAXACKTOSEND  16
#define ACKTIMEOUT    (TICRATE /2)   // 500 ms

typedef struct {
  byte acknum;
  byte   destinationnode;
  USHORT lenght;
  ULONG  sendtime;
  char pak[MAXTEXTCMD];
} ackpak_t;

// table of packet that was not acknowleged can be resend
ackpak_t ackpak[MAXACKPACKETS];
// little queu of ack to send to a node
byte     acktosend[MAXNETNODES][MAXACKTOSEND];
// head and tail of eatch queu
byte     acktosend_head[MAXNETNODES],acktosend_tail[MAXNETNODES],nextacknum;

// return a free acknum and copy netbuffer in the ackpak table
boolean GetFreeAcknum(byte *freeack)
{
   int i;

   for(i=0;i<MAXACKPACKETS;i++)
       if(ackpak[i].acknum==0)
       {
          ackpak[i].acknum=nextacknum;
          nextacknum++;
          if(nextacknum==0)
              nextacknum++;
          ackpak[i].destinationnode=doomcom->remotenode;
          ackpak[i].lenght=doomcom->datalength;
          ackpak[i].sendtime=I_GetTime();
          memcpy(ackpak[i].pak,netbuffer,ackpak[i].lenght);

          *freeack=ackpak[i].acknum;

          return true;
       }
#ifdef PARANOIA
   CONS_Printf("No more free ackpacket\n");
#endif
   return false;
}

// Get a ack to send it the queu of this node
byte GetAcktosend(int node)
{
   byte b;

   if(acktosend_tail[node]==acktosend_head[node])
   {
      // no new "ack to send", resend the latest in case of
      b=acktosend[node][(acktosend_tail[node]-1)%MAXACKTOSEND];
      return b;
   }
   b=acktosend[node][acktosend_tail[node]];
   acktosend_tail[node] = (acktosend_tail[node]+1)%MAXACKTOSEND;
   return b;
}

// don't ack packet because we are busy with other stuff
boolean noackret=0;

// we have got a packet proceed the ack request and ack return
boolean inline Processackpak()
{
   int i;
   boolean goodpacket=true;

// received a ack return remove the ack in the list
   if(netbuffer->ackreturn)
   {
       // search the ackbuffer and free it
       for(i=0;i<MAXACKPACKETS;i++)
           if(ackpak[i].acknum==netbuffer->ackreturn)
           {
#ifdef DEBUGFILE
     if(debugfile)
         fprintf(debugfile,"Remove ack %d\n",ackpak[i].acknum);
#endif
               ackpak[i].acknum=0;
               break;
           }
   }

// received a packet with ack put it in to queue for send the ack
   if(netbuffer->ack && !noackret)
   {
      int  node=doomcom->remotenode;
      byte ack=netbuffer->ack;
      int  newhead=(acktosend_head[node]+1)%MAXACKTOSEND;

      // check if it is not allready in the queue
      for( i=newhead;
          i!=acktosend_head[node];
           i=(i+1)%MAXACKTOSEND    )
          if(acktosend[node][i]==ack)
          {
#ifdef DEBUGFILE
     if(debugfile)
         fprintf(debugfile,"Discard ack %d (duplicated)\n",ack);
#endif
              goodpacket=false; // discard packet (duplicat)
             break;
          }

      if(newhead != acktosend_tail[node])
      {
          acktosend[node][acktosend_head[node]]=ack;
          acktosend_head[node] = newhead;
      }
      else // buffer full discard packet, sender will resend it
           // remark that we can admit the packet but we will not detect the duplication after :(
          return false;
   }
   return goodpacket;
}


// resend the data if needed
void AckTicker(void)
{
   int i;

   for(i=0;i<MAXACKPACKETS;i++)
      if(ackpak[i].acknum && ackpak[i].sendtime+ACKTIMEOUT<I_GetTime())
      {
          memcpy(netbuffer,ackpak[i].pak,ackpak[i].lenght);
          ackpak[i].sendtime=I_GetTime();
#ifdef DEBUGFILE
    if(debugfile)
        fprintf(debugfile,"Resend ack %d\n",ackpak[i].acknum);
#endif
          HSendPacket(ackpak[i].destinationnode,false,ackpak[i].acknum);
      }
}

boolean AllAckReceived(void)
{
   int i;

   for(i=0;i<MAXACKPACKETS;i++)
      if(ackpak[i].acknum)
          return false;

   return true;
}

void InitAck()
{
   int i;

   for(i=0;i<MAXACKPACKETS;i++)
      ackpak[i].acknum=0;

   for(i=0;i<MAXNETNODES;i++)
   {
      acktosend_head[i]=0;
      acktosend_tail[i]=0;
   }
   nextacknum=1;
}

// -----------------------------------------------------------------
//  end of acknowledge function
// -----------------------------------------------------------------

//
// Checksum
//
unsigned NetbufferChecksum (void)
{
    unsigned    c;
    int         i,l;
    unsigned char   *buf;

    c = 0x1234567;

    // compute checksum even under Linux!!! 19990226 by Kin
    l = doomcom->datalength - 4;
    buf = (unsigned char*)netbuffer+4;
    for (i=0 ; i<l ; i++,buf++)
        c += (*buf) * (i+1);

    return c;
}

#ifdef DEBUGFILE

void fprintfstring(char *s,byte len)
{
    int i;

    for (i=0 ; i<len ; i++)
       if(s[i]<32)
           fprintf (debugfile,"[%d]",s[i]);
       else
           fprintf (debugfile,"%c",s[i]);
    fprintf(debugfile,"\n");
}

void DebugPrintpacket(char *header)
{
    fprintf (debugfile,"%-12s (from %d,ack %d,ackret %d,size %d) type(%d) : "
                      ,header
                      ,doomcom->remotenode
                      ,netbuffer->ack
                      ,netbuffer->ackreturn
                      ,doomcom->datalength
                      ,netbuffer->packettype);

    switch(netbuffer->packettype)
    {
       case PLAYERCFG:
           fprintf(debugfile
                  ,"PLAYERCFG\n    lenght %d number %d\n"
                  ,netbuffer->u.textcmd[0]
                  ,netbuffer->u.textcmd[1]);
           break;
       case SERVERTICS:
           fprintf(debugfile
                  ,"SERVER\n    firsttic %d numberoftics %d ntextcmd %d\n    "
                  ,ExpandTics (netbuffer->u.serverpak.starttic)
                  ,netbuffer->u.serverpak.numtics
                  ,&((char *)netbuffer)[doomcom->datalength] - (char *)&netbuffer->u.serverpak.cmds[doomcom->numplayers*netbuffer->u.serverpak.numtics]);
           fprintfstring((char *)&netbuffer->u.serverpak.cmds[doomcom->numplayers*netbuffer->u.serverpak.numtics]
                      ,&((char *)netbuffer)[doomcom->datalength] - (char *)&netbuffer->u.serverpak.cmds[doomcom->numplayers*netbuffer->u.serverpak.numtics]);
           break;
       case CLIENTCMD:
           fprintf(debugfile
                  ,"CLIENT\n    tic %4d resendfrom %d localtic %d\n"
                  ,ExpandTics (netbuffer->u.clientpak.client_tic)
                  ,ExpandTics (netbuffer->u.clientpak.resendfrom)
                  ,0 /*netbuffer->u.clientpak.cmd.localtic*/);
           break;
       case CLIENT2CMD:
           fprintf(debugfile
                  ,"CLIENTSPLIT\n    tic %4d resendfrom %d\n"
                  ,ExpandTics (netbuffer->u.clientpak.client_tic)
                  ,ExpandTics (netbuffer->u.clientpak.resendfrom));
           break;
       case CLIENTMIS:
           fprintf(debugfile
                  ,"CLIENTMIS\n    tic %4d resendfrom %d\n"
                  ,ExpandTics (netbuffer->u.clientpak.client_tic)
                  ,ExpandTics (netbuffer->u.clientpak.resendfrom));
           break;
       case CLIENT2MIS:
           fprintf(debugfile
                  ,"CLIENTSPLITMIS\n    tic %4d resendfrom %d\n"
                  ,ExpandTics (netbuffer->u.clientpak.client_tic)
                  ,ExpandTics (netbuffer->u.clientpak.resendfrom));
           break;
       case TEXTCMD:
           fprintf(debugfile
                  ,"TEXTCMD lenght %d\n    "
                  ,*(unsigned char*)netbuffer->u.textcmd);
           fprintfstring(netbuffer->u.textcmd+1,netbuffer->u.textcmd[0]);
           break;
       case TEXTCMD2:
           fprintf(debugfile
                  ,"TEXTCMD2 lenght %d\n    "
                  ,*(unsigned char*)netbuffer->u.textcmd);
           fprintfstring(netbuffer->u.textcmd+1,netbuffer->u.textcmd[0]);
           break;
       case SERVERCFG:
           fprintf(debugfile
                  ,"SERVERCFG\n    start %d playermask %x session %u\n"
                              "    player %d/%d, serverplayer %d\n"
                  ,netbuffer->u.servercfg.start
                  ,netbuffer->u.servercfg.playerdetected
                  ,netbuffer->u.servercfg.session
                  ,netbuffer->u.servercfg.playernum
                  ,netbuffer->u.servercfg.totalplayernum
                  ,netbuffer->u.servercfg.serverplayer);
           break;
       default:
           fprintf(debugfile,"UNKNOWN %d\n",netbuffer->packettype);
    }
#if 0
    for(i=0;i<doomcom->datalength;i++)
    {
       fprintf(debugfile," %d",((char *)&(netbuffer))[i]);
    }
    fprintf(debugfile,"\n");
#endif
}
#endif

//
// HSendPacket
//
boolean HSendPacket(int   node,boolean ack ,byte acknum)
{
    doomcom->datalength += BASEPACKETSIZE;
    if (!node)
    {
        if((rebound_head+1)%MAXREBOUND==rebound_tail)
        {
#ifdef PARANOIA
            CONS_Printf("No more rebound buf\n");
#endif
            return false;
        }
        memcpy(&reboundstore[rebound_head],netbuffer,doomcom->datalength);
        rebound_head=(rebound_head+1)%MAXREBOUND;
#ifdef DEBUGFILE
        if (debugfile)
        {
            doomcom->remotenode = node;
            DebugPrintpacket("SENDLOCAL");
        }
#endif
        return true;
    }

    if (demoplayback)
        return true;

    if (!netgame)
        I_Error ("Tried to transmit to another node");

    // do this before GetFreeAcknum because this function
    // backup the current paket
    doomcom->remotenode = node;
    if(doomcom->datalength<=0)
    {
#ifdef DEBUGFILE
        if (debugfile)
            DebugPrintpacket("HSendPacket : unknow packet type\n");
#endif
        return false;
    }

    if(acknum==0 && node<MAXNETNODES) // can be a broadcast
        netbuffer->ackreturn=GetAcktosend(node);
    if(ack)
    {
        if( !GetFreeAcknum(&netbuffer->ack) )
            return false;
    }
    else
        netbuffer->ack=acknum;

#ifdef DEBUGFILE
    if (debugfile)
        DebugPrintpacket("SEND");
#endif

    netbuffer->checksum=NetbufferChecksum ();
    I_NetSend();
    return true;
}

//
// HGetPacket
// Returns false if no packet is waiting
// Check Datalenght and checksum
//
boolean HGetPacket (void)
{
    // get a packet from my
    if (rebound_tail!=rebound_head)
    {
        memcpy(netbuffer,&reboundstore[rebound_tail],MAXPACKETLENGHT);
        rebound_tail=(rebound_tail+1)%MAXREBOUND;
        doomcom->remotenode = 0;
#ifdef DEBUGFILE
        if (debugfile)
           DebugPrintpacket("GETLOCAL");
#endif
        return true;
    }

    if (!netgame || demoplayback)
        return false;

    I_NetGet();

    if (doomcom->remotenode == -1)
        return false;

    if (doomcom->remotenode >= MAXNETNODES)
    {
#ifdef DEBUGFILE
        if (debugfile)
            fprintf (debugfile,"receive packet from node %d !\n", doomcom->remotenode);
#endif

        return false;
    }

    if (netbuffer->checksum != NetbufferChecksum ())
    {
#ifdef DEBUGFILE
        if (debugfile)
            fprintf (debugfile,"Bad packet checksum\n");
#endif
        return false;
    }

#ifdef DEBUGFILE
    if (debugfile)
        DebugPrintpacket("GET");
#endif

    // proceed the ack and ackreturn field
    if(!Processackpak())
        return false;    // discated (duplicated)

    return true;
}

//
// D_CheckNetGame
// Works out player numbers among the net participants
//
void D_CheckNetGame (void)
{
    int p;

    InitAck();
    rebound_tail=0;
    rebound_head=0;

    software_MAXPACKETLENGHT=MAXPACKETLENGHT;
    p=M_CheckParm ("-packetsize");
    if(p)
    {
        p=atoi(myargv[p+1]);
        if(p<75)
           p=75;
        if(p>512)
           p=512;
        software_MAXPACKETLENGHT=p;
    }

    // I_InitNetwork sets doomcom and netgame
    // check and initialize the network driver
    I_InitNetwork ();

    if (doomcom->id != DOOMCOM_ID)
        I_Error ("Doomcom buffer invalid!");
    if (doomcom->numnodes>MAXNETNODES)
        I_Error ("To much nodes (%d), max:%d",doomcom->numnodes,MAXNETNODES);

    netbuffer = (doomdata_t *)&doomcom->data;
    ticdup=1;

#ifdef DEBUGFILE
    if (M_CheckParm ("-debugfile"))
    {
        char    filename[20];
        sprintf (filename,"debug%i.txt",doomcom->consoleplayer);
        CONS_Printf ("debug output to: %s\n",filename);
        debugfile = fopen (filename,"w");
    }
#endif

    D_ClientServerInit();
}


void D_CloseConnection( void )
{
    netgame = false;
}
