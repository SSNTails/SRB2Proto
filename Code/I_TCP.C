//  This is not realy Os dependant because all Os have the same Socket api
//  Just use '#ifdef' for Os dependant stuffs

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __WIN32__
#include <winsock.h>
#else
#if !defined(SCOUW2) && !defined(SCOUW7)
#include <arpa/inet.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

/* BP: is anyone use this ?
#if !defined(LINUX) && !defined(SCOOS5) && !defined(_AIX)
#include <sys/filio.h>
#endif
*/

#ifdef __DJGPP__
#include <lsck/lsck.h>
#define LIBSOCK073

#ifdef LIBSOCK073
#include <winsock.h>
#endif // libsocket073

#endif // djgpp

#endif // win32

#include "doomdef.h"
#include "i_system.h"
#include "i_net.h"
#include "d_net.h"
#include "m_argv.h"

#include "doomstat.h"

#ifdef __WIN32__
    // some undifined under win32
    #define IPPORT_USERRESERVED 5000
    #define errno             h_errno // some very strange things happen when not use h_error ?!?
    #define EWOULDBLOCK WSAEWOULDBLOCK
    #define EMSGSIZE    WSAEMSGSIZE
    #define ECONNREFUSED  WSAECONNREFUSED
#else // linux or djgpp
    #define  SOCKET ULONG
    #define  INVALID_SOCKET -1
#endif

#ifndef LINUX
// winsock stuff (in winsock a socket in not a file)
#define ioctl ioctlsocket
#define close closesocket
#endif

#define BROADCASTADDR MAXNETNODES

struct  sockaddr_in  clientaddress[MAXNETNODES+1],serveraddress;
SOCKET  insocket,sendsocket;
int     DOOMPORT = (IPPORT_USERRESERVED +0x1d );
boolean nodeconnected[MAXNETNODES+1];


char *UDP_AddrToStr(struct sockaddr_in *sk)
{
    static char s[50];

    sprintf(s,"%d.%d.%d.%d:%d",((byte *)(&(sk->sin_addr.s_addr)))[0],
                               ((byte *)(&(sk->sin_addr.s_addr)))[1],
                               ((byte *)(&(sk->sin_addr.s_addr)))[2],
                               ((byte *)(&(sk->sin_addr.s_addr)))[3],
                               ntohs(sk->sin_port));
    return s;
}

void UDP_Get(void)
{
    int                 i;
    int                 c;
    struct sockaddr_in  fromaddress;
    int                 fromlen;

    fromlen = sizeof(fromaddress);
    c = recvfrom (insocket, (char *)&doomcom->data, MAXPACKETLENGHT, 0,
                  (struct sockaddr *)&fromaddress, &fromlen );
    if (c == -1 )
    {
        if (errno == EWOULDBLOCK || errno == EMSGSIZE)
        {
             doomcom->remotenode = -1;      // no packet
             return;
        }
        I_Error ("UDP_Get: %s",strerror(errno));
    }

    // find remote node number
    for (i=0 ; i<MAXNETNODES ; i++)
        if ( fromaddress.sin_addr.s_addr == clientaddress[i].sin_addr.s_addr )
        {
            doomcom->remotenode = i;      // good packet from a game player
            doomcom->datalength = c;
            return;
        }

    // not found
    if(cv_allownewplayer.value)
    {
        int j;

        // find a free slot
        for(j=0;j<MAXNETNODES;j++)
        {
            if( !nodeconnected[j] )
            {
                memcpy(&clientaddress[j],&fromaddress,fromlen);
                clientaddress[j].sin_port = htons(DOOMPORT);
                nodeconnected[j]=true;
#ifdef DEBUGFILE
                if( debugfile )
                    fprintf(debugfile,"New node detected : node:%d address:%s\n",j,UDP_AddrToStr(&clientaddress[j]));
#endif
                doomcom->remotenode = j; // good packet from a game player
                doomcom->datalength = c;
                return;
            }
        }
    }

    // packet is not from one of the players (new game broadcast)
    doomcom->remotenode = -1;               // no packet
    return;

    // byte swap
    // -= insert here endian conversions =-
}

void UDP_Send(void)
{
    int         c;

    // -= insert here endian conversions =-

    if( !nodeconnected[doomcom->remotenode] )
        return;

    //printf ("sending %i\n",gametic);
    c = sendto (sendsocket , (char *)&doomcom->data, doomcom->datalength
                ,0,(struct sockaddr *)&clientaddress[doomcom->remotenode]
                ,sizeof(struct sockaddr_in));

    // ECONNREFUSED was send by linux port
    if (c == -1 && errno!=ECONNREFUSED)
        I_Error ("Udp_Send error: %s",strerror(errno));
}

void UDP_FreeNodenum(int numnode)
{
    nodeconnected[numnode]=false;
}

//
// UDPsocket
//
SOCKET UDP_Socket (void)
{
    SOCKET s;

    // allocate a socket
    s = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s<0 || s==INVALID_SOCKET)
        I_Error ("Udpsocket: Can't create socket: %s",strerror(errno));

    return s;
}

void UDP_Shutdown( void )
{
    close(insocket);
    close(sendsocket);
#ifdef __WIN32__
    WSACleanup();
#else
#ifdef __GO32__
    lsck_uninit();
#endif
#endif
}

//
// BindToLocalPort
//
void BindToLocalPort( int socketnum, int port )
{
    int                 v;
    struct sockaddr_in  address;

    memset (&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = port;

    v = bind (socketnum, (void *)&address, sizeof(address));
    if (v == -1)
        I_Error ("BindToPort: %s", strerror(errno));
}

int GetLocalAddress (void)
{
    char hostname[255];
    struct hostent *hostentry;  // host information entry

    // get local address
    if (gethostname (hostname, sizeof(hostname)) == -1)
        I_Error ("GetLocalAddress : gethostname: errno %d",errno);

    hostentry = gethostbyname (hostname );
    if (!hostentry)
        I_Error ("GetLocalAddress : gethostbyname: couldn't get local host");

    return *(int *)hostentry->h_addr_list[0];
}

#ifdef __WIN32__
WSADATA winsockdata;
#endif

int I_InitTcpNetwork( void )
{
    int    trueval = true;
    int    i;
    struct hostent*     hostentry;      // host information entry
    char   serverhostname[255];

    // parse network game options,
    if ( M_CheckParm ("-server") )
    {
        server=true;

        if( M_IsNextParm() )
            doomcom->numnodes=atoi(M_GetNextParm());
        else
            I_Error("missing argument parameter : -server <number of computers>");

        if (doomcom->numnodes<1 || doomcom->numnodes>MAXNETNODES)
            doomcom->numnodes=MAXNETNODES;
    }
    else
    {
        if( M_CheckParm ("-connect") )
        {
            if(M_IsNextParm())
                strcpy(serverhostname,M_GetNextParm());
            else
                serverhostname[0]=0; // assuming server in the LAN, use broadcast to detect it
            server=false;
        }
        else
            // no command line for tcp/ip :(
            return 0;
    }

#ifdef __WIN32__
    if( WSAStartup(MAKEWORD(1,1),&winsockdata) )
        I_Error("No Tcp/Ip driver detected");    
#else
    #ifdef __GO32__
        #ifdef LIBSOCK073
            lsck_init();
            if( !wsock_init() )
        #else
            if( !lsck_init() )
        #endif
                I_Error("No Tcp/Ip driver detected");
    #endif
#endif


    if ( (i = M_CheckParm ("-udpport"))!=0 )
        DOOMPORT = atoi(myargv[i+1]);
    for(i=0;i<MAXNETNODES;i++)
        nodeconnected[i]=false;

    nodeconnected[0] = true; // always connected to self
    nodeconnected[BROADCASTADDR] = true;
    I_NetSend     = UDP_Send;
    I_NetGet      = UDP_Get;
    I_NetShutdown = UDP_Shutdown;
    I_NetFreeNodenum = UDP_FreeNodenum;

    doomcom->extratics=1; // internet is very high ping

//    doomcom->numnodes = 1;      // this node for sure
    memset(clientaddress,0,sizeof(clientaddress));

    clientaddress[0].sin_family      = AF_INET;
    clientaddress[0].sin_port        = htons(DOOMPORT);
    clientaddress[0].sin_addr.s_addr = GetLocalAddress(); // my how ip

    // setup broadcast adress to MAXNETNODE entry
    clientaddress[MAXNETNODES].sin_family      = AF_INET;
    clientaddress[MAXNETNODES].sin_port        = htons(DOOMPORT);
    clientaddress[MAXNETNODES].sin_addr.s_addr = INADDR_BROADCAST;

    if( !server )
    {
        if(serverhostname[0])
        {
            // find ip of the server
            serveraddress.sin_family      = AF_INET;
            serveraddress.sin_port        = htons(DOOMPORT);
            serveraddress.sin_addr.s_addr = inet_addr(serverhostname);

            if(serveraddress.sin_addr.s_addr==INADDR_NONE) // not a ip ask to the dns
            {
                CONS_Printf("Resolving %s\n",serverhostname);
                hostentry = gethostbyname (serverhostname);
                if (!hostentry)
                    I_Error ("GetHostByName: couldn't find %s", serverhostname);

                serveraddress.sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
            }
            CONS_Printf("Server is %s\n",inet_ntoa(*(struct in_addr *)&serveraddress.sin_addr.s_addr));
            servernode = 1;
            memcpy(&clientaddress[(byte)servernode],&serveraddress,sizeof(serveraddress));
            nodeconnected[(byte)servernode] = true;
        }
        else
            servernode = BROADCASTADDR;
    }
    else
    {
        servernode = 0;
        memcpy(&serveraddress,&clientaddress[0],sizeof(serveraddress));
    }

    // build the two socket
    insocket = UDP_Socket ();
    BindToLocalPort (insocket,htons(DOOMPORT));
    // make it non blocking
    ioctl (insocket, FIONBIO, &trueval);

    sendsocket = UDP_Socket ();
    // make sendsocket broadcastable
    setsockopt(sendsocket, SOL_SOCKET, SO_BROADCAST, (char *)&trueval, sizeof(trueval));

    return 1;
}
