//-----------------------------------------------------------------------------
//
// i_system.c :
//
// DESCRIPTION:
//
//     Misc. stuff
//     Startup & Shutdown routines for music,sound,timer,keyboard,...
//     Signal handler to trap errors and exit cleanly.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <io.h>
#include <stdarg.h>
#include <sys/time.h>


#ifdef DJGPP
 #include <dpmi.h>
 #include <go32.h>
 #include <pc.h>
 #include <dos.h>
 #include <crt0.h>
 #include <sys/segments.h>
 #include <sys/nearptr.h>

 #include <keys.h>
#endif


#include "../doomdef.h"
#include "../m_misc.h"
#include "../i_video.h"
#include "../i_sound.h"

#include "../d_net.h"
#include "../g_game.h"

#include "../d_main.h"

#include "../m_argv.h"

#include "../w_wad.h"
#include "../z_zone.h"
#include "../g_input.h"

#include "../console.h"

#ifdef __GNUG__
 #pragma implementation "../i_system.h"
#endif
#include "../i_system.h"
#include "../i_joy.h"

//### let's try with Allegro ###
#define  alleg_mouse_unused
//#define  alleg_timer_unused
#define  alleg_keyboard_unused
//#define  alleg_joystick_unused
#define  alleg_gfx_driver_unused
#define  alleg_palette_unused
#define  alleg_graphics_unused
#define  alleg_vidmem_unused
#define  alleg_flic_unused
#define  alleg_sound_unused
#define  alleg_file_unused
#define  alleg_datafile_unused
#define  alleg_math_unused
#define  alleg_gui_unused
#include <allegro.h>
//### end of Allegro include ###



// Do not execute cleanup code more than once. See Shutdown_xxx() routines.
byte graphics_started;
byte keyboard_started;
byte sound_started;
byte timer_started;


/* Keyboard handler stuff */
_go32_dpmi_seginfo oldkeyinfo,newkeyinfo;
volatile char nextkeyextended;
static void I_KeyboardHandler();

/* Mouse stuff */
boolean mouse_detected;

volatile ULONG ticcount;   //returned by I_GetTime(), updated by timer interrupt


int     mb_used = 6;


void I_Tactile ( int   on,   int   off,   int   total )
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t        emptycmd;
ticcmd_t*       I_BaseTiccmd(void)
{
    return &emptycmd;
}


//added:11-02-98: commented out because its not used, got to find why its there
//int  I_GetHeapSize (void)
//{
//    return mb_used*1024*1024;        // what's this for?
//}


//
//  Allocates the base zone memory,
//  this function returns a valid pointer and size,
//  else it should interrupt the program immediately.
//
//added:11-02-98: now checks if mem could be allocated, this is still
//    prehistoric... there's a lot to do here: memory locking, detection
//    of win95 etc...
//
boolean   win95;
boolean   lockmem;

void I_DetectWin95 (void)
{
    __dpmi_regs     r;

    r.x.ax = 0x160a;        // Get Windows Version
    __dpmi_int(0x2f, &r);

    if(r.x.ax || r.h.bh < 4)    // Not windows or earlier than Win95
    {
        win95 = false;
    }
    else
    {
        CONS_Printf ("Windows 95 detected\n");
        win95 = true;
    }

    //added:11-02-98: I'm tired of w95 swapping my memory, so I'm testing
    //                a better way of allocating the mem... still experimental
    //                that's why it's a switch.
    if ( M_CheckParm ("-lock") )
    {
        lockmem = true;
        CONS_Printf ("Memory locking requested.\n");
    }
    else
        lockmem = false;
}


byte* I_ZoneBase (int*  size)
{
    void*      pmem;

    // new memory stuff
    _go32_dpmi_meminfo     info;
    // ---

    // detect the big Bill fake
    I_DetectWin95 ();
    i_love_bill = win95;
    // do it the old way
    if (!lockmem)
    {
        *size = mb_used*1024*1024;

        pmem = malloc (*size);

        if (!pmem)
        {
            I_Error("Could not allocate %d megabytes.\n"
                    "Please use -mb parameter and specify a lower value.\n", mb_used);
        }
        // use it for prevent lag in page steeling
        memset(pmem,0,*size);

        return (byte *) pmem;
    }

    // the new way (EXPERIMENTAL)

    _go32_dpmi_get_free_memory_information(&info);
    if (info.available_physical_pages != -1)
    {
        CONS_Printf("Physical pages available : %ld\n", info.available_physical_pages);
        CONS_Printf("Available memory         : %ld\n", info.available_memory);

        CONS_Printf("Available physical memory: %ld\n", info.available_physical_pages<<12);

    }
    else
        I_Error("Sorry, could not get physical mem info,\n"
                "try without the -lock parameter.\n");

    I_Error ("Beta-testing lock mem. This is not implemented.\n");
    return NULL;
}

void I_GetFreeMem(void)
{
    // new memory stuff
    _go32_dpmi_meminfo     info;

    _go32_dpmi_get_free_memory_information(&info);
    if (info.available_physical_pages != -1)
    {
        CONS_Printf("Available memory         : %7d kb\n", info.available_memory>>10);

        CONS_Printf("Available physical memory: %7d kb\n", info.available_physical_pages<<2);
                                                      // <<12 for convert page to byte
    }                                                 // >>10 for convert byte to kb
}


/*==========================================================================*/
// I_GetTime ()
/*==========================================================================*/
ULONG inline I_GetTime (void)
{
    return ticcount;
}

int joystick_detected;
JoyType_t   Joystick;

//
// I_Init
//


void I_WaitJoyButton (void)
{
     CON_Drawer ();
     I_FinishUpdate ();        // page flip or blit buffer

     do {
         poll_joystick();
     } while (!(joy_b1 || joy_b2));

     while (joy_b1 || joy_b2)
         poll_joystick();
}


void I_InitJoystick (void)
{
    //init the joystick
    joystick_detected=0;
    if (cv_usejoystick.value && !M_CheckParm("-nojoy"))
    {
        joy_type = JOY_TYPE_4BUTTON;
        if(initialise_joystick()==0) {

            switch(cv_usejoystick.value) {
               case 1 : joy_type = JOY_TYPE_4BUTTON; break;
               case 2 : joy_type = JOY_TYPE_STANDARD;    break;
               case 3 : joy_type = JOY_TYPE_6BUTTON;     break;
               case 4 : joy_type = JOY_TYPE_WINGEX;      break;
               case 5 : joy_type = JOY_TYPE_FSPRO;       break;
               // new since 1.28 support from allegro 3.11
               case 6 : joy_type = JOY_TYPE_8BUTTON;     break;
               case 7 : joy_type = JOY_TYPE_SIDEWINDER;  break;
               case 8 : joy_type = JOY_TYPE_GAMEPAD_PRO; break;
               case 9 : joy_type = JOY_TYPE_SNESPAD_LPT1;break;
               case 10: joy_type = JOY_TYPE_SNESPAD_LPT2;break;
               case 11: joy_type = JOY_TYPE_SNESPAD_LPT3;break;
               case 12: joy_type = JOY_TYPE_WINGWARRIOR; break;
            }
            // only gamepadstyle joysticks
            Joystick.bGamepadStyle=true;

            CONS_Printf("\2CENTER the joystick and press a button:"); I_WaitJoyButton ();
            initialise_joystick();
            CONS_Printf("\nPush the joystick to the UPPER LEFT corner and press a button\n"); I_WaitJoyButton ();
            calibrate_joystick_tl();
            CONS_Printf("Push the joystick to the LOWER RIGHT corner and press a button\n"); I_WaitJoyButton ();
            calibrate_joystick_br();
            if(joy_type== JOY_TYPE_WINGEX || joy_type == JOY_TYPE_FSPRO)
            {
                CONS_Printf("Put Hat at Center and press a button\n"); I_WaitJoyButton ();
                calibrate_joystick_hat(JOY_HAT_CENTRE);
                CONS_Printf("Put Hat at Up and press a button\n"); I_WaitJoyButton ();
                calibrate_joystick_hat(JOY_HAT_UP);
                CONS_Printf("Put Hat at Down and press a button\n"); I_WaitJoyButton ();
                calibrate_joystick_hat(JOY_HAT_DOWN);
                CONS_Printf("Put Hat at Left and press a button\n"); I_WaitJoyButton ();
                calibrate_joystick_hat(JOY_HAT_LEFT);
                CONS_Printf("Put Hat at Right and press a button\n"); I_WaitJoyButton ();
                calibrate_joystick_hat(JOY_HAT_RIGHT);
            }
            joystick_detected=1;
        }
        else
        {
            CONS_Printf("\2No Joystick detected.\n");
        }
    }
}



//
// I_Error
//
extern boolean demorecording;

//added:18-02-98: put an error message (with format) on stderr
void I_OutputMsg (char *error, ...)
{
    va_list     argptr;

    va_start (argptr,error);
    vfprintf (stderr,error,argptr);
    va_end (argptr);

    // dont flush the message!
}

int errorcount=0; // fuck recursive errors
int shutdowning=false;

//added 31-12-97 : display error messy after shutdowngfx
void I_Error (char *error, ...)
{
    va_list     argptr;
    // added 11-2-98 recursive error detecting

    if(shutdowning)
    {
        errorcount++;
        if(errorcount==5)
           I_ShutdownGraphics();
        if(errorcount==6)
           I_ShutdownSystem();
        if(errorcount>7)
          exit(-1);       // recursive errors detected
    }
    shutdowning=true;

    // put message to stderr
    va_start (argptr,error);
    fprintf (stderr, "Error: ");
    vfprintf (stderr,error,argptr);
#ifdef DEBUGFILE
    if (debugfile)
    {
        fprintf (debugfile,"I_Error :");
        vfprintf (debugfile,error,argptr);
    }
#endif

    va_end (argptr);

    //added:18-02-98: save one time is enough!
    if (!errorcount)
    {
        M_SaveConfig (NULL);   //save game config, cvars..
    }

    //added:16-02-98: save demo, could be useful for debug
    //                NOTE: demos are normally not saved here.
    if (demorecording)
        G_CheckDemoStatus();

    D_QuitNetGame ();

    /* shutdown everything that was started ! */
    I_ShutdownSystem();

    fprintf (stderr, "\nPress ENTER");
    fflush( stderr );
    getchar();

    exit(-1);
}


//
// I_Quit : shutdown everything cleanly, in reverse order of Startup.
//
void I_Quit (void)
{
    byte* endoom;

    //added:16-02-98: when recording a demo, should exit using 'q' key,
    //        but sometimes we forget and use 'F10'.. so save here too.
    if (demorecording)
        G_CheckDemoStatus();

    M_SaveConfig (NULL);   //save game config, cvars..

 #ifdef PC_DOS
    endoom = W_CacheLumpName("ENDOOM",PU_CACHE);
 #endif

    //added:03-01-98: maybe it needs that the ticcount continues,
    // or something else that will be finished by ShutdownSystem()
    // so I do it before.
    D_QuitNetGame ();

    /* shutdown everything that was started ! */
    I_ShutdownSystem();

 #ifdef PC_DOS
    puttext(1,1,80,25,endoom);
    gotoxy(1,24);
 #endif

    if(shutdowning || errorcount)
        I_Error("Error detected (%d)",errorcount);

    fflush(stderr);

    exit(0);
}


//added:12-02-98: does want to work!!!! rhaaahahha
void I_WaitVBL(int count)
{
   while(count-->0);
   {
     do {
     } while (inportb(0x3DA) & 8);
     do {
     } while (!(inportb(0x3DA) & 8));
   }

}

//  Fab: this is probably to activate the 'loading' disc icon
//       it should set a flag, that I_FinishUpdate uses to know
//       whether it draws a small 'loading' disc icon on the screen or not
//
//  also it should explicitly draw the disc because the screen is
//  possibly not refreshed while loading
//
void I_BeginRead (void)
{
}

//  Fab: see above, end the 'loading' disc icon, set the flag false
//
void I_EndRead (void)
{
}

byte*   I_AllocLow(int length)
{
    byte*       mem;

    mem = (byte *)malloc (length);
    memset (mem,0,length);
    return mem;

}

//#define MOUSE2
/* Secondary Mouse*/
#ifdef MOUSE2
_go32_dpmi_seginfo oldmouseinfo,newmouseinfo;
boolean mouse2_started=0;
int     mouse2port;
int     mouse2irq;
int     mouse2x,mouse2y;
boolean mouse2buttons;
// internal use
volatile int     bytenum;
volatile byte    combytes[3];

//
// support a secondary mouse without mouse driver !
//
// take from the PC-GPE by Mark Feldman
void I_MicrosoftMouseIntHandler()
{
  char   dx,dy;
  byte   inbyte;

  // Get the port byte
  inbyte = inportb(mouse2port);

  // Make sure we are properly "synched"
  if((inbyte & 64)== 64)
      bytenum = 0;

  // Store the byte and adjust bytenum
  combytes[bytenum] = inbyte;
  bytenum++;

  // Have we received all 3 bytes?
  if(bytenum==3)
  {
      // Yes, so process them
      dx = ((combytes[0] & 3) << 6) + combytes[1];
      dy = ((combytes[0] & 12) << 4) + combytes[2];
//      if dx >= 128 then dx := dx - 256;
//      if dy >= 128 then dy := dy - 256;
      mouse2x+= dx;
      mouse2y+= dy;
      mouse2buttons = (combytes[0] & (32+16)>>4);

      // And start on first byte again
      bytenum = 0;
  }

  // Acknowledge the interrupt
  outportb(0x20,0x20);
}
END_OF_FUNCTION(I_MicrosoftMouseIntHandler);

//
//  Removes the mouse2 handler.
//
void I_ShutdownMouse2()
{
    if( !mouse2_started )
        return;

    asm("cli");
    _go32_dpmi_set_protected_mode_interrupt_vector(mouse2irq, &oldmouseinfo);
    _go32_dpmi_free_iret_wrapper(&newmouseinfo);
    asm("sti");
}

//
//  Installs the mouse2 handler.
//
void I_StartupMouse2()
{
    int starttime;
    if(mouse2_started) // can't start it two time !
        return;

    mouse2x=mouse2y=mouse2buttons=0;
    // COM2
    mouse2irq=0xb;
    mouse2port=0x2F8;
    // Microsoft compatible
    newkeyinfo.pm_offset=(int)I_MicrosoftMouseIntHandler;

    asm("cli");
    _go32_dpmi_get_protected_mode_interrupt_vector(mouse2irq, &oldmouseinfo);
    newkeyinfo.pm_selector=_go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&newmouseinfo);
    _go32_dpmi_set_protected_mode_interrupt_vector(mouse2irq, &newmouseinfo);

    LOCK_VARIABLE(bytenum);
    LOCK_VARIABLE(mouse2x);
    LOCK_VARIABLE(mouse2y);
    LOCK_VARIABLE(mouse2buttons);
    LOCK_VARIABLE(mouse2port);
    _go32_dpmi_lock_data(combytes,sizeof(combytes));
    LOCK_FUNCTION(I_MicrosoftMouseIntHandler);

    asm("sti");

    outportb(0x21,0);  // enable all irqs

    inportb(mouse2port+5);
    outportb(mouse2port,0xFF); // send anything
    starttime=I_GetTime()+3;
    while(!bytenum && starttime<I_GetTime())
        ;
    if(bytenum)
    {
        if(combytes[0]=='M')
            CONS_Printf("Microsoft Secondary Mouse detected\n");
        else
            CONS_Printf("Secondary Mouse detected (unknow type)\n");
        bytenum=0;
    }
    else
        CONS_Printf("No Secondary Mouse detected\n");

    //register shutdown mouse2 code.
    I_AddExitFunc(I_ShutdownMouse2);
    mouse2_started = true;
}
#endif

//  Initialise the mouse. Doesnt need to be shutdown.
//
void I_StartupMouse (void)
{
    __dpmi_regs r;

    // mouse detection may be skipped by setting usemouse false
    if(cv_usemouse.value == 0)
    {
        mouse_detected=false;
        return;
    }

    //detect mouse presence
    r.x.ax=0;
    __dpmi_int(0x33,&r);

    //added:03-01-98:
    if( r.x.ax == 0 && cv_usemouse.value != 2)
    {
        mouse_detected=false;
        CONS_Printf("\2I_StartupMouse: mouse not present.\n");
        return;
    }
    else
        mouse_detected=true;

    //hide cursor
    r.x.ax=2;
    __dpmi_int(0x33,&r);

    //reset mickey count
    r.x.ax=0x0b;
    __dpmi_int(0x33,&r);

#ifdef MOUSE2
    // start handle secondary mouse !
    I_StartupMouse2();
#endif
}

void I_GetEvent (void)
{
    __dpmi_regs r;
    event_t event;

    int i;

#ifdef MOUSE2
    // mouse may be disabled during the game by setting usemouse false
    if (mouse2_started)
    {
        //mouse movement
        static int lastbuttons2=0;
        int a=0;

        // post key event for buttons
        if (mouse2buttons!=lastbuttons2)
        {
            int j=1,k;
            k=(mouse2buttons ^ lastbuttons2); // only changed bit to 1
            lastbuttons2=mouse2buttons;

            for(i=0;i<MOUSE2BUTTONS;i++,j<<=1)
                if(k & j)
                {
                    if(mouse2buttons & j)
                       event.type=ev_keydown;
                    else
                       event.type=ev_keyup;
                    event.data1=KEY_2MOUSE1+i;
                    D_PostEvent(&event);
                    CONS_Printf("posted buttons change\n");
                    a=1;
                }
        }

        if ((mouse2x!=0)||(mouse2y!=0))
        {
            event.type=ev_mouse;
            event.data1=0;
//          event.data1=buttons;    // not needed
            event.data2=mouse2x;
            event.data3=-mouse2y;
            mouse2x=0;
            mouse2y=0;
            CONS_Printf("posted position change\n");
            a=1;

            D_PostEvent(&event);
        }
//        if(!a)
//           CONS_Printf("Nothing posted\n");
    }
#endif
    if(mouse_detected)
    {
        //mouse movement
        int xmickeys,ymickeys,buttons;
        static int lastbuttons=0;

        r.x.ax=0x0b;           // ask the mouvement not the position
        __dpmi_int(0x33,&r);
        xmickeys=(signed short)r.x.cx;
        ymickeys=(signed short)r.x.dx;
        r.x.ax=0x03;
        __dpmi_int(0x33,&r);
        buttons=r.x.bx;

        // post key event for buttons
        if (buttons!=lastbuttons)
        {
            int j=1,k;
            k=(buttons ^ lastbuttons); // only changed bit to 1
            lastbuttons=buttons;

            for(i=0;i<MOUSEBUTTONS;i++,j<<=1)
                if(k & j)
                {
                    if(buttons & j)
                       event.type=ev_keydown;
                    else
                       event.type=ev_keyup;
                    event.data1=KEY_MOUSE1+i;
                    D_PostEvent(&event);
                }
        }

        if ((xmickeys!=0)||(ymickeys!=0))
        {
          event.type=ev_mouse;
          event.data1=0;
//          event.data1=buttons;    // not needed
          event.data2=xmickeys;
          event.data3=-ymickeys;
          lastbuttons=buttons;

          D_PostEvent(&event);
        }

    }
    //joystick
    if (joystick_detected)
    {
        static int lastjoybuttons=0;
        int joybuttons;

        poll_joystick();
        // I assume that true is 1
        joybuttons=joy_b1+(joy_b2<<1)+(joy_b3<<2)+(joy_b4<<3)
                         +(joy_b5<<4)+(joy_b6<<5)+(joy_b7<<6)+(joy_b8<<7);

        switch(joy_hat) {
          case JOY_HAT_UP   : joybuttons|=1<<10;break;
          case JOY_HAT_DOWN : joybuttons|=1<<11;break;
          case JOY_HAT_LEFT : joybuttons|=1<<12;break;
          case JOY_HAT_RIGHT: joybuttons|=1<<13;break;
        }

        // post key event for buttons
        if(joybuttons!=lastjoybuttons)
        {
            int j=1,k;
            k=(joybuttons ^ lastjoybuttons); // only changed bit to 1
            lastjoybuttons=joybuttons;

            for(i=0;i<JOYBUTTONS;i++,j<<=1)
                if(k & j)          // test the eatch bit and post the corresponding event
                {
                    if(joybuttons & j)
                       event.type=ev_keydown;
                    else
                       event.type=ev_keyup;
                    event.data1=KEY_JOY1+i;
                    D_PostEvent(&event);
                }
        }

        event.type=ev_joystick;
        event.data1=0;
        event.data2=0;
        event.data3=0;

        if(joy_left)
           event.data2=-1;
        if(joy_right)
           event.data2=1;
        if(joy_up)
           event.data3=-1;
        if(joy_down)
           event.data3=1;

        D_PostEvent(&event);
    }
}

//
//  Timer user routine called at ticrate.
//
void I_TimerISR (void)
{
   //  IO_PlayerInput();      // old doom did that
   ticcount++;

}
END_OF_FUNCTION(I_TimerISR);


//added:08-01-98: we don't use allegro_exit() so we have to do it ourselves.
void I_ShutdownTimer (void)
{
    if( !timer_started )
        return;
    remove_timer();
}


//
//  Installs the timer interrupt handler with timer speed as TICRATE.
//
void I_StartupTimer(void)
{
   ticcount = 0;

   //lock this from being swapped to disk! BEFORE INSTALLING
   LOCK_VARIABLE(ticcount);
   LOCK_FUNCTION(I_TimerISR);

   if( install_timer() != 0 )
      I_Error("I_StartupTimer: could not install timer.");

   if( install_int_ex( I_TimerISR, BPS_TO_TIMER(TICRATE) ) != 0 )
      //should never happen since we use only one.
      I_Error("I_StartupTimer: no room for callback routine.");

   //added:08-01-98: remove the timer explicitly because we don't use
   //                Allegro 's allegro_exit() shutdown code.
   I_AddExitFunc(I_ShutdownTimer);
   timer_started = true;
}


//added:07-02-98:
//
//
byte ASCIINames[128] =
{
//  0       1       2       3       4       5       6       7
//  8       9       A       B       C       D       E       F
    0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
    '7',    '8',    '9',    '0', KEY_MINUS,KEY_EQUALS,KEY_BACKSPACE, KEY_TAB,
    'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
    'o',    'p',    '[',    ']', KEY_ENTER,KEY_CTRL,'a',    's',
    'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
    '\'',   '`', KEY_SHIFT, '\\',   'z',    'x',    'c',    'v',
    'b',    'n',    'm',    ',',    '.',    '/', KEY_SHIFT, '*',
 KEY_ALT,KEY_SPACE,KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,KEY_NUMLOCK,KEY_SCROLLLOCK,KEY_KEYPAD7,
 KEY_KEYPAD8,KEY_KEYPAD9,KEY_MINUSPAD,KEY_KEYPAD4,KEY_KEYPAD5,KEY_KEYPAD6,KEY_PLUSPAD,KEY_KEYPAD1,
 KEY_KEYPAD2,KEY_KEYPAD3,KEY_KEYPAD0,KEY_KPADDEL,      0,      0,      0,      KEY_F11,
    KEY_F12,0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0
};

int pausepressed=0;

void I_KeyboardHandler()
{
    unsigned char ch;
    event_t       event;

    ch=inportb(0x60);

    if(pausepressed>0)
        pausepressed--;
    else
        if(ch==0xE1) // pause key
        {
          event.type=ev_keydown;
          event.data1=KEY_PAUSE;
          D_PostEvent(&event);
          pausepressed=5;
        }
        else
        if(ch==0xE0) // extended key handled at next call
        {
          nextkeyextended=1;
        }
        else
        {
          if((ch&0x80)==0)
            event.type=ev_keydown;
          else
            event.type=ev_keyup;

          ch&=0x7f;

          if(nextkeyextended)
          {
            nextkeyextended=0;

            if(ch==70)  // crtl-break
            {
                asm ("movb $0x79, %%al
                     call ___djgpp_hw_exception"
                     : : :"%eax","%ebx","%ecx","%edx","%esi","%edi","memory");
            }

            // remap lonely keypad slash
            if (ch==53)
                event.data1 = KEY_KPADSLASH;
            else
            // remap the bill gates keys...
            if (ch>=91 && ch<=93)
                event.data1 = ch + 0x80;    // leftwin, rightwin, menu
            else
            // remap non-keypad extended keys to a value<128, but
            // make them different than the KEYPAD keys.
            if (ch>=71 && ch<=83)
                event.data1 = 0x80 + ch + 30;
            else if (ch==28)
                event.data1 = KEY_ENTER;    // keypad enter -> return key
            else if (ch==29)
                event.data1 = KEY_CTRL;     // rctrl -> lctrl
            else if (ch==56)
                event.data1 = KEY_ALT;      // ralt -> lalt
            else
                ch = 0;
            if (ch)
                D_PostEvent(&event);
          }
          else
          {
            if (ASCIINames[ch]!=0)
              event.data1=ASCIINames[ch];
            else
              event.data1=ch+0x80;
            D_PostEvent(&event);
          }
        }
//  reset keyoard
//    { char b;
//     b=inportb(0x61);      // ???
//     outportb(0x61,b|80);  // ???   run without it
//     outportb(0x61,b);     // ???
     outportb(0x20,0x20);
//    }
}
END_OF_FUNCTION(I_KeyboardHandler);

//  Return a key that has been pushed, or 0
//  (replace getchar() at game startup)
//
int I_GetKey (void)
{
    if( keyboard_started )
    {
    event_t   *ev;

    if (eventtail != eventhead)
    {
        ev = &events[eventtail];
        eventtail = (++eventtail)&(MAXEVENTS-1);
        if (ev->type == ev_keydown)
            return ev->data1;
        else
            return 0;
    }
    return 0;
    }

    // keyboard not started use the bios call trouth djgpp
    if(_conio_kbhit())
    {
        int key=getch();
        if(key==0) key=getch()+256;
        return key;
    }
    else
        return 0;

}


//
//  Removes the keyboard handler.
//
void I_ShutdownKeyboard()
{
    if( !keyboard_started )
        return;

    asm("cli");
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &oldkeyinfo);
    _go32_dpmi_free_iret_wrapper(&newkeyinfo);
    asm("sti");
}

//
//  Installs the keyboard handler.
//
void I_StartupKeyboard()
{
    nextkeyextended=0;

    asm("cli");
    _go32_dpmi_get_protected_mode_interrupt_vector(9, &oldkeyinfo);
    newkeyinfo.pm_offset=(int)I_KeyboardHandler;
    newkeyinfo.pm_selector=_go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&newkeyinfo);
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &newkeyinfo);

    LOCK_VARIABLE(nextkeyextended);
    LOCK_VARIABLE(pausepressed);
    _go32_dpmi_lock_data(ASCIINames,sizeof(ASCIINames));
    LOCK_FUNCTION(I_KeyboardHandler);

    _go32_dpmi_lock_data(events,sizeof(events));
    LOCK_VARIABLE(eventhead);
    LOCK_FUNCTION(D_PostEvent);

    asm("sti");

    //added:08-01-98:register shutdown keyboard code.
    I_AddExitFunc(I_ShutdownKeyboard);
    keyboard_started = true;
}




//added:08-01-98:
//
//  Clean Startup & Shutdown handling, as does Allegro.
//  We need this services for ourselves too, and we don't want to mix
//  with Allegro, because someone might not use Allegro.
//  (all 'exit' was renamed to 'quit')
//
#define MAX_QUIT_FUNCS     16
typedef void (*quitfuncptr)();
static quitfuncptr quit_funcs[MAX_QUIT_FUNCS] =
               { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
               };


//added:08-01-98:
//
//  Adds a function to the list that need to be called by I_SystemShutdown().
//
void I_AddExitFunc(void (*func)())
{
   int c;

   for (c=0; c<MAX_QUIT_FUNCS; c++) {
      if (!quit_funcs[c]) {
         quit_funcs[c] = func;
         break;
      }
   }
}


//added:08-01-98:
//
//  Removes a function from the list that need to be called by
//   I_SystemShutdown().
//
void I_RemoveExitFunc(void (*func)())
{
   int c;

   for (c=0; c<MAX_QUIT_FUNCS; c++) {
      if (quit_funcs[c] == func) {
         while (c<MAX_QUIT_FUNCS-1) {
            quit_funcs[c] = quit_funcs[c+1];
            c++;
         }
         quit_funcs[MAX_QUIT_FUNCS-1] = NULL;
         break;
      }
   }
}



 //added:03-01-98:
//
// signal_handler:
//  Used to trap various signals, to make sure things get shut down cleanly.
//
static void signal_handler(int num)
{
static char msg[] = "Oh no! Back to reality!\r\n";

    //D_QuitNetGame ();  //say 'byebye' to other players when your machine
                        // crashes?... hmm... do they have to die with you???

    I_ShutdownSystem();

    _write(STDERR_FILENO, msg, sizeof(msg)-1);

    signal(num, SIG_DFL);
    raise(num);
}


//added:08-01-98: now this replaces allegro_init()
//
//  REMEMBER: THIS ROUTINE MUST BE STARTED IN i_main.c BEFORE D_DoomMain()
//
//  This stuff should get rid of the exception and page faults when
//  Doom bugs out with an error. Now it should exit cleanly.
//
int  I_StartupSystem(void)
{

   // some 'more globals than globals' things to initialize here ?
   graphics_started = false;
   keyboard_started = false;
   sound_started = false;
   timer_started = false;
   cdaudio_started = false;

   // check for OS type and version here ?


   signal(SIGABRT, signal_handler);
   signal(SIGFPE , signal_handler);
   signal(SIGILL , signal_handler);
   signal(SIGSEGV, signal_handler);
   signal(SIGTERM, signal_handler);
   signal(SIGINT , signal_handler);
   signal(SIGKILL, signal_handler);
   signal(SIGQUIT, signal_handler);

   return 0;
}


//added:08-01-98:
//
//  Closes down everything. This includes restoring the initial
//  pallete and video mode, and removing whatever mouse, keyboard, and
//  timer routines have been installed.
//
//  NOTE : Shutdown user funcs. are effectively called in reverse order.
//
void I_ShutdownSystem()
{
   int c;

   for (c=MAX_QUIT_FUNCS-1; c>=0; c--)
      if (quit_funcs[c])
         (*quit_funcs[c])();

}
