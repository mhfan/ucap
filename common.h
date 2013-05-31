/****************************************************************
 * $ID: common.h       Fri, 30 Jul 2004 15:52:35 +0800  mhfan $ *
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
 * Last modified: Wed, 01 Dec 2004 17:31:59 +0800      by mhfan #
 ****************************************************************/
#ifndef COMMON_H
#define COMMON_H

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "shmem.h"

//#define	CFG_PM
#define	GUI_CONTROL
//#define	CLI_CONTROL
#define	DEBUG
#define	DEBUG_HDLOAD
//#define	DEBUG_SWAP_BUF
//#define	DISABLE_SARCO
//#define	DUMP_OUTPUTS

/* some MACROs defined for debug */
#ifdef	DEBUG
#define DEBUG_DECODER
#define dtrace			do {fprintf(stdout, "\n\033[36mTRACE"     \
					"\033[1;34m==>\033[33m%16s"       \
					"\033[36m: \033[32m%4d\033[36m: " \
					"\033[35m%-24s \033[34m"          \
					"[\033[0;37m%s\033[1;34m,"        \
					" \033[0;36m%s\033[1;34m]"        \
					"\033[0m\n", __FILE__, __LINE__,  \
					__FUNCTION__/* __func__ */,       \
					__TIME__, __DATE__);              \
				} while (0)	    /* defined by mhfan  */ 
#define dprint(...)		do {fprintf(stdout, "\n\033[33m" __VA_ARGS__);\
				    fprintf(stdout, "\033[0m");		      \
				    fflush (stdout);			      \
				} while (0)  /* print debugging information. */
#define	EXIT(err)		do {fprintf(stderr, "\033[1;31mEXIT: \033[35m"\
					    #err "(%d).\033[0m\n", err);      \
				    exit(err); \
				} while (0)
#else
#define dtrace
#define dprint(...)		
#define	EXIT(err)		exit(err)
#endif

/* used memory address/size definitions */
#define SRAM_BASE		    0x20000000
#ifndef	TOTLE_MEM
#define	TOTLE_MEM		    0x1000000
#endif
#define	BUF_BASE		    0x800000
#define	BUF_MAX_SIZE		    0x4000		/* 16 KB */
#define SHM_SIZE		    BUF_MAX_SIZE	/* 16 KB */
#ifndef	GUI_SHM_BASE
#define	GUI_SHM_SIZE		    1024		/*  1 KB */
#define	GUI_SHM_BASE		    (TOTLE_MEM - GUI_SHM_SIZE)
#endif
#define	BUF_TOTAL_SIZE		    (TOTLE_MEM - BUF_BASE - SHM_SIZE)
#define	BUF_END			    (BUF_BASE + BUF_TOTAL_SIZE)
#define SHM_BASE		    (TOTLE_MEM - SHM_SIZE)
#define SHM_END			    (SHM_BASE + SHM_SIZE)
#if 0 	// obsoleted
#define HD_SHM_SIZE		    (SHM_SIZE - GUI_SHM_SIZE)
#define HD_SHM_BASE		    SHM_BASE
#define FILE_INFO_SIZE		    HD_SHM_SIZE
#define FILE_INFO_BASE		    HD_SHM_BASE
#define HD_SHM_END		    (HD_SHM_BASE + HD_SHM_SIZE)
#define FILE_INFO_END		    (FILE_INFO_BASE + FILE_INFO_SIZE)
#endif	/* comment by mhfan */
#define BUF_NUM			    (BUF_TOTAL_SIZE / BUF_MAX_SIZE)

#define	BIG_ENDIAN
#define	WORK_PATH		    "/mnt/"
#define	HDL_NAME		    "hdload"
#define	UCS2GBK_DAT		    "ucs2gbk.dat"
#ifdef	DUMP_OUTPUTS
#define	DSP_PATH		    WORK_PATH "dump.out"
#else
#define	DSP_PATH		    "/dev/dsp"
#endif//DUMP_OUTPUTS
#ifdef	CFG_PM
#define PM_DEVICE		    "/dev/pm5249"
#include			    "pm5249.h"
#endif

#define	FBWD_DELAY		    (1<<17)
#define	BUF_THRESHOLD		    (BUF_MAX_SIZE << 3)
#define	INVALID_SIZE		    INT_MAX
#define	WMA_FRAMES_PER_THOUSAND	    93

/* signals definitions */
#define	SIG_READY		    SIGUSR1
#define	SIG_LOAD		    SIGUSR1

enum err_code {
    ERR_SUCCESSFUL,
    ERR_SRAM_ALLOCATE,
    ERR_CREATE_PROC,
    //ERR_INIT_WMAD,
    ERR_DSP_OPEN,
    ERR_SIG_INIT,
    ERR_SIG_SEND,
    ERR_MALLOC,
    ERR_FORK
};

typedef enum cmd_st {
    CMD_STAT_READY,
    CMD_STAT_CONT,
    CMD_STAT_RETURN,
    CMD_STAT_MEM,
    CMD_STAT_NMEM
}   CmdStatus;

typedef enum audio_fmt {
    AUDIO_UNK,
    AUDIO_MP3,
    AUDIO_WMA,
    AUDIO_OGG,
    AUDIO_WAV
}   AudioFmt;

typedef struct buf_info {
    int	   frames;		    // how many frames in this buffer
    struct file_info* fi;	    // occupied by the file
    struct buf_info *prev, *next;   // double linked list
    void*  base;		    // real base address
}   BufInfo;

typedef struct file_info {	    // too big a struct
    char*  path;		    // path to the file
    AudioFmt fmt;		    // audio format
    off_t  size, offset, loff;	    // file size, overwrited & loadded offset
    struct { char    *title, *artist; } id3tag;	    // only for mp3
    struct { BufInfo *begin, *end, *cur; } bufs;    // occupying buffers
    struct file_info *prev,  *next; // double linked list
}   FileInfo;

typedef struct play_list {
    int	     sum;		    // all files sumary
    PlayRule rule;		    // play rule
    FileInfo *fiarr;		    // file-info array
    FileInfo *lcur, *pcur;	    // loading and playing cursor.
    off_t    poff, pa, pb;	    // playing offset and memory point A & B
}   PlayList;

#endif//COMMON_H
/******************** End Of File: common.h ********************/
