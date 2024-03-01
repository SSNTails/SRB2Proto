// Fixed point arithemtics, implementation.

#ifndef __M_FIXED__
#define __M_FIXED__

#include "doomtype.h"

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS                16
#define FRACUNIT                (1<<FRACBITS)
typedef int fixed_t;
#define FIXED_TO_FLOAT(x) (((float)x) / 65536.0)

//
// Declare those functions:
/*
fixed_t FixedMul (fixed_t a, fixed_t b);
fixed_t FixedDiv (fixed_t a, fixed_t b);
fixed_t FixedDiv2 (fixed_t a, fixed_t b);
*/


#ifdef __WIN32__
    //Microsoft VisualC++
    fixed_t __cdecl FixedMul (fixed_t a, fixed_t b);
    //fixed_t FixedDiv (fixed_t a, fixed_t b);
    fixed_t __cdecl FixedDiv2 (fixed_t a, fixed_t b);
#else
    #ifdef __WATCOMC__
    #pragma aux FixedMul =  \
        "imul ebx",         \
        "shrd eax,edx,16"   \
        parm    [eax] [ebx] \
        value   [eax]       \
        modify exact [eax edx]

    #pragma aux FixedDiv2 = \
        "cdq",              \
        "shld edx,eax,16",  \
        "sal eax,16",       \
        "idiv ebx"          \
        parm    [eax] [ebx] \
        value   [eax]       \
        modify exact [eax edx]
    #else
    //DJGPP
        static inline fixed_t FixedMul (fixed_t a, fixed_t b)         //asm
        {
  fixed_t ret;
  asm (
        "imull %%edx             \n"
        "shrdl $16, %%edx,%%eax  \n"

       : "=a" (ret)
       : "a" (a), "d" (b)
       : "%edx" );
  return ret;
        }
        static inline fixed_t FixedDiv2 (fixed_t a, fixed_t b)
        {
  fixed_t ret;
  asm (
         "movl  %%eax,%%edx      \n" // these two instructions allow the next
         "sarl  $31,%%edx        \n" // two to pair, on the Pentium processor.
         "shldl $16,%%eax,%%edx  \n"
         "sall  $16,%%eax        \n"
         "idivl %%ecx            \n"

       : "=a" (ret)
       : "a" (a), "c" (b)
       : "%edx" );
  return ret;
        }
    #endif
#endif

static inline fixed_t FixedDiv (fixed_t a, fixed_t b)
{
    //I_Error("<a: %ld, b: %ld>",(long)a,(long)b);

    if ( (abs(a)>>14) >= abs(b))
        return (a^b)<0 ? MININT : MAXINT;

    return FixedDiv2 (a,b);
}

#endif //m_fixed.h
