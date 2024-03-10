
//
// asm_defs.h : must match the C structures !!!
//

#ifndef __ASM_DEFS__
#define __ASM_DEFS__

 // this makes variables more noticable,
// and make the label match with C code

// Linux, unlike DOS, has no "_" 19990119 by Kin
#ifdef LINUX
#define C(label) label
#else
#define C(label) _##label
#endif

/* This is a more readable way to access the arguments passed from C code   */
/* PLEASE NOTE: it is supposed that all arguments passed from C code are    */
/*              32bit integer (int, long, and most *pointers)               */
#define ARG1      8(%ebp)
#define ARG2      12(%ebp)
#define ARG3      16(%ebp)
#define ARG4      20(%ebp)
#define ARG5      24(%ebp)
#define ARG6      28(%ebp)
#define ARG7      32(%ebp)
#define ARG8      36(%ebp)
#define ARG9      40(%ebp)      //(c)tm ... Allegro by Shawn Hargreaves.


#endif
