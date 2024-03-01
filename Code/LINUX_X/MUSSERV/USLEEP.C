#include <signal.h>
#include <sys/types.h>

#ifdef SCOOS5
#include <sys/itimer.h>
#endif

#if defined(SCOUW2) || defined(SCOUW7)
#include <sys/time.h>
#endif

#include "usleep.h"

extern int pause (void);
extern pid_t getpid (void);

volatile static int waiting;

static void getalrm(i)
  int i;
{
    waiting = 0;
}

void usleep(t)
  unsigned t;
{
    static struct itimerval it, ot;
    void (*oldsig)();
    long nt;

    it.it_value.tv_sec = t / 1000000;
    it.it_value.tv_usec = t % 1000000;
    oldsig = (void (*)()) signal(SIGALRM, getalrm);
    waiting = 1;
    if (setitimer(ITIMER_REAL, &it, &ot))
	return /*error*/;
    while (waiting) {
	pause();
    }
    signal(SIGALRM, oldsig);
    if (ot.it_value.tv_sec + ot.it_value.tv_usec > 0) {
      nt = ((ot.it_value.tv_sec * 1000000L) + ot.it_value.tv_usec) - t;
      if (nt <= 0) {
	kill(getpid(), SIGALRM);
      } else {
	ot.it_value.tv_sec = nt / 1000000;
	ot.it_value.tv_usec = nt % 1000000;
	setitimer(ITIMER_REAL, &ot, 0);
      }
    }
}
