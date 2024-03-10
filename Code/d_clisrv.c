
// d_clisrv.c :
//      DOOM Network game communication and protocol,
//      all OS independend parts.

#include "doomdef.h"
#include "command.h"
#include "i_net.h"
#include "i_system.h"
#include "d_net.h"
#include "d_netcmd.h"
#include "d_clisrv.h"
#include "d_main.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "keys.h"
#include "doomstat.h"
#include "m_argv.h"
#include "m_menu.h"
#include "console.h"
#include "d_netfil.h"
#include "p_setup.h"

#define DEBUGFILE
#ifdef DEBUGFILE
#define DEBFILE(msg) { if(debugfile) fprintf(debugfile,msg) }
#else
#define DEBFILE(msg)
#endif

//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// maketic is the tic that hasn't had control made for it yet
// server:
//   nettics is the tic for eatch node
//   firsttictosend is the lowest value of nettics
// client:
//   neededtic is the tic needed by the client for run the game
//   firsttictosend is used to optimize a condition
// normaly maketic>=gametic>0,

// 1=disable MAXLAG support
#define MAXLAG                  1

#define PREDICTIONQUEUE         BACKUPTICS
#define PREDICTIONMASK          (PREDICTIONQUEUE-1)
// more precise version number to compare in network
#define SUBVERSION              4

#if MAXLAG>BACKUPTICS
Error maxlag > backuptics
#endif

extern  doomdata_t*   netbuffer;        // points inside doomcom
extern  doomcom_t*    doomcom;

boolean       server;           // true or false but !server=client
byte          serverplayer;
ULONG         session;

// server specific vars
byte          playernode[MAXPLAYERS];
ULONG         cl_maketic[MAXNETNODES];
byte          nodetoplayer[MAXNETNODES];
byte          playerpernode[MAXNETNODES]; // used specialy for scplitscreen
boolean       nodeingame[MAXNETNODES];  // set false as nodes leave game
ULONG         nettics[MAXNETNODES];     // what tic the client have received
ULONG         supposedtics[MAXNETNODES];// nettics prevision for smaller packet
ULONG         firstticstosend;          // min of the nettics
boolean       dedicated;                // dedicate server
short         consistancy[BACKUPTICS];
ULONG         maketic;
boolean       acceptnewnode;

// client specific
#ifdef CLIENTPREDICTION
ticcmd_t      localcmds[PREDICTIONQUEUE];
ULONG         localgametic;
#else
ticcmd_t      localcmds;
#endif
boolean       cl_packetmissed;
boolean       drone;
// here it is for the secondary local player (splitscreen)
ticcmd_t      localcmds2;


byte          localtextcmd[MAXTEXTCMD];
byte          localtextcmd2[MAXTEXTCMD]; // splitscreen
char          servernode;       // the number of the server node
ULONG         neededtic;

// engine
ticcmd_t      netcmds[BACKUPTICS][MAXPLAYERS];
byte          textcmds[BACKUPTICS][MAXPLAYERS][MAXTEXTCMD];

consvar_t cv_playdemospeed  = {"playdemospeed","0",0,CV_Unsigned};

void D_Cleartic(int tic);

extern  int      viewangleoffset;


// some software don't support largest packet
// (original sersetup, not exactely, but the probabylity of sending a packet
// of 512 octet is like 0.1)
USHORT software_MAXPACKETLENGHT;

int ExpandTics (int low)
{
    int delta;

    delta = low - (maketic&0xff);

    if (delta >= -64 && delta <= 64)
        return (maketic&~0xff) + low;
    if (delta > 64)
        return (maketic&~0xff) - 256 + low;
    if (delta < -64)
        return (maketic&~0xff) + 256 + low;
#ifdef PARANOIA
    I_Error ("ExpandTics: strange value %i at maketic %i",low,maketic);
#endif
    return 0;
}

// -----------------------------------------------------------------
//  Some extra data function for handle textcmd buffer
// -----------------------------------------------------------------

void (*listnetxcmd[MAXNETXCMD])(char **p,int playernum);

void RegisterNetXCmd(netxcmd_t id,void (*cmd_f) (char **p,int playernum))
{
#ifdef PARANOIA
   if(id>=MAXNETXCMD)
      I_Error("command id %d too big",id);
   if(listnetxcmd[id]!=0)
      I_Error("Command id %d already used",id);
#endif
   listnetxcmd[id]=cmd_f;
}

void SendNetXCmd(byte id,void *param,int nparam)
{
   if(demoplayback)
       return;

   if(localtextcmd[0]+1+nparam>MAXTEXTCMD)
   {
#ifdef PARANOIA
       I_Error("No more place in the buffer for netcmd %d\n",id);
#else
       CONS_Printf("\2Net Command fail\n");
#endif
       return;
   }
   localtextcmd[0]++;
   localtextcmd[localtextcmd[0]]=id;
   if(param && nparam)
   {
       memcpy(&localtextcmd[localtextcmd[0]+1],param,nparam);
       localtextcmd[0]+=nparam;
   }
}

// splitscreen player
void SendNetXCmd2(byte id,void *param,int nparam)
{
   if(demoplayback)
       return;

   if(localtextcmd2[0]+1+nparam>MAXTEXTCMD)
   {
#ifdef PARANOIA
       I_Error("No more place in the buffer for netcmd %d\n",id);
#else
       CONS_Printf("\2Net Command fail\n");
#endif
       return;
   }
   localtextcmd2[0]++;
   localtextcmd2[localtextcmd2[0]]=id;
   if(param && nparam)
   {
       memcpy(&localtextcmd2[localtextcmd2[0]+1],param,nparam);
       localtextcmd2[0]+=nparam;
   }
}


void ExtraDataTicker(void)
{
    int  i,tic,time;
    byte *curpos,*bufferend;

    tic=gametic % BACKUPTICS;

    for(i=0;i<MAXPLAYERS;i++)
        if(playeringame[i])
        {
            curpos=(byte *)&(textcmds[tic][i]);
            bufferend=&curpos[curpos[0]+1];
            curpos++;
            while(curpos<bufferend)
            {
                if(*curpos < MAXNETXCMD && listnetxcmd[*curpos])
                {
                    byte id=*curpos;
                    curpos++;
                    if(debugfile)
                    {
                        fprintf(debugfile,"executing x_cmd %d ply %d",id,i);
                        time=I_GetTime();
                    }
                    (listnetxcmd[id])((char **)&curpos,i);
                    if(debugfile)
                        fprintf(debugfile," done\n");
                }
                else
                    I_Error("Got unknow net command [%d]=%d (max %d)\n"
                           ,curpos-(byte *)&(textcmds[tic][i])
                           ,*curpos,textcmds[tic][i][0]);
            }
        }
}

// -----------------------------------------------------------------
//  end of extra data function
// -----------------------------------------------------------------

// -----------------------------------------------------------------
//  extra data function for lmps
// -----------------------------------------------------------------

// desciption of extradate byte of LEGACY 1.12 not the same of the 1.20
// 1.20 don't have the extradata bits fields but a byte for eatch command
// see XD_xxx in d_netcmd.h
//
// if extradatabit is set, after the ziped tic you find this :
//
//   type   |  description
// ---------+--------------
//   byte   | size of the extradata
//   byte   | the extradata (xd) bits : see XD_...
//            with this byte you know what parameter folow
// if(xd & XDNAMEANDCOLOR)
//   byte   | color
//   char[MAXPLAYERNAME] | name of the player
// endif
// if(xd & XD_WEAPON_PREF)
//   byte   | original weapon switch : boolean, true if use the old
//          | weapon switch methode
//   char[NUMWEAPONS] | the weapon switch priority
//   byte   | autoaim : true if use the old autoaim system
// endif
boolean AddLmpExtradata(byte **demo_point,int playernum)
{
    int  tic;

    tic=gametic % BACKUPTICS;
    if(textcmds[tic][playernum][0]==0)
        return false;

    memcpy(*demo_point,textcmds[tic][playernum],textcmds[tic][playernum][0]+1);
    *demo_point += textcmds[tic][playernum][0]+1;
    return true;
}

extern netxcmd_t NameAndColorCmd,WeaponPrefCmd;

void ReadLmpExtraData(byte **demo_pointer,int playernum)
{
    unsigned char nextra,ex;

    if(!demo_pointer)
    {
        textcmds[gametic%BACKUPTICS][playernum][0]=0;
        return;
    }
    nextra=**demo_pointer;
    if(demoversion==112) // support old demos v1.12
    {
        int    size=0;
        char   *p=*demo_pointer+1; // skip nextra

        textcmds[gametic%BACKUPTICS][playernum][0]=0;

        ex=*p++;
        if(ex & 1)
        {
            size=textcmds[gametic%BACKUPTICS][playernum][0];
            textcmds[gametic%BACKUPTICS][playernum][size+1]=XD_NAMEANDCOLOR;
            memcpy(&textcmds[gametic%BACKUPTICS][playernum][size+2],
                   p,
                   MAXPLAYERNAME+1);
            p+=MAXPLAYERNAME+1;
            textcmds[gametic%BACKUPTICS][playernum][0]+=MAXPLAYERNAME+1+1;
        }
        if(ex & 2)
        {
            size=textcmds[gametic%BACKUPTICS][playernum][0];
            textcmds[gametic%BACKUPTICS][playernum][size+1]=XD_WEAPONPREF;
            memcpy(&textcmds[gametic%BACKUPTICS][playernum][size+2],
                   p,
                   NUMWEAPONS+2);
            p+=NUMWEAPONS+2;
            textcmds[gametic%BACKUPTICS][playernum][0]+=NUMWEAPONS+2+1;
        }
        nextra--;
    }
    else
        memcpy(textcmds[gametic%BACKUPTICS][playernum],*demo_pointer,nextra+1);
    // increment demo pointer
    *demo_pointer +=nextra+1;
}

// -----------------------------------------------------------------
//  extra data function for lmps
// -----------------------------------------------------------------

//
// CheckAbort
//
int CheckAbort (void)
{
    ULONG   stoptic;

    stoptic = I_GetTime () + TICRATE/4;  // wait 0.25 sec
    while (I_GetTime() < stoptic)
         I_StartTic ();

    stoptic = I_GetKey();
    if (stoptic==KEY_ESCAPE)
        I_Error ("Network game synchronization aborted.");
    return stoptic;
}

//
// SendPlayerConfig
//
// send a special packet for declare how many player in local
// used only in arbitratrenetstart()
void SendPlayerConfig(byte mode)
{
   netbuffer->packettype=PLAYERCFG;

   if (drone)
       netbuffer->u.clientcfg.localplayers=0;
   else
   if (cv_splitscreen.value)
       netbuffer->u.clientcfg.localplayers=2;
   else
       netbuffer->u.clientcfg.localplayers=1;
   netbuffer->u.clientcfg.version = VERSION;
   netbuffer->u.clientcfg.subversion = SUBVERSION;
   netbuffer->u.clientcfg.mode = mode;

   doomcom->datalength = sizeof(clientconfig_pak);
   HSendPacket(servernode,false,0);
}


void SendServerConfig(char start,boolean notimecheck)
{
    int i,playermask=0;
    char wadfilename[MAX_WADPATH];

    netbuffer->packettype=SERVERCFG;
    for(i=0;i<MAXPLAYERS;i++)
         if(playeringame[i])
              playermask|=1<<i;

    netbuffer->u.servercfg.version         = VERSION;
    netbuffer->u.servercfg.subversion      = SUBVERSION;
    netbuffer->u.servercfg.session         = session;

    netbuffer->u.servercfg.serverplayer    = serverplayer;
    netbuffer->u.servercfg.start           = start;
    netbuffer->u.servercfg.totalplayernum  = doomcom->numplayers;
    netbuffer->u.servercfg.playerdetected  = playermask;

    i=0;
    while(wadfiles[i])
    {
        strcpy(wadfilename,wadfiles[i]->filename);
        nameonly(wadfilename);
        // 8.3 no lfn to fix
        memcpy(netbuffer->u.servercfg.fileneeded[i].name,wadfilename,SFN); 
        if(notimecheck)
            netbuffer->u.servercfg.fileneeded[i].timestamp = 0;
        else
        netbuffer->u.servercfg.fileneeded[i].timestamp = wadfiles[i]->timestamp;
        i++;
    }
    netbuffer->u.servercfg.fileneedednum=i;
    doomcom->datalength = (int)&( ((doomdata_t *)0)->u.servercfg.fileneeded[i]);
// missing : (wads), and new parameters like solidecorps
    
                        //sizeof(serverconfig_pak);
// the last with ack
    for(i=0;i<doomcom->numnodes;i++)
    {
        netbuffer->u.servercfg.playernum   = nodetoplayer[i];
        HSendPacket(i,start,0);
    }
}

boolean CL_CheckFiles(boolean notimecheck)
{
    int i,j,found;
    char wadfilename[MAX_WADPATH];

    // the first is the iwad (the main wad file)
    strcpy(wadfilename,wadfiles[0]->filename);
    nameonly(wadfilename);
    if( strnicmp(wadfilename,netbuffer->u.servercfg.fileneeded[0].name,SFN)!=0)
        I_Error("You must use the same IWAD file (main wad file)");
    
    for (i=1;i<netbuffer->u.servercfg.fileneedednum;i++)
    {
         if(devparm) CONS_Printf("searching for %s",netbuffer->u.servercfg.fileneeded[i].name);
         // check for allready loaded file
         j=1;
         found=false;
         while(wadfiles[j])
         {
             strcpy(wadfilename,wadfiles[j]->filename);
             nameonly(wadfilename);
             if( strnicmp(wadfilename,netbuffer->u.servercfg.fileneeded[i].name,SFN)==0 && 
                 (notimecheck || wadfiles[j]->timestamp==netbuffer->u.servercfg.fileneeded[i].timestamp || netbuffer->u.servercfg.fileneeded[i].timestamp==0))
             {
                 if(devparm) CONS_Printf(" already loaded\n");
                 found = true;
                 break;
             }
             j++;
         }
         if(found)
            continue;

         // load at runtime now
         memcpy(wadfilename,netbuffer->u.servercfg.fileneeded[i].name,SFN);
         wadfilename[SFN]=0;
         if(notimecheck)
             j = recsearch(wadfilename,0);
         else
         j = recsearch(wadfilename,netbuffer->u.servercfg.fileneeded[i].timestamp);
         if(j==1)
         {
             P_AddWadFile(wadfilename);
             if(devparm) CONS_Printf(" loaded at runtime\n");
         }
         else
         if(j==2)
         {
             CONS_Printf("\2File %s found but with differant date\n",wadfilename);
             return false; // not found
         }
         else // j==0
         {
             CONS_Printf("\2File %s not found\n",wadfilename);
             return false; // not found
         }
    }
    return true;
}

// add a player via a node
void SV_AddPlayer(int node,int playernumber)
{
    int j;

    nodetoplayer[node]=doomcom->numplayers;

    // insert in player table
    for(j=0;j<playernumber;j++)
    {
        if(!playeringame[doomcom->numplayers])
        {
            playeringame[doomcom->numplayers]=true;
            playernode[doomcom->numplayers]=node;
            CONS_Printf("Player %d is in the game (node %d)\n"
                            ,doomcom->numplayers,node);
            doomcom->numplayers++;
        }
    }
}

typedef enum {
   cl_searching,
   cl_ready,
   cl_notthefiles
} cl_mode_t;

//
// D_ArbitrateNetStart
//
// send player and game config too
void D_ArbitrateNetStart (void)
{
    int     i;
    char    start=0;
    int     ontime=0;
    byte    cl_mode=cl_searching;
    boolean onetime[MAXNETNODES];
    boolean notimecheck = M_CheckParm("-notime");

    // servernode can be -1 : the client wait a packet from the server to know
    //                        where the server is (dos protocol)
    //                   32 : le client send a broadcast message and then the server reply
    //                0->31 : le client know where the serser is

    // for the moment doomcom->numnode is the number of node waited and this don't quit
    // until there is no numnodes nodes

    if(server)
    {
        CONS_Printf("Waiting %d nodes...\n",doomcom->numnodes);
        noackret=1;
        for(i=0;i<MAXNETNODES;i++) onetime[i]=false;
    }
    else
        if(servernode<0 || servernode>MAXNETNODES)
            CONS_Printf("Searching the server...\n");
        else
            CONS_Printf("Contacting the server...\n");

    do {
        CheckAbort ();

        AckTicker();

        // server send there config before to be sure to have to player number 0
        // this hack because HGetPacket get first local packet then remote packet
        if(server && !dedicated)
            SendPlayerConfig (cl_ready);

        for(i = 10 ; i>0 ; --i)
          if( HGetPacket() )
          {
              if( !server && netbuffer->packettype==SERVERCFG )
              {
                  //
                  // Client Part
                  //
                  // got server config packet

                  int j;

                  if( servernode==-1 || servernode>=MAXNETNODES)
                      servernode=doomcom->remotenode;

                  nodeingame[(byte)servernode]=true;

                  if( VERSION    != netbuffer->u.servercfg.version && 
                      SUBVERSION != netbuffer->u.clientcfg.subversion)
                      I_Error ("Different DOOM versions cannot play a net game!");
                  session        = netbuffer->u.servercfg.session;

                  serverplayer   = netbuffer->u.servercfg.serverplayer;
                  doomcom->numplayers = netbuffer->u.servercfg.totalplayernum;
                  if (serverplayer>0)
                      playernode[serverplayer]=servernode;

                  if( ontime==0 )
                  {
                      ontime=1;
                      if( CL_CheckFiles(notimecheck) || M_CheckParm("-nofiles") )
                          cl_mode=cl_ready;
                      else
                          cl_mode=cl_notthefiles;
                      CONS_Printf("Found, waiting Start message...\n");
                  }
                  start = netbuffer->u.servercfg.start;

                  if( netbuffer->u.servercfg.playernum == 0xff )
                      I_Error("Maximum of player reached\n");

                  consoleplayer  = netbuffer->u.servercfg.playernum;
                  playernode[consoleplayer]=0;
                  if( consoleplayer & DRONE )
                  {
                      if(!drone)
                          I_Error("Outch I'am not DRONE\n");
                      else
                          consoleplayer &= ~DRONE;
                  }

                  for(j=0;j<MAXPLAYERS;j++)
                      playeringame[j]=(netbuffer->u.servercfg.playerdetected & (1<<j))!=0;
              }
              if(server && netbuffer->packettype==PLAYERCFG
                        && !nodeingame[doomcom->remotenode]
                        && netbuffer->u.clientcfg.version==VERSION
                        && netbuffer->u.clientcfg.subversion==SUBVERSION) // new node
              {
                  int playernumber=netbuffer->u.clientcfg.localplayers;

                  //
                  // Server Part
                  //
                  switch(netbuffer->u.clientcfg.mode) {
                      case cl_searching :
                           break;
                      case cl_notthefiles:
                           if( !onetime[doomcom->remotenode] )
                           {
                               CONS_Printf("\2node %d can't join (don't have all the files)\n",doomcom->remotenode);
                               onetime[doomcom->remotenode]=true;
                           }
                           break;
                      case cl_ready:
                  if (doomcom->numplayers+playernumber>=MAXPLAYERS)
                      nodetoplayer[doomcom->remotenode]=0xff; // too much player, refuse joining
                  else
                  {
                      nodeingame[doomcom->remotenode]=true;
                      playerpernode[doomcom->remotenode]=playernumber;

                      if (!playernumber) //drone
                          nodetoplayer[doomcom->remotenode]=0 | DRONE;
                      else
                          SV_AddPlayer(doomcom->remotenode,playernumber);
                  } // players<32
                           break;
                  } // switch


              } // server
          } // if getpacket

        // the last is sended with ack
        if(server && !start)
        {
            int i,nodecount=0;
            for(i=0;i<MAXNETNODES;i++)
               if(nodeingame[i])
                  nodecount++;
            start=(nodecount==doomcom->numnodes);
            SendServerConfig(start,notimecheck);
        }
        // client to server, send also the ack
        if(servernode!=-1 && !server)
            SendPlayerConfig (cl_mode);

    } while (!(
                (!server && start)
               ||(server && AllAckReceived() && start )
              )
            );
    CONS_Printf ("player %i of %i (%i nodes)\n",
            consoleplayer+1, doomcom->numplayers, doomcom->numnodes);
#ifdef DEBUGFILE
    if(debugfile)
        fprintf(debugfile,"Synchronisation Finished, player %i of %i (%i nodes)\n",
                          consoleplayer+1, doomcom->numplayers, doomcom->numnodes);
#endif
    noackret=0;

    consoleplayer&= ~DRONE;
    displayplayer = consoleplayer;
    if( cv_splitscreen.value )
    {
        secondarydisplayplayer=consoleplayer+1;
        multiplayer=1;
    }
    D_SendPlayerConfig();

//    while(HGetPacket());
}

void D_Cleartic(int tic)
{
    int i;

    for(i=0;i<MAXPLAYERS;i++)
        textcmds[tic%BACKUPTICS][i][0]=0;
}

void ExecuteExitCmd(char **cp,int playernum)
{
    if(!demoplayback)
    {
        if( server )
            nodeingame[playernode[playernum]] = false;
        I_NetFreeNodenum(playernode[playernum]);
    }
    playeringame[playernum] = false;
    CONS_Printf("\2%s left the game\n",player_names[playernum]);
//    if (demorecording)
//        G_CheckDemoStatus ();
}

void StartTitle (void)
{
    int i;

    if (demorecording)
        G_CheckDemoStatus ();

    // reset client/server code
    server=true;
    firstticstosend=maketic;
    localtextcmd[maketic]=0;
    localtextcmd2[maketic]=0;
    cl_packetmissed=false;
    session=0;
    D_CloseConnection();         // netgame=false
    servernode=0;
    for(i=1;i<MAXNETNODES;i++)
        nodeingame[i]=0;
    nodeingame[0]=true;

    // reset game engine
    for(i=0;i<BACKUPTICS;i++)
        D_Cleartic(i);
    CV_SetValue(&cv_deathmatch,0);
    CV_SetValue(&cv_splitscreen,0);
    CV_SetValue(&cv_respawnmonsters,0);
    for(i=1;i<MAXPLAYERS;i++)
        playeringame[i] = 0;
    playeringame[0]=true;
    doomcom->numplayers=1;
    nomonsters  = false;
    consoleplayer=displayplayer=secondarydisplayplayer=0;
    D_StartTitle ();
}

void ExecuteQuitCmd(char **cp,int playernum)
{
    M_StartMessage("Server have Shutdown\n\nPress Esc",NULL,true);
    StartTitle();
}

void Command_GetPlayerNum(void)
{
    int i;

    for(i=0;i<MAXPLAYERS;i++)
        if(playeringame[i])
            if(serverplayer==i)
               CONS_Printf("\2num:%2d  node:%2d  %s\n",i,playernode[i],player_names[i]);
            else
               CONS_Printf("num:%2d  node:%2d  %s\n",i,playernode[i],player_names[i]);
}

int nametonum(char *name)
{
    int playernum,i;

    if( strcmp(name,"0")==0 )
        return 0;
    else
    {
        playernum=atoi(name);
        if(playernum==0)
        {
            for(i=0;i<MAXPLAYERS;i++)
               if(playeringame[i] && strcmp(player_names[i],name)==0)
                   return i;
            CONS_Printf("there is no player named\"%s\"\n",name);
            return -1;
        }
    }
    return playernum;
}

void Command_Kick(void)
{
    char  buf[3];

    if (COM_Argc() != 2)
    {
        CONS_Printf ("kick <playername> or <playernum> : kick a player\n");
        return;
    }

    if(server)
    {
        buf[0]=nametonum(COM_Argv(1));
        if(buf[0]==-1)
           return;
        buf[1]=1; // message 1 : "go away"
        SendNetXCmd(XD_KICK,&buf,2);
    }
    else
        CONS_Printf("You are not the server\n");

}

void Got_KickCmd(char **p,int playernum)
{
    int pnum=*(*p)++;
    CONS_Printf("%s have been kicked ",player_names[pnum]);
    switch(*(*p)++)
    {
       case 1: CONS_Printf("(Go away)\n");
               break;
       case 2: CONS_Printf("(Consistancy failure)\n");
               break;
    }
    if(pnum==consoleplayer)
    {
         StartTitle();
         M_StartMessage("You have been kicked by the server\n",NULL,false);
    }
    else
         ExecuteExitCmd(NULL,pnum);
}

CV_PossibleValue_t maxplayers_cons_t[]={{1,"MIN"},{32,"MAX"},{0,NULL}};

consvar_t cv_allownewplayer = {"sv_allownewplayer","1",0,CV_OnOff};
consvar_t cv_maxplayers     = {"sv_maxplayers","32",CV_NETVAR,maxplayers_cons_t,NULL,32};

//
// D_CheckNetGame
// Works out player numbers among the net participants
//
void D_ClientServerInit (void)
{
    int    i;

    for (i=0 ; i<MAXNETNODES ; i++)
    {
        nodeingame[i] = false;
        nodetoplayer[i]=0;     // not realy but what else ?
        nettics[i]=0;
        supposedtics[i]=0;
        cl_maketic[i]=0;
    }
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        playeringame[i]=false;
        playernode[i]=-1;
    }

    maketic=0;
    gametic=0;
    neededtic=0;
    cl_packetmissed=false;
#ifdef CLIENTPREDICTION
    localgametic = 0;
#endif
    localtextcmd[0]=0;
    localtextcmd2[0]=0;
    for(i=0;i<BACKUPTICS;i++)
        D_Cleartic(i);

    session=0;

    consoleplayer = doomcom->consoleplayer;
    playernode[consoleplayer]=0;
    viewangleoffset=0;
    if(M_CheckParm("-left"))
    {
        drone = true;
        viewangleoffset = ANG90;
    }
    if(M_CheckParm("-right"))
    {
        drone = true;
        viewangleoffset = -ANG90;
    }
    if(M_CheckParm("-dedicated"))
    {
        dedicated=true;
        nodeingame[0]=true;
        serverplayer=-1;
    }
    else
        serverplayer=consoleplayer;

    if(server)
        servernode=0;
    else
        if( servernode>=0 && servernode<MAXNETNODES)
            nodeingame[(byte)servernode]=true;

    RegisterNetXCmd(XD_EXIT,ExecuteExitCmd);
    RegisterNetXCmd(XD_QUIT,ExecuteQuitCmd);
    COM_AddCommand("getplayernum",Command_GetPlayerNum);
    COM_AddCommand("kick",Command_Kick);

#ifdef JOININGAME
    CV_RegisterVar (&cv_allownewplayer);
    CV_RegisterVar (&cv_maxplayers);
#endif

    RegisterNetXCmd(XD_KICK,Got_KickCmd);
    doomcom->numplayers=0;

    cv_allownewplayer.value=1;
    D_ArbitrateNetStart ();
    cv_allownewplayer.value=0;

#ifdef DEBUGFILE
    if(debugfile)
        fprintf(debugfile,"\n-=-=-=-=-=-=-= Game Begin =-=-=-=-=-=-=-\n\n");
#endif

//    while(CheckAbort()!=KEY_ENTER);
}

void SendAllTics (void);
void SendClientCmd (void);
void GetPackets (void);

//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame (void)
{
    if (!netgame || demoplayback)
        return;

#ifdef DEBUGFILE
    if (debugfile)
    {
        fprintf(debugfile,"===========================================================================\n"
                          "                  Quiting Game, closing connection\n"
                          "===========================================================================\n");
    }
#endif

    if(server)
    {
        D_Cleartic(maketic); // clear textcmds
        textcmds[maketic % BACKUPTICS][0][0]=1;
        textcmds[maketic % BACKUPTICS][0][1]=XD_QUIT;
        maketic++;
        SendAllTics();
    }
    else
    {
        SendNetXCmd(XD_EXIT,NULL,0);
        if(cv_splitscreen.value)
            SendNetXCmd2(XD_EXIT,NULL,0);
        SendClientCmd();
    }

    // wait the ackreturn with timout of 5 Sec
    if( netgame )
    {
        ULONG timout=I_GetTime()+5*TICRATE;
        ULONG tictac=I_GetTime();
        HGetPacket();
        while(timout>I_GetTime() && !AllAckReceived())
        {
            while(tictac==I_GetTime()) ;
            tictac=I_GetTime();
            HGetPacket();
            AckTicker();
        }
    }

    if (I_NetShutdown)
        I_NetShutdown();

#ifdef DEBUGFILE
    if (debugfile)
    {
        fprintf(debugfile,"===========================================================================\n"
                          "                         Log finish\n"
                          "===========================================================================\n");
        fclose (debugfile);
    }
#endif
}

// used at txtcmds received to check packetsize bound
int TotalTextCmdPerTic(int tic)
{
    int i,total=1; // num of textcmds per tic

    tic %= BACKUPTICS;

    for(i=0;i<MAXPLAYERS;i++)
        if( textcmds[tic][i][0] )
            total += (textcmds[tic][i][0] + 2); // size and playernum

    return total;
}

//
// GetPackets
//
void GetPackets (void)
{
    int         netconsole;
    ULONG       realend,realstart;
    int         p=maketic%BACKUPTICS;
    byte        *pak,*txtpak,numtxtpak;
    int         node;

    while ( HGetPacket() )
    {
        node=doomcom->remotenode;
        if(!nodeingame[node])
        {
#ifdef DEBUGFILE
            if (debugfile)
                fprintf(debugfile,"Received packet from unknow host %d\n",node);
#endif
            // anyone trying to join !
            if(server)
            {
                // accept by default
                if(doomcom->numplayers+1<=cv_maxplayers.value)
                {
            session++;
                        doomcom->numnodes++;
            D_ArbitrateNetStart();
            return;
        }
                else
                {
                    I_NetFreeNodenum(node);
                    // FIXME: send a refuse packet
                }
            }
            else // client just skip it
                continue;
        }
        netconsole = nodetoplayer[doomcom->remotenode];
#ifdef PARANOIA
        if(!(netconsole & DRONE) && netconsole>=MAXPLAYERS)
           I_Error("bad table nodetoplayer : node %d player %d"
                  ,doomcom->remotenode,netconsole);
#endif


        switch(netbuffer->packettype) {
// -------------------------------------------- SERVER RECEIVE ----------
            case CLIENT2CMD :
            case CLIENTCMD  :
            case CLIENTMIS  :
            case CLIENT2MIS :
                if(!server)
#ifdef PARANOIA
                    I_Error("GetPackets : got client cmd in non server !?");
#else
                    return;
#endif
                // to save bytes, only the low byte of tic numbers are sent
                // Figure out what the rest of the bytes are
                realstart  = ExpandTics (netbuffer->u.clientpak.client_tic);
                realend = ExpandTics (netbuffer->u.clientpak.resendfrom);

                // update the nettics
                if(nettics[doomcom->remotenode]<realend)
                     nettics[doomcom->remotenode] = realend;
                if( (netbuffer->packettype==CLIENTMIS)
                  ||(netbuffer->packettype==CLIENT2MIS) )
                    supposedtics[doomcom->remotenode] = realend;

                // don't do anything for drones just update there nettics
                if(netconsole & DRONE)
                     break;

                // check consistancy
                if(gametic>realstart && realstart>gametic-BACKUPTICS+1 && consistancy[realstart%BACKUPTICS]!=netbuffer->u.clientpak.consistancy)
                {
                    char buf[3];
#ifdef DEBUGFILE
                    if(debugfile)
                        fprintf(debugfile,"[%d] %d != %d\n",realstart,consistancy[realstart%BACKUPTICS],netbuffer->u.clientpak.consistancy);
#endif
                    buf[0]=netconsole;
                    buf[1]=2;
                    SendNetXCmd(XD_KICK,&buf,2);
                }

                // can't erace tic not sent
                if( cl_maketic[node]>=firstticstosend+BACKUPTICS )
                    cl_maketic[node]=firstticstosend+BACKUPTICS-1;
                // can't change the past
                if( cl_maketic[node]<maketic )
                    cl_maketic[node]=maketic;
                // this on is to decrese the latency
                if( cl_maketic[node]>=maketic+MAXLAG )
                {
#ifdef DEBUGFILE
                    if(debugfile)
                        fprintf(debugfile,"Can't backup tic anymore for player %d\n",netconsole);
#endif
                    cl_maketic[node]=maketic+MAXLAG-1;
                    // check on time more the tic sent
                    if(cl_maketic[node]>=firstticstosend+BACKUPTICS)
                       cl_maketic[node]=firstticstosend+BACKUPTICS-1;

                }

                memcpy(&netcmds[cl_maketic[node]%BACKUPTICS][netconsole]
                      ,&netbuffer->u.clientpak.cmd
                      ,sizeof(ticcmd_t));

                if( netbuffer->packettype==CLIENT2CMD )
                {
                    memcpy(&netcmds[cl_maketic[node]%BACKUPTICS][netconsole+1]
                          ,&netbuffer->u.client2pak.cmd2
                          ,sizeof(ticcmd_t));
                }

                cl_maketic[node]++;

                break;
            case TEXTCMD2 : // splitscreen special
                netconsole++;
            case TEXTCMD :
                // special hack for dedicated
//                if(netbuffer->u.textcmd[1]==XD_EXIT)
//                    nodeingame[doomcom->remotenode]=0;
                if((netconsole&DRONE)==0) // not a drone
                {
                    int j;
                    ULONG tic=maketic;

                    j=software_MAXPACKETLENGHT-(netbuffer->u.textcmd[0]+2+BASEPACKETSIZE+doomcom->numplayers*sizeof(ticcmd_t));

                    // search a tic that have enouth space in the ticcmd
                    while(TotalTextCmdPerTic(tic)>j || netbuffer->u.textcmd[0]+textcmds[tic % BACKUPTICS][netconsole][0]>MAXTEXTCMD)
                        tic++;

                    if(tic>firstticstosend+BACKUPTICS)
                        I_Error("GetPacket: Too much textcmd (node %d, player %d)\n",node,netconsole);
                    if( debugfile )
                        fprintf(debugfile,"textcmd put in tic %d at position %d (player %d)\n",tic,textcmds[p][netconsole][0]+1,netconsole);
                    p=tic % BACKUPTICS;
                    memcpy(&textcmds[p][netconsole][textcmds[p][netconsole][0]+1]
                          ,netbuffer->u.textcmd+1
                          ,netbuffer->u.textcmd[0]);
                    textcmds[p][netconsole][0]+=netbuffer->u.textcmd[0];
                }
                break;
// -------------------------------------------- CLIENT RECEIVE ----------
            case SERVERTICS :
                realstart  = ExpandTics (netbuffer->u.serverpak.starttic);
                realend    = realstart+netbuffer->u.serverpak.numtics;

                txtpak=(char *)&netbuffer->u.serverpak.cmds[doomcom->numplayers*netbuffer->u.serverpak.numtics];

                if(realend>gametic+BACKUPTICS)
                    realend=gametic+BACKUPTICS;
                cl_packetmissed=realstart>neededtic;

                if(realstart<=neededtic && realend>neededtic)
                {
                    ULONG i,j;
                    pak=(char *)&netbuffer->u.serverpak.cmds;

                    for(i=realstart;i<realend;i++)
                    {
                    // copy the tics

                       memcpy(netcmds[i%BACKUPTICS]
                             ,pak
                             ,doomcom->numplayers*sizeof(ticcmd_t));
                       pak+=doomcom->numplayers*sizeof(ticcmd_t);

                    // copy the textcmds

                       for(j=0;j<MAXPLAYERS;j++)
                           textcmds[i%BACKUPTICS][j][0]=0;

                       numtxtpak=*txtpak++;
                       for(j=0;j<numtxtpak;j++)
                       {
                           int k=*txtpak++; // playernum

                           memcpy(textcmds[i%BACKUPTICS][k]
                                 ,txtpak
                                 ,txtpak[0]+1);
                           txtpak+=txtpak[0]+1;
                       }
                    }

                    neededtic=realend;
                }
#ifdef DEBUGFILE
                else
                   if (debugfile)
                       fprintf (debugfile,"frame not in bound : %u\n", neededtic);
#endif
                break;
            case PLAYERCFG :
#ifdef DEBUGFILE
                   if (debugfile)
                       fprintf (debugfile,"got player config during game\n");
#endif
                   break;
            case SERVERCFG :
                   // the server ask a new synchronisation.
                   if (netbuffer->u.servercfg.session != session)
                   {
#ifdef DEBUGFILE
                       if (debugfile)
                           fprintf (debugfile,"resynchronize a new player hase come\n");
#endif

                       D_ArbitrateNetStart();
                   }
#ifdef DEBUGFILE
                   else
                       if (debugfile)
                           fprintf (debugfile,"got server config during game\n");
#endif

                   break;
        } // end switch
    } // end while
}

//
// NetUpdate
// Builds ticcmds for console player,
// sends out a packet
//
// no more use random generator, because at very first tic isen't yet synchronized
short Consistancy(void)
{
    short ret=0;
    int   i;

    for(i=0;i<MAXPLAYERS;i++)
        if(playeringame[i])
        {
            if(players[i].mo)
                ret+=players[i].mo->x;
        }
    return ret;
}

// send the client packet to the server
void SendClientCmd (void)
{
/* oop can do that until i have implemented a real dead reckoning
    static ticcmd_t lastcmdssent;
    static int      lastsenttime=-TICRATE;

    if( memcmp(&localcmds,&lastcmdssent,sizeof(ticcmd_t))!=0 || lastsenttime+TICRATE/3<I_GetTime())
    {
        lastsenttime=I_GetTime();
*/
    netbuffer->packettype=CLIENTCMD;
    netbuffer->u.clientpak.client_tic = gametic;

#ifdef CLIENTPREDICTION
//    fprintf(debugfile,"clientsend tic %d\n",localgametic-1);
    memcpy(&netbuffer->u.clientpak.cmd, &localcmds[(localgametic-1)& PREDICTIONMASK], sizeof(ticcmd_t));
#else
    memcpy(&netbuffer->u.clientpak.cmd, &localcmds, sizeof(ticcmd_t));
#endif
    if (cl_packetmissed)
        netbuffer->packettype++;
    netbuffer->u.clientpak.resendfrom = neededtic;
    netbuffer->u.clientpak.consistancy = Consistancy();
    // send a special packet with 2 cmd for splitscreen
    if (cv_splitscreen.value)
    {
        netbuffer->packettype+=2;
        memcpy(&netbuffer->u.client2pak.cmd2, &localcmds2, sizeof(ticcmd_t));
        doomcom->datalength = sizeof(client2cmd_pak);
    }
    else
        doomcom->datalength = sizeof(clientcmd_pak);

    HSendPacket (servernode,false,0);

    // send extra data if needed
    if (localtextcmd[0])
    {
        netbuffer->packettype=TEXTCMD;
        memcpy(netbuffer->u.textcmd,localtextcmd,localtextcmd[0]+1);
        doomcom->datalength = localtextcmd[0]+1;
        // all extra data have been sended
        if( HSendPacket(servernode,true,0) ) // send can be fail for some reasons...
        localtextcmd[0]=0;
    }

    // send extra data if needed for player 2 (splitscreen)
    if (localtextcmd2[0])
    {
        netbuffer->packettype=TEXTCMD2;
        memcpy(netbuffer->u.textcmd,localtextcmd2,localtextcmd2[0]+1);
        doomcom->datalength = localtextcmd2[0]+1;
        // all extra data have been sended
        if( HSendPacket(servernode,true,0) ) // send can be fail for some reasons...
        localtextcmd2[0]=0;
    }

}


// send the server packet
// send tic from firsttictosend to maketic-1
void SendAllTics (void)
{
    ULONG realfirsttic,lasttictosend=maketic,i;
    int  j,packsize;
    char *bufpos=(char *)&netbuffer->u.serverpak.cmds;
    char *ntextcmd,*begintext;

    realfirsttic=maketic;
    for(i=0;i<MAXNETNODES;i++)
        if( nodeingame[i] && realfirsttic>supposedtics[i] )
            realfirsttic = supposedtics[i];
    if(realfirsttic==maketic)
    {
#ifdef DEBUGFILE
        if (debugfile)
            fprintf (debugfile,"Nothing to send mak=%u\n",maketic);
#endif
        return;
    }

// compute the lenght of the packet and cut it if too large
    packsize=(int)&( ((doomdata_t *)0)->u.serverpak.cmds[0]);
    for(i=realfirsttic;i<lasttictosend;i++)
    {
        packsize+=sizeof(ticcmd_t)*doomcom->numplayers;
        packsize++; // the ntextcmd byte
        for(j=0;j<MAXPLAYERS;j++)
            if( playeringame[j] && textcmds[i%BACKUPTICS][j][0] )
                packsize += (textcmds[i%BACKUPTICS][j][0]+2); // the player num, the number of byte
        if(packsize>software_MAXPACKETLENGHT)
        {
            lasttictosend=i-1;
// CONS_Printf("Ajusting packet size to %d (orginal %d)\n",lasttictosend,maketic);
            // too bad : too much player have send extradata and there is too
            //           mutch data in one tic. Too avoid it put the data
            //           on the next tic. Hey it is the server !
            if(i==realfirsttic)
                I_Error("Tic to large can send %d data for %d players\n"
                        "use a largest value for '-packetsize' (max 512)\n"
                        ,packsize,doomcom->numplayers);
            break;
        }
    }

// Send the tics

    netbuffer->packettype=SERVERTICS;
    netbuffer->u.serverpak.starttic=realfirsttic;
    netbuffer->u.serverpak.numtics=lasttictosend-realfirsttic;

    for(i=realfirsttic;i<lasttictosend;i++)
    {
         memcpy(bufpos
               ,netcmds[i%BACKUPTICS]
               ,doomcom->numplayers*sizeof(ticcmd_t));
         bufpos+=doomcom->numplayers*sizeof(ticcmd_t);
    }

    begintext=bufpos;

// add textcmds
    for(i=realfirsttic;i<lasttictosend;i++)
    {
        ntextcmd=bufpos++;
        *ntextcmd=0;
        for(j=0;j<MAXPLAYERS;j++)
        {
            int size=textcmds[i%BACKUPTICS][j][0];

            if(playeringame[j] && size)
            {
                (*ntextcmd)++;
                *bufpos++ = j;
                memcpy(bufpos,textcmds[i%BACKUPTICS][j],size+1);
                bufpos += size+1;
            }
        }
    }
    doomcom->datalength = bufpos - (char *)&(netbuffer->u);

    // node 0 is me !
    supposedtics[0] = lasttictosend-doomcom->extratics;
    // send to  all client but not to me
    for(i=1;i<MAXNETNODES;i++)
       if( nodeingame[i] )
       {
           HSendPacket(i,false,0);
           supposedtics[i] = lasttictosend-doomcom->extratics;
       }
}

extern fixed_t angleturn[];

void inline CopyTic(ticcmd_t *a,ticcmd_t *b)
{
    memcpy(a,b,sizeof(ticcmd_t));
#ifndef ABSOLUTEANGLE
    if(abs(b->angleturn)==angleturn[1])
       a->angleturn=b->angleturn;  // hack for keyboarders for jerknes of turning
    else
       a->angleturn=0;
#endif
}

//
// TryRunTics
//
void Local_Maketic(int realtics)
{
    I_StartTic ();       // i_getevent
    D_ProcessEvents ();  // menu responder ???!!!
                         // Cons responder
                         // game responder call :
                         //    HU_responder,St_responder, Am_responder
                         //    F_responder (final)

    // the server can run more fast than the client
//    if(gametic>=localgametic)
//        localgametic=gametic+1;

    rendergametic=gametic;
#ifndef CLIENTPREDICTION

    // translate inputs (keyboard/mouse/joystick) into game controls
    G_BuildTiccmd (&localcmds,realtics);
    if (cv_splitscreen.value)
        G_BuildTiccmd2(&localcmds2,realtics);

#else
/*
    if(realtics>1)  // do the same of the server
    {
        int i;
        for(i=1;i<realtics;i++)
        {
           CopyTic(&localcmds[localgametic & PREDICTIONMASK]
                  ,&localcmds[(localgametic-1) & PREDICTIONMASK]);
           if(gametic+PREDICTIONQUEUE-2<=localgametic)
               break;
           localgametic++;
        }
    }
*/
    G_BuildTiccmd (&localcmds[localgametic & PREDICTIONMASK],realtics);
    localcmds[localgametic & PREDICTIONMASK].localtic = localgametic;

  // -2 because we send allway the -1
    if(gametic+PREDICTIONQUEUE-2>localgametic)
        localgametic++;
#endif
}

// create missed tic
void SV_Maketic(void)
{
    int j;

    for(j=0;j<MAXNETNODES;j++)
       if(nodeingame[j])
       {
           if(cl_maketic[j]<=maketic)   // we don't received this tic
           {
               int i,player=nodetoplayer[j];
#ifdef DEBUGFILE
               if( debugfile )
                   fprintf(debugfile,"MISS tic%4u for node %d\n",maketic,j);
#endif
               if( devparm )
               CONS_Printf("\2Client Misstic %d\n",maketic);
               // copy the old tic
               for(i=0;i<playerpernode[j];i++,player++)
                   CopyTic(&netcmds[maketic%BACKUPTICS][player]
                          ,&netcmds[(maketic-1)%BACKUPTICS][player]);
               cl_maketic[j]=maketic+1;          // good now
           }
       }
}

extern  boolean advancedemo;
extern  boolean singletics;
static  int     load;

void TryRunTics (int realtics)
{
    int         availabletics;
    int         counts;

    // the machine have laged but is not so bad
    if(realtics>5)
        if(server)
            realtics=1;
        else
            realtics=5;

    if(singletics)
        realtics = 1;

    NetUpdate();

    if(demoplayback)
    {
        neededtic = gametic + realtics + cv_playdemospeed.value;
        // start a game after a demo
        maketic+=realtics;
        firstticstosend=maketic;
    }

#ifdef DEBUGFILE
    if(realtics==0)
        if(load) load--;
#endif
    GetPackets ();

    // availabletic = number of tic in buffer that can by run
    availabletics = neededtic - gametic;

    // decide how many tics to run
    // depend of the real time ex: 5 tic availeble but time is 3 tic
#if 0
    if (realtics <availabletics-1)
        counts = realtics+1;           // try to ratrape the retard
    else
    if (realtics < availabletics)
        counts = realtics;
    else
        counts = availabletics;
#else
//    if (availabletics>1)
        counts = availabletics;
/*
    else
    if (realtics < availabletics)
        counts = realtics;
    else
        counts = availabletics;
*/
#endif

#ifdef DEBUGFILE
    if (debugfile && (realtics || counts))
    {
        fprintf (debugfile,
                 "------------ Tryruntic : REAL:%i AVAIL:%i COUNT:%i LOAD: %i\n",
                 realtics, availabletics,counts,load);
        load=100000;
    }
#endif
#ifdef CLIENTPREDICTION
    // a little hack from client prediction code
    if(players[consoleplayer].mo)
       players[consoleplayer].mo->flags &=~MF_NOSECTOR;
#endif

    if (counts > 0)
    {
        if (advancedemo)
            D_DoAdvanceDemo ();
        else
        // run the count * tics
        while (counts--)
        {
#ifdef DEBUGFILE
            if (debugfile)
                fprintf (debugfile,"============ Runing tic %u (local %d)\n",gametic, 0 /* netcmds[gametic%BACKUPTICS][consoleplayer].localtic*/);
#endif
            if(server && !demoplayback)
                consistancy[gametic%BACKUPTICS]=Consistancy();
            
            COM_BufExecute();
            G_Ticker ();
            ExtraDataTicker();
            gametic++;
            // skip plaused tic in a demo
            if(demoplayback && paused) counts=1;
        }
    }
}


#ifdef CLIENTPREDICTION
int oldgametic=0;
int oldlocalgametic=0;

// make sure (gamestate==GS_LEVEL && !server) before calling this functioon
void D_PredictPlayerPosition(void)
{
    // extrapolate the render position
    if(oldgametic!=gametic || oldlocalgametic!=localgametic)
    {
        player_t *p=&players[consoleplayer];
        int      cmdp,resynch=0;

#ifdef PARANOIA
        if(!p->mo) I_Error("Pas d'mobj");
#endif

        // try to find where the server have put my localgametic :P
        // and resynch with this
        if(oldgametic!=gametic)
        {
            for(cmdp=oldgametic;cmdp<localgametic;cmdp++)
            {
                if( debugfile )
                    fprintf(debugfile,"-> localcmds[%d]=%d\n",cmdp,localcmds[cmdp & PREDICTIONMASK].localtic);
                if(netcmds[(gametic-1)%BACKUPTICS][consoleplayer].localtic ==
                   localcmds[cmdp & PREDICTIONMASK].localtic )
                {
                    int i;

                    resynch=1;
                    if(cmdp==gametic-1)
                    {
                        if( debugfile )
                            fprintf(debugfile,"-> good prediction %d\n",cmdp);
                        cmdp=oldlocalgametic;
                        break;          // nothing to do
                                        // leave the for
                    }

                    localgametic=gametic+localgametic-cmdp-1;
                    for(i=gametic;i<localgametic;i++)
                        memcpy(&localcmds[i & PREDICTIONMASK],
                               &localcmds[(cmdp+i+1-gametic) & PREDICTIONMASK],
                               sizeof(ticcmd_t));
                    if(debugfile)
                        fprintf(debugfile,"-> resynch on %d, local=%d\n",cmdp,localgametic);

                    // reset spirit position to player position
                    memcpy(p->spirit,p->mo,sizeof(mobj_t));
                    p->spirit->type=MT_SPIRIT;
                    p->spirit->info=&mobjinfo[MT_SPIRIT];
                    p->spirit->flags=p->spirit->info->flags;

                    cmdp=gametic;
                    break;  // leave the for
                }
                }
            if(!resynch)
            {
                if(devparm)
                    CONS_Printf("\2Miss prediction\n");
                if( debugfile )
                    fprintf(debugfile,"-> Miss Preditcion\n");
            }
        }
        else
            cmdp=oldlocalgametic;

        if (debugfile)
            fprintf (debugfile,"+-+-+-+-+-+- PreRuning tic %d to %d\n",cmdp,localgametic);

        p->mo->flags &=~(MF_SOLID|MF_SHOOTABLE); // put player temporarily in noclip

        // execute tic prematurely
        while(cmdp!=localgametic)
        {
            // sets momx,y,z
            P_MoveSpirit(p,&localcmds[cmdp & PREDICTIONMASK]);
            cmdp++;
            // move using momx,y,z
            P_MobjThinker(p->spirit);
        }
        // bac to reality :) and put the player invisible just for the render
        p->mo->flags |=(MF_SOLID|MF_SHOOTABLE|MF_NOSECTOR);

        oldlocalgametic=localgametic;
        oldgametic=gametic;
    }
}
#endif

void NetUpdate(void)
{
    static int gametime=0;
    int        nowtime;
    int        realtics;
    int        i;
    int        counts;

    nowtime  = I_GetTime ();
    realtics = nowtime - gametime;

    if (realtics <= 0)   // nothing new to update
        return;
    if(realtics>5)
        if(server)
            realtics=1;
        else
            realtics=5;

    gametime = nowtime;

    if(!server)
        maketic = neededtic;

    Local_Maketic (realtics);    // make local tic, and call menu ?!
    if(server && !demoplayback)
        SendClientCmd ();        // send it
    GetPackets ();               // get packet from client or from server

    // client send the command after a receive of the server
    // the server send before because in single player is beter

    if(!server)
        SendClientCmd ();   // send tic cmd
    else
    {
        if(!demoplayback)
        {

            firstticstosend=gametic;
            for(i=0;i<MAXNETNODES;i++)
                if(nodeingame[i] && nettics[i]<firstticstosend)
                    firstticstosend=nettics[i];

            // make sure the client can follow

            counts=realtics;

            if( maketic+counts-firstticstosend>=BACKUPTICS )
                counts=firstticstosend+BACKUPTICS-maketic-1;

            for(i=0;i<counts;i++)
            {
                SV_Maketic();       // create missed tics
                maketic++;          // all tic are now proceed make the next
                //D_Cleartic();
            }
            for(i=maketic;(unsigned)i<firstticstosend+BACKUPTICS;i++) // clear only when acknoledged
                D_Cleartic(i);                 // clear the maketic the new tic
            SendAllTics();

            neededtic=maketic; // the server is a client too
        }
    }
    AckTicker();
    M_Ticker ();
    CON_Ticker();
}
