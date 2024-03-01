// win_net.c : network interface


#include <errno.h>

#include "../doomdef.h"

#include "../i_system.h"
#include "../d_event.h"
#include "../d_net.h"
#include "../m_argv.h"

#include "../doomstat.h"

#include "../i_net.h"

#include "../z_zone.h"

int I_InitTcpNetwork(void);
//
// NETWORKING
//

void Internal_Get(void)
{
     I_Error("Get without netgame\n");
}

void Internal_Send(void)
{
     I_Error("Send without netgame\n");
}

void Internal_FreeNodenum(int nodenum)
{}

//
// I_InitNetwork
//
void I_InitNetwork (void)
{
  int netgamepar;

  I_NetGet  = Internal_Get;
  I_NetSend = Internal_Send;
  I_NetShutdown = NULL;
  I_NetFreeNodenum = Internal_FreeNodenum;

  netgamepar = M_CheckParm ("-net");
  if(!netgamepar)
  {
      doomcom=Z_Malloc(sizeof(doomcom_t),PU_STATIC,NULL);
      memset(doomcom,0,sizeof(doomcom_t));
      doomcom->id = DOOMCOM_ID;

      if(!I_InitTcpNetwork())
      {
	  netgame = false;
	  server = true;
	  multiplayer = false;

	  doomcom->numplayers = doomcom->numnodes = 1;
	  doomcom->deathmatch = false;
	  doomcom->consoleplayer = 0;
	  doomcom->ticdup = 1;
	  doomcom->extratics = 0;
	  return;
      }
  } // else net game
  else
  {
      I_Error("-net not supported, use -server and -connect\n"
              "see docs for more\n");
  }
  // net game

  netgame = true;
  multiplayer = true;
}
