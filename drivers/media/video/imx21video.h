#ifndef _IMX21VIDEO_H_
#define _IMX21VIDEO_H_

#define DEVNAME			"imx21video"

#define RGB565_FORMAT(h,w)	((h) * (w) * 2)
#define MAX_IMX21VIDEO		1

#ifndef O_NONCAP
#define O_NONCAP		O_TRUNC
#endif

#define FBUFFERS		2
#define FBUF_PAGES		7
#define FBUF_SIZE		(PAGE_SIZE << FBUF_PAGES)

typedef struct imx21video_win
{
    int w, h;
} imx21video_win_t;

typedef struct imx21video_gbuf
{
    int stat;
#define GBUFFER_UNUSED		0
#define GBUFFER_GRABBING	1
#define GBUFFER_DONE		2
#define GBUFFER_ERROR		3
    struct timeval tv;

    u16 width;
    u16 height;
} imx21video_gbuf_t;

typedef struct imx21video_dev
{
    struct video_device vdev;
    struct video_picture picture;
    struct video_audio audio_dev;

    unsigned int nr;
    
    spinlock_t slock;
    struct semaphore lock;
    int user;
    int capuser;
    
    int channels;
    int audios;
    
    struct imx21video_win maxwin;
    struct imx21video_win minwin;

    int cur_fream;
    wait_queue_head_t capq;
    struct imx21video_gbuf gbuf[FBUFFERS];
    char *fbuffer;
} imx21video_dev_t;

#endif	/* _IMX21VIDEO_H_ */
