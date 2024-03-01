// m_fixed.c :  fixed point implementation.

#include "i_system.h"
#include "m_fixed.h"

#ifdef __WIN32__
#define INT64  __int64
#else
#define INT64  long long
#endif

// Fixme. __USE_C_FIXED__ or something.
#ifndef USEASM
fixed_t FixedMul (fixed_t a, fixed_t b)
{
    return ((INT64) a * (INT64) b) >> FRACBITS;
}
fixed_t FixedDiv2 (fixed_t a, fixed_t b)
{
#if 0
    INT64 c;
    c = ((INT64)a<<16) / ((INT64)b);
    return (fixed_t) c;
#endif

    double c;

    c = ((double)a) / ((double)b) * FRACUNIT;

    if (c >= 2147483648.0 || c < -2147483648.0)
        I_Error("FixedDiv: divide by zero");
    return (fixed_t) c;
}

/*
//
// FixedDiv, C version.
//
fixed_t FixedDiv ( fixed_t   a, fixed_t    b )
{
    //I_Error("<a: %ld, b: %ld>",(long)a,(long)b);

    if ( (abs(a)>>14) >= abs(b))
        return (a^b)<0 ? MININT : MAXINT;

    return FixedDiv2 (a,b);
}
*/
#endif // useasm
