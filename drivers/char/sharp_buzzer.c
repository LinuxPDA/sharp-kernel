/*
 *  linux/drivers/char/sharp_buzzer.c
 *
 *  Driver for Buzzer devices On SHARP PDA
 *
 * Copyright (C) 2001  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  ChangeLog:
 *   04-16-2001 Lineo Japan, Inc. ...
 *   12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *   26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/pm.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>

#ifndef SHARP_BUZZER_MAJOR
# define SHARP_BUZZER_MAJOR 227 /* number of bare sharp_buzzer driver */
#endif

#if 0
#define USE_IMELODY_FORMAT
#endif


#undef  USE_IBMBASIC_FORMAT

#if defined(USE_IMELODY_FORMAT) && defined(USE_IBMBASIC_FORMAT)
#error "should not define iMelody and IBMBasic at a time."
#endif /* USE_IMELODY_FORMAT && USE_IBMBASIC_FORMAT */


#define DEBUGPRINT(s)   /* printk s */

static int not_implemented_yet(void)
{
  printk("NOT IMPLEMENTED YET!!\n");
  return 0;
}

#ifdef CONFIG_IRIS
extern int iris_play_sound_by_id(int soundid,int volume);
extern int iris_play_sound_by_hz(unsigned int hz,unsigned int msecs,int volume);
extern int iris_buzzer_dev_init(void);
extern int iris_buzzer_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int iris_suspend_buzzer(void);
extern int iris_resume_buzzer(void);
extern int iris_buzzer_ioctl(struct inode *inode,struct file *filp,unsigned int command,unsigned long arg);
#define play_sound_by_id(id,vol)  iris_play_sound_by_id((id),(vol))
#define stop_sound_by_id(id)      not_implemented_yet() /* !!!!! need to implement !!!!!!! */
#define play_sound_by_hz(hz,msecs,vol)  iris_play_sound_by_hz((hz),(msecs),(vol))
#define buzzer_dev_init()  iris_buzzer_dev_init()
#define buzzer_supported_arch(w)       iris_buzzer_supported((w))
#define suspend_buzzer_arch()          iris_suspend_buzzer()
#define resume_buzzer_arch()           iris_resume_buzzer()
#endif
#ifdef CONFIG_SA1100_COLLIE
extern int collie_play_sound_by_id(int soundid,int volume);
extern int collie_play_sound_by_hz(unsigned int hz,unsigned int msecs,int volume);
extern int collie_buzzer_dev_init(void);
extern int collie_buzzer_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int collie_suspend_buzzer(void);
extern int collie_resume_buzzer(void);
#define play_sound_by_id(id,vol)       collie_play_sound_by_id((id),(vol))
#define stop_sound()                   collie_stop_sound()
#define stop_sound_by_id(id)           collie_stop_sound()
#define play_sound_by_hz(hz,msecs,vol) collie_play_sound_by_hz((hz),(msecs),(vol))
#define buzzer_dev_init()              collie_buzzer_dev_init()
#define buzzer_supported_arch(w)       collie_buzzer_supported((w))
#define suspend_buzzer_arch()          collie_suspend_buzzer()
#define resume_buzzer_arch()           collie_resume_buzzer()
#endif

#if defined (CONFIG_ARCH_PXA_POODLE) || defined (CONFIG_ARCH_PXA_CORGI)
extern int poodle_play_sound_by_id(int soundid,int volume);
extern int poodle_play_sound_by_hz(unsigned int hz,unsigned int msecs,int volume);
extern int poodle_buzzer_dev_init(void);
extern int poodle_buzzer_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int poodle_suspend_buzzer(void);
extern int poodle_resume_buzzer(void);
#define play_sound_by_id(id,vol)       poodle_play_sound_by_id((id),(vol))
#define stop_sound()                   poodle_stop_sound()
#define stop_sound_by_id(id)           poodle_stop_sound()
#define play_sound_by_hz(hz,msecs,vol) poodle_play_sound_by_hz((hz),(msecs),(vol))
#define buzzer_dev_init()              poodle_buzzer_dev_init()
#define buzzer_supported_arch(w)       poodle_buzzer_supported((w))
#define suspend_buzzer_arch()          poodle_suspend_buzzer()
#define resume_buzzer_arch()           poodle_resume_buzzer()
#endif

#if defined (CONFIG_ARCH_PXA_TOSA)
extern int tosa_play_sound_by_id(int soundid,int volume);
extern int tosa_play_sound_by_hz(unsigned int hz,unsigned int msecs,int volume);
extern int tosa_buzzer_dev_init(void);
extern int tosa_buzzer_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int tosa_suspend_buzzer(void);
extern int tosa_resume_buzzer(void);
#define play_sound_by_id(id,vol)       tosa_play_sound_by_id((id),(vol))
#define stop_sound()                   tosa_stop_sound()
#define stop_sound_by_id(id)           tosa_stop_sound()
#define play_sound_by_hz(hz,msecs,vol) tosa_play_sound_by_hz((hz),(msecs),(vol))
#define buzzer_dev_init()              tosa_buzzer_dev_init()
#define buzzer_supported_arch(w)       tosa_buzzer_supported((w))
#define suspend_buzzer_arch()          tosa_suspend_buzzer()
#define resume_buzzer_arch()           tosa_resume_buzzer()
#endif	/* CONFIG_ARCH_PXA_TOSA */

/*
 * logical level drivers
 */

int buzzer_volumes[SHARP_BUZ_WHICH_MAX+1]; /* set volume by % */
int buzzer_mutings[SHARP_BUZ_WHICH_MAX+1];

static int duration; /* (msecs) */
static int base_hz_c; /* (hz) */
#define INITIAL_DURATION 600 /* (msecs) */
#define INITIAL_BASE_HZ  440 /* (hz) */

void rest(unsigned int ticks)
{
  int msecs = ticks * 1000 / HZ;
  //printk("rest %d ticks --> %d msecs\n",ticks,msecs);
  mdelay(msecs);
}
void tone(unsigned int thz,unsigned int ticks)
{
  int msecs = ticks * 1000 / HZ;
  //printk("tone %d Hz , %d ticks --> %d msecs\n",thz,ticks,msecs);
  play_sound_by_hz(thz, msecs,buzzer_volumes[SHARP_BUZ_WRITESOUND]);
}

#ifdef  USE_IBMBASIC_FORMAT

/*
 * spkr.c -- device driver for console speaker
 *
 * v1.4 by Eric S. Raymond (esr@snark.thyrsus.com) Aug 1993
 * modified for FreeBSD by Andrew A. Chernov <ache@astral.msk.su>
 *
 * $FreeBSD: /c/ncvs/src/sys/i386/isa/spkr.c,v 1.49 2001/03/26 12:40:51 phk Exp $
 */
/**************** PLAY STRING INTERPRETER BEGINS HERE **********************
 *
 * Play string interpretation is modelled on IBM BASIC 2.0's PLAY statement;
 * M[LNS] are missing; the ~ synonym and the _ slur mark and the octave-
 * tracking facility are added.
 * Requires tone(), rest(), and endtone(). String play is not interruptible
 * except possibly at physical block boundaries.
 */

typedef int     bool;
#define TRUE    1
#define FALSE   0

#define dtoi(c)         ((c) - '0')

static int octave;      /* currently selected octave */
static int whole;       /* whole-note time at current tempo, in ticks */
static int value;       /* whole divisor for note time, quarter note = 1 */
static int fill;        /* controls spacing of notes */
static bool octtrack;   /* octave-tracking on? */
static bool octprefix;  /* override current octave-tracking state? */

/*
 * Magic number avoidance...
 */
#define SECS_PER_MIN    60      /* seconds per minute */
#define WHOLE_NOTE      4       /* quarter notes per whole note */
#define MIN_VALUE       64      /* the most we can divide a note by */
#define DFLT_VALUE      4       /* default value (quarter-note) */
#define FILLTIME        8       /* for articulation, break note in parts */
#define STACCATO        6       /* 6/8 = 3/4 of note is filled */
#define NORMAL          7       /* 7/8ths of note interval is filled */
#define LEGATO          8       /* all of note interval is filled */
#define DFLT_OCTAVE     4       /* default octave */
#define MIN_TEMPO       32      /* minimum tempo */
#define DFLT_TEMPO      120     /* default tempo */
#define MAX_TEMPO       255     /* max tempo */
#define NUM_MULT        3       /* numerator of dot multiplier */
#define DENOM_MULT      2       /* denominator of dot multiplier */

/* letter to half-tone:  A   B  C  D  E  F  G */
static int notetab[8] = {9, 11, 0, 2, 4, 5, 7};

/*
 * This is the American Standard A440 Equal-Tempered scale with frequencies
 * rounded to nearest integer. Thank Goddess for the good ol' CRC Handbook...
 * our octave 0 is standard octave 2.
 */
#define OCTAVE_NOTES    12      /* semitones per octave */
static int pitchtab[] =
{
/*        C     C#    D     D#    E     F     F#    G     G#    A     A#    B*/
/* 0 */   65,   69,   73,   78,   82,   87,   93,   98,  103,  110,  117,  123,
/* 1 */  131,  139,  147,  156,  165,  175,  185,  196,  208,  220,  233,  247,
/* 2 */  262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
/* 3 */  523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,
/* 4 */ 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1975,
/* 5 */ 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951,
/* 6 */ 4186, 4435, 4698, 4978, 5274, 5588, 5920, 6272, 6644, 7040, 7459, 7902,
};

static void
playinit(void)
{
    octave = DFLT_OCTAVE;
    whole = (HZ * SECS_PER_MIN * WHOLE_NOTE) / DFLT_TEMPO;
    fill = NORMAL;
    value = DFLT_VALUE;
    octtrack = FALSE;
    octprefix = TRUE;   /* act as though there was an initial O(n) */
}

/* play tone of proper duration for current rhythm signature */
static void
playtone(int pitch, int value, int sustain)
{
    register int        sound, silence, snum = 1, sdenom = 1;

    /* this weirdness avoids floating-point arithmetic */
    for (; sustain; sustain--)
    {
        /* See the BUGS section in the man page for discussion */
        snum *= NUM_MULT;
        sdenom *= DENOM_MULT;
    }

    if (value == 0 || sdenom == 0)
        return;

    if (pitch == -1)
        rest(whole * snum / (value * sdenom));
    else
    {
        sound = (whole * snum) / (value * sdenom)
                - (whole * (FILLTIME - fill)) / (value * FILLTIME);
        silence = whole * (FILLTIME-fill) * snum / (FILLTIME * value * sdenom);

        tone(pitchtab[pitch], sound);
        if (fill != LEGATO)
            rest(silence);
    }
}

static int
abs(n)
        int n;
{
    if (n < 0)
        return(-n);
    else
        return(n);
}

#define isdigit(c) ( (c) >= '0' && (c) <= '9' )
#define toupper(c) ( (c) >= 'a' && (c) <= 'z' ? (c) - 'a' + 'A' : (c))

/* interpret and play an item from a notation string */
static void
playstring(char* cp, int slen)
{
    int         pitch, oldfill, lastpitch = OCTAVE_NOTES * DFLT_OCTAVE;

#define GETNUM(cp, v)   for(v=0; isdigit(cp[1]) && slen > 0; ) \
                                {v = v * 10 + (*++cp - '0'); slen--;}
    for (; slen--; cp++)
    {
        int             sustain, timeval, tempo;
        register char   c;

	c = toupper(*cp);

        switch (c)
        {
        case 'A':  case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':

            /* compute pitch */
            pitch = notetab[c - 'A'] + octave * OCTAVE_NOTES;

            /* this may be followed by an accidental sign */
            if (cp[1] == '#' || cp[1] == '+')
            {
                ++pitch;
                ++cp;
                slen--;
            }
            else if (cp[1] == '-')
            {
                --pitch;
                ++cp;
                slen--;
            }

            /*
             * If octave-tracking mode is on, and there has been no octave-
             * setting prefix, find the version of the current letter note
             * closest to the last regardless of octave.
             */
            if (octtrack && !octprefix)
            {
                if (abs(pitch-lastpitch) > abs(pitch+OCTAVE_NOTES-lastpitch))
                {
                    ++octave;
                    pitch += OCTAVE_NOTES;
                }

                if (abs(pitch-lastpitch) > abs((pitch-OCTAVE_NOTES)-lastpitch))
                {
                    --octave;
                    pitch -= OCTAVE_NOTES;
                }
            }
            octprefix = FALSE;
            lastpitch = pitch;

            /* ...which may in turn be followed by an override time value */
            GETNUM(cp, timeval);
            if (timeval <= 0 || timeval > MIN_VALUE)
                timeval = value;

            /* ...and/or sustain dots */
            for (sustain = 0; cp[1] == '.'; cp++)
            {
                slen--;
                sustain++;
            }

            /* ...and/or a slur mark */
            oldfill = fill;
            if (cp[1] == '_')
            {
                fill = LEGATO;
                ++cp;
                slen--;
            }

            /* time to emit the actual tone */
            playtone(pitch, timeval, sustain);

            fill = oldfill;
            break;

        case 'O':
            if (cp[1] == 'N' || cp[1] == 'n')
            {
                octprefix = octtrack = FALSE;
                ++cp;
                slen--;
            }
            else if (cp[1] == 'L' || cp[1] == 'l')
            {
                octtrack = TRUE;
                ++cp;
                slen--;
            }
            else
            {
                GETNUM(cp, octave);
                if (octave >= sizeof(pitchtab) / sizeof(pitchtab[0]) / OCTAVE_NOTES)
                    octave = DFLT_OCTAVE;
                octprefix = TRUE;
            }
            break;

        case '>':
            if (octave < sizeof(pitchtab) / sizeof(pitchtab[0]) / OCTAVE_NOTES - 1)
                octave++;
            octprefix = TRUE;
            break;

        case '<':
            if (octave > 0)
                octave--;
            octprefix = TRUE;
            break;

        case 'N':
            GETNUM(cp, pitch);
            for (sustain = 0; cp[1] == '.'; cp++)
            {
                slen--;
                sustain++;
            }
            oldfill = fill;
            if (cp[1] == '_')
            {
                fill = LEGATO;
                ++cp;
                slen--;
            }
            playtone(pitch - 1, value, sustain);
            fill = oldfill;
            break;

        case 'L':
            GETNUM(cp, value);
            if (value <= 0 || value > MIN_VALUE)
                value = DFLT_VALUE;
            break;

        case 'P':
        case '~':
            /* this may be followed by an override time value */
            GETNUM(cp, timeval);
            if (timeval <= 0 || timeval > MIN_VALUE)
                timeval = value;
            for (sustain = 0; cp[1] == '.'; cp++)
            {
                slen--;
                sustain++;
            }
            playtone(-1, timeval, sustain);
            break;

        case 'T':
            GETNUM(cp, tempo);
            if (tempo < MIN_TEMPO || tempo > MAX_TEMPO)
                tempo = DFLT_TEMPO;
            whole = (HZ * SECS_PER_MIN * WHOLE_NOTE) / tempo;
            break;

        case 'M':
            if (cp[1] == 'N' || cp[1] == 'n')
            {
                fill = NORMAL;
                ++cp;
                slen--;
            }
            else if (cp[1] == 'L' || cp[1] == 'l')
            {
                fill = LEGATO;
                ++cp;
                slen--;
            }
            else if (cp[1] == 'S' || cp[1] == 's')
            {
                fill = STACCATO;
                ++cp;
                slen--;
            }
            break;
        }
    }
}
#endif /*  USE_IBMBASIC_FORMAT */

/*
 *  query about support of buzzer.
 */

static int is_buzzer_supported(int which)
{
  if( which > SHARP_BUZ_WHICH_MAX || which < 0 ) return -EINVAL;
  return(buzzer_supported_arch(which));
}

/*
 * make-sound entry point for each device drivers
 */
void sharpbuz_make_keytouch_sound(void)
{
  play_sound_by_id(SHARP_BUZ_KEYSOUND,buzzer_volumes[SHARP_BUZ_KEYSOUND]);
}

void sharpbuz_make_paneltouch_sound(void)
{
  play_sound_by_id(SHARP_BUZ_TOUCHSOUND,buzzer_volumes[SHARP_BUZ_TOUCHSOUND]);
}

/*
 * operations....
 */

static int device_initialized = 0;

static int sharpbuz_open(struct inode *inode, struct file *filp)
{
  int minor = MINOR(inode->i_rdev);
  if( minor != SHARP_BUZZER_MINOR ) return -ENODEV;
  if( ! device_initialized ){
    int i;
#if 0
    buzzer_dev_init();
#endif
    for(i=0;i<=SHARP_BUZ_WHICH_MAX;i++){
      buzzer_volumes[i] = 0;
    }
#ifdef USE_IBMBASIC_FORMAT
    playinit();
#endif /* USE_IBMBASIC_FORMAT */
    device_initialized = 1;
  }
  duration = INITIAL_DURATION;
  base_hz_c = INITIAL_BASE_HZ;
  DEBUGPRINT(("SHARP Buz Opened %x\n",minor));
  MOD_INC_USE_COUNT;
  return 0;
}

static int sharpbuz_release(struct inode *inode, struct file *filp)
{
#ifdef CONFIG_SA1100_COLLIE
  stop_sound();
#endif

  DEBUGPRINT(("SHARP Buz Closed\n"));
  MOD_DEC_USE_COUNT;
  return 0;
}

#ifdef USE_IMELODY_FORMAT
extern int imelody_play_block(char* bs,int len);
#endif /* USE_IMELODY_FORMAT */

static ssize_t sharpbuz_write(struct file * file, char * buf,size_t count, loff_t *ppos)
{
#ifdef USE_IBMBASIC_FORMAT
  /* this is IBM Basic version */
  playstring(buf,count);
#endif /* USE_IBMBASIC_FORMAT */
#ifdef USE_IMELODY_FORMAT
  /* this is iMelody version */
  imelody_play_block(buf,count);
#endif /* USE_IMELODY_FORMAT */
#ifdef CONFIG_SA1100_COLLIE
  return collie_set_buffer(buf,count);
#endif

  return count;
}

#ifdef USE_IMELODY_FORMAT
extern int imelody_ioctl(struct inode *inode,
			 struct file *filp,
			 unsigned int command,
			 unsigned long arg);
#endif /* USE_IMELODY_FORMAT */


static int sharpbuz_ioctl(struct inode *inode,
			  struct file *filp,
			  unsigned int command,
			  unsigned long arg)
{
  int error;
  switch( command ) {
  case SHARP_BUZZER_MAKESOUND:
    {
      DEBUGPRINT(("sharpbuz makesound %d\n",arg));
      if( arg < 0 || arg > SHARP_BUZ_WHICH_MAX ) return -EINVAL;
      if( buzzer_mutings[arg] ) return 0;
      return play_sound_by_id(arg,buzzer_volumes[arg]);
    }
    break;
  case SHARP_BUZZER_STOPSOUND:
    {
      DEBUGPRINT(("sharpbuz stopsound %d\n",arg));
      if( arg < 0 || arg > SHARP_BUZ_WHICH_MAX ) return -EINVAL;
      if( buzzer_mutings[arg] ) return 0;
      return stop_sound_by_id(arg);
    }
    break;
  case SHARP_BUZZER_SETVOLUME:
    {
      int vol;
      int support_vol;
      sharp_buzzer_status* puser = (sharp_buzzer_status*)arg;
      vol = puser->volume;
      DEBUGPRINT(("sharpbuz setvol vol=%d\n",vol));
      if( vol > SHARP_BUZ_VOLUME_MAX || vol < SHARP_BUZ_VOLUME_OFF )
	return -EINVAL;
      if( vol < 0 || vol > 100 )
	return -EINVAL;
      if( puser->which > SHARP_BUZ_WHICH_MAX ) return -EINVAL;
      if( puser->which == SHARP_BUZ_ALL_SOUNDS ){
	int i;
	for(i=0;i<=SHARP_BUZ_WHICH_MAX;i++){
	  support_vol = is_buzzer_supported(i); 
	  if( ! support_vol || support_vol < 0 ) continue;
	  buzzer_volumes[i] = support_vol * vol / 100; /* convert volume value to arch */
	  stop_sound_by_id(i);
	}
      }else{
	support_vol = is_buzzer_supported(puser->which);
	DEBUGPRINT(("support_vol = %d , vol = %d\n",support_vol,vol));
	if( ! support_vol || support_vol < 0 ) return -EINVAL;
	buzzer_volumes[puser->which] = support_vol * vol / 100;;
	DEBUGPRINT(("set value = %d\n",buzzer_volumes[puser->which]));
	stop_sound_by_id(puser->which);
      }
    }
    break;
  case SHARP_BUZZER_SETMUTE:
    {
      int vol;
      sharp_buzzer_status* puser = (sharp_buzzer_status*)arg;
      DEBUGPRINT(("sharpbuz setmute\n"));
      vol = ( puser->mute ? 1 : 0 );
      if( puser->which > SHARP_BUZ_WHICH_MAX ) return -EINVAL;
      if( puser->which == SHARP_BUZ_ALL_SOUNDS ){
	int i;
	for(i=0;i<=SHARP_BUZ_WHICH_MAX;i++){
	  buzzer_mutings[i] = vol;
	  
	}
      }else{
	buzzer_mutings[puser->which] = vol;
      }
    }
    break;
  case SHARP_BUZZER_GETVOLUME:
    {
      int vol;
      int support_vol;
      sharp_buzzer_status* puser = (sharp_buzzer_status*)arg;
      DEBUGPRINT(("sharpbuz getvol\n"));
      if( puser->which > SHARP_BUZ_WHICH_MAX || puser->which < 0 ) return -EINVAL;
      support_vol = is_buzzer_supported(puser->which);
      if( ! support_vol || support_vol < 0 ) return -EINVAL;
      vol = buzzer_volumes[puser->which] * 100 / support_vol;  /* convert volume value to base 100% */
      error = put_user(vol,&(puser->volume));
      if( error ) return error;
      error = put_user(buzzer_mutings[puser->which],&(puser->mute));
      if( error ) return error;
    }
    break;
  case SHARP_BUZZER_ISSUPPORTED:
    {
      int status;
      sharp_buzzer_status* puser = (sharp_buzzer_status*)arg;
      DEBUGPRINT(("sharpbuz issupported\n"));
      status = is_buzzer_supported(puser->which);
      if( status < 0 ) return -EINVAL;
      error = put_user(status,&(puser->volume));
      if( error ) return error;
    }
    break;

#ifdef CONFIG_SA1100_COLLIE
  case SHARP_BUZZER_SET_BUFFER:
    {
      int status;
      status = collie_buzzer_set_buf(arg);
      if( status < 0 ) return -EINVAL;
    }
    break;
#endif

  default:
#ifdef CONFIG_IRIS
    {
      int result;
      result = iris_buzzer_ioctl(inode,filp,command,arg);
      if( result != EINVAL ) return result;
    }
#endif
#ifdef USE_IMELODY_FORMAT
    {
      int result;
      result = imelody_ioctl(inode,filp,command,arg);
      if( ! result ) return result;
    }
#endif /* USE_IMELODY_FORMAT */
    return -EINVAL;
  }
  return 0;
}

/*
 * The file operations
 */
struct file_operations sharp_buzzer_fops = {
  open:    sharpbuz_open,
  release: sharpbuz_release,
  write:   sharpbuz_write,
  ioctl:   sharpbuz_ioctl
};

/*
 *  power management
 */

#ifdef CONFIG_PM

static struct pm_dev *buz_pm_dev;

static int buz_pm_callback(struct pm_dev *pm_dev,
			   pm_request_t req, void *data)
{
  switch (req) {
  case PM_STANDBY:
    break;
  case PM_BLANK:
    break;
  case PM_UNBLANK:
    break;
  case PM_SUSPEND:
    suspend_buzzer_arch();
    break;
  case PM_RESUME:
    resume_buzzer_arch();
    break;
  }
  return 0;
}

#endif


/*
 *  init and exit
 */
#if defined(SHARPCHAR_USE_MISCDEV)

#include <linux/miscdevice.h>

static struct miscdevice sharpbuz_device = {
  SHARP_BUZZER_MINOR,
  "sharp_buz",
  &sharp_buzzer_fops
};

static int __init sharpbuz_init(void)
{
  DEBUGPRINT(("sharpbuz init\n"));
  if( misc_register(&sharpbuz_device) )
    printk("failed to register sharpbuz\n");

#ifdef CONFIG_PM
  buz_pm_dev = pm_register(PM_SYS_DEV, 0, buz_pm_callback);
#endif


#if 1
    buzzer_dev_init();
#endif


  return 0;
}

static void __exit sharpbuz_cleanup(void)
{
  DEBUGPRINT(("sharpbuz cleanup\n"));
  misc_deregister(&sharpbuz_device);
}

module_init(sharpbuz_init);
module_exit(sharpbuz_cleanup);

#else /* ! SHARPCHAR_USE_MISCDEV */

#if 0 /* bare driver should not be supported */

static int sharpbuz_major = SHARP_BUZZER_MAJOR;

static int __init sharpbuz_init(void)
{
  int result;
  DEBUGPRINT(("sharpbuz init\n"));
  if( ( result = register_chrdev(sharpbuz_major,"sharpbuz",&sharp_buzzer_fops) ) < 0 ){
    DEBUGPRINT(("sharpbuz failed\n"));
    return result;
  }
  if( sharpbuz_major == 0 ) sharpbuz_major = result; /* dynamically assigned */
  DEBUGPRINT(("sharpbuz registered %d\n",sharpbuz_major));
  return 0;
}

static void __exit sharpbuz_cleanup(void)
{
  DEBUGPRINT(("sharpbuz cleanup\n"));
  unregister_chrdev(sharpbuz_major,"sharpbuz");
}

module_init(sharpbuz_init);
module_exit(sharpbuz_cleanup);
#endif

#endif /* ! SHARPCHAR_USE_MISCDEV */

/*
 *   end of source
 */
