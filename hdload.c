/****************************************************************
 * $ID: hdload.c       Fri, 30 Jul 2004 13:05:24 +0800  mhfan $ *
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
 * Last modified: Wed, 01 Dec 2004 16:37:45 +0800      by mhfan #
 ****************************************************************/

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "id3tag.h"
#include "common.h"

#ifndef	DEBUG_HDLOAD
#undef	dtrace
#undef	dprint
#define dtrace
#define dprint(...)
#endif//DEBUG_HDLOAD

int fd=-1;
#ifdef	CFG_PM
int pmfd=-1;
#endif//CFG_PM
int loaded_buf=0;
sigset_t sigmask, sigset;
static sigjmp_buf jmpbuf;
BufInfo bufs_arr[BUF_NUM];
PlayList* playlist=(PlayList*)(GUI_SHM_BASE - sizeof (PlayList));

char* tagstr(struct id3_tag const *tag, char const *id)
{   // get the tag string from ID.
    struct id3_frame const *frame;
    union id3_field const *field;
    unsigned long const *ucs4;

    frame = id3_tag_findframe(tag, id, 0);
    if (frame == 0) return NULL;
    field = id3_frame_field(frame, 1);
    ucs4  = id3_field_getstrings(field, 0);
    if (!strcmp(id, ID3_FRAME_GENRE))
	ucs4 = id3_genre_name(ucs4);
    return id3_ucs4_latin1duplicate(ucs4);
}

void init_bufs()
{
    int i;  void* base=(void*)BUF_BASE;

    for (i=0; i<BUF_NUM; ++i) {
	bufs_arr[i].fi = NULL;
	bufs_arr[i].base = base;
	base += BUF_MAX_SIZE;
	bufs_arr[i].next = &bufs_arr[i+1];
	bufs_arr[i].prev = &bufs_arr[i-1];
    }	dtrace;
    bufs_arr[BUF_NUM-1].next = &bufs_arr[0];
    bufs_arr[0].prev = &bufs_arr[BUF_NUM-1];
}

static inline FileInfo* next_play(FileInfo* fi)
{
    FileInfo* tfi=fi;
    for (fi=fi->next; fi != tfi; fi=fi->next) {
	if ((loaded_buf += fi->loff / BUF_MAX_SIZE) >= BUF_NUM) break;
	if (fi->loff < fi->size || fi->offset > 0) return fi;
    }						   return NULL;
}

void hdload()
{
    FileInfo *fi, *tfi;

#ifdef	CFG_PM
    ioctl(pmfd, PM_SET_CPU_FREQ, 140);
    ioctl(pmfd, PM_LOCK_CPU_FREQ);
#endif//CFG_PM
NEXT:
    fi = playlist->lcur;
    if (fd != -1) close(fd);
    dprint("\033[1mload file: %s\n", fi->path);
    if ((fd = open(fi->path, O_RDONLY)) < 0) {
	dprint("Can't open `%s'(fi=%p)!\n", fi->path, fi);
	fi->size = 0;					goto RTN;
    }

    if (fi->loff == 0) { fi->offset = 0;
	fi->bufs.begin = fi->bufs.end = (fi->prev->bufs.end ?
			 fi->prev->bufs.end : &bufs_arr[loaded_buf]);
	if (fi->size == INVALID_SIZE) {
	    if (fi->fmt == AUDIO_MP3) {
		struct id3_tag* id3tag =
		       id3_file_tag(id3_file_fdopen(fd, 0));
		fi->id3tag.title  = tagstr(id3tag, ID3_FRAME_TITLE);
		fi->id3tag.artist = tagstr(id3tag, ID3_FRAME_ARTIST);
	    }	fi->size = lseek(fd, 0, SEEK_END);
	}
    } else if (fi->offset > 0) {
	dprint("%s: loff=%lx, offset=%lx\n", fi->path, fi->loff, fi->offset);
	lseek(fd,  fi->offset - BUF_MAX_SIZE, SEEK_SET);
	while (loaded_buf++ < BUF_NUM) {
	    if ((tfi = fi->bufs.begin->prev->fi)) {
		sigprocmask(SIG_BLOCK,  &sigmask, NULL);//   block SIG_LOAD
		tfi->bufs.end->fi = NULL;
		tfi->bufs.end = tfi->bufs.end->prev;
		tfi->loff -= BUF_MAX_SIZE;
		sigprocmask(SIG_SETMASK, &sigset, NULL);// unblock SIG_LOAD
	    }

	    read (fd, fi->bufs.begin->prev->base, BUF_MAX_SIZE);

	    sigprocmask(SIG_BLOCK,  &sigmask, NULL);	//   block SIG_LOAD
	    fi->bufs.begin = fi->bufs.begin->prev;
	    fi->bufs.begin->fi = fi;
	    fi->offset -= BUF_MAX_SIZE;
	    sigprocmask(SIG_SETMASK, &sigset, NULL);	// unblock SIG_LOAD

	    if (fi->offset == 0) break;
	    lseek(fd, -(BUF_MAX_SIZE<<1), SEEK_CUR);
	}						goto RTN;
    }	    lseek(fd, fi->loff, SEEK_SET);

dtrace;
    while (loaded_buf++ < BUF_NUM) {
	if ((tfi = fi->bufs.end->fi)) {
	    sigprocmask(SIG_BLOCK,  &sigmask, NULL);	//   block SIG_LOAD
	    tfi->bufs.begin->fi = NULL;
	    tfi->bufs.begin = tfi->bufs.begin->next;
	    tfi->offset += BUF_MAX_SIZE;
	    sigprocmask(SIG_SETMASK, &sigset, NULL);	// unblock SIG_LOAD
	}

	if ((read(fd, fi->bufs.end->base, BUF_MAX_SIZE)) == 0) {
	    dprint("\033[1m%s: begin=%p, end=%p, loff=%ld\n", fi->path,
		    fi->bufs.begin->base, fi->bufs.end->base, fi->loff);
	    if (tfi) {
		sigprocmask(SIG_BLOCK,  &sigmask, NULL);//   block SIG_LOAD
		tfi->bufs.begin = tfi->bufs.begin->prev;
		tfi->bufs.begin->fi = tfi;
		tfi->offset -= BUF_MAX_SIZE;
		sigprocmask(SIG_SETMASK, &sigset, NULL);// unblock SIG_LOAD
	    }
	    if ((playlist->lcur=next_play(fi)))		goto NEXT;
	    else					goto RTN;
	}

	sigprocmask(SIG_BLOCK,  &sigmask, NULL);	//   block SIG_LOAD
	fi->bufs.end->fi = fi;
	fi->loff += BUF_MAX_SIZE;
	fi->bufs.end = fi->bufs.end->next;
	sigprocmask(SIG_SETMASK, &sigset, NULL);	// unblock SIG_LOAD
    }
    dprint("\033[1m%s: begin=%p, end=%p, loff=%ld\n", fi->path,
	    fi->bufs.begin->base, fi->bufs.end->base, fi->loff);
RTN:
    loaded_buf = 0;
    playlist->lcur = NULL;
#ifdef	CFG_PM
    ioctl(pmfd, PM_UNLOCK_CPU_FREQ);
    ioctl(pmfd, PM_SET_CPU_FREQ, 56);
#endif//CFG_PM
}

void sighdl_load(int signum)
{
dtrace;
#ifdef	CFG_PM
    ioctl(pmfd, PM_UNLOCK_CPU_FREQ);
#endif//CFG_PM
    siglongjmp(jmpbuf, signum);
}

void sighdl_term(int signum)
{
dtrace;
#ifdef	CFG_PM
    ioctl(pmfd, PM_UNLOCK_CPU_FREQ);
#endif//CFG_PM
    exit(0);
}

int main(void)
{   // do initialization, then pause to wait for signal.

    setpriority(PRIO_PROCESS, 0, -19);	    init_bufs();

#ifdef	CFG_PM
    if((pmfd = open(PM_DEVICE, O_RDONLY)) < 0) 
	dprint("Can't open Power Maneger device!\n");
#endif//CFG_PM

    { struct sigaction sar;	   
    sar.sa_flags = 0;
    sigemptyset(&sar.sa_mask);
    sar.sa_handler = sighdl_load;
    if (sigaction(SIG_LOAD, &sar, NULL) < 0) EXIT(ERR_SIG_INIT);

    sar.sa_handler = sighdl_term;
    if (sigaction(SIGTERM,  &sar, NULL) < 0) EXIT(ERR_SIG_INIT);
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIG_LOAD);
    sigprocmask(0, NULL, &sigset);  // save the signal mask set

    if (kill(getppid(), SIG_READY))	     EXIT(ERR_SIG_SEND);
    else dprint("### hdload has already initialized.\n");

    if (sigsetjmp(jmpbuf, 1)) {
	sigprocmask(SIG_SETMASK, &sigset, NULL);      hdload();
    }

    for (;;) pause();
}

#if 0 	/* comment by mhfan */
int stricmp(const char* s, const char* d)
{   // defined by mhfan, an implementation of case-insensitive `strcmp'.
    while (*s!='\0' && *d!='\0' && tolower(*(s++))==tolower(*(d++))) ;
    return *s != *d;
}
#endif /* comment by mhfan */

/******************** End Of File: hdload.c ********************/
