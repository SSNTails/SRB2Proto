
// f_wipe.h : Mission start screen wipe/melt, special effects.


#ifndef __F_WIPE_H__
#define __F_WIPE_H__

//--------------------------------------------------------------------------
//                        SCREEN WIPE PACKAGE
//--------------------------------------------------------------------------

enum
{
    // simple gradual pixel change for 8-bit only
    wipe_ColorXForm,

    // weird screen melt
    wipe_Melt,

    wipe_NUMWIPES
};


int wipe_StartScreen
( int           x,
  int           y,
  int           width,
  int           height );


int wipe_EndScreen
( int           x,
  int           y,
  int           width,
  int           height );


int wipe_ScreenWipe
( int           wipeno,
  int           x,
  int           y,
  int           width,
  int           height,
  int           ticks );

#endif
