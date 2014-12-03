/*
 * SA1100+UCB1200 Audio Device Driver
 * Copyright (c) 1998,1999 Carl Waldspurger
 *
 * moved from arch/arm/special/audio-sa100-mcp.c to
 * drivers/char/ucb1200_audio.c
 * Brad Parker brad@heeltoe.com
 */

#ifndef	MIN
#define MIN(a,b)                (((a)<(b))?(a):(b))
#define MAX(a,b)                (((a)>(b))?(a):(b))
#endif

#define AUDIO_FORMAT_MASK	0x0fff

#define AUDIO_FORMAT_CHAN_MASK	0x0f00
#define AUDIO_MONO		0x0100
#define AUDIO_STEREO		0x0200

#define AUDIO_FORMAT_SIZE_MASK	0x00ff
#define AUDIO_8BIT		0x0008
#define AUDIO_16BIT		0x0010

#define AUDIO_8BIT_MONO		(AUDIO_8BIT | AUDIO_MONO)
#define AUDIO_8BIT_STEREO	(AUDIO_8BIT | AUDIO_STEREO)
#define AUDIO_16BIT_MONO	(AUDIO_16BIT | AUDIO_MONO)
#define AUDIO_16BIT_STEREO	(AUDIO_16BIT | AUDIO_STEREO)

//#define AUDIO_FORMAT_DEFAULT		AUDIO_8BIT_MONO
#define AUDIO_FORMAT_DEFAULT		AUDIO_16BIT_MONO

#define AUDIO_RATE_DEFAULT		8000
//#define AUDIO_RATE_DEFAULT		16000
//#define AUDIO_RATE_DEFAULT		22050
//#define AUDIO_RATE_DEFAULT		44100

#define AUDIO_RATE_MIN			2952
#define AUDIO_RATE_MAX			62500

#define AUDIO_GAIN_DEFAULT		0x10
#define AUDIO_GAIN_MIN			0
#define AUDIO_GAIN_MAX			0x1f

#define AUDIO_ATT_DEFAULT		0
#define AUDIO_ATT_MIN			0
#define AUDIO_ATT_MAX			0x1f

#define AUDIO_OUTPUT_MIX_VOLUME_DEFAULT	80
#define AUDIO_OUTPUT_MIX_VOLUME_MIN	0
#define AUDIO_OUTPUT_MIX_VOLUME_MAX	100

#define AUDIO_OSS_IOCTL_TYPE 		'P'

/*
 * constants
 *
 */

/* names */
#define	AUDIO_NAME			"ucb1200_audio.c"
#define	AUDIO_NAME_VERBOSE		"UCB1200 audio driver"
#define	AUDIO_VERSION_STRING		"$Revision: 1.1.1.1 $"

/* device numbers */
#define	AUDIO_MAJOR			(42)

#define AUDIO_OUTPUT_BUF_SIZE_MIN	0
#define AUDIO_OUTPUT_BUF_SIZE_MAX	4096

/* interrupt numbers */
#define	MCP_IRQ				IRQ_Ser4MCP

/* client parameters */
#define	AUDIO_BUFFER_SIZE_DEFAULT	(4 * 1024)
#define	AUDIO_CLIENT_ID_INVALID		(-1)
#define	AUDIO_CLIENT_NBUFS_MIN		(2)
#define	AUDIO_CLIENT_NBUFS_MAX		(16)
#define	AUDIO_CLIENT_NBUFS_DEFAULT	(AUDIO_CLIENT_NBUFS_MIN)

/* mic defaults */
#define	MIC_BUF_META_LG_PAGES		(4)
#define	MIC_BUF_META_NBYTES		(PAGE_SIZE<<MIC_BUF_META_LG_PAGES)
#define	MIC_BUF_LG_PAGES		(4)
#define	MIC_BUF_NBYTES			(PAGE_SIZE<<MIC_BUF_LG_PAGES)
#define	MIC_BUF_NSAMPLES		(2048*4)

#define AUDIO_INPUT_LG_WAKEUP_DEFAULT	(8)
#define AUDIO_INPUT_LG_WAKEUP_MIN	(1)
#define AUDIO_INPUT_LG_WAKEUP_MAX	(16)

#define	AUDIO_MIC_TIMEOUT_JIFFIES	(8 * HZ)
#define	MIC_BUF_MIN_CLIENT_XFER		(64)

/* locks */
#define	AUDIO_RATE_UNLOCKED		(AUDIO_CLIENT_ID_INVALID)

/* rate conversion constants */
#define	AUDIO_RATE_CVT_FRAC_BITS	(16)
#define	AUDIO_RATE_CVT_FRAC_UNITY	(1 << AUDIO_RATE_CVT_FRAC_BITS)
#define	AUDIO_RATE_CVT_FRAC_MASK	(AUDIO_RATE_CVT_FRAC_UNITY - 1)


#ifdef CONFIG_SA1100_ITSY
/* itsy-specific codec register values */
#define	CODEC_ITSY_IO_DIRECTIONS	(0x8200)
#define	CODEC_ITSY_IO_BUTTONS		(0x01ff)
#endif

/* codec register fields and values */
#define	CODEC_ASD_NUMERATOR		/*(375000)*/ (374406)
#define	CODEC_AUDIO_ATT_MASK		(0x001f)
#define	CODEC_AUDIO_DIV_MASK		(0x007f)
#define	CODEC_AUDIO_GAIN_MASK		(0x0f80)
#define	CODEC_AUDIO_LOOP		(0x0100)
#define CODEC_AUDIO_MUTE		(0x2000)
#define CODEC_AUDIO_IN_ENABLE		(0x4000)
#define CODEC_AUDIO_OUT_ENABLE		(0x8000)
#define CODEC_AUDIO_DYN_VFLAG_ENA	(1 << 12)
#define	CODEC_AUDIO_OFFSET_CANCEL	(1 << 13)
/*
 * types
 *
 */

typedef unsigned char uchar;
typedef unsigned long audio_offset;

typedef int audio_client_id;
typedef enum { AUDIO_INPUT, AUDIO_OUTPUT } audio_io_dir;

typedef struct {
  /* buffer */
  char *data;
  int  size;

  /* valid data */
  int  next;
  int  count;

  /* stream position (in samples) */
  audio_offset offset;
} audio_buf;

typedef struct {
  /* buffer management */
  wait_queue_head_t waitq; 
  char  *meta_data;
  short *data;
  ulong ndata;
  ulong nbytes;
  uint  lg_pages;
  uint  nmmap;

  /* masks for periodic events */
  uint  lg_wakeup;
  ulong wakeup_mask;
  ulong next_mask;
  ulong inactive_mask;
} mic_buf;

typedef struct {
	int up_mask;
	int down_mask;
} audio_button_match;

typedef struct {
	unsigned long interrupts;
	unsigned long wakeups;
	unsigned long samples;
	unsigned long fifo_err;
} audio_stats;

typedef struct {
	unsigned long next;
} audio_input_mmap_meta;

typedef struct {
	char a[MIC_BUF_NBYTES];
	char b[MIC_BUF_META_NBYTES];
} audio_input_mmap;


typedef struct {
  /* codec register values */
  uint audio_ctl_a;
  uint audio_ctl_b;
  uint codec_mode;

  /* common parameters */
  uint attenuation;
  uint gain;
  uint rate;
  uint loop;
  uint offset_cancel;

  /* common mic buffer */
  mic_buf *mic;

  /* unique client id generator */
  audio_client_id client_id_next;

  /* clients */
  int nclients;
  audio_client_id rate_locker;
  
#ifdef	CONFIG_POWERMGR
  /* power management */
  powermgr_id pm_id;
  ulong last_active;
#endif
} audio_shared;

typedef struct audio_client {
  /* linked list */
  struct audio_client *prev, *next;

  /* identifier */
  audio_client_id id;

  /* output buffer management */
  wait_queue_head_t waitq;
  audio_buf buf[AUDIO_CLIENT_NBUFS_MAX];
  int buf_size;
  int nbufs;
  int intr_buf;
  int copy_buf;

  /* sample format */
  uint format;
  uint format_lg_bytes;

  /* output parameters */
  uint mix_volume;
  uint rate;

  /* rate-conversion state */
  short rate_cvt_prev;
  short rate_cvt_next;
  uint  rate_cvt;
  ulong rate_cvt_ratio;
  ulong rate_cvt_pos;

  /* stream position (in samples) */
  audio_offset offset;

  /* dropped samples */
  audio_offset dropped;
  int drop_fail;

  /* flags */
  uint reset;
  uint pause;

  /* XXX hack: button state */
  audio_button_match buttons_match;
  int buttons_watch;
} audio_client;

typedef struct {
  audio_client *input;
  audio_client *output;
} audio_io_client;

typedef struct {
  /* device management */
  audio_shared *shared;
  audio_io_dir io_dir;
  int handle_interrupts;
  int fifo_interrupts;

  /* client management */
  audio_client *clients;
  int nclients;

  /* client activity timestamp */
  ulong client_io_timestamp;

  /* stream position (in samples) */
  audio_offset offset;

  /* statistics */
  audio_stats stats;
} audio_dev;

