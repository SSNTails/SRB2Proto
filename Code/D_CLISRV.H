// d_clisrv.h : high level networking stuff

#ifndef __D_CLISRV__
#define __D_CLISRV__

#include "d_ticcmd.h"
#include "d_netcmd.h"

//
// Network play related stuff.
// There is a data struct that stores network
//  communication related stuff, and another
//  one that defines the actual packets to
//  be transmitted.
//

// Networking and tick handling related.
#define BACKUPTICS            32
#define DRONE               0x80    // bit set in consoleplayer

#define MAXTEXTCMD           256
#define TXPACKETSIZE         256
//
// Packet structure
//
typedef enum
{
    NOTHING,      // to send a nop through network :)
    SERVERCFG,    // start game
    CLIENTCMD,    // ticcmd of the client
    CLIENTMIS,
    CLIENT2CMD,   // 2 cmd in the packed for splitscreen
    CLIENT2MIS,
    SERVERTICS,   // all cmd for the tic
    TEXTCMD,      // extra text command from the client
    TEXTCMD2,     // extra text command from the client (splitscreen)
    PLAYERCFG,    // start game
    STARTLEVEL
} packettype_t;

// client to server packet
typedef struct
{
   byte        client_tic;
   byte        resendfrom;
   short       consistancy;
   ticcmd_t    cmd;
} clientcmd_pak;

// splitscreen packet
// WARNING : must have the same format of clientcmd_pak, for more easy use
typedef struct
{
   byte        client_tic;
   byte        resendfrom;
   short       consistancy;
   ticcmd_t    cmd;
   ticcmd_t    cmd2;
} client2cmd_pak;

// Server to client packet
// this packet is too large !!!!!!!!!
typedef struct
{
   byte        starttic;
   byte        numtics;
   ticcmd_t    cmds[45]; // normaly [BACKUPTIC][MAXPLAYERS] but too large
//   char        textcmds[BACKUPTICS][MAXTEXTCMD];
} servertics_pak;

#define SFN  12

typedef struct {
    char   name[SFN];
    time_t timestamp;
} fileneeded_t;

typedef struct
{
   byte        version;    // exe from differant version don't work
   ULONG       subversion; // contain build version and maybe crc
   ULONG       session;    // incresed eatch resynchronisation

   // server lunch stuffs
   byte        serverplayer;
   byte        totalplayernum;
   byte        playernum;

   byte        start;
   ULONG       playerdetected; // bit field
   byte        fileneedednum;
   fileneeded_t fileneeded[32];   // MAX_WADFILES
} serverconfig_pak;

typedef struct
{
   byte        version;    // exe from differant version don't work
   ULONG       subversion; // contain build version and maybe crc
   byte        localplayers;
   byte        mode;
} clientconfig_pak;

typedef struct
{
   char        fileid;
   ULONG       position;
   USHORT      size;
   byte        data[TXPACKETSIZE];
} filetx_pak;


//
// Network packet data.
//
typedef struct
{
    unsigned   checksum;
    byte       ack;           // if not null the client ask a acknolege
                              // the server must to resend the ack
    byte       ackreturn;     // the return of the ack number

    byte       packettype;
    union  {   clientcmd_pak    clientpak;
               client2cmd_pak   client2pak;
               servertics_pak   serverpak;
               serverconfig_pak servercfg;
               byte             textcmd[MAXTEXTCMD+1];
               filetx_pak       filetxpak;
               clientconfig_pak clientcfg;
           } u;

} doomdata_t;

#define BASEPACKETSIZE ((int)&( ((doomdata_t *)0)->u))

extern boolean dedicated;
extern boolean server;
extern USHORT  software_MAXPACKETLENGHT;
extern boolean acceptnewnode;
extern char    servernode;
extern boolean drone;

extern consvar_t cv_allownewplayer;
extern consvar_t cv_maxplayers;

// used in d_net, the only depandence
int    ExpandTics (int low);
void   D_ClientServerInit (void);

// initialise the other field
void   RegisterNetXCmd(netxcmd_t id,void (*cmd_f) (char **p,int playernum));
void   SendNetXCmd(byte id,void *param,int nparam);
void   SendNetXCmd2(byte id,void *param,int nparam); // splitsreen player

// Create any new ticcmds and broadcast to other players.
void   NetUpdate (void);
void   D_PredictPlayerPosition(void);

// Broadcasts special packets to other players
//  to notify of game exit
void   D_QuitNetGame (void);

//? how many ticks to run?
void   TryRunTics (int realtic);

// extra data for lmps
boolean AddLmpExtradata(byte **demo_p,int playernum);
void   ReadLmpExtraData(byte **demo_pointer,int playernum);

// translate a playername in a player number return -1 if not found and
// print a error message in the console
int    nametonum(char *name);

#endif
