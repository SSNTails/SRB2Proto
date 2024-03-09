// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief Macros to read/write from/to a char*, used for packet creation and such

#ifndef __BIG_ENDIAN__
//
// Little-endian machines
//
#define WRITEBYTE(p,b)      do { byte* p_tmp = (byte*)p; *p_tmp = (byte)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITECHAR(p,b)      do { char* p_tmp = (char*)p; *p_tmp = (char)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITESHORT(p,b)     do { short* p_tmp = (short*)p; *p_tmp = (short)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITEUSHORT(p,b)    do { USHORT* p_tmp = (USHORT*)p; *p_tmp = (USHORT)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITELONG(p,b)      do { long* p_tmp = (long*)p; *p_tmp = (long)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITEULONG(p,b)     do { ULONG* p_tmp = (ULONG*)p; *p_tmp = (ULONG)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITEFIXED(p,b)     do { fixed_t* p_tmp = (fixed_t*)p; *p_tmp = (fixed_t)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITEANGLE(p,b)     do { angle_t* p_tmp = (angle_t*)p; *p_tmp = (angle_t)(b); p_tmp++; p = (void*)p_tmp; } while(0)
#define WRITESTRING(p,b)    { int tmp_i = 0; do { WRITECHAR(p, b[tmp_i]); } while(b[tmp_i++]); }
#define WRITESTRINGN(p,b,n) { int tmp_i = 0; do { WRITECHAR(p, b[tmp_i]); if(!b[tmp_i]) break; tmp_i++; } while(tmp_i < n); }
#define WRITEMEM(p,s,n)     memcpy(p, s, n);p+=n
#define WRITEDOUBLE(p,b)    do { double *p_tmp = (double *)p; *p_tmp = (double)(b); p_tmp++; p = (void*)p_tmp; } while(0)

#ifdef __GNUC__
#define READBYTE(p)         ({ byte* p_tmp = (byte*)p; byte b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READCHAR(p)         ({ char* p_tmp = (char*)p; char b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READSHORT(p)        ({ short* p_tmp = (short*)p; short b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READUSHORT(p)       ({ USHORT* p_tmp = (USHORT*)p; USHORT b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READLONG(p)         ({ long* p_tmp = (long*)p; long b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READULONG(p)        ({ ULONG* p_tmp = (ULONG*)p; ULONG b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READFIXED(p)        ({ fixed_t* p_tmp = (fixed_t*)p; fixed_t b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READANGLE(p)        ({ angle_t* p_tmp = (angle_t*)p; angle_t b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#define READDOUBLE(p)       ({ double* p_tmp = (double*)p; double b = *p_tmp; p_tmp++; p = (void*)p_tmp; b; })
#else
#define READBYTE(p)         *((byte   *)p)++
#define READCHAR(p)         *((char   *)p)++
#define READSHORT(p)        *((short  *)p)++
#define READUSHORT(p)       *((USHORT *)p)++
#define READLONG(p)         *((long   *)p)++
#define READULONG(p)        *((ULONG  *)p)++
#define READFIXED(p)        *((fixed_t*)p)++
#define READANGLE(p)        *((angle_t*)p)++
#define READDOUBLE(p)       *((double *)p)++
#endif

#define READSTRING(p,s)     { int tmp_i = 0; do { s[tmp_i] = READBYTE(p); } while(s[tmp_i++]); }
#define SKIPSTRING(p)       while(READBYTE(p))
#define READMEM(p,s,n)      memcpy(s, p, n); p += n
#else
//
// definitions for big-endian machines with alignment constraints.
//
// Write a value to a little-endian, unaligned destination.
//
static inline void writeshort(void * ptr, int val)
{
	char * cp = ptr;
	cp[0] = val ;  val >>= 8;
	cp[1] = val ;
}

static inline void writelong(void * ptr, int val)
{
	char * cp = ptr;
	cp[0] = val ;  val >>= 8;
	cp[1] = val ;  val >>= 8;
	cp[2] = val ;  val >>= 8;
	cp[3] = val ;
}

static inline void writedouble(void * ptr, INT64 val)
{
	char * cp = ptr;
	cp[0] = val ;  val >>= 8;
	cp[1] = val ;  val >>= 8;
	cp[2] = val ;  val >>= 8;
	cp[3] = val ;  val >>= 8;
	cp[4] = val ;  val >>= 8;
	cp[5] = val ;  val >>= 8;
	cp[6] = val ;  val >>= 8;
	cp[7] = val ;
}

#define WRITEBYTE(p,b)      *((byte   *)p)++ = (byte)(b)
#define WRITECHAR(p,b)      *((char   *)p)++ = (char)(b)
#define WRITESHORT(p,b)     writeshort(((short *)p)++,  (short)(b))
#define WRITEUSHORT(p,b)    writeshort(((u_short*)p)++, (USHORT)(b))
#define WRITELONG(p,b)      writelong (((long  *)p)++,  (long)(b))
#define WRITEULONG(p,b)     writelong (((u_long *)p)++, (u_long)(b))
#define WRITEFIXED(p,b)     writelong (((fixed_t*)p)++, (fixed_t)(b))
#define WRITEANGLE(p,b)     writelong (((angle_t*)p)++, (angle_t)(b))
#define WRITESTRING(p,b)    { int tmp_i=0; do { WRITECHAR(p,b[tmp_i]); } while(b[tmp_i++]); }
#define WRITESTRINGN(p,b,n) { int tmp_i=0; do { WRITECHAR(p,b[tmp_i]); if(!b[tmp_i]) break;tmp_i++; } while(tmp_i<n); }
#define WRITEMEM(p,s,n)     memcpy(p, s, n);p+=n
// New "functions" Tails
#define WRITEDOUBLE(p,b)      writedouble(((double*)p)++,  *(INT64 *)&b) //Alam_GBC: Test Me!

// Read a signed quantity from little-endian, unaligned data.
//
static inline short readshort(void * ptr)
{
	char   *cp  = ptr;
	u_char *ucp = ptr;
	return (cp[1] << 8)  |  ucp[0] ;
}

static inline u_short readushort(void * ptr)
{
	u_char *ucp = ptr;
	return (ucp[1] << 8) |  ucp[0] ;
}

static inline long readlong(void * ptr)
{
	char   *cp  = ptr;
	u_char *ucp = ptr;
	return (cp[3] << 24) | (ucp[2] << 16) | (ucp[1] << 8) | ucp[0] ;
}

static inline u_long readulong(void * ptr)
{
	u_char *ucp = ptr;
	return (ucp[3] << 24) | (ucp[2] << 16) | (ucp[1] << 8) | ucp[0] ;
}

//Alam_GBC: From SDL

static inline unsigned int Swapui(unsigned int D)
{
	return((D<<24)|((D<<8)&0x00FF0000)|((D>>8)&0x0000FF00)|(D>>24));
}

static inline double readouble(void * ptr)
{
	unsigned int hi, lo;
	long long llr, *val = ptr;

	llr = *val;
	/* Separate into high and low 32-bit values and swap them */
	lo = (unsigned int)(llr&0xFFFFFFFF);
	llr >>= 32;
	hi = (unsigned int)(llr&0xFFFFFFFF);
	llr = Swapui(lo);
	llr <<= 32;
	llr |= Swapui(hi);
	return *(double *)&llr;
}

#define READBYTE(p)         *((byte   *)p)++
#define READCHAR(p)         *((char   *)p)++
#define READSHORT(p)        readshort ( ((short*) p)++)
#define READUSHORT(p)       readushort(((USHORT*) p)++)
#define READLONG(p)         readlong  (  ((long*) p)++)
#define READULONG(p)        readulong ( ((ULONG*) p)++)
#define READFIXED(p)        readlong  (  ((long*) p)++)
#define READANGLE(p)        readulong ( ((ULONG*) p)++)
#define READDOUBLE(p)       readouble (((double*)p)++) //Alam_GBC: Test Me!
#define READSTRING(p,s)     { int tmp_i=0; do { s[tmp_i]=READBYTE(p);  } while(s[tmp_i++]); }
#define SKIPSTRING(p)       while(READBYTE(p))
#define READMEM(p,s,n)      memcpy(s, p, n);p+=n
#endif //__BIG_ENDIAN__
