// m_argv.c :

#include <string.h>

#include "doomdef.h"
#include "command.h"

int             myargc;
char**          myargv;
static int      found;

//
// M_CheckParm
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
int M_CheckParm (char *check)
{
    int         i;

    for (i = 1;i<myargc;i++)
    {
        if ( !strcasecmp(check, myargv[i]) )
        {
            found=i;
            return i;
        }
    }
    found=0;
    return 0;
}

// return true if there is available parameters
boolean M_IsNextParm(void)
{
    if(found>0 && found+1<myargc && myargv[found+1][0] != '-' && myargv[found+1][0] != '+')
        return true;
    return false;
}

// return the next parameter after a M_CheckParm
// NULL if not found use M_IsNext to fin,d if there is a parameter
char *M_GetNextParm(void)
{
    if(M_IsNextParm())
    {
        found++;
        return myargv[found];
    }
    return NULL;
}

// push all parameters begining by '+'
void M_PushSpecialParameters( void )
{
    int     i;
    char    s[256];
    boolean onetime=false;

    for (i = 1;i<myargc;i++)
    {
        if ( myargv[i][0]=='+' )
        {
            strcpy(s,&myargv[i][1]);
            i++;

            // get the parameter of the command too
            for(;i<myargc && myargv[i][0]!='+' && myargv[i][0]!='-' ;i++)
            {
                strcat(s," ");
                if(!onetime) { strcat(s,"\"");onetime=true; }
                strcat(s,myargv[i]);
            }
            if( onetime )    { strcat(s,"\"");onetime=false; }
            strcat(s,"\n");

            // push it
            COM_BufAddText (s);
            i--;
        }
    }
}
