
// doomtype.h : doom games standard types

#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifdef __WIN32__
#include <windows.h>
#endif
typedef unsigned long ULONG;
typedef unsigned short USHORT;

#ifdef __MSC__
    // Microsoft VisualC++
    #define strncasecmp             strnicmp
    #define strcasecmp              stricmp
    #define inline                  __inline
#else
    #ifdef __WATCOMC__
        #include <dos.h>
        #include <sys\types.h>
        #include <direct.h>
        #include <malloc.h>
        #define strncasecmp             strnicmp
        #define strcasecmp              strcmpi
    #endif
#endif
// added for Linux 19990220 by Kin
#ifdef LINUX
#define stricmp(x,y) strcasecmp(x,y)
#define strnicmp(x,y,n) strncasecmp(x,y,n)
#endif


#ifndef __BYTEBOOL__
    #define __BYTEBOOL__

    // Fixed to use builtin bool type with C++.
    //#ifdef __cplusplus
    //    typedef bool boolean;
    //#else

        typedef unsigned char byte;

        //faB: clean that up !!
        #ifdef __WIN32__
            #define false   FALSE           // use windows types
            #define true    TRUE
            #define boolean BOOL
        #else
            typedef enum {false, true} boolean;
        #endif
    //#endif // __cplusplus
#endif // __BYTEBOOL__


// Predefined with some OS.
#ifndef __WIN32__
#include <values.h>
#endif

#ifndef MAXCHAR
#define MAXCHAR   ((char)0x7f)
#endif
#ifndef MAXSHORT
#define MAXSHORT  ((short)0x7fff)
#endif
#ifndef MAXINT
#define MAXINT    ((int)0x7fffffff)
#endif
#ifndef MAXLONG
#define MAXLONG   ((long)0x7fffffff)
#endif

#ifndef MINCHAR
#define MINCHAR   ((char)0x80)
#endif
#ifndef MINSHORT
#define MINSHORT  ((short)0x8000)
#endif
#ifndef MININT
#define MININT    ((int)0x80000000)
#endif
#ifndef MINLONG
#define MINLONG   ((long)0x80000000)
#endif


#endif  //__DOOMTYPE__
