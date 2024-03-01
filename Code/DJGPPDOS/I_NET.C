// i_net.c : network interface

#ifdef PC_DOS

#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#include <go32.h>
#include <pc.h>
#include <dpmi.h>
#include <dos.h>
#include <sys/nearptr.h>

#include "../doomdef.h"

#include "../i_system.h"
#include "../d_event.h"
#include "../d_net.h"
#include "../m_argv.h"

#include "../doomstat.h"
#include "../z_zone.h"
#include "../i_net.h"
#include "../i_tcp.h"

//
// NETWORKING
//

typedef enum
{
    CMD_SEND    = 1,
    CMD_GET     = 2

} command_t;

static  int doomatic;

void External_Driver_Get(void);
void External_Driver_Send(void);

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
  }
  else
  {  // externals drivers specific

      __djgpp_nearptr_enable();
      // set up for network
      doomcom=(doomcom_t *)(__djgpp_conventional_base+atoi(myargv[netgamepar+1]));
      CONS_Printf("I_DosNet : Using int 0x%x for communication\n",doomcom->intnum);

      doomatic=M_CheckParm("-doomatic");
      server = (doomcom->consoleplayer == 0);
      servernode=-1;

      I_NetGet  = External_Driver_Get;
      I_NetSend = External_Driver_Send;
      I_NetShutdown = NULL;
      if (M_CheckParm ("-extratic"))
          doomcom->extratics = 1;
      else
          doomcom->extratics = 0;
  }

  // net game

  netgame = true;
  multiplayer = false;

  // litle optimitation with doomatic
  // it store the boolean packetreceived in the doomcom->drone
  // (see External_Driver_Get)
  doomcom->ticdup = 1;
}

void External_Driver_Get(void)
{
    __dpmi_regs r;

    doomcom->command=CMD_GET;

    // it normaly save a task switch to the processor
    if(doomatic && !doomcom->drone)
    {
        doomcom->remotenode = -1;
        return;
    }

    __dpmi_int(doomcom->intnum,&r);
}

void External_Driver_Send(void)
{
    __dpmi_regs r;

    doomcom->command=CMD_SEND;
    __dpmi_int(doomcom->intnum,&r);
}

#endif
