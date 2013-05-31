/****************************************************************
 * $ID: ucap.c         Sun, 01 Aug 2004 18:15:14 +0800  mhfan $ *
 *                                                              *
 * Description:                                                 *
 *                                                              *
 * Maintainer:  ∑∂√¿ª‘(Meihui Fan)  <mhfan@ustc.edu>            *
 *                                                              *
 * CopyRight (c)  2004  HHTech                                  *
 *   www.hhcn.com, www.hhcn.org                                 *
 *   All rights reserved.                                       *
 *                                                              *
 * This file is free software;                                  *
 *   you are free to modify and/or redistribute it   	        *
 *   under the terms of the GNU General Public Licence (GPL).   *
 *                                                              *
 * Last modified: Thu, 09 Dec 2004 10:42:38 +0800      by mhfan #
 ****************************************************************/

#include <linux/soundcard.h>

#include "wmadec.h"
#include "mp3dec.h"
#include "common.h"

#define	FRAME_SIZE		    WMA_FRAME_SIZE
#define	SARCO_BUF_SIZE		    (2*WMA_DEC_BUF_SIZE)
//#define	FRAME_SIZE		    MP3_FRAME_SIZE
//#define	SARCO_BUF_SIZE		    (3*MP3_DEC_BUF_SIZE)
#define	MAX_WAIT_LOOP		    256

#define PRINT_VERSION		    do { \
	fprintf(stdout, "\033[2J\n\033[1;33mThe \033[31muC\033[33mLinux " \
			"\033[31mA\033[33mudio \033[31mP\033[33mlayer" \
			"\033[0m (\033[32mV \033[1m0.1\033[0m)\n" \
			"\033[1;34m--- maintained by " \
			"\033[36mMeihui Fan \033[0;37m" \
			"<\033[35mmhfan@ustc.edu\033[37m>\033[0m\n\n" \
		); } while (0)	    /* mhfan */

/* global variable definitions */
int dspfd;
pid_t pid_hdl;
int play_frames;
ShmData* shm=(ShmData*)GUI_SHM_BASE;
CmdStatus cmd_status = CMD_STAT_READY;
PlayList* playlist=(PlayList*)(GUI_SHM_BASE - sizeof (PlayList));

tWMAFileHeader* wma_fh;
short buf_out_pcm[2*FRAME_SIZE];
long sram_ptr = SRAM_BASE;
#ifndef	DISABLE_SARCO
short sarco_out_buf[SARCO_BUF_SIZE];
SARCO_global_struct* sarco_ptr;
#endif//DISABLE_SARCO

void init_playlist(int fsum, char* path[]);
void reset_playlist(int first, int last, PlayRule rule);
CmdStatus proc_cmd();
void preload(FileInfo* fi);

extern void mp3dec(void);
extern void init_mp3d(void);
extern int  init_wmad(void);
extern void wmadec(void);

static void sighdl_ready(int signum) { }

static void sighdl_term(int signum)
{
    free(playlist->fiarr);	      close(dspfd);
#ifdef	CLI_CONTROL
    fprintf(stdout, "End of playback, Goodbye!\n");
#endif//CLI_CONTROL
    if (kill(pid_hdl, SIGTERM)) EXIT(ERR_SIG_SEND);
    usleep (1000);
}

static inline void init_dsp(int freq, int stereo, int playbits)
{
    // Check if data stream is stereo, otherwise must play mono.
    int bits = playbits==16 ? AFMT_S16_BE : AFMT_U8;
    if (ioctl(dspfd, SNDCTL_DSP_SPEED, &freq)	   < 0 || // slowly
	ioctl(dspfd, SNDCTL_DSP_STEREO, &stereo)   < 0 ||
	ioctl(dspfd, SNDCTL_DSP_SAMPLESIZE, &bits) < 0)
        dprint("ERROR: Unable to initialize the DSP defice!\n");
}

static inline void clear_dspbuf()
{
#define SNDCTL_DSP_CLEARBUF	_SIO ('P', 11)
    int speed = 0;
    ioctl(dspfd, SNDCTL_DSP_SPEED, &speed);	// slowlly blocked
    ioctl(dspfd, SNDCTL_DSP_CLEARBUF);
    speed = 44100;
    ioctl(dspfd, SNDCTL_DSP_SPEED, &speed);
}

void play_progress(/*int sec*/)
{
    int sec=0, wav_GetDecodedTime(void);
    switch (playlist->pcur->fmt) {
    case AUDIO_MP3:
	sec = play_frames * 418/*bytes*/ / (128/*Kbps*/ * 125);	break;
    case AUDIO_WMA:
	sec = play_frames * WMA_FRAMES_PER_THOUSAND / 1000;	break;
#if 0 	/* comment by mhfan */
	    play_frames *
		(playlist->poff - wma_fh->first_packet_offset) /
		 wma_fh->packet_size / 1000;
	    (float)((float)wma_fh->duration * 
	    (float)(playlist->poff - wma_fh->first_packet_offset) /
	    (float)(wma_fh->last_packet_offset - wma_fh->first_packet_offset +
		    wma_fh->packet_size) / (float)1000);
#endif	/* comment by mhfan */
    case AUDIO_WAV:
	sec = wav_GetDecodedTime();
    case AUDIO_OGG:		case AUDIO_UNK:
    }
    // considered the PCM-DSP buffer here
#ifdef	GUI_CONTROL
    { int ratio = playlist->poff * 100 / playlist->pcur->size;
    shm->elapsed = sec;		shm->ratio = ratio;
    }
#endif//GUI_CONTROL
#ifdef	CLI_CONTROL
    {	int hr=0, min=0, i;
    if (sec > 3599) { hr = sec / 3600;	sec %= 3600; }
    if (sec > 59)   { min= sec / 60;	sec %= 60;   }
    ratio = ratio * 7 / 10;
    fprintf(stdout, " \033[32m");
    for (i=0; i < ratio; ++i) fprintf(stdout, "#");
    for (; i < 70; ++i)	      fprintf(stdout, " ");
    if (hr) fprintf(stdout, " \033[36m[\033[1;31m%02d:", hr);
    else    fprintf(stdout, " \033[36m[\033[1;31m");
    fprintf(stdout, "%02d:%02d\033[0;36m]\033[0m\r", min, sec);
    fflush (stdout);
    }
#endif//CLI_CONTROL
}

void play_fileinfo(int bitrate, int freq, int duration
#ifdef	CLI_CONTROL
		 , int chans, char* verstr
#endif//CLI_CONTROL
		  )
{
    char* unk="Unknown";
#ifdef	GUI_CONTROL
    char* str = playlist->pcur->id3tag.title  ?
		playlist->pcur->id3tag.title  : unk;
    strncpy(shm->title +1, str, MAX_TAGINFO_SIZE-1);
	  str = playlist->pcur->id3tag.artist ?
		playlist->pcur->id3tag.artist : unk;
    strncpy(shm->artist+1, str, MAX_TAGINFO_SIZE-1);
    shm->title[0] = shm->artist[0] = 'r';
    shm->duration = duration;
    shm->bitrate  = bitrate;
    shm->freq	  = freq;
#endif//GUI_CONTROL
#ifdef	CLI_CONTROL
    {	int hr=0, min=0;
    if (duration > 3599) { hr = duration / 3600; duration %= 3600; }
    if (duration > 59)	 { min= duration / 60;	 duration %= 60;   }
    fprintf(stdout, "\033[1;32mTitle:  %s\nArtist: %s\n"
		    "%s, %dKHz, %dKbps(%s), %s(%d)\n"
		    "Playback in progress ......"
		    " [%d:%02d:%02d]\033[0m\n",
		    playlist->pcur->id3tag.title  ?
		    playlist->pcur->id3tag.title  : unk,
		    playlist->pcur->id3tag.artist ?
		    playlist->pcur->id3tag.artist : unk,
		    verstr, freq, bitrate<0 ? -bitrate : bitrate,
		    bitrate<0 ? "VBR"  : "CBR",
		    (chans==1 ? "Mono" : "Stereo"), chans,  // debug
		    hr, min, duration);
    }
#endif//CLI_CONTROL
}

inline void preload(FileInfo* fi)
{
    if (fi == playlist->lcur)		     return;

    if (fi->loff < fi->size)		     playlist->lcur = fi;
    else if (fi->next->loff < BUF_THRESHOLD) playlist->lcur = fi->next;
    else if (fi->next->offset > (BUF_TOTAL_SIZE>>3)) {
	fi->next->loff = fi->next->offset = 0;	
	playlist->lcur = fi->next;
    }	else				     return;
dtrace;
    if (kill(pid_hdl, SIG_LOAD))
	dprint("Failed to send signal to hdload.\n");
}

CmdStatus proc_cmd()
{
    extern unsigned mp3_frame_len;
    FileInfo* fi = playlist->pcur;
    int  fbwd=0, undelay=0, frame_len=0, pause=0, cmd=CMD_INVALID;
#ifndef	GUI_CONTROL
    fd_set fdset;		struct timeval tv={0, 0};
#endif//GUI_CONTROL
    do {
LOOP:
#ifndef	GUI_CONTROL
	FD_ZERO(&fdset);	FD_SET(STDIN_FILENO, &fdset);
	if (select(1, &fdset, NULL, NULL, &tv)>0 &&
				FD_ISSET(STDIN_FILENO, &fdset))
	    cmd = getchar();
	else if (pause) {	usleep(SUSPEND_USEC);	continue; }
	else if (fbwd) {	cmd = fbwd;		break;	  }
	else			return CMD_STAT_READY;
#else	/* get command from GUI controlling */
	if (shm->inform != COMM_COMMAND) {
	    if (pause) {	usleep(SUSPEND_USEC);	continue; }
	    else if (fbwd) {	cmd = fbwd;		break;	  }
	    else		return CMD_STAT_READY;
	}
	shm->inform = COMM_INVALID;
	while ((cmd = shm->command) == CMD_INVALID) usleep(DELAY_USEC);
	shm->command= CMD_INVALID;
#endif//GUI_CONTROL
	dprint("get a command: `%c'(%d).\n", cmd, cmd);
	if (cmd == CMD_PAUSE) {
#if 1 	/* comment by mhfan */
	    int speed = (pause = !pause) ? 0 : 44100;
	    ioctl(dspfd, SNDCTL_DSP_SPEED, &speed);
#else	// refer to MPlayer libao2/ao_oss.c
	    if ((pause = !pause)) {
		ioctl(dspfd, SNDCTL_DSP_RESET, NULL); close(dspfd);
	    } else if ((dspfd=open(DSP_PATH, O_WRONLY)) < 0)
		dprint("Can't open audio device: %s", DSP_PATH);
#endif 	/* comment by mhfan */
	}
    } while (pause);

    switch (cmd) {
#ifdef	GUI_CONTROL
    case CMD_OVERSONG:
    case CMD_SELECT:
	reset_playlist(shm->first, shm->last, shm->rule);
	playlist->pcur = playlist->fiarr + shm->selidx;
	playlist->pcur = playlist->pcur->prev;		break;
#endif//GUI_CONTROL
    case CMD_NEXT:					break;
    case CMD_PREV:	 playlist->sum += 2;
	playlist->pcur = fi->prev->prev;		break;
    case CMD_REPLAY:	 playlist->sum++;
	playlist->pcur = fi->prev;			break;
    case CMD_FORWARD:
	if (! fbwd) {	fbwd = cmd;    clear_dspbuf(); undelay = 1024;
	    switch (fi->fmt) {
	    case AUDIO_MP3:
		play_frames -= fi->bufs.cur->frames;
		if (!(shm->bitrate < 0 &&
			(frame_len = playlist->poff / play_frames)))
		    frame_len = mp3_frame_len;		break;
	    case AUDIO_WMA:
		play_frames -= fi->bufs.cur->frames;
		frame_len = fi->size * WMA_FRAMES_PER_THOUSAND /
			wma_fh->duration - 20;		break;
		//frame_len = wma_fh->packet_size / 5;	break;
	    case AUDIO_WAV:
	    case AUDIO_OGG:		case AUDIO_UNK:
	    }
	}
	if (fi->loff - playlist->poff < BUF_THRESHOLD) preload(fi);
	fi->bufs.cur = fi->bufs.cur->next;
	playlist->poff += BUF_MAX_SIZE;
	if (frame_len)
	    play_frames += (fi->bufs.cur->frames = BUF_MAX_SIZE / frame_len);
	    //play_frames += (fi->bufs.cur->frames =
	    //	    play_frames * BUF_MAX_SIZE / playlist->poff;
	else {	off_t offset = playlist->poff;
	    wav_AdpcmDecode(fi->bufs.cur->base, BUF_MAX_SIZE, NULL, &offset);
	}
	if (!(playlist->poff+BUF_MAX_SIZE < fi->size))	return CMD_STAT_RETURN;
	play_progress();		usleep(FBWD_DELAY - undelay);
	if (undelay < (FBWD_DELAY>>1))  undelay <<= 1;	goto LOOP;
    case CMD_BACKWARD:
	if (fi->bufs.cur == fi->bufs.begin) {		playlist->sum += 2;
	    playlist->pcur = fi->prev->prev;		return CMD_STAT_RETURN;
	}
	if (! fbwd) {	fbwd = cmd;	clear_dspbuf();	undelay = 1024; }
	playlist->poff -= BUF_MAX_SIZE;
	fi->bufs.cur = fi->bufs.cur->prev;
	play_frames -= fi->bufs.cur->frames;
	if (fi->offset > 0) {
	    preload(fi);		usleep(SUSPEND_USEC);
	}   play_progress();		usleep(FBWD_DELAY - undelay);
	if (undelay < (FBWD_DELAY>>1))  undelay <<= 1;	goto LOOP;
    case CMD_ENDFB:	fbwd = 0;			return CMD_STAT_CONT;
    case CMD_MEM:					return CMD_STAT_MEM;
    case CMD_NMEM:  playlist->pa = playlist->pb = 0;	return CMD_STAT_NMEM;
#ifdef	GUI_CONTROL
    case CMD_CHANGERULE: reset_playlist(shm->first, shm->last, shm->rule);
							return CMD_STAT_READY;
#endif//GUI_CONTROL
    case CMD_QUIT:	playlist->rule=PR_SEQUENCE;
			playlist->sum = 1;		break;
    default :						return CMD_STAT_READY;
    }			clear_dspbuf();			return CMD_STAT_RETURN;
}

void reset_playlist(int first, int last, PlayRule rule)
{
    int* flag=malloc((last = last-first+1) * sizeof (int));
    FileInfo *fi, *fi0 = playlist->fiarr + first;
    dprint("first=%d, last=%d, rule=%d\n", first, last+first-1, rule);

    srandom(time(NULL) | getpid());	// initilize a random generator.
    if (rule==PR_SHUFFLE || rule==PR_RANLOOP) {
	int i, *ran=malloc(last * sizeof (int));
	for (first=0; first < last; ++first) ran[first] = 0;
	for (first=0; first < last; ++first) {
	    while (ran[(i=random()%last)]) ;
	    flag[first] = i;	 ran[i] = 1;
	}			 free(ran);
    } else for (first=0; first < last; ++first) flag[first] = first;

    fi = playlist->lcur = fi0 + flag[0];
    for (first=1; first < last; ++first) {
	fi->next = fi0 + flag[first];
	fi->next->prev = fi;
	fi = fi->next;
    }
    playlist->sum  = last;
    playlist->rule = rule;
    fi->next = playlist->lcur;
    playlist->lcur->prev = fi;
    playlist->lcur = NULL;
    free(flag);
dtrace;
}

void init_playlist(int fsum, char* path[])
{
    int i;	    FileInfo* fi;
    if (! (fi = playlist->fiarr = malloc(fsum * sizeof (FileInfo))))
	EXIT(ERR_MALLOC);
    dprint("fsum=%d, path=%p\n", fsum, path);

    for (i=0; i < fsum; ++i) { char* fns=strrchr(path[i], '.');
	     if (!strcasecmp(fns, ".mp3")) fi->fmt = AUDIO_MP3;
	else if (!strcasecmp(fns, ".wma")) fi->fmt = AUDIO_WMA;
	else if (!strcasecmp(fns, ".wav")) fi->fmt = AUDIO_WAV;
	else if (!strcasecmp(fns, ".ogg")) fi->fmt = AUDIO_OGG;
	else 				   fi->fmt = AUDIO_UNK;
	dprint("### playlist->path: %s(idx=%d)\n", path[i], i);
	fi->offset = fi->loff = 0;
	fi->size = INVALID_SIZE;
	fi->path = path[i];
	fi = fi + 1;
    }
dtrace;
}

int main(int argc, char* argv[])
{
    PRINT_VERSION;
#ifndef	GUI_CONTROL
    if (argc < 2) {
	fprintf(stdout, "Usage: %s foo.mp3 bar.wma ...\n", argv[0]);
	return 0;
    }
    init_playlist(argc-1, &argv[1]);
    reset_playlist(0, argc-2, PR_RANLOOP);
    playlist->pcur = playlist->fiarr;
    playlist->lcur = NULL;
#else	/* get all path from GUI shared memeory */
    shm->sign = 1;		//usleep(SUSPEND_USEC);
    init_playlist(shm->fsum, shm->path);
    reset_playlist(shm->first, shm->last, shm->rule);
    playlist->pcur = playlist->fiarr + shm->selidx;
#endif//GUI_CONTROL

    if ((pid_hdl=vfork()) < 0)	EXIT(ERR_FORK);
    if ( pid_hdl==0 && execl(WORK_PATH HDL_NAME, HDL_NAME, NULL)<0)
				EXIT(ERR_CREATE_PROC);

    {	struct sigaction sar;
    sar.sa_handler = sighdl_ready;
    sar.sa_flags = SA_RESTART;
    sigemptyset(&sar.sa_mask);
    if (sigaction(SIG_READY, &sar, NULL) < 0) EXIT(ERR_SIG_INIT);

    sar.sa_handler = sighdl_term;
    if (sigaction(SIGTERM, &sar, NULL) < 0) EXIT(ERR_SIG_INIT);
    }				pause();

    setpriority(PRIO_PROCESS, 0, -20);	// Make self the top priority process!

    if ((dspfd = open(DSP_PATH,
#ifdef	DUMP_OUTPUTS
		    O_WRONLY | O_CREAT
#else
		    O_WRONLY//, 0660
#endif
		    )) < 0)	EXIT(ERR_DSP_OPEN);
    init_dsp(44100, 1, 16);

    for (;;) {	FileInfo* fi = playlist->pcur; int  cnt;
#ifdef	GUI_CONTROL
	for (cnt=0; shm->inform  == COMM_INVALID;) {
#ifdef	DEBUG
	    if (++cnt > MAX_WAIT_LOOP)
		dprint("Wait too long for GUI: %d.\n", __LINE__);
#endif//DEBUG
	    usleep(DELAY_USEC);
	}      shm->inform   = COMM_INVALID;
	for (cnt=0; shm->command == CMD_INVALID;) {
#ifdef	DEBUG
	    if (++cnt > MAX_WAIT_LOOP)
		dprint("Wait too long for GUI: %d.\n", __LINE__);
#endif//DEBUG
	    usleep(DELAY_USEC);
	}      shm->inform   = COMM_INVALID;
	if    (shm->command == CMD_QUIT)	break;
	       shm->command  = CMD_INVALID;
#endif//GUI_CONTROL
#ifdef	CLI_CONTROL
	fprintf(stdout, "\033[1mPlaying audio file: %s(idx=%ld). begin=%p\n",
		fi->path, fi - playlist->fiarr, fi->bufs.begin->base);
#endif//CLI_CONTROL
	if (fi->offset > (BUF_TOTAL_SIZE>>3) ||
		(fi->loff - fi->offset) < (BUF_TOTAL_SIZE>>3))
	    fi->loff = fi->offset = 0;
	for (cnt=0; fi->loff < (BUF_THRESHOLD<<1) || fi->offset > 0;) {
#if 0 	/* comment by mhfan */
	    if (cnt) {	cnt = 0;		preload(fi); }
	    usleep(SUSPEND_USEC);
#else
	    preload(fi);			usleep(SUSPEND_USEC);
#ifdef	DEBUG
	    if (++cnt > (MAX_WAIT_LOOP>>2))
		dprint("Wait too long for hdload: %d.\n", __LINE__);
#endif//DEBUG
#endif	/* comment by mhfan */
	    if (fi->size == 0) { fi->size = INVALID_SIZE;	goto NEXT; }
	}   playlist->pa = playlist->pb = playlist->poff = 0;
	fi->bufs.cur = fi->bufs.begin;
	switch (fi->fmt) {
	case AUDIO_MP3:		 init_mp3d();	mp3dec();	break;
	case AUDIO_WMA:	    if (!init_wmad())	wmadec();	break;
	case AUDIO_WAV:	    if (!init_wavd())	wavdec();	break;
	case AUDIO_OGG:		//init_oggd();	oggdec();	break;
	case AUDIO_UNK:
	    dprint("Unknown audio format: %s\n", fi->path);	break;
	}   // waiting for over playing
NEXT:
dtrace;
	playlist->pcur= playlist->pcur->next;
#ifdef	GUI_CONTROL
	shm->playidx  = playlist->pcur - playlist->fiarr;
#endif//GUI_CONTROL
	if (!(--playlist->sum)) {
dtrace;
	    if (playlist->rule < PR_SEQLOOP) {
#ifdef	GUI_CONTROL
		shm->feedback = COMM_EXIT;
#endif//GUI_CONTROL
		break;
	    }
	    else if (shm->rule != playlist->rule)
#ifdef	GUI_CONTROL
		reset_playlist(shm->first, shm->last, shm->rule);
#else
		reset_playlist(0, argc-2, PR_RANLOOP);
#endif//GUI_CONTROL
	}
#ifdef	GUI_CONTROL
	shm->feedback = COMM_OVERSONG;
#endif//GUI_CONTROL
    }	sighdl_term(SIGTERM);		    return 0;
}

/******************** End Of File: ucap.c ********************/
