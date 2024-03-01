// Emacs style mode select   -*- C++ -*- 
// GGI target code for LxDoom
// by Colin Phipps (cph@lxdoom.linuxgames.com)
// Portions copyright iD software
//
// $Id: l_video_ggi.c,v 1.1 1999/01/26 09:27:08 cphipps Exp $
//
// checked mouse 19990314 by Kin
#define CHECKEDMOUSE

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include <ggi/ggi.h>

// it must be here 19990117 by Kin
#include "doomdef.h"
// added 19990125 by Kin
#include "../vid_copy.h"

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "m_menu.h"
#include "d_main.h"
#include "s_sound.h"
// added for 1.27 19990220 by Kin
#include "g_input.h"
#include "st_stuff.h"
#include "g_game.h"
#include "i_video.h"

boolean showkey=0; // force to no 19990118 by Kin
int vid_modenum = 0; // index for vid mode list 19990119 by Kin
// added for 1.27 19990220 by Kin
rendermode_t    rendermode=render_soft;
byte graphics_started = 0;
boolean highcolor = false;

boolean expand_buffer = false;
static ggi_visual_t screen;
static const ggi_pixelformat* pixelformat;
static int multiply = 1;
static byte* out_buffer;

static int ggi_screenwidth, ggi_screenheight;

// Mask of events that we are interested in
static const ggi_event_mask ev_mask = \
emKeyPress | emKeyRelease | emPtrRelative | emPtrButtonPress | \
emPtrButtonRelease;

// Unused vars to preserve in config file
int leds_always_off;
int use_vsync;

////////////////////////////////////////////////////////////////
// Input handling utility functions

// Null keyboard translation to satisfy m_misc.c
int I_DoomCode2ScanCode(int c)
{
  return c;
}

int I_ScanCode2DoomCode(int c)
{
  return c;
}

//
// I_GIITranslateKey
//
// Translate keys from LibGII
// Adapted from Linux Heretic v0.9.2

int I_GIITranslateKey(const ggi_key_event* key)
{
  int label = key->label, sym = key->sym;
  int rc=0;
  switch(label) {
  case GIIK_CtrlL:  case GIIK_CtrlR:  rc=KEY_CTRL; 	break;
  case GIIK_ShiftL: case GIIK_ShiftR: rc=KEY_SHIFT;	break;
  case GIIK_MetaL:  case GIIK_MetaR:
  case GIIK_AltL:   case GIIK_AltR:   rc=KEY_ALT;	break;
  case GIIUC_BackSpace:   rc = KEY_BACKSPACE;     break; 
  case GIIUC_Escape:      rc = KEY_ESCAPE;        break; 
  case GIIK_Delete:       rc = KEY_DEL;        break; 
  case GIIK_Insert:       rc = KEY_INS;        break; 
  case GIIK_PageUp:       rc = KEY_PGUP;        break; 
  case GIIK_PageDown:     rc = KEY_PGDN;      break; 
  case GIIK_Home:         rc = KEY_HOME;          break; 
  case GIIK_End:  rc = KEY_END;           break; 
  case GIIUC_Tab:	              rc = KEY_TAB;	break;
  case GIIK_Up:	                      rc = KEY_UPARROW;	break;
  case GIIK_Down:	              rc = KEY_DOWNARROW;	break;
  case GIIK_Left:	              rc = KEY_LEFTARROW;	break;
  case GIIK_Right:                    rc = KEY_RIGHTARROW;	break;
  case GIIK_Enter:                    rc = KEY_ENTER;		break;
  case GIIK_F1:	rc = KEY_F1;		break;
  case GIIK_F2:	rc = KEY_F2;		break;
  case GIIK_F3:	rc = KEY_F3;		break;
  case GIIK_F4:	rc = KEY_F4;		break;
  case GIIK_F5:	rc = KEY_F5;		break;
  case GIIK_F6:	rc = KEY_F6;		break;
  case GIIK_F7:	rc = KEY_F7;		break;
  case GIIK_F8:	rc = KEY_F8;		break;
  case GIIK_F9:	rc = KEY_F9;		break;
  case GIIK_F10:	rc = KEY_F10;		break;
  case GIIK_F11:	rc = KEY_F11;		break;
  case GIIK_F12:	rc = KEY_F12;		break;
  case GIIK_Pause:rc = KEY_PAUSE;		break;
  case GIIK_PSlash:rc = KEY_KPADSLASH;          break;
  case GIIK_P0:rc = KEY_KEYPAD0;break;
  case GIIK_P1:rc = KEY_KEYPAD1;break;
  case GIIK_P2:rc = KEY_KEYPAD2;break;
  case GIIK_P3:rc = KEY_KEYPAD3;break;
  case GIIK_P4:rc = KEY_KEYPAD4;break;
  case GIIK_P5:rc = KEY_KEYPAD5;break;
  case GIIK_P6:rc = KEY_KEYPAD6;break;
  case GIIK_P7:rc = KEY_KEYPAD7;break;
  case GIIK_P8:rc = KEY_KEYPAD8;break;
  case GIIK_P9:rc = KEY_KEYPAD9;break;
  case GIIK_PPlus:rc = KEY_PLUSPAD;break;
  case GIIK_PMinus:rc = KEY_MINUSPAD;break;
  case GIIK_PDot:rc = KEY_KPADDEL;break;
  case GIIK_CapsLock:rc = KEY_CAPSLOCK;break;
  case GIIK_ScrollLock:rc = KEY_SCROLLLOCK;break;
  case GIIK_NumLock:rc = KEY_NUMLOCK;break;
	default:
	  if ((label > '0' && label < '9') || 
	      label == '.' || 
	      label == ',') { 
	      /* Must use label here, or it won't work when shift 
	         is down */ 
	    rc = label; 
	  } else if (sym < 256) { 		    
	      /* ASCII key - we want those */
	    rc = sym;
	      /* We want lowercase */
	    if (rc >='A' && rc <='Z') rc -= ('A' - 'a');
	    switch (sym) {
	      /* Some special cases */
	    case '+': rc = KEY_EQUALS;	break;
	    case '-': rc = KEY_MINUS; 
	    default:			break;
	    }
	  }
  }
  return rc;
}

unsigned short I_GIITranslateButtons(const ggi_pbutton_event* ev)
{
  return ev->button;
}

////////////////////////////////////////////////////////////////
// API

//
// I_StartFrame
//
void I_StartFrame (void)
{
  // If we reuse the blitting buffer in the next rendering, 
  // make sure it is reusable now
  if (!expand_buffer)
    ggiFlush(screen);
}

void I_StartTic(void) {
  I_GetEvent();
}

#ifdef CHECKEDMOUSE
int btmask = 0;
#endif

void I_GetEvent(void)
{
  struct timeval nowait = { 0, 0 };

  while (ggiEventPoll(screen, ev_mask, &nowait)!=evNothing) {
    // There is a desirable event
    ggi_event ggi_ev;
    event_t doom_ev;
    int m_x = 0, m_y = 0; // Mouse motion
    unsigned short buttons; // Buttons
    int i,j;
    // GII will return modified button "number"

    ggiEventRead(screen, &ggi_ev, ev_mask);

    switch(ggi_ev.any.type) {
    case evKeyPress:
      doom_ev.type = ev_keydown;
      if ((doom_ev.data1= I_GIITranslateKey(&ggi_ev.key)) >0 )
	D_PostEvent(&doom_ev);
      //fprintf(stderr,"p:%4x\n",doom_ev.data1);
      break;
    case evKeyRelease:
      doom_ev.type = ev_keyup;
      if ((doom_ev.data1= I_GIITranslateKey(&ggi_ev.key)) > 0 )
	D_PostEvent(&doom_ev);
      //fprintf(stderr,"r:%4x\n",doom_ev.data1);
      break;
    case evPtrRelative:
      m_x += ggi_ev.pmove.x;
      m_y += ggi_ev.pmove.y;
      break;
    case evPtrButtonPress:
      buttons = I_GIITranslateButtons(&ggi_ev.pbutton);
#ifdef CHECKEDMOUSE
      if(btmask&(1<<buttons)) {
	 break;
      } else btmask |= (1<<buttons);
#endif
      doom_ev.type = ev_keydown;
      // translate button2 in ggi to MOUSE3 19990225 by Kin
      doom_ev.data1=KEY_MOUSE1+(buttons^(buttons>>1))-1;
      D_PostEvent(&doom_ev);
      break;
    case evPtrButtonRelease:
      buttons = I_GIITranslateButtons(&ggi_ev.pbutton);
#ifdef CHECKEDMOUSE
      if(!(btmask&(1<<buttons))) {
	 break;
      } else btmask ^= (1<<buttons);
#endif
      doom_ev.type = ev_keyup;
      doom_ev.data1=KEY_MOUSE1+(buttons^(buttons>>1))-1;
      D_PostEvent(&doom_ev);
      break;
    default:
      //I_Error("I_StartTic: Bad GGI event type");
    }

    if (m_x||m_y) {
      doom_ev.type = ev_mouse;
      // 4x mouse like xdoom 19990227 by Kin
      doom_ev.data2 = m_x << 2;
      doom_ev.data3 = -(m_y << 2);
      D_PostEvent(&doom_ev);
    }
  }
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
  // Finish up any output
  ggiFlush(screen);
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
  
  int		i;

  // draws little dots on the bottom of the screen
  if (devparm) {
    static int	lasttic;
    int		tics;
    
    i = I_GetTime();
    tics = i - lasttic;
    lasttic = i;
    if (tics > 20) tics = 20;
    
    for (i=0 ; i<tics*2 ; i+=2)
      screens[0][ (vid.height-1)*vid.width + i] = 0xff;
    for ( ; i<20*2 ; i+=2)
      screens[0][ (vid.height-1)*vid.width + i] = 0x0;
  }
  
  // scales the screen size before blitting it
  // disabled for now 19990221 by Kin
  //  if (expand_buffer)
  //  (*I_ExpandImage)(out_buffer, screens[0]);

  // Blit it
  ggiPutBox(screen, 0, 0, ggi_screenwidth, 
	    ggi_screenheight, out_buffer);

}

//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
  memcpy(scr, screens[0], vid.width*vid.height*vid.bpp);
}

void I_SetPalette(byte* palette)
{

  if (!highcolor) {
    ggi_color ggi_pal[256];
    ggi_color* p;
    int i;
    byte *gamma = gammatable[scr_gamma];
    // PseudoColor mode, so set the palette

    for (i=0, p=ggi_pal; i<256; i++) {
      // Translate palette entry to 16 bits per component
      p->r = gamma[palette[0]] << 8;
      p->g = gamma[palette[1]] << 8;
      p->b = gamma[palette[2]] << 8;
      
      p++; palette+=3;
    }

    ggiSetPalette(screen, 0, 256, ggi_pal);
    //  } else {
    // TrueColor mode, so rewrite conversion table

    //    I_SetPaletteTranslation(palette);
  }
}

void I_ShutdownGraphics(void)
{

  ggiRemoveFlags(screen, GGIFLAG_ASYNC);

  if (expand_buffer) 
    free(out_buffer);

  ggiExit();
}

#define MAX_GGIMODES 18

const ggi_coord temp_res[MAX_GGIMODES]= { 
        {320,200},
        {320,240},
        {320,350}, /* EGA text */
        {360,400}, /* VGA text */
        {400,300},
        {480,300},
        {512,384},
        {640,200}, /* EGA */
        {640,350}, /* EGA */
        {640,400},
        {640,480},
        {720,350}, /* MDA text */
        {720,400}, /* VGA text */
        {800,600},
        {1024,768},
        {1152,864},
        {1280,1024},
        {1600,1200}
};

ggi_coord real_res[MAX_GGIMODES+1];
char vidname[MAX_GGIMODES+1][10];
ggi_mode vidmodes[MAX_GGIMODES+1];
int rescount;

// dummy for test 19990221 by Kin
int   VID_NumModes(void) {
  return rescount;
}

char  *VID_GetModeName(int modenum) {
  return vidname[modenum];
}

int VID_GetModeForSize( int w, int h) {

  int i;

  for (i=0; i<rescount;i++) {
    if(real_res[i].x==w) {
      if(real_res[i].y==h) {
	return i;
      }
    }
  }

  for (i=0; i<rescount;i++) {
    if(real_res[i].x>=w) {
      if(real_res[i].y>=h) {
	break;
      }
    }
  }

  if(i<rescount) {
    return i;
  }

  return 0;

}

void I_StartupGraphics(void)
{
  int i;

  fprintf(stderr, "I_StartupGraphics:");

  if (ggiInit())
    I_Error("Failed to initialise GGI\n");

  if (!(screen = ggiOpen(NULL))) // Open default visual
    I_Error("Failed to get default visual\n");

  { // Check for screen enlargement
    char str[3] = { '-', 0, 0 };
    int n;

    for (n=1; n<4; n++) {
      str[1] = n + '0';
      if (M_CheckParm(str)) multiply = n;
    }
  }

  highcolor = M_CheckParm("-highcolor");

  for(i=0,rescount=0;i<MAX_GGIMODES;i++) {
    if(!ggiCheckSimpleMode(screen,temp_res[i].x,temp_res[i].y,2,
			   (highcolor?GT_15BIT:GT_8BIT),&vidmodes[rescount]))
      {
	memcpy(&real_res[rescount],&temp_res[i],sizeof(ggi_coord));
	sprintf(vidname[rescount],"%4dx%4d",temp_res[i].x,temp_res[i].y);
	fprintf(stderr,"mode %s\n",vidname[rescount]);
	rescount++;
      } else {
	if(GT_DEPTH(vidmodes[rescount].graphtype)==(highcolor?15:8)) {
	  //if((vidmodes[rescount].visible.x>temp_res[i-1].x &&
	  //    vidmodes[rescount].visible.x<temp_res[i+1].x &&
	  //    vidmodes[rescount].visible.y>=temp_res[i-1].y) ||
	  //   (vidmodes[rescount].visible.y>temp_res[i-1].y &&
	  //    vidmodes[rescount].visible.y<temp_res[i+1].y &&
	  //    vidmodes[rescount].visible.x>=temp_res[i-1].x)) {
	  if(vidmodes[rescount].visible.x==temp_res[i].x &&
	     vidmodes[rescount].visible.y==temp_res[i].y) {
	    real_res[rescount].x = vidmodes[rescount].visible.x;
	    real_res[rescount].y = vidmodes[rescount].visible.y;
	    sprintf(vidname[rescount],"%4dx%4d",
		    real_res[rescount].x,real_res[rescount].y);
	    fprintf(stderr,"suggested mode %s\n",vidname[rescount]);
	    rescount++;
	  }
	}
      }
  }
  if(!rescount) {
    I_Error("No video modes available!");
    return;
  }
  out_buffer = screens[0] = NULL;
  VID_SetMode(0);
  // Go asynchronous
  ggiAddFlags(screen, GGIFLAG_ASYNC);

  // Mask events
  ggiSetEventMask(screen, ev_mask);
  // added for 1.27 19990220 by Kin
  graphics_started = 1;

}

int VID_SetMode(int modenum) {

  if(screens[0]) free(screens[0]);
  //if(expand_buffer) {
  //  if(out_buffer) free(out_buffer);
  //}

  vid.bpp = (highcolor?2:1);
  ggi_screenwidth = vid.width = real_res[modenum].x;
  ggi_screenheight = vid.height = real_res[modenum].y;
  vid.rowbytes = vid.width*vid.bpp;
  vid.recalc = 1;

  if (ggiSetMode(screen,&vidmodes[modenum])) {
    I_Error("Failed to set mode");
    return 0;
  }

  vid.buffer = vid.direct = screens[0] =
    (unsigned char *) malloc (((vid.width * vid.height * NUMSCREENS)
			      +(vid.width * ST_HEIGHT)) * vid.bpp);

  // Allocate enlarement buffer if needed
  //if (expand_buffer)
  //  out_buffer = malloc(ggi_screenheight*ggi_screenwidth*vid.bpp);
  //else
  out_buffer = (byte*)screens[0];

  return 1;

}
