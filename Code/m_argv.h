
// m_argv.h :

#ifndef __M_ARGV__
#define __M_ARGV__

//
// MISC
//
extern  int     myargc;
extern  char**  myargv;

// Returns the position of the given parameter
// in the arg list (0 if not found).
int  M_CheckParm (char* check);


// push all parameters bigining by a +, ex : +map map01
void M_PushSpecialParameters( void );

// return true if there is available parameters
// use it befor M_GetNext 
boolean M_IsNextParm(void);

// return the next parameter after a M_CheckParm
// NULL if not found use M_IsNext to find if there is a parameter
char *M_GetNextParm(void);


#endif //__M_ARGV__
