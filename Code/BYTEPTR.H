#define WRITEBYTE(p,b)      *((byte   *)p)++ = b
#define WRITECHAR(p,b)      *((char   *)p)++ = b
#define WRITESHORT(p,b)     *((short  *)p)++ = b
#define WRITEUSHORT(p,b)    *((USHORT *)p)++ = b
#define WRITELONG(p,b)      *((long   *)p)++ = b
#define WRITEULONG(p,b)     *((ULONG  *)p)++ = b
#define WRITEFIXED(p,b)     *((fixed_t*)p)++ = b
#define WRITESTRING(p,b)    { int tmp_i=0; do { WRITECHAR(p,b[tmp_i]); } while(b[tmp_i++]); }
#define WRITESTRINGN(p,b,n) { int tmp_i=0; do { WRITECHAR(p,b[tmp_i]); if(!b[tmp_i]) break;tmp_i++; } while(tmp_i<n); }

#define READBYTE(p)         *((byte   *)p)++
#define READCHAR(p)         *((char   *)p)++
#define READSHORT(p)        *((short  *)p)++
#define READUSHORT(p)       *((USHORT *)p)++
#define READLONG(p)         *((long   *)p)++
#define READULONG(p)        *((ULONG  *)p)++
#define READFIXED(p)        *((fixed_t*)p)++
#define READSTRING(p,s)     { int tmp_i=0; do { s[tmp_i]=READBYTE(p);  } while(s[tmp_i++]); }
#define SKIPSTRING(p)       while(READBYTE(p))
