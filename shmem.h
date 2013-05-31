/****************************************************************
 * $ID: shmem.h        Mon, 23 Aug 2004 11:19:31 +0800  mhfan $ *
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
 * Last modified: Wed, 01 Dec 2004 17:56:56 +0800      by mhfan #
 ****************************************************************/
#ifndef	SHMEM_H
#define	SHMEM_H

#define TOTLE_MEM		    0x1000000
#define	GUI_SHM_SIZE		    1024    /* reserve 1 KB */
#define	GUI_SHM_BASE		    (TOTLE_MEM - GUI_SHM_SIZE)
#define	UCAP_SHM_BASE		    GUI_SHM_BASE
#define	MAX_TAGINFO_SIZE	    64

#define	SUSPEND_USEC		    8192
#define	DELAY_USEC		    128

typedef enum play_rule {
    PR_SEQUENCE=0,		    // default rule
    PR_SHUFFLE,			    // random  rule
    PR_SEQLOOP,			    // sequence loop rule
    PR_RANLOOP			    // random   loop rule
}   PlayRule;

typedef enum cmd_word {
    CMD_INVALID	    = 0 ,	    // invalid command
    CMD_CHANGERULE  ='m',	    // change the current playrule
    CMD_SELECT	    ='s',	    // play current selected file
    CMD_OVERSONG    ='o',	    // stop current playing song
    CMD_REPLAY	    ='r',	    // replay current playing file
    CMD_QUIT	    ='q',	    // quit audio player
    CMD_PAUSE	    ='l',	    // immediately pause stop
    CMD_NEXT	    ='n',	    // playback     next file
    CMD_PREV	    ='p',	    // playback previous file
    CMD_FORWARD	    ='f',	    //  forward playing
    CMD_BACKWARD    ='b',	    // backward playing
    CMD_ENDFB	    ='e',	    // end of forward/backward
    CMD_MEM	    ='a',	    // memory current playing point
    CMD_NMEM	    ='c'	    // end of memory playing
}   CmdWord;

typedef	enum comm_type {
    COMM_INVALID=0,		    // invalid
    COMM_READY,			    // ready for communication
    COMM_COMMAND,		    // to send a command
    COMM_SUCCESS,		    // excute command successfully
    COMM_OVERSONG,		    // finished this song
    COMM_FAILD,			    // excute command faild
    COMM_TAGINFO,		    // tag infor ready
    COMM_EXIT			    // UCAP exit
}   CommType;

typedef struct __attribute__((packed)) shm_data {
    /* following are information used by communication. GUI <==> UCAP */
    int sign;			    // sign for GUI-UCAP valid
    int inform;			    // inform UCAP to receive command
    int command;		    // command word

    /* following are the playing information.		GUI <=== UCAP */
    int feedback;		    // communication feedback by UCAP.
    int playidx;		    // current playing file index
    int duration;		    // duration time (seconds)
    int elapsed;		    // elapsed  time (seconds)

    int ratio;			    // percent ratio
    int rule;			    // play rule
    int freq;			    // (KHz)
    int bitrate;		    // (Kbps)
    char title [MAX_TAGINFO_SIZE];  // Tag infor
    char artist[MAX_TAGINFO_SIZE];  // Tag infor

    /* following are the playlist information.		UCAP <=== GUI */
    int first;			    // first file index of the playlist
    int last;			    //  last file index of the playlist
    int selidx;			    // selected file index
    int fsum;			    // files sumary number

    char* *path;		    // pointer to the files array
}   ShmData;

#endif//SHMEM_H
/********************* End Of File: shmem.h *********************/
