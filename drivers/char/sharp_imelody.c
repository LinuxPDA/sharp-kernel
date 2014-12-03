/*
 *  linux/drivers/char/sharp_imelody.c
 *
 *  Parser for iMelody format On SHARP PDA
 *
 */

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
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
#include <linux/hdreg.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>

#define IMELODY_STRCMP(s,t)        strcmp((s),(t))
#define IMELODY_STRCASECMP(s,t)    strcasecmp((s),(t))
#define IMELODY_STRNCMP(s,t,n)     strncmp((s),(t),(n))
#define IMELODY_STRCASENCMP(s,t,n) strncasecmp((s),(t),(n))

#define IMELODY_PARSING_ERROR      -1
#define IMELODY_SPEC_ERROR         -2

#define DEFAULT_OCTAVE_PREFIX      4

#define PARSEDPRINT(s)  /* printk s */
#define DEBUGPRINT(s)   /* printk s */


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

#define SECS_PER_MIN    60      /* seconds per minute */
#define WHOLE_NOTE      4       /* quarter notes per whole note */
#define DFLT_TEMPO      120     /* default tempo */
#define NORMAL_FILL       20
#define STACCATO_FILL      1
#define DFLT_FILL       (NORMAL_FILL)

static int whole = (HZ * SECS_PER_MIN * WHOLE_NOTE) / DFLT_TEMPO;
static int fill_ratio  = DFLT_FILL;
static int style = 0;

extern void tone(unsigned int thz,unsigned int ticks);
extern void rest(unsigned int ticks);
extern int buzzer_volumes[SHARP_BUZ_WHICH_MAX+1];

static int do_rest(int duration,int spec)
{
  int dur;
  PARSEDPRINT(("do_rest(%d,%c)\n",duration,spec));
  dur = whole >> duration;
  switch( spec ){
  case '.' : /* dotted note */
    dur = dur + ( dur >> 1 );
    break;
  case ':' : /* double dotted note */
    dur = dur + ( dur >> 1 ) + ( dur >> 2 );
    break;
  case ';' : /* 2/3 length */
    dur = ( dur << 1 ) / 3;
    break;
  default: /* nop */
    break;
  }
  rest(dur);
  return 0;
}

static int do_led(int turnon)
{
  PARSEDPRINT(("do_led(%d)\n",turnon));
  /* Currently , Do Nothing ! */
  return 0;
}

static int do_vibe(int turnon)
{
  PARSEDPRINT(("do_vibe(%d)\n",turnon));
  /* Currently , Do Nothing ! */
  return 0;
}

static int do_volume_plus(void)
{
  PARSEDPRINT(("do_volume_plus()\n"));
  buzzer_volumes[SHARP_BUZ_WRITESOUND]++;
  return 0;
}

static int do_volume_minus(void)
{
  PARSEDPRINT(("do_volume_minus()\n"));
  buzzer_volumes[SHARP_BUZ_WRITESOUND]--;
  return 0;
}

static int do_volume_vol(int vol)
{
  PARSEDPRINT(("do_volume_vol(%d)\n",vol));
  buzzer_volumes[SHARP_BUZ_WRITESOUND] = vol;
  return 0;
}

static int do_note(int octave,int note,int note_prefix,int duration,int spec)
{
  int pitch;
  int hz;
  int dur;
  PARSEDPRINT(("do_note(%d,%c,%d,%d,%c)\n",octave,note,note_prefix,duration,spec));
  pitch = notetab[note - 'a'] + ( octave -1 ) * OCTAVE_NOTES + note_prefix;
  hz = pitchtab[pitch];
  dur = whole >> duration;
  switch( spec ){
  case '.' : /* dotted note */
    dur = dur + ( dur >> 1 );
    break;
  case ':' : /* double dotted note */
    dur = dur + ( dur >> 1 ) + ( dur >> 2 );
    break;
  case ';' : /* 2/3 length */
    dur = ( dur << 1 ) / 3;
    break;
  default: /* nop */
    break;
  }
  switch(style){
  case 1: /* this is for Continuous Style (S1) */
    tone(hz,dur);
    break;
  case 2: /* this is for Staccato Style (S2) */
    tone(hz,dur*fill_ratio/(fill_ratio+1));
    rest(dur/(fill_ratio+1));
    break;
  case 0: /* this is for Normal Style (S0) */
  default:
    tone(hz,dur*fill_ratio/(fill_ratio+1));
    rest(dur/(fill_ratio+1));
    break;
  }
  return 0;
}

static void do_parsing_error(char* all,char* cur,char* file,int line)
{
  PARSEDPRINT(("parsing error at %s (line %d) , all=%s , cur=%s\n",file,line,all,cur));
}

#define RETURN_PARSING_ERROR(retval,all,cur) {do_parsing_error((all),(cur),__FILE__,__LINE__); return (retval);}
#define RETURN_IF_FAIL(r)  { if((r)) return (r); }
#define HOOK_BEFORE_NEXT   /* nop */

int imelody_play_block(char* bs,int len)
{
  int c1;
  int result = 0;
  char* all = bs;
  char* repeat_start = NULL;
  char* repeat_end = NULL;
  int repeat_len = 0;
  int repeat_len_startup = 0;
  int repeat_left = 0;
  while( len > 0 && *bs != '\0' ){
    c1 = *(bs++);
    len--;
    /* <block> := <silence>|<note>|<led>|<vib>|<volume>|<backlight> */
    switch(c1){
    case '(':
      {
	int count = 0;
	/* <repeat> := '(' <block>+ '@' <repeat-count> [<volume-modif>] ')' */
	if( repeat_start ){
	  /* already repeating... this case should not happen on CLASS 1.0 SPEC */
	  RETURN_PARSING_ERROR(IMELODY_SPEC_ERROR,all,bs);
	}
	/* backup current position , i.e. top of <repeat> */
	repeat_start = bs;
	repeat_len_startup = len;
	PARSEDPRINT(("top of <repeat> seems to be %s (%d)\n",repeat_start,repeat_len_startup));
	/* search for repeat count */
	while( len > 0 && *bs != '\0' && *bs != '@' ){
	  bs++;
	  len--;
	}
	if( len == 0 ){
	  /* string ends before matching '@'. this should not happen */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	if( *bs != '@' ){
	  /* string ends before matching '@'. this should not happen */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	/* move to top of <repeat-count> */
	bs++;
	len--;
	count = 0;
	while( len > 0 ){
	  if( *bs >= '0' && *bs <= '9' ){
	    count = count * 10 + ( *(bs++) - '0' );
	    len--;
	  }else{
	    break;
	  }
	}
	if( count == 0 ){
	  repeat_left = -1;
	}else{
	  repeat_left = count;
	}
	PARSEDPRINT(("number of <repeat> seems to be %d\n",repeat_left));
	/* check for <volume-modif> */
	if( *bs == 'V' ){
	  bs++;
	  len--;
	  if( *bs == '+' ){
	    bs++;
	    len--;
	    /* --- DO 'VOLUME ++' --- */
	    result = do_volume_plus();
	    RETURN_IF_FAIL(result);
	  }
	  else if( *bs == '-' ){
	    bs++;
	    len--;
	    /* --- DO 'VOLUME --' --- */
	    result = do_volume_minus();
	  RETURN_IF_FAIL(result);
	  }else{
	    /* <volume-modif> is only avail in <repeat>. "V0"... is not aval. */
	    RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	  }
	}
	/* check if end of <repeat> */
	if( *bs != ')' ){
	  /* end of ')' is not found at supposed pos. */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	/* get position at the next char of the end of <repeat> */
	bs++;
	len--;
	/* save end of <repeat> pos */
	repeat_end = bs;
	repeat_len = len;
	PARSEDPRINT(("end of <repeat> seems to be %s (%d)\n",repeat_end,repeat_len));
	/* restore position to the top of <repeat> , and continue playing */
	bs = repeat_start;
	len = repeat_len_startup;
      }
      break;
    case '@':
      {
	/* return to top of <repeat> in case of repeating */
	if( ! repeat_start ){
	  /* not repeating ... should not happen */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	if( repeat_left < 0 ){
	  /* <repeat> block never last. */
	  bs = repeat_start;
	  len = repeat_len_startup;
	}else if( (--repeat_left) == 0 ){
	  /* <repeat> block last. */
	  bs = repeat_end;
	  len = repeat_len;
	  repeat_start = NULL;
	  repeat_end = NULL;
	  repeat_len_startup = 0;
	  repeat_len = 0;
	}else{
	  /* <repeat> block continue */
	  bs = repeat_start;
	  len = repeat_len_startup;
	}
      }
      break;
    case ')':
      {
	if( ! repeat_start ){
	  /* not repeating ... should not happen */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	/* this should not happen ? */
	RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
      }
      break;
    case 'r':
      {
	/* <silence> := <rest><duration>[<duration-spec>] = r[0-5]{1}[.:;]{0,1} */
	int duration = 0;
	int spec = 0;
	if( *bs < '0' || *bs > '5' ){
	  /* this should not happen */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	duration = *(bs++) - '0';
	len--;
	if( *bs == '.' || *bs == ':' || *bs == ';' ){
	  spec = *(bs++);
	  len--;
	}
	/* --- DO 'REST' , using duration , spec --- */
	result = do_rest(duration,spec);
	RETURN_IF_FAIL(result);
	break;
      }
    case 'l':
      {
	/* <led> := "ledon" | "ledoff" */
	int turnon = 0;
	if( ! IMELODY_STRNCMP(bs,"edon",4) ){
	  turnon = 1;
	}else if( ! IMELODY_STRNCMP(bs,"edoff",5) ){
	  turnon = 0;
	}else{
	  /* element that starts with 'l' but is not a 'ledon/off' , should not happen ! */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	/* --- DO 'LED' , using turnon --- */
	result = do_led(turnon);
	RETURN_IF_FAIL(result);
	break;
      }
      break;
    case 'v':
      {
	/* <vib> := "vibeon" | "vibeoff" */
	int turnon = 0;
	if( ! IMELODY_STRNCMP(bs,"ibeon",5) ){
	  turnon = 1;
	}else if( ! IMELODY_STRNCMP(bs,"ibeoff",6) ){
	  turnon = 0;
	}else{
	  /* element that starts with 'v' but is not a 'vibeon/off' , should not happen ! */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	/* --- DO 'VIBE' , using turnon --- */
	result = do_vibe(turnon);
	RETURN_IF_FAIL(result);
	break;
      }
      break;
    case 'V':
      {
	/* <volume> := "V0" | ... | "V15" | "V+" | "V-" */
	if( *bs == '+' ){
	  bs++;
	  len--;
	  /* --- DO 'VOLUME ++' --- */
	  result = do_volume_plus();
	  RETURN_IF_FAIL(result);
	  break;
	}
	else if( *bs == '-' ){
	  bs++;
	  len--;
	  /* --- DO 'VOLUME --' --- */
	  result = do_volume_minus();
	  RETURN_IF_FAIL(result);
	  break;
	}
	else if( *bs >= '0' && *bs <= '9' ){
	  int vol = *(bs++) - '0';
	  len--;
	  if( vol == 1 && *bs >= '0' && *bs <= '5' ){
	    vol = vol * 10 + ( *(bs++) - '0' );
	    len--;
	  }
	  /* --- DO 'VOLUME' using vol --- */
	  result = do_volume_vol(vol);
	  RETURN_IF_FAIL(result);
	  break;
	}else{
	  /* element that starts with 'V' but is not a '<volume>' , should not happen ! */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
      }
      break;
    default:
      {
	/* <note> := [<octave-pref>]<basic-ess-iss-note><duration>[<duration-spec>]
	   = (*[0-8]){0,1}([#&][dga]|[#][cf]|[&][eb])[0-5][.;:]{0,1} */
	int octave = DEFAULT_OCTAVE_PREFIX;
	int note_pref = 0; /* 0 for none , 1 for sharp , -1 for flat */
	int note = 0;
	int duration = 0;
	int spec = 0;
	/* <backlight> := "backon" | "backoff" */
	if( c1 == 'b' && ! IMELODY_STRNCMP(bs,"ack",3) ){
	  /* <backlight> := "backon" | "backoff" */
	  int turnon = 0;
	  if( ! IMELODY_STRNCMP(bs,"ackon",5) ){
	    turnon = 1;
	  }else if( ! IMELODY_STRNCMP(bs,"ackoff",6) ){
	    turnon = 0;
	  }else{
	    /* element that starts with "back" but not "backon/off" , should not happen ! */
	    RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	  }
	  /* --- DO 'BACKLIGHT' , using turnon --- */
	  break;
	}else{
	  /* element starts with 'b' may be a '<note>'. continue... */
	}
	if( c1 == '*' ){
	  /* <octave-pref> := "*0" | ... | "*8" */
	  if( *bs >= '0' && *bs <= '8' ){
	    octave = *(bs++) - '0';
	    len--;
	    c1 = *(bs++);
	    len--;
	  }else{
	    /* element that starts with '*' but is not a '*[0-8]' , should not happen ! */
	    RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	  }
	}
	if( c1 == '#' ){
	  /* <iss-note> begins with '#' */
	  note_pref = 1;
	  c1 = *(bs++);
	  len--;
	}else if( c1 == '&' ){
	  /* <ess-note> begins with '&' */
	  note_pref = -1;
	  c1 = *(bs++);
	  len--;
	}else{
	  /* <basic-note> has neigher '#' or '&' */
	  note_pref = 0;
	}
	/* at this point , all 'note' should begin with one of [a-g] */
	if( c1 < 'a' || c1 > 'g' ){
	  /* illegal note. should not happen ! */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	note = c1;
	/* check note with note_prefix */
	switch(note){
	case 'c':
	case 'f':
	  if( note_pref == -1 ){
	    /* 'c' or 'f' with '&'.... this should not happen */
	    RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	  }
	  break;
	case 'e':
	case 'b':
	  if( note_pref == 1 ){
	    /* 'e' or 'b' with '#'.... this should not happen */
	    RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	  }
	  break;
	case 'd':
	case 'g':
	case 'a':
	default: /* this case not happen. but... */
	  /* these note can have either '#' or '&' */
	  break;
	}
	/* check duration */
	if( *bs < '0' || *bs > '5' ){
	  /* this should not happen */
	  RETURN_PARSING_ERROR(IMELODY_PARSING_ERROR,all,bs);
	}
	duration = *(bs++) - '0';
	len--;
	if( *bs == '.' || *bs == ':' || *bs == ';' ){
	  spec = *(bs++);
	  len--;
	}
	/* --- DO 'NOTE' , using duration , octave , note , note_pref , dur , spec --- */
	result = do_note(octave,note,note_pref,duration,spec);
	RETURN_IF_FAIL(result);
	break;
      }
      break;
    }
    HOOK_BEFORE_NEXT;
  }
  return 0;
}

int imelody_set_style(char* stylestr)
{
  if( stylestr[0] == 'S' ) stylestr++;
  switch( *stylestr ){
  case '1':
    style = 1;
    break;
  case '2':
    style = 2;
    fill_ratio = STACCATO_FILL;
    break;
  case '0':
    style = 0;
    fill_ratio = NORMAL_FILL;
    break;
  default:
    /* do nothing */
    break;
  }
  return 0;
}

int imelody_set_volume(char* volume)
{
  if( volume[0] == 'V' ) volume++;
  if( *volume >= '0' && *volume <= '9' ){
    int vol = *(volume++) - '0';
    if( vol == 1 && *volume >= '0' && *volume <= '5' ){
      vol = vol * 10 + ( *(volume++) - '0' );
    }
    buzzer_volumes[SHARP_BUZ_WRITESOUND] = vol;
  }else{
    /* do nothing */
  }
  return 0;
}

int imelody_set_beat(char* beat)
{
  int val = 0;
  while( *beat >= '0' && *beat <= '9' ){
    val = val * 10 + *(beat++) - '0';
  }
  if( val >= 25 && val <= 900 ){
    whole = (HZ * SECS_PER_MIN * WHOLE_NOTE) / val;
  }
  return 0;
}

#define SHARP_IMELODY_IOCTL_START (SHARP_BUZZER_IOCTL_START+0x80)
#define SHARP_IMELODY_SETSTYLE    (SHARP_IMELODY_IOCTL_START+0)
#define SHARP_IMELODY_SETVOLUME   (SHARP_IMELODY_IOCTL_START+1)
#define SHARP_IMELODY_SETBEAT     (SHARP_IMELODY_IOCTL_START+2)

int imelody_ioctl(struct inode *inode,
		  struct file *filp,
		  unsigned int command,
		  unsigned long arg)
{
  int error;
  switch( command ) {
  case SHARP_IMELODY_SETSTYLE:
    {
      char* puser = (char*)arg;
      if( ! puser ) return -EINVAL;
      DEBUGPRINT(("sharpimelody setstyle\n"));
      imelody_set_style(puser);
    }
    break;
  case SHARP_IMELODY_SETVOLUME:
    {
      char* puser = (char*)arg;
      if( ! puser ) return -EINVAL;
      DEBUGPRINT(("sharpimelody setvol\n"));
      imelody_set_volume(puser);
    }
    break;
  case SHARP_IMELODY_SETBEAT:
    {
      char* puser = (char*)arg;
      if( ! puser ) return -EINVAL;
      DEBUGPRINT(("sharpimelody setbeat\n"));
      imelody_set_beat(puser);
    }
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

/*
 *   end of source
 */
