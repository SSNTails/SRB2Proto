/*
 * CD audio support for XDoom
 *
 * Copyright (C) 1998 by Udo Munk <udo@umserver.umnet.de>
 *
 * This code is provided AS IS and there are no guarantees, none.
 * Feel free to share and modify, if you make some improvements I
 * would like to see them please.
 */

/*
 * This CD audio module uses the Linux/BSD/Solaris ioctl()
 * interface, to play audio CD's.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>

#ifdef LINUX
#include <sys/ioctl.h>
#include <linux/cdrom.h>
//#include <linux/time.h>
#endif

#define DRIVE "/dev/cdrom"	/* the device to use */
#define STARTTRACK 1

void start_timer(void);
void stop_timer(void);
void handle_timer(int);

static int fd;			/* filedescriptor for the CD drive */
//static int track = STARTTRACK-1;/* current playing track */
static struct cdrom_tochdr toc_header;	/* TOC header */
static struct cdrom_ti ti;		/* track/index to play */
static struct cdrom_volctrl volume;	/* volume */
static struct itimerval value, ovalue;	/* timer for sub-channel control */
static struct sigaction act, oact;

/*
 * This function is called once by XDoom to initialize the CD audio
 * system.
 * 
 * This function should return 0 if it was possible to initialize
 * CD audio successfull. It should return another value if there
 * was any problem, in this case XDoom will fall back to playing
 * music with musserver.
 */
void I_InitCD(void)
{
    fprintf(stdout, "CD audio (ioctl): initializing...\n");

    /* try to open the CD drive */
    if ((fd = open(DRIVE, O_RDONLY)) < 0) {
	fprintf(stderr, "CD audio (ioctl): can't open %s\n", DRIVE);
	return;	/* failed, use musserver */
    }

    /* turn drives motor on, some drives won't work without */
    if (ioctl(fd, CDROMSTART) != 0) {
	fprintf(stderr, "CD audio (ioctl): can't start drive motor\n");
	return;	/* failed, use musserver */
    }

    /* get the TOC header, so we know how many audio tracks */
    if (ioctl(fd, CDROMREADTOCHDR, &toc_header) != 0) {
	fprintf(stderr, "CD audio (ioctl): can't read TOC header\n");
	return;	/* failed, use musserver */
    } else {
	fprintf(stderr, "CD audio (ioctl): using tracks %d - %d\n",
		(int) toc_header.cdth_trk0,
		(int) toc_header.cdth_trk1);
    }
	
    fprintf(stdout, "CD audio (ioctl): ready\n");

    return;		/* good lets play CD audio... */
}

/*
 * This function is called once by XDoom if the program terminates.
 * Just stop playing and close the drive...
 */
void I_ShutdownCD(void)
{
    stop_timer();
    ioctl(fd, CDROMSTOP);
    close(fd);
}

/*
 * XDoom registers the next song to play with the resource names found
 * in IWAD files. One could think up an algorithm to map CD tracks to
 * the sound tracks XDoom uses, but I don't have a good idea for that in
 * the moment, so this implementation here is rather simple. It just
 * playes one track after the other and in case you start a new game
 * the CD is played from the beginning. Please note that the starting
 * track is set to track 2. This is because of the Quake CD, track 1
 * contains the program and the songs start with track 2. If you don't
 * like this change the define STARTTRACK above.
 */
/* removed.... */

/*
 * XDoom calls this function to start playing the song registered before.
 */
void I_PlayCD(int track,boolean looping)
{
    struct cdrom_tocentry toc;
try_next:
    if(track > toc_header.odth_trk1)
       track = (track % (toc_header.odth_trk1-STARTTRACK+1)) + 1;
/* get toc entry for the track */
    toc.cdte_track = track;
    toc.cdte_format = CDROM_MSF;
    ioctl(fd, CDROMREADTOCENTRY, &toc);
    
    /* is this an audio track? */
    if (toc.cdte_ctrl & CDROM_DATA_TRACK) {
	/* nope, try next track */
	track++;
        goto try_next;
    }

    ti.cdti_trk0 = track;
    ti.cdti_ind0 = 0;
    ti.cdti_trk1 = track;
    ti.cdti_ind1 = 0;
    ioctl(fd, CDROMPLAYTRKIND, &ti);
    if(looping)
       start_timer();
}

/*
 * XDoom calls this function to stop playing the current song.
 */
void I_StopTrackCD(void)
{
    stop_timer();
    ioctl(fd, CDROMSTOP);
}

/*
 * XDoom calls this function to pause the current playing song.
 * their "pause" is here 19990110 by Kin 
*/
void I_StopCD(int handle)
{
    stop_timer();
    ioctl(fd, CDROMPAUSE);
}

/*
 * XDoom calls this function to resume playing the paused song.
 */
void I_ResumeCD(void)
{
    ioctl(fd, CDROMRESUME);
    start_timer();
}

/*
 * XDoom calls this function whenever the volume for the music is changed.
 * The ioctl() interface uses a volume setting of 0 - 255, so the values
 * XDoom sends are mapped into this valid range.
 */
void I_SetVolumeCD(int vol)
{
    if ((vol >= 0) && (vol <= 15)) {
	volume.channel0 =  vol * 255 / 15;
	volume.channel1 =  vol * 255 / 15;
	volume.channel2 =  0;
	volume.channel3 =  0;
	ioctl(fd, CDROMVOLCTRL, &volume);
    }
}

/*
 * start a 1 second timer
 */
void start_timer()
{
    act.sa_handler = handle_timer;
    act.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &act, &oact);

    value.it_interval.tv_sec	= 1;
    value.it_interval.tv_usec	= 0;
    value.it_value.tv_sec	= 1;
    value.it_value.tv_usec	= 0;
    setitimer(ITIMER_REAL, &value, &ovalue);
}

/*
 * stop the timer
 */
void stop_timer()
{
    setitimer(ITIMER_REAL, &ovalue, NULL);
    sigaction(SIGALRM, &oact, NULL);
}

/*
 * Check the CD drives sub-channel information, if the track is completed
 * and if so, restart it.
 */
void handle_timer(int val)
{
    static struct cdrom_subchnl subchnl;

    subchnl.cdsc_format = CDROM_MSF;	/* get result in MSF format */
    ioctl(fd, CDROMSUBCHNL, &subchnl);

    if (subchnl.cdsc_audiostatus == CDROM_AUDIO_COMPLETED) {
	ti.cdti_trk0 = track;
	ti.cdti_ind0 = 0;
	ti.cdti_trk1 = track;
	ti.cdti_ind1 = 0;
	ioctl(fd, CDROMPLAYTRKIND, &ti);
    }
}
