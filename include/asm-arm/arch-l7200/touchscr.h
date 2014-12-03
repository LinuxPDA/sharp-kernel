/* Created by ISDCorp 1999 www.isdcorp.com */
/*
** The head file is for touch screen
*/
#define TS_DEV		"/dev/ts"

#ifdef CONFIG_ARCH_LINKUP

#define MIN_X_THRESHOLD         1700
#define MAX_X_THRESHOLD         31600
#define MIN_Y_THRESHOLD         3600
#define MAX_Y_THRESHOLD         28600

#define MAX_POS_X       239
#define MAX_POS_Y       319
#define MIN_POS_X       0
#define MIN_POS_Y       0

#else

#define MIN_X_THRESHOLD		1700
#define MAX_X_THRESHOLD		31600
#define MIN_Y_THRESHOLD		3600
#define MAX_Y_THRESHOLD		28600

#define MAX_POS_X	639
#define MAX_POS_Y	239
#define MIN_POS_X	0
#define MIN_POS_Y	0

#endif

typedef struct {
#if 0
	int	x;	/* MIN_POS_X and MAX_POS_X */
	int	y;	/* MIN_POS_Y and MAX_POS_Y */
#else
	short pressure;
	short x;
	short y;
	short millisecs;
#endif
} ts_pos_t;

