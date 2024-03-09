// d_net.h : part of layer 4 (transport) (tp4) of the osi model
//           assure the reception of packet and proceed a checksums

#ifndef __D_NET__
#define __D_NET__

//
// Network play related stuff.
// There is a data struct that stores network
//  communication related stuff, and another
//  one that defines the actual packets to
//  be transmitted.
//

#define DEBUGFILE
// Max computers/players in a game.
#define MAXNETNODES             32

#define MAXSPLITSCREENPLAYERS   2    // maximum number of players on a single computer

extern boolean nodeingame[MAXNETNODES];

extern  FILE*       debugfile;
extern  boolean     noackret;

extern  int         ticdup;

void    AckTicker(void);
boolean AllAckReceived(void);

// if ack return true if packet send 0 else
boolean HSendPacket(int   node,boolean ack ,byte acknum);
boolean HGetPacket (void);
void    D_CheckNetGame (void);
void    D_CloseConnection( void );

#endif
